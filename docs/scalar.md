# Scalar panel

How the scalar update works, what it costs, and why it runs on the GUI thread.

---

## How it works

`scalarThread` (a `TimingThread`) fires every 2 s. The slot, `MainWindow::UpdateScalar()`
(`mainwindow.cpp:1460`), is queued to and **runs on the GUI thread**. Per tick:

1. returns at once if the scalar window is hidden (`:1463`); see *Known gaps*;
2. for each board, one after another: `digi->ReadStat()` (`:1498`), which is 64 ×
   `GetValue(SelfTrgRate)` plus one `ReadData` on the stats handle (`ClassDigitizer2Gen.cpp:774`),
   then `setText` on two line edits per channel and the file-size label;
3. one Influx POST for all boards (`:1557`), if Influx is configured;
4. `UpdateStatus()` on the settings panel and `UpdateThreshold()` on the SOLARIS panel, if those
   are open (`:1562`, `:1564`).

The poller is started by `OpenScaler()` and `StartACQ()`, and stopped in `CloseDigitizers()`.
**The scope neither starts nor stops it.** The scope's `TellACQOnOff` only flips the scalar's
"ACQ On/Off" label, so during a scope run the scalar updates only if its window is open.

---

## Cost per tick

Measured with `tests/scalarTiming` on a VX2745 through the app's own `Digitizer2Gen`, 100 samples
per condition:

| Per board | mean | median | p95 |
|---|---|---|---|
| `ReadStat`, no run | 11.2 ms | 11.2 | 12.3 |
| `ReadStat`, run, no data | 11.4 ms | 11.2 | 11.6 |
| `ReadStat`, run, test pulse at 60 k hits/s | 11.3 ms | 11.3 | 11.7 |

Almost all of it is the 64 `GetValue` calls (~0.18 ms each). Data load makes no difference.

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

## Window layout: one container per column

`SetUpScalar()` (`mainwindow.cpp:1343`) puts each column (the channel numbers, then one per board)
in its own container widget with a spacing-0 grid. The line edits are **not** all direct children
of `scalarLayout`.

Why: Qt paints sibling widgets with one recursion level per sibling
(`QWidgetPrivate::paintSiblingsRecursive`). With all 128 line edits per board plus labels under one
parent, 16 or more boards overflow the default 8 MB stack and segfault on the first paint
(`tests/scalarTiming --paint-only --flat`). Per container it is ~130 siblings at most; 40 boards
paint without problems.

Details that keep the rows aligned:
- containers are added `Qt::AlignTop`, so a board with fewer channels keeps its natural height and
  is not stretched out of line;
- the channel-number labels are fixed to a `QLineEdit`'s height, so label row *ch* lines up with
  row *ch* of every board (`--mixed` checks this with a 32-channel board: 0 px offset);
- `CleanUpScalar()` deletes the containers, which owns everything in them. `leTriggers` /
  `leSaved` / `lbFileSize` are only cleared, never deleted through.

---

## What CAEN_FELib tolerates (measured)

`tests/testFELibConcurrency`, raw FELib (no app code), each phase run serialized as a control and
then fully concurrent, 60 s per phase, 8 threads:

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
  So `hit->dataType` can be read from another thread across a start-run (`tests/hitReuse` checks
  re-call safety under ASan/UBSan).
- The lightest async design is **one** worker for all boards, not one per board. FELib serializes
  per connection anyway; the only gain is that a dead board cannot freeze the GUI.

---

## Known gaps

- **Closing the scalar window stops the Influx stream.** `UpdateScalar` returns before the POST when
  the window is hidden (`:1463`). The fix needs no threading: compute the rates and post before the
  visibility check, and skip only the `setText` calls.
- **A dead board freezes the GUI** for 64 × the CAEN timeout per tick.
- `UpdateStatus()` (LED/ACQ status, temperatures) is also a blocking hardware read on the GUI
  thread while the settings panel is open.
- The board has been seen not to answer `/cmd/armacquisition`, with the call blocking indefinitely.
  It answered normally afterwards. Cause unknown.

---

## Test-station quirks

With the internal test pulse on ch 0–3 (ch 0–3 configured identically):
- **ch0 fires at exactly 50 Hz with energy 3531**, while ch1–3 follow the 10 kHz pulse at 2173. In
  `Aux/testRawDecoder` ch0 gives no hits at all, although its counters show ~9948 triggers.
- ch52 produces ~549 stray hits (E ≈ 10275) in some runs.
- So `testRawDecoder` Checks 1 and 4 fail because of board state, not the decoder. Energy checks
  must use one reference per channel.

---

## Tools

All in `tests/`. Hardware tools need the board free (DAQ GUI closed); check with
`ss -tn | grep 192.168.0.100` first.

| Tool | Hardware | Build | What |
|---|---|---|---|
| `scalarTiming` | yes (`--paint-only`: no) | `tests/build_race.sh` (needs `make` first) | per-board `ReadStat` cost in 3 conditions + paint cost; `--boards N` sets the estimate size, `--pulse` adds test-pulse load (saves/restores/verifies ch 0–3 regs), `--flat` paints a single-parent layout for comparison, `--mixed` checks row alignment with a 32-ch board |
| `testFELibConcurrency` | yes | `g++ -std=c++17 -O2 -pthread testFELibConcurrency.cpp -o testFELibConcurrency -lCAEN_FELib` | serialized-vs-concurrent FELib test; `--pulse` acquisition phase, `--solo` data reader alone (control for data anomalies), `--skip-get` |
| `hitReuse` | no | `g++ -std=c++17 -g -O1 -fsanitize=address,undefined -I.. hitReuse.cpp -o hitReuse` | `SetDataType` re-call safety under ASan |

Run Qt tools with `QT_QPA_PLATFORM=offscreen`. Board-mutating runs write a rescue file to the cwd
and delete it after a verified restore. Never pipe them into `head`/`less`.
