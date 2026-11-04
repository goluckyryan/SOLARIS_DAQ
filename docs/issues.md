# Open issues

Known problems and unverified behaviour in the current code. When an item is fixed, delete it.
Scalar-specific gaps are listed in `scalar.md` (*Known gaps*).

---

## Raw format: every stop-run is reported as a lost data tail

In Raw mode, after `StopACQ`, `Digitizer2Gen::ReadData()` keeps returning timeouts and never
`CAEN_FELib_Stop`, even after the board's Stop Run event has arrived. `MainWindow::StopACQ()`
waits at most `drainTimeoutSec` (2 s) for each `ReadDataThread` to end on `Stop`. When the wait
expires, it force-stops the thread, logs "not drained … data tail is lost" in red, and adds the
board to `drainFailedSN`, which goes into the stop comment and the elog.

So, as far as can be told from the code and a direct Raw read loop on the test station, a Raw
saving run always ends with that warning and the run record says its tail was lost, even when it
was not. Not yet confirmed in the GUI. DPP formats are not affected.

Possible fix: in the Raw path, end the read loop once the Stop Run special event has been decoded
(`RawDecoder::ParseBlob` sees it at the top level), instead of waiting for `CAEN_FELib_Stop`.

---

## `Histogram1D`: half a bin is not drawn at each end

Each bin is one vertex at its centre, drawn with `QCPGraph::lsStepCenter`. The polyline starts at
`xMin + 0.5*dX` and ends at `xMax - 0.5*dX`, but the x axis range is `[xMin, xMax]`. So the outer
half of the first and last bins is blank. The counts are correct and the tooltip shows them.

At 1000 bins the gap is 0.05% of the width. On a 20-bin histogram it is 2.5% at each end, and the
end bars look clipped.

Possible fix: widen the axis by half a bin on each side, wherever the range is set:

```cpp
xAxis->setRangeLower(xMin - 0.5*dX);
xAxis->setRangeUpper(xMax + 0.5*dX);
```

Adding phantom vertices outside the range would instead need an index offset in every accessor.

---

## Not yet checked on hardware

These behaviours build and the GUI launches, but nobody has confirmed them on a board.

Without hardware (dummy digitizers, GUI launched as `env -i HOME=$HOME DISPLAY=$DISPLAY ./SOLARIS_DAQ`):
- the dummy/disconnected paths in the SOLARIS panel and the settings panel, including setting a
  coincidence time while `coinTime` is empty (reachable only with no hardware).

On a live board:

| Area | Check |
|---|---|
| SOLARIS panel trigger mode | Cycle all five trigger modes; the combo must read back what was written. Include a single-channel detector group. |
| SOLARIS panel Refresh | Change a threshold in the digitizer settings panel, press "Refresh Settings" on the SOLARIS panel; the new value must appear (`RefreshSettings()` re-reads every connected board). |
| Coincidence time with a dummy | One dummy board ahead of a real one, set a coincidence time; the real board must take it. |
| Channel status lamps | Disable one channel; only that channel's lamp row may change. |
| VGA tab | On a non-VX2745 board, `bdVGA` must stay disabled across an ACQ stop/start. Check the tabs for anything else deliberately disabled. |
| Inquiry / Copy tab | Two boards, ACQ started on one only; the Inquiry/Copy box must be disabled. |
| File-write warning | Point a run at a small data path (e.g. a tmpfs) and fill it. The red warning must appear and repeat about every 10 s with a growing count, with ACQ still On. The next run must start clean, with its first file numbered `_000`. |
