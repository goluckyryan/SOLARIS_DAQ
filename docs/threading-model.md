# Threading and data ownership

Every thread in the DAQ, what state it owns, and what is safe to call from where.

`online-analysis-design.md` covers the analyzer's two threads and the reasoning behind them. This is
the whole-system view.

---

## The threads

| thread | count | created | what it does |
|---|---|---|---|
| **GUI** | 1 | Qt | everything with a widget in it |
| **`ReadDataThread`** | one per board | `mainwindow.cpp:843` | tight loop on `digi->ReadData()`, and `SaveDataToFile()` when saving. `QThread::HighestPriority`. **The only producer** for that board's rings |
| **`scalarThread`** | 1 | `mainwindow.cpp:99` | a `TimingThread`; fires `MainWindow::UpdateScalar` every 2 s |
| **`updateTraceThread`** | 1, while the Scope is open | `scope.cpp:67` | a `TimingThread`; fires `Scope::UpdateScope` |
| **`SingleSpectra` worker** | 1, while that window exists | `SingleHistograms.cpp:334-339` | drains the per-channel `ringBuffer` into atomic count arrays |
| **Analyzer builder worker** | 1, while the Analyzer exists | `Analyzer.cpp` | `OnlineEventBuilder::BuildEvents()` → `EventRing` |
| **Analyzer analysis worker** | 1, same | `Analyzer.cpp` | drains `EventRing` → `ProcessEvent` → atomic counts |

Note what `TimingThread` actually does (`CustomThreads.h:95-128`): it sleeps in 100 ms steps and
emits `TimeUp()`. Because the receiver lives in the GUI thread, Qt queues the signal, so
**`UpdateScalar` and `UpdateScope` run on the GUI thread, not on the timing thread.** The thread is
only a clock. Its 100 ms granularity is also why `SingleSpectra` uses a `QTimer` plus `moveToThread`
instead — it needs 50 ms.

## Who owns what

| state | producer | consumers | protection |
|---|---|---|---|
| `digi[i]->ringBuffer[ch]` (HitSummary) | `ReadDataThread` | `SingleSpectra` worker; `UpdateScalar` reads only `index()` | lock-free `RingBuffer` |
| `digi[i]->traceRingBuffer` | `ReadDataThread` | `Scope`, on the GUI thread | lock-free `RingBuffer` |
| `digi[i]->hitRing` (LeanHit) | `ReadDataThread` | Analyzer builder worker | lock-free `RingBuffer` |
| `EventRing` | Analyzer builder worker | Analyzer analysis worker | lock-free, `Reader` holds the cursor |
| histogram count arrays | Analyzer analysis worker, `SingleSpectra` worker | GUI thread publishes | `std::atomic<uint32_t>` |
| `digi[i]->hit` | `ReadDataThread` | **nobody else may touch it** | convention only |
| digitizer settings (`ReadValue`/`WriteValue`) | GUI, Scope, settings panel | — | **`digiMTX[i]`** |
| `realTime`/`deadTime`/`triggerCount`… | `ReadDataThread` (raw path) and `ReadStat()` | `UpdateScalar` | **none** (see *Known rough edges*) |

`digiMTX[MaxNumberOfDigitizer]` (`mainwindow.cpp:31`, declared `extern` in `CustomThreads.h:14`) is
the only mutex, and it guards **hardware access**, not data. Held across network round-trips, so
never take it on a data path — the rings are lock-free precisely so nothing on the hot path has to.

## The `RingBuffer` contract

Single producer, multiple consumers, no locks. `push()` does a relaxed load, writes the slot, then a
**release** store of the index; `index()` is an **acquire** load. `advance()` is deliberately a plain
load-and-store rather than `fetch_add`, so the DAQ hot path pays no lock-prefixed instruction — which
is only correct because there is exactly one producer. **Never add a second writer to an existing
ring.**

A consumer must detect being lapped. The documented discipline (`RingBuffer.h:8-13`): read `index()`,
copy with `at()`, re-read `index()`, discard if the producer advanced by `size()` or more.

Two idioms exist in the codebase and they are not equivalent:

- **`scope.cpp:785-792`** — copy, re-read, reject. Correct.
- **`SingleHistograms.cpp:1094-1110`** (`SingleSpectra::FillHistograms`) — checks the gap once, then drains. The producer can lap it
  *inside* the loop. Tolerable for histogram entries; **not** for event building, which is why
  `OnlineEventBuilder::PullNext()` re-checks per copy.

## The surprise: the Scope is a second owner of the DAQ threads

`Scope` is constructed with `ReadDataThread **` (`scope.cpp:15`) and drives **the same threads**
`MainWindow::StartACQ` uses. `StartScope()` (`scope.cpp:654-723`):

```cpp
digi[iDigi]->SetDataFormat(DataFormat::ALL);   // forces the format
digi[iDigi]->traceRingBuffer.clear();          // this run's traces only
digi[iDigi]->StartACQ();
readDataThread[iDigi]->SetSaveData(false);     // scope runs never save
readDataThread[iDigi]->start(QThread::HighestPriority);
...
emit TellACQOnOff(true);
```

So there are **two independent paths that start and stop acquisition**, and the Scope silently
overrides the data format to `ALL` because it needs traces.

`TellACQOnOff` is how the rest of the GUI finds out. `MainWindow` connects it
(`mainwindow.cpp:1101-1114`) to update the ACQ indicator, start/stop `SingleSpectra` and write the
InfluxDB start/stop point. **Any new
consumer of acquisition state must hook both paths** — `StartACQ`/`StopACQ` *and* this signal — or it
silently misses every scope-driven run.

The Analyzer deliberately does **not**: it is driven by its own enable checkbox and is independent of
ACQ entirely.

## Stop ordering, and why it is the way it is

`MainWindow::StopACQ` (`mainwindow.cpp:601-723`) does things in an order that matters:

1. quiesce consumers first — `singleSpectra->stopTimer()`
2. stop the boards under `digiMTX`
3. **join the read threads** — for a **save run** `Stop()` is deliberately *not* called first, so the
   loop drains until FELib returns `CAEN_FELib_Stop`. The wait is capped at `drainTimeoutSec` (2 s,
   `mainwindow.cpp:667`): a board whose link died never returns `Stop`. On timeout the thread is
   force-stopped, the tail is lost, and the board is recorded in the stop comment and the elog.
4. only then close the files, so sizes are final
5. elog and `endRunScript.sh` last

After step 3 every ring is static, which is what lets `Scope::DrawTraceFromBuffer` read with no lap
check at all (`scope.cpp:858-880`).

## Rules of thumb

- **Never touch a Qt widget off the GUI thread.** Not even indirectly: `Histogram1D::Fill()` writes a
  `QString` into a `QCPItemText`, which is a use-after-free against the GUI thread's `draw()`. Workers
  count into atomic arrays and the GUI thread publishes them; see `online-analysis-design.md`.
- **Never take `digiMTX` on a data path.**
- **Never add a second producer to a `RingBuffer`.**
- **Hook both ACQ paths**, or handle neither and use your own control like the Analyzer does.
- Prefer marshalling a call to the thread that owns the state (`Qt::BlockingQueuedConnection`) over
  inventing a flag. The Analyzer does this. `SingleSpectra` instead uses a `suspendFilling` flag and
  `WaitForFillToDrain()`, which gives up after 200 ms.

## Known rough edges

- `realTime`/`deadTime`/`triggerCount`/`savedEventCount` have no synchronisation. Under Raw the
  `ReadDataThread` writes them (from the decoder's time/counter events) while `UpdateScalar` reads
  them on the GUI thread. Under the DPP formats both the write (`ReadStat()`) and the read are on
  the GUI thread.
