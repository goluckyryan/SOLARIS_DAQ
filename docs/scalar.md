# Scalar panel

How the scalar update works, how the Accepted rate is calculated, what it costs, and why it runs
on the GUI thread.

---

## How it works

`scalarThread` (a `TimingThread`) fires every 2 s. The slot, `MainWindow::UpdateScalar()`
(`mainwindow.cpp:1466`), is queued to and **runs on the GUI thread**. Per tick:

1. returns at once if the scalar window is hidden (`:1469`); see *Known gaps*;
2. for each board, one after another: `digi->ReadStat()` (`:1504`), which is 64 ×
   `GetValue(SelfTrgRate)` plus one `ReadData` on the stats handle (`ClassDigitizer2Gen.cpp:774`);
   then the Accepted rate for every channel (next section); then `setText` on two line edits per
   channel and the file-size label;
3. one Influx POST for all boards (`:1567`), if Influx is configured;
4. `UpdateStatus()` on the settings panel and `UpdateThreshold()` on the SOLARIS panel, if those
   are open (`:1572`, `:1574`).

Each board has two columns:

| Column | Meaning | Source |
|---|---|---|
| **Triggers [Hz]** | how often the channel's discriminator fires, before any coincidence | the board's `SelfTrgRate` parameter |
| **Accepted [Hz]** | how many hits per second actually reach the PC, after any hardware coincidence/veto | counted on the PC, see below |

The same numbers go to Influx as `Rate` and `AccpRate`, and to the elog as `<Bd:Ch:TrigRate>` and
`<Bd:Ch:AcceptRate>`.

The poller is started by `OpenScaler()` and `StartACQ()`, and stopped in `CloseDigitizers()`.
**The scope neither starts nor stops it.** The scope's `TellACQOnOff` only flips the scalar's
"ACQ On/Off" label, so during a scope run the scalar updates only if its window is open.

---

## How Accepted is calculated

**In one sentence:** count the hits the PC receives on each channel, and every 2 s divide the
increase in that count by the time that passed.

**1. Counting.** Every time `Digitizer2Gen::ReadData()` gets a hit, it pushes it into that
channel's ring buffer, `ringBuffer[ch]` (DPP formats `ClassDigitizer2Gen.cpp:1074`, Raw `:1016`).
The ring's write index is a running total of every hit ever pushed on that channel. It only goes
up: it is never cleared, and it keeps counting when the 10 000-entry ring wraps around.
`GetAcceptedCount(ch)` (`ClassDigitizer2Gen.h:164`) returns it. `ReadData()` does no extra
work for it. It is the same counter `SingleSpectra` fills its histograms from, so the scalar and the
histograms see the same hits.

*Why the ring does not wrap the count.* `RingBuffer::push()` (`RingBuffer.h:22`) stores the hit
in slot `writeIndex % N` but increments `writeIndex` itself without any modulo, and `index()`
returns that raw value. So when the ring laps, old hit *data* is overwritten, but the *count* keeps
going: 0, 1, …, 10 000, 10 001, …. Measured: with a channel taking up to 200 000 hits/s (about
40 ring laps between two scalar ticks), Accepted still matched the board's saved rate within 0.5%,
and the count matched an independent hit-by-hit count exactly.
- Overflow: `writeIndex` is `unsigned long`, 64-bit on Linux. At the 7.4 M hits/s ceiling it
  overflows after ~79 000 years. (On Windows `unsigned long` is 32-bit; the app is Linux-only.)
- Reset: `RingBuffer::clear()` sets the index back to 0, but nothing calls it on `ringBuffer[ch]`.
  If something ever does, the "count went down" rule below turns it into one 0 reading, not a
  garbage rate.

**2. Rate.** In `UpdateScalar()` (`mainwindow.cpp:1511`), for each board:

```
now      = steady_clock (PC time), read once per board
dt       = now - oldAcceptedTimeNs[board]
for each channel:
  count  = GetAcceptedCount(ch)
  rate   = (count - oldAcceptedCount[board][ch]) / dt      [Hz]
  oldAcceptedCount[board][ch] = count
oldAcceptedTimeNs[board] = now
```

The rate is shown as 0 when:
- it is the first tick after start-run or after opening the digitizers. There is no starting point
  yet; this tick only records one. `oldAcceptedTimeNs` is reset to 0 in both places (`:495`, `:865`);
- less than 1 s has passed since the last tick;
- the count went down (should not happen; it is a safety check).

**3. Why the PC clock, not the board's real-time counter.** The hits are counted when they arrive at
the PC. The board's `REAL_TIME` belongs to the stats snapshot, not to the moment the hits were read,
so the PC clock is the matching time base.

**Why not the board's saved counter.** The board's `SAVED_EVENT_CNT` counts an event **before** the
coincidence check. On the test station, ch0 needs a coincidence with TRG-IN (ch0 = 10 + 5 Hz,
ch52 = 50 + 5 Hz, ch52 -> ITLA -> TRG-OUT -> TRG-IN). Measured on the test station:

| ch | Triggers | board saved counter | Accepted (hits received) |
|---|---|---|---|
| 0  | 15.0 | 15.0 | **5.0** |
| 52 | 55.0 | 55.0 | 55.0 |

The board counter is still read: the elog's `<SavedCount>` and `<Realtime>` use it.

**Things to know**
- **Short-term lumpiness.** The board sends data in batches, so a single 2 s reading can be a little
  off (one reading of 5.5 instead of 5.0 in the test). The average is right.
- **Readout ceiling.** Accepted can only count hits the PC has read. If the board produces more than
  one readout thread can take, Accepted shows the readout rate, which is what was really received.
  See *Accepted rate ceiling* below.
- **No run, no count.** Accepted is 0 when no run is going (`ReadData` is not running).
- **File errors do not lower it.** If saving to disk fails, the hits were still received, so they
  still count. The DAQ log warns about file errors separately.
- **Hidden window.** While the scalar window is hidden, `UpdateScalar` returns early and the
  baseline is not updated. The first reading after reopening is the average over the whole hidden
  time.

---

## Cost per tick

Measured on a VX2745 through the app's own `Digitizer2Gen`, 100 samples per condition:

| Per board | mean | median | p95 |
|---|---|---|---|
| `ReadStat`, no run | 11.2 ms | 11.2 | 12.3 |
| `ReadStat`, run, no data | 11.4 ms | 11.2 | 11.6 |
| `ReadStat`, run, test pulse at 60 k hits/s | 11.3 ms | 11.3 | 11.7 |

Almost all of it is the 64 `GetValue` calls (~0.18 ms each). Data load makes no difference.

The Accepted calculation itself (measured without hardware, 64 channels):

| Per board per tick | median | p99 |
|---|---|---|
| Accepted, no data arriving | 0.10 µs | 0.10 µs |
| Accepted, DAQ thread writing all 64 rings flat out | 0.32 µs | 0.44 µs |

That is about 0.003% of the 11 ms `ReadStat`. Reading the counters does not slow the DAQ thread at
the real cadence: its push rate was 624 M/s with no reader and 633 M/s with a reader 1000 times more
often than the real 2 s (noise). Only a reader spinning nonstop slowed it (−72%).

Painting (offscreen Qt, real window structure): 20 boards = 8.6 ms `setText` + 38 ms repaint.

**GUI block per tick, 20 boards: ≈ 20 × 11.3 + 46 ≈ 0.27 s, about 14% of the 2 s tick.** About one
`GetValue` in 50 takes ~215 ms instead of ~0.2 ms. If that lands in a tick it adds ~0.2 s, so the
worst case is ~0.5 s. That is well inside the tick, which is why the update stays on the GUI
thread.

Not included:
- **Influx**: skipped entirely when not configured. With a server, one POST of ~2560 lines
  (~115 KB) is estimated at 10–30 ms on a LAN (not measured). A dead server is capped by the curl
  timeout, 1 s connect / 3 s total (`ClassInfluxDB.cpp:270`).
- **A dead or unplugged board**: each of its 64 `GetValue` calls waits the CAEN timeout, so the GUI
  freezes for 64 × timeout per tick.
- 20 boards' DAQ threads loading the host and network at once (measured with one board).

---

## Accepted rate ceiling

Measured with the internal test pulse on ch 1–63 (ch0 and ch52 untouched),
4 s per step, rates from the second half. VX2745 on 10 GbE.

| Case | Max Accepted per board |
|---|---|
| Minimum format (app default), not saving | **7.37 M hits/s** |
| Minimum format, saving to file | **7.1 M hits/s** |
| Everything format, `WaveSaving = OnRequest` (normal: no traces sent outside the scope) | same as Minimum (checked to 3.2 M/s) |
| Everything format, `WaveSaving = Always`, 32 µs traces | **~14.6 k hits/s** |

Below the ceiling, Accepted matched the board's saved rate within ~0.5%, up to 6.2 M hits/s.
Above it, Accepted stays flat while the board keeps saving more (12.4 M/s in, 7.37 M/s read).
If that lasts, the board's buffer fills and the board drops events itself (Saved < Triggers).

The ceiling is the single readout thread on the PC, not the counter and not the network. Minimum
costs ~136 ns per `ReadData()` call. With traces, every trace is unpacked and copied, and that run
already moved ~1.9 Gb/s, more than a 1 Gb link could.

---

## Window layout: one container per column

`SetUpScalar()` (`mainwindow.cpp:1349`) puts each column (the channel numbers, then one per board)
in its own container widget with a spacing-0 grid. The line edits are **not** all direct children
of `scalarLayout`.

Why: Qt paints sibling widgets with one recursion level per sibling
(`QWidgetPrivate::paintSiblingsRecursive`). With all 128 line edits per board plus labels under one
parent, 16 or more boards overflow the default 8 MB stack and segfault on the first paint.
Per container it is ~130 siblings at most; 40 boards
paint without problems.

Details that keep the rows aligned:
- containers are added `Qt::AlignTop`, so a board with fewer channels keeps its natural height and
  is not stretched out of line;
- the channel-number labels are fixed to a `QLineEdit`'s height, so label row *ch* lines up with
  row *ch* of every board (measured with a 32-channel board next to 64-channel ones: 0 px offset);
- `CleanUpScalar()` deletes the containers, which owns everything in them. `leTriggers` /
  `leAccepted` / `lbFileSize` are only cleared, never deleted through.

---

## What CAEN_FELib tolerates (measured)

Measured with raw FELib calls (no app code), each phase run serialized as a control and then fully
concurrent, 60 s per phase, 8 threads:

| Phase | errors | wrong values | data anomalies |
|---|---|---|---|
| GetValue, all threads on one handle | 0 | 0 | – |
| GetValue, one child handle per thread | 0 | 0 | – |
| ReadData(ep) + ReadData(stats) + 8 × GetValue + SetValue, test pulse on | 0 | 0 | 0 (1.8 M hits) |

- No corruption or deadlock with concurrent calls on one connection. This is evidence, not proof:
  races are probabilistic.
- Concurrent `GetValue` is only ~7–17% faster than serialized (~1200/s): **FELib queues control
  calls per connection.** Extra threads do not read one board faster; only separate boards
  (separate connections) run in parallel.
- `ReadData(stat_handle)` returns instantly (3.6 M calls/s, no timeouts): a local snapshot, not a
  network read.
- The FELib header says `SetReadDataFormat` is not thread safe on the same handle, and
  `Open`/`Close` are not thread safe at all.

---

## Constraints on moving `ReadStat()` off the GUI thread

All of the following is safe because it runs on the GUI thread. Each item becomes a problem if
`ReadStat()` runs on another thread.

- **`chValue` is a `std::string` cache.** `ReadValue()` assigns `chValue[ch][index] = ans`
  (`ClassDigitizer2Gen.cpp:147`). One writer thread plus one reader is a torn read at worst: rate
  strings fit the 15-char SSO buffer, and only long error strings use the heap. **Two writers can
  double-free**, and there are other writers of the same entry. `SelfTrgRate` is entry 0 of the
  channel list (`DigiParameters.h:639`), so `ReadAllSettings()` rewrites it. That is called from the
  scope (`scope.cpp:130`), `SOLARISpanel` (`:659`) and the settings panel
  (`digiSettingsPanel.cpp:2317`). The scope also calls `ReadValue(SelfTrgRate)` directly
  (`scope.cpp:775`).
  → A worker must read into its own snapshot and let the GUI thread write `chValue`.
- **The scope reads the trigger rate live**, `ReadValue` every 0.1 s (`MiniTraceUpdateTimeSec`). If
  it read from memory instead, something would have to refresh memory during scope runs; the scalar
  is not tied to the scope.
- **Start-run re-configures the stats handle.** `SetDataFormat()` calls
  `SetReadDataFormat(stat_handle)`, which must not overlap another call on that handle.
  → `digiMTX[i]` around `SetDataFormat` (in `StartACQ` and `scope.cpp:699`) and around the worker's
  read.
- **`hit` lives for the life of the digitizer object.** `SetDataFormat()` creates it once
  (`ClassDigitizer2Gen.cpp:544`) and re-calls `Hit::SetDataType()`, which zeroes the fixed probe
  arrays instead of reallocating them. The 20 MB raw buffer is allocated on first Raw use and kept.
  So `hit->dataType` can be read from another thread across a start-run (re-calling
  `SetDataType()` is clean under ASan/UBSan).
- The lightest async design is **one** worker for all boards, not one per board. FELib serializes
  per connection anyway; the only gain is that a dead board cannot freeze the GUI.

---

## Known gaps

- **Closing the scalar window stops the Influx stream.** `UpdateScalar` returns before the POST when
  the window is hidden (`:1469`). The fix needs no threading: compute the rates and post before the
  visibility check, and skip only the `setText` calls.
- **A dead board freezes the GUI** for 64 × the CAEN timeout per tick.
- `UpdateStatus()` (LED/ACQ status, temperatures) is also a blocking hardware read on the GUI
  thread while the settings panel is open.
- The board has been seen not to answer `/cmd/armacquisition`, with the call blocking indefinitely.
  It answered normally afterwards. Cause unknown.

---

## Test-station quirks

With the internal test pulse on ch 0–3 (ch 0–3 configured identically):
- **ch0 fires at exactly 50 Hz with energy 3531**, while ch1–3 follow the 10 kHz pulse at 2173.
- ch52 produces ~549 stray hits (E ≈ 10275) in some runs.
- So energy checks must use one reference per channel.
