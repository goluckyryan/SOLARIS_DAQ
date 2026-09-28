# The run lifecycle

What happens between pressing Start and Stop, in order, and which parts of that order are
load-bearing rather than incidental.

Both sequences live in `mainwindow.cpp`: `StartACQ()` at `:415`, `StopACQ()` at `:580`.

---

## Start

**1. Run ID and comment — only for a saving run.** `runID` increments and becomes `runIDStr`,
zero-padded to 3 digits. If a manual comment is required, a modal dialog blocks here; **cancelling
decrements `runID` back** (`:456`) so a refused start does not burn a number. A no-save run skips
all of this and logs "Start no-save Run".

**2. Per board, iterating backwards** — `for(i = nDigi-1; i >= 0; i--)` at `:478`. The direction is
deliberate: `digi[0]` is the sync master and must be armed **last**, so the slaves are already
waiting when it starts. Dummies are skipped entirely.

For each board:

- reset the per-channel scaler bookkeeping
- `SetDataFormat(dataFormatID)` from the combo. **This is where the Raw-vs-decoded fork is chosen**,
  and therefore which of the two hit-push paths in `ReadData()` will run
- `WaveSaving` = `Always` for `ALL`/`OneTrace`, else `OnRequest`
- if saving: create the run folder, dump the board's settings to
  `<expName>_<runID>XSetting_<SN>.dat`, and `OpenOutFile()`
- `digi[i]->StartACQ()` — `/cmd/armacquisition` then `/cmd/swstartacquisition`
- `readDataThread[i]->SetSaveData(...)` and `start(QThread::HighestPriority)`

**3. Consumers.** `singleSpectra->startTimer()` (`:520`). The Analyzer is deliberately **not** here —
it is driven by its own enable checkbox and is independent of ACQ.

**4. Bookkeeping.** elog start entry, `expName.sh`, the run-timestamp file, InfluxDB `StartStop`
point.

## Stop

The order matters more here, and three steps are load-bearing.

**1. Stop comment dialog** — blocks in `exec()` **while the DAQ threads are still running**. That is
intentional: data keeps flowing while the operator types.

**2. Quiesce consumers first** — `singleSpectra->stopTimer()` at `:603`, *before* the boards are
touched.

**3. Stop the boards**, backwards again, each under `digiMTX[i]`: `StopACQ()` then `WaveSaving` back
to `OnRequest`. `isACQRunning = false`.

**4. Join the read threads** (`:654-658`) — and here is the subtle part:

```cpp
if( !chkSaveRun->isChecked() ) readDataThread[i]->Stop();   // force-stop ONLY for a no-save run
readDataThread[i]->quit();
readDataThread[i]->wait();
```

For a **save run, `Stop()` is deliberately not called.** The loop is left to drain until FELib
returns `CAEN_FELib_Stop`, which guarantees the tail of the run reaches disk. Force-stopping would
silently truncate every file. (`quit()` has no effect on these threads — they override `run()` and
have no event loop — so the exit is via the flag or that break.)

**5. Close the files** — only now, after the join, so sizes are final. `CloseOutFile()` also
`chmod`s each file read-only.

**6. elog stop entry** — placed here rather than at the "Run stopped" message specifically so the
file sizes it reports are the final ones.

**7. `scripts/endRunScript.sh`**, started detached.

After step 4 every ring buffer is static, which is what lets `Scope::DrawTraceFromBuffer` read with
no lap check at all.

## What a saving run produces

```
<rawDataPath>/[run<ID>/]
├── <expName>_<runID>_<digiIdx>_<SN>_000.sol     one per board, 2 GB rollover
├── <expName>_<runID>_<digiIdx>_<SN>_001.sol
└── <expName>_<runID>XSetting_<SN>.dat           register dump per board
```

`run<ID>/` subfolder only when `isSaveSubFolder` is set. See `sol-file-format.md` for the file
layout and `settings-system.md` for the `.dat`.

## The other way to start acquisition

**The Scope starts and stops the same `ReadDataThread`s independently** of everything above, forcing
`DataFormat::ALL` and never saving. It announces this with `TellACQOnOff`, which `MainWindow` uses to
update the indicator and drive `SingleSpectra`.

So there are two ACQ paths. Anything that needs to know whether data is flowing must hook **both**
`StartACQ`/`StopACQ` and that signal — or, like the Analyzer, deliberately hook neither and use its
own control. See `threading-model.md`.

## Traps

- **`StopACQ` dereferences `readDataThread[i]` with no null check** (`:655`), and it is `NULL` for
  every dummy board (`:816`). One unreachable IP plus a start/stop cycle is a crash.
- **A cancelled start decrements `runID`**, so run numbers stay contiguous — but any external tool
  that latched the incremented value will disagree.
- **`SetDataFormat` is applied per run**, so changing the combo between runs changes the on-disk
  format. Files from one experiment are not necessarily all the same format.
- **`CloseOutFile()` does not null `outFile` after `fclose`**, so two stops in a row double-close.
- The stop-comment dialog blocking while data flows is intentional, but it means a run does not end
  when you press Stop — it ends when you dismiss the dialog.

## Related

- `threading-model.md` — who runs on which thread, and the Scope's second ACQ path
- `sol-file-format.md` — what lands on disk
- `experiment-workflow.md` — run folders, the analysis folder, git
