# analyzers/

Online analyses live here. Each `.cpp` is compiled by `Makefile` into its own shared object in
`build/`, which the **Analyzer window** loads at runtime — the DAQ binary contains no analysis,
and never needs a rebuild when you write one.

```
your file in analyzers/            the Analyzer window (Analyzer.cpp)
+----------------------+           +----------------------------------+
| class MyAnalysis     |  dlopen   | [Rescan][Rebuild][Reload] log   |  host chrome, always here
|   Declare()   -------+---------> | +----------------------------+ |
|   ProcessEvent()     |  vtable   | |  WorkArea: YOUR canvas     | |  <- splitters, boxes, plots,
|   SaveState()        |           | |  (the analysis lays this   | |     and any CLAIMED host
|                      |           | |   out entirely)            | |     controls you asked for
+----------------------+           | +----------------------------+ |
        ^                          |  vault: unclaimed controls     |  hidden, at defaults
        |  one ABI header:         +----------------------------------+
        v
     ../Analysis.h   (ANALYSIS_ABI_VERSION — the host refuses anything else)
```

## Build, select, hot-reload

```bash
make -C analyzers                  # build every analysis  -> build/<name>.so
make -C analyzers build/Foo.so     # one file, ~1 s
```

Inside the running Analyzer window:

- **Rebuild** — runs the Makefile for you, output in the log pane; the running analysis is
  untouched if the compile fails.
- **Reload** — re-dlopens the `.so` (every Reload gets a uniquely-named temp copy, so you can
  never be served a stale handle) and rebuilds the canvas. The DAQ does not restart.
- **Rescan** — re-lists `build/*.so`.

**Which analysis is loaded?** Whatever the **Analysis selector** in the top bar picks; the list
comes from `build/*.so` (Rescan refreshes it). Switching analyses rebuilds the work area with the
chosen one.

## The rules that keep the DAQ alive

1. **No SOLARIS symbols.** `Analysis` and `HistRegistry` are pure abstract; you may only talk
   through the vtables they hand you. Plain Qt is fine (`libQt6*` is a shared library, like
   libc) — splitters, group boxes, buttons, even custom-painted widgets. `Q_OBJECT`/moc is not
   needed and not available: `connect()` to a plain lambda with a widget as the context object.
   The Makefile check proves it: `nm -D -u build/Foo.so` must show only libc / libstdc++ / libQt6\*.
2. **Threading.** `Declare()`, `LoadState()` and any `connect()`ed callback run on the GUI
   thread with the workers parked; `ProcessEvent()`, `OnRunStart()`, `OnRunStop()` on the
   analysis thread. Parameter members (the `int`s behind `ChannelParam`) are written by the host
   **on the analysis thread, between ProcessEvent calls** — plain `int` members are therefore
   correct, no atomics.
3. **Never fill a widget.** You never get one you can draw on: `Hist1D/Hist2D` fill through the
   registry into atomic counter arrays; the GUI thread publishes them. Widgets are only ever
   *placed*, never painted from `ProcessEvent`.
4. **Teardown order.** The host destroys the work area (every widget in your canvas, claimed
   ones included) **before** `DestroyAnalysis`, so a button callback can never fire into a
   half-dead window, and claimed controls are re-parked before they could die with your tree.

## Declare() — what you get

```cpp
void Declare(HistRegistry & reg, const AnalysisContext & ctx) override;
```

- **Plots**: `Hist1D(name, xLabel, nBin, lo, hi, placement)` / `Hist2D(...)` return ids used by
  `Fill1/Fill2` in `ProcessEvent`. `placement` is an opaque `QLayout *` (add the plot there), or
  null and you place the `View1(id)` / `View2(id)` widget yourself. Call them from a button
  callback **during the run** and the plot appears immediately; from `ProcessEvent` the id is
  valid at once and the widget appears at the next publish tick.
- **Pickers**: `ChannelParam(label, &digiVar, &chVar, placement)` — the host builds both combo
  boxes (only the host knows the boards), seeds them from your initialiser values, fences every
  change back through the analysis thread, and **clears counts on change** so a plot never mixes
  two selections. `BoolParam` likewise for one checkbox.
- **Layout**: `WorkArea()` is the content widget — yours completely. Set whatever layout tree
  you want inside it. `SetWindowTitle()`, `ResizeWindow()` dress the window.
- **Host plumbing (v4: the bar is fixed)**: the selector, Enable checkbox, coincidence-window and
  guard-factor spins live permanently in the host's top bar — an analysis can never move or hide
  the control that stops it or rescues it from a broken `.so`. It may drive the *values*:
  `SetTimeWindow(ns)` / `SetGuardFactor(f)` (idle-only, the bar spins follow), read them with
  `TimeWindow()` / `GuardFactor()`, and claim `StatusControl()` — the one widget with no bar cell,
  which only exists on screen once asked for. Guard time is always `factor x window`.
- **Your own persistence**: `SaveState(kv)/LoadState(kv)` — flat numeric/string keys, stored per
  analysis in `state/<name>.json`, saved on deselect/close, restored right after `Declare()`,
  *before* the pickers seed — so restore your members there, not earlier.

## The two examples

- **`Coincidence.cpp`** — the minimal shape: A/B channel pickers, one E(A) x E(B) matrix in a
  splitter layout. Claims nothing: the bar's Enable checkbox and spins just work for it.
- **`MUSICAnalyzer.cpp`** — the maximal shape (port of FSUDAQ's MUSIC): strip-array matrices
  (Left / Right / L+R-sum / multiplicity), a gain-match table persisted through SaveState, a
  builder-window request via `SetTimeWindow(10000)`, and the status label claimed into its rail.

## Testing without hardware or a GUI

```bash
make -C Aux testOnlineBuilder
./Aux/testOnlineBuilder --synth --boards 2 --coinc 20000 --seconds 1 --window 200 \
                        --analysis "MUSIC"
```

links every analysis statically through the same `ANALYSIS_ENTRY` macro, replays real `.sol`
files (`--replay run_x.sol`) or synthetic coincidences, and prints a fill tally per histogram.
Analyses must therefore survive `WorkArea() == nullptr` — declare first, arrange only if there
is a canvas. (This is also why `ANALYSIS_ENTRY` is one-per-file: the plugin entry points are
fixed `extern "C"` names.)

## ABI versioning

`../Analysis.h` is the contract (vtables, `AnalysisContext`, `BuiltHit`); the `.so` exports
`AnalysisABIVersion()` and the host refuses a mismatch with a log line instead of crashing in a
shifted vtable slot. Editing the ABI header rebuilds every analysis — that is the Makefile's
`$(ABI)` dependency doing its job, not a bug.
