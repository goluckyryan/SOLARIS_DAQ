# Plan: dummy digitizers as signal generators

**Status: approved, not yet implemented.** Written before any code, and kept here so the reasoning
survives — including the four findings that changed the design after the first draft. See
`online-analysis-design.md` for the decisions behind the pipeline this plugs into.

## Context

A dummy `Digitizer2Gen` is created for every IP that fails to connect. Until now its only job was to
hold a settings set and round-trip it to a file without hardware — documented as *"a settings
object, never a data source"*.

This inverts that. A dummy gains an optional **signal generator**: per-channel Poisson hits with
Gaussian energies on a shared clock, fed into the real pipeline — `hitRing` → `OnlineEventBuilder`
→ `EventRing` → the Analyzer. With no beam and no hardware you can exercise the whole online chain,
including the cross-board merge, which has never run on a digitizer.

The replacement rule is better than the old one: **what matters is whether a board produces data,
not whether it is a dummy.**

It is not the large restructure it looks like — `ReadData()` is where a board becomes hits, so
generating there leaves the thread model, rings, builder, event ring and analyzer untouched. But it
is larger than *just* a generator, because several places quietly assume "dummy ⇒ silent", and three
of them break the feature outright.

## What has to be fixed before the feature can work at all

**1. With no hardware, there is no GUI to use.** [mainwindow.cpp:821](mainwindow.cpp#L821) gates on
`nDigiConnected > 0` — so with every IP bogus, which is the whole point of a generator, there is no
Start ACQ button, no SingleSpectra and no `Analyzer` object at all. Also
`maxNumChannelAcrossDigitizer` is only updated inside the `IsConnected()` branch, so it stays 0
while `SetUpScalar()` still lays out channel rows.

**2. `hit` is `NULL` for a dummy**, because it is only allocated in `SetDataFormat()`. So
[CustomThreads.h:38](CustomThreads.h#L38)'s `digi->hit->ClearTrace()` is a null dereference the
moment a dummy ever returns `Stop`. That makes "never return `Stop`" a *memory-safety invariant*,
not merely a save policy — far too fragile to leave implicit. Guard `hit`, and give `SetDataFormat`
a dummy branch that allocates it without touching CAEN.

**3. The dummy would be invisible where it matters.** `boardIndex` has four consumers and only one
is the builder. `QtHistRegistry` holds its **own copy** ([Analyzer.cpp:550](Analyzer.cpp#L550)) and
that is what populates the Channel A / Channel B combos. Refreshing only the builder's views would
put the dummy in the event stream but leave it **unselectable in the Coincidence tab** — the feature
would look finished and be useless for the one plot it exists to produce. `Analyzer::Reload()`
already does the right dance (disable → rebuild registry and widgets → re-enable); reuse it.

## Design

### The seam

`if( isDummy ) return GenerateDummyHits();` as the **first line** of `ReadData()`, above the
`FPGAType` guard. Verified: the only consumers of that return value are
[CustomThreads.h:35](CustomThreads.h#L35) and the standalone `Aux/test.cpp`.

### Synthetic data can never reach a .sol file — three independent guards

`ReadDataThread` only saves on `Success`, so returning **`CAEN_FELib_Timeout` (-11)** already
prevents it. But the user said *never*, and each extra guard is one line, so also: `if(isDummy)
return;` at the top of `OpenOutFile()` and `SaveDataToFile()`, and `SetSaveData(false)` for dummies
in `StartACQ`. Then no single future edit can start writing synthetic runs.

### Time base — one origin, captured once

The `steady_clock` origin must be taken **once in `MainWindow::StartACQ`, before the per-board
loop**, and handed to every board. If each board captured its own inside `Digitizer2Gen::StartACQ`,
they would be offset by however long the loop body takes — and that body does `SetDataFormat`, a
64-channel `WaveSaving` write, `SaveSettingsToFile` and `OpenOutFile` over the network, tens of ms.
Against a 100 ns coincidence window that is a factor of 10⁵: the difference between a dt peak and
flat background.

`tick2ns` is **0** for a dummy and `SetDummy` never fixes it, so emit ns directly and never route a
dummy through `* tick2ns`. Set `tick2ns = 8` and a `ModelName` in `SetDummy` anyway, so the panel
stops showing "0.0 ns".

### Cross-board coincidence: a counter-based stream, not a shared queue

A shared mutex-protected schedule is the obvious design and it is **wrong**: whichever board's
thread reaches group *k* first would append hits to *other* boards' queues, and those boards may
already have emitted past that timestamp — producing exactly the out-of-order hits that trip
`monotonicityViolations`.

Instead make group *k* a pure function of `(seed, k)` — `splitmix64(seed ^ k)` giving its time,
multiplicity, board set, channels and energies. Every board independently enumerates the groups in
`[lastFrontier, frontier)` and keeps its own members. No lock, no queue, no shared mutable state
beyond an immutable seed and origin; no lock jitter smearing the dt peak; and a board armed mid-run
needs no special case, because there is no sequential RNG state to fast-forward.

Singles use a per-channel `nextTime[ch]` min-scan merged with that board's group members — naturally
sorted, O(64) per hit, no sort buffer. **Per-board monotonicity is a hard requirement**, not a
nicety.

### Pacing and catch-up

`ReadDataThread` has no sleep of its own, so the generator must sleep — but **until the next due
hit**, clamped to `[0.2 ms, 10 ms]`, not a flat 1 ms. The upper clamp matters because `Stop()` only
sets a flag checked at the top of the loop, so the sleep bound *is* the stop latency and
`StopACQ` does `wait()` with no timeout.

Advance the frontier by `min(now, lastFrontier + 50 ms)` and cap hits per call (~4096). Without
that, a descheduled thread or a slept laptop makes "emit everything due" produce millions of hits in
one call, lapping every ring and freezing the GUI on stop. Count the skipped time so it is visible.

If the consumer stalls, **keep producing** — that is what a real board does and exactly why
`EventRing` exists. No backpressure; a "wait for space" path would hang `StopACQ`.

### Arming, and the view list

`IsDataSource()` = `isDummy ? generatorArmed : isConnected`, where `generatorArmed` is a **single
master switch**, not "any channel enabled". Per-channel enables must only change *what a board
emits*, never *which boards are sources* — otherwise the builder's board set, the set `fillHitRing`
was armed on, and the set the registry knows about drift apart mid-run.

Add `OnlineEventBuilder::SetViews()` (reassign views, resize board state, `Reset()`) rather than
recreating the builder, which would dangle `BuildWorker`'s raw pointer. Call it from
`SetEnabled(true)` **and** re-run `SelectAnalysis()` so the registry, combos and `AnalysisContext`
are rebuilt together.

Two consequences to expect rather than debug:

- `Reset()` is mandatory (without it the new board's hits are below `settledBelow` and get dropped
  as late) but it also jumps every cursor to "now", discarding other boards' buffered hits. So only
  change the view set while disabled.
- After every enable, `hitRing.clear()` makes `snapIndex == 0`, so the build horizon is 0 and **no
  events build** until every board has pushed once or the 500 ms stall timeout expires. With a 1 Hz
  channel that is a visible half-second pause.

Also add ~2 ms between `fillHitRing.store(false)` and `hitRing.clear()` in `SetEnabled(false)`: the
DAQ thread may have read the flag as true and be mid-push while `clear()` memsets 4 MiB. The enable
path is already safe; the disable path is not, and a bursty generator widens the window.

### Settings tab

Added to the per-digi `bdTab` ([digiSettingsPanel.cpp:280](digiSettingsPanel.cpp#L280)), only when
`IsDummy()`. **Not** into `inputTab/trapTab/probeTab/otherTab` — `EnableControl()` recursively
`findChildren`-disables those, which would grey the generator out during a run, exactly when you
want to change a rate. Put a comment at `EnableControl()` saying the omission is deliberate.

Per channel: enable, rate Hz, energy mean, energy sigma; plus an all-channels master row, board
group rate and jitter, and the master arm. Parameters are written by the GUI and read by
`ReadDataThread`, so each is a relaxed `std::atomic`, as `fillHitRing` already is — and **not** under
`digiMTX`, which the GUI holds across network writes. When a rate changes mid-run set
`nextTime[ch] = frontier + exponential(newRate)`; never recompute from the origin, or you inject
backdated hits.

Side-car text file keyed by **board index, not serial number** — `SetDummy(i)` sets
`serialNumber = i`, so dummy SNs are 0,1,2… and collide with real ones.

## Build order

Steps 1-3 are pure bug fixes with no new behaviour, so the feature lands on a stable base.

1. **Existing null-derefs and one silent bug.** Null-guard `readDataThread[i]` at
   [mainwindow.cpp:655](mainwindow.cpp#L655) and `:383`, and [scope.cpp:716](scope.cpp#L716);
   guard `digi->hit` in `CustomThreads.h`; and fix [scope.cpp:648](scope.cpp#L648), where
   `IsDummy()` does `return` instead of `continue` — so a dummy at index 0 makes `StartScope()`
   silently do nothing for *every* board.
   *Verify:* bogus IP + real board, Start/Stop ACQ, no crash; Scope still starts board 1.
2. **Usable GUI with zero connected boards.** Move the `maxNumChannelAcrossDigitizer` update out of
   the `IsConnected()` branch; gate SingleSpectra / Analyzer / Start ACQ on connected **plus**
   dummies, keeping hardware-only buttons on connected.
3. **Dummy-safe ACQ and the save lockout.** Dummy branches in `StartACQ`/`StopACQ`/`SetDataFormat`;
   `OpenOutFile`/`SaveDataToFile` early-return; threads created for dummies; `StartACQ` restructured
   to skip hardware but start the thread.
   *Verify:* all-dummy **saving** run writes zero files and stops promptly.
4. **The generator**, header-only and Qt-free so `Aux/` can link it, plus the `ReadData` seam. Add
   `--dummygen` to `Aux/testOnlineBuilder`: drive N generators into real rings through the real
   builder for 30 s and assert `monotonicityViolations == 0`, `totalHitsLate == 0`, group dt inside
   the window, singles rate within Poisson error of the setting. **Before any GUI work** — this is
   the only place the ordering contract can be tested deterministically.
5. **Analyzer**: `IsDataSource()`, `SetViews()`, and the `Reload()`-style rebuild on enable.
6. **The settings tab**, last and lowest risk.

## Verification

1. Step 4's console test passes; `--selftest` still 9/9.
2. All-dummy: Start ACQ, enable analysis — events build with `monotonicity`, `dropped` and `late`
   all zero, and the Coincidence tab's combos **list the dummy boards**.
3. **The point of the change**: Channel A on board 0, B on board 1 — `tB - tA` shows a peak at zero
   with flat sidebands, and multiplicity broadens as the window widens. First end-to-end exercise of
   the cross-board merge.
4. SingleSpectra shows the generated singles at the configured rate and energy distribution.
5. **No file is written** during a saving run with dummies.
6. Start/stop with an unreachable IP no longer crashes.
7. Arm the generator after the Analyzer window is open, then enable analysis — the board is picked
   up and is selectable.
8. Generator disarmed → dummy excluded, real boards unaffected.
9. Tab appears only for dummies, stays live during a run, values persist, and a mid-run rate change
   does not break monotonicity.

**Verify monotonicity with one long continuous run, not with toggles.** `Reset()` sets
`hasLastPulledTs = false`, so the first hit after every enable cannot trip the check — a generator
ordering bug would be hidden at exactly the moment you are looking for it.

## Risks

- `monotonicityViolations` is both the generator's correctness signal and the tripwire for the
  real-hardware per-board sorting assumption. A generator bug could be misread as a hardware
  finding. The console test is the authority.
- Synthetic data now appears in SingleSpectra and the Analyzer, so window titles and the status line
  must make it obvious which boards are fake.
