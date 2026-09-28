# Plan: online analysis in broker mode

**Status: designed, not implemented.** The event builder and the analysis run inside the broker;
the GUI and `solaris-cli` turn them on and off and display the result. Written after the
master→broker merge brought the online analyzer onto this branch, where it works in standalone mode
and is disabled in broker mode. See `online-analysis-design.md` for the decisions behind the
pipeline this extends.

## Context

The Analyzer event-builds out of each board's in-process `hitRing`, which is filled by whichever
process holds the CAEN connection. In broker mode that is the daemon, so the GUI has no hit data,
and `mainwindow.cpp` does not construct the Analyzer at all — it says why, rather than opening a
window that silently shows nothing.

This is not a half-finished port. Master has no broker and the broker branch never had an analyzer,
so nothing ever connected the two.

## Decision

**The event builder and the analysis both run in the broker. On/off is an explicit command from
the GUI and from `solaris-cli`. Clients receive histograms.**

The builder belongs in the broker regardless of anything else: that is where the hits are, and
moving built events out instead would carry the same volume as the raw hits, so there would be
nothing to gain. Once the builder is there, the analysis follows it — otherwise the events have to
cross a process boundary anyway and the reason for moving the builder disappears.

Two consequences, accepted knowingly:

- **A plugin that segfaults takes the daemon down mid-run.** This is the cost of the choice. Note
  it is the *plugin*, not the builder: the builder is our code with 9 self-tests behind it. See
  *Containing the plugin* for what reduces this without a second process.
- **One analysis, shared by every client.** For an online monitor this is arguably right — everyone
  in the counting room sees the same spectra — but changing a channel selection changes it for
  everyone.

## Why the existing hit feed cannot be reused

The obvious shortcut is to build events from what the daemon already publishes. It does not work,
for two independent reasons.

**Nothing on the wire carries a timestamp.** `PUB_HIT_SUMMARY` packs
`{channel, energy, energy_short}` — 5 bytes drawn from the per-channel `ringBuffer<HitSummary>`
that feeds `SingleSpectra`. Event building needs `LeanHit`: 16 bytes with `timestamp` (ns, already
scaled by `tick2ns`) and `fine_timestamp`. The whole merge algorithm is a minimum over per-board
newest timestamps; with no timestamps there is nothing to order on.

**And that feed is deliberately lossy**, so adding a timestamp would not rescue it.
[BrokerServer.cpp:765](../broker/BrokerServer.cpp#L765) publishes every 100 ms and keeps only the
newest 1000 hits per channel:

```cpp
if (currentIdx - lastIdx > RingBufferSize) lastIdx = currentIdx - RingBufferSize;
unsigned long count = std::min(currentIdx - lastIdx, (unsigned long)1000);
unsigned long startIdx = currentIdx - count;   // newest-wins; older hits discarded
```

Right for a live energy histogram, where only current shape matters. Fatal for event building,
where a missing hit does not thin the statistics — it converts a coincidence into two singles.

## What already exists

Very little has to be built from nothing.

- **The daemon already has the 4 MiB hit rings.** `core/ClassDigitizer2Gen.h` is compiled into
  `solaris-broker`, so every board in the daemon has `RingBuffer<LeanHit, 262144>` — identical to
  standalone, identical to master. It is simply never switched on, because `fillHitRing` is stored
  `true` only from [Analyzer.cpp:681](../GUI/Analyzer.cpp#L681).
- **The analysis chain is Qt-free on purpose, for exactly this.** From
  `online-analysis-design.md`: the pure-abstract boundary *"is what makes `dlopen` work, and it is
  the same property an out-of-process host would need."*
- **A headless host already works.** `Aux/testOnlineBuilder` runs `Create` → `Declare` →
  `ProcessEvent` with no Qt, no GUI, no CAEN. Running the builder and an analysis outside the GUI
  is not new architecture; it is an existing, tested binary pointed at live rings instead of a file.
- **The builder is strictly a reader.** `DigiHitView::ring` is
  `const RingBuffer<LeanHit, LeanHitRingSize> *` and nothing in `OnlineEventBuilder.cpp` writes to a
  ring.
- **`DigiManager` already has a mode-agnostic ring seam.** `GetRingBuffer()` and
  `GetTraceRingBuffer()` dispatch on mode; a hit ring would be the third instance of a mechanism
  that already works twice.

## Control surface

Analysis on/off is **daemon state.** The precedent is already in the protocol: `REQ_START_ACQ` is
explicit, persistent state, and this mirrors it. If the operator turned analysis on, a GUI dying
must not silently turn it off, any more than it stops the run.

| Request | |
|---|---|
| `REQ_ANALYSIS_LIST` | names of the `.so` plugins the daemon can see |
| `REQ_ANALYSIS_SELECT {name}` | load it; replaces any current one |
| `REQ_ANALYSIS_ENABLE {onOff}` | sets `fillHitRing` on every contributing board, resets the builder, starts/stops the worker |
| `REQ_ANALYSIS_STATUS` | selected name, on/off, builder counters |
| `REQ_ANALYSIS_HIST_DEFS` | histogram definitions from `Declare()` — names, axes, binning, grid position |
| `REQ_ANALYSIS_SET_PARAM {id, value}` | a `ChannelParam` / `BoolParam`; clears histograms, as in-process |
| `PUB_HISTOGRAM` | bin contents, ~2 Hz |

`PUB_HISTOGRAM` needs no sequence numbers or gap accounting: **each message is a full snapshot, not
an increment**, so a dropped one costs nothing and the next repairs it. Histograms are also tiny —
a 512-bin 1D spectrum is 2 KB, negligible at 2 Hz.

CLI verbs alongside the existing `start` / `stop` / `status`:

```
analysis list
analysis select Coincidence
analysis on | off
analysis status            # counters: dropped, late, monotonicity, events
```

That makes a headless run scriptable, and gives a way to check the builder is healthy without
opening a GUI.

## Containing the plugin

Cheap mitigations, none of which need a second process:

- **ABI check before load** — already implemented; the host refuses a `.so` built against a
  different `ANALYSIS_ABI_VERSION` rather than crashing in a vtable call.
- **`try`/`catch` around `ProcessEvent()`**, disabling the analysis and logging on an escaping
  exception. This does not save you from a segfault, but it does from the common mistakes.
- **Analysis on its own thread**, as in-process today, so a slow analysis backs up the event ring
  instead of stalling the builder and lapping the hit rings.
- **Never `dlclose`** — already the rule, and the reason hot-reload is safe.

What none of these fix is a wild pointer. If that becomes a real operational problem, move the host
out of the daemon — see *Alternatives*.

## Work to do

1. **`DigiManager::GetHitRing(int digi)`**, standalone returning `digi[d]->hitRing`. Switch
   `Analyzer::MakeViews()` to it. No behaviour change; standalone must still pass its self-tests.
2. **Replace the view-list predicate.** `Analyzer::BuildViewList()` skips `IsDummy()` boards. Use
   the rule `plan-dummy-generator.md` reached from the other direction: **what matters is whether a
   board produces data, not whether it is a dummy.**
3. **Builder + plugin host in the broker.** Add `OnlineEventBuilder.cpp` and `AnalysisPlugin.cpp`
   to `broker/Makefile` with `-ldl` — both are already Qt-free. Set `fillHitRing` on enable. At
   this point it is verifiable with `solaris-cli` alone and no GUI: this is `testOnlineBuilder`
   pointed at live rings.
4. **Histogram protocol** and the CLI verbs above.
5. **GUI viewer mode.** Reuse the existing Analyzer widgets, fed by `SetBinContents()` /
   `SetCellContents()` from the subscriber instead of from a local worker. The existing rule that
   only the GUI thread touches a histogram widget carries over unchanged — and the worker that
   would have violated it no longer exists in this process.

Standalone mode keeps working exactly as it does now, unchanged, throughout.

## Verification

- **Same run, both modes, same events.** The strongest test available: replay one `.sol` file
  through the daemon and through standalone and require the built events to match hit for hit. The
  builder is deterministic given the same hit sequence, so any difference is the transport.
- **`testOnlineBuilder` stays the oracle.** The builder does not change, so its 9 self-tests must
  keep passing untouched. If this plan makes them need editing, something has gone wrong.
- **Kill the GUI.** `kill -9` mid-run with analysis on: the daemon must keep acquiring, keep
  building and keep publishing. Reconnecting must show the same histograms still accumulating, not
  a fresh start.
- **CLI and GUI agree.** `analysis off` from the CLI must be visible in the GUI, and vice versa.
  Neither is the owner of the state; both are views of it.
- **Cost when off.** With analysis off, the daemon's CPU and network use must be indistinguishable
  from today — `fillHitRing` false, no builder thread, no `PUB_HISTOGRAM`.
- **Analysis across a run boundary.** Turn analysis on mid-run, and start a new run with analysis
  already on. Both must work; the second must call `OnRunStop`/`OnRunStart`.

## Risks

- **A plugin crash is now a run-ending event.** Accepted in *Decision*, mitigated in *Containing
  the plugin*. The operational rule that follows: a plugin gets tested against a replayed file
  through `testOnlineBuilder` before it is selected in the daemon during a real run.
- **Builder CPU competes with acquisition.** The builder's merge is a linear scan over boards per
  event, in the same process as `ReadData()`. At high rate this is real. Measure it before trusting
  it; the builder thread should be lower priority than the readout thread.
- **Timestamp offsets between boards.** The builder normalises units but does not correct per-board
  offsets; boards must share a clock and start together. Broker mode does not change this, but it
  makes it easier to analyse boards that were never actually synchronised.
- **A second hit path to keep in step.** `HitSummary`/`SingleSpectra` and `LeanHit`/Analyzer are
  filled from the same `ReadData()` and should not disagree, but a future change to one must
  consider the other.

## Alternatives

Two were considered seriously and are recorded because they will come up again.

**Stream `LeanHit` to each GUI** and let every client run its own builder — attractive because the
GUI code barely changes and each client gets its own parameters. Rejected on bandwidth and, more
importantly, on silent loss: 16 B/hit is ~16 MB/s per 1 MHz board, ~64 MB/s for four, and ZMQ PUB
discards under backpressure with nothing detecting it at the receiver. It would need a per-board
`firstSeq` in every message and gap accounting surfaced to the operator; without that it shows
confident wrong physics, which is worse than the current honest refusal.

**Hit rings in shared memory, analysis in a separate `solaris-analysis` process** — the daemon
allocates the rings with `shm_open`, the analysis process maps them read-only and publishes
histograms on its own socket. This is strictly better on the two costs accepted above: a plugin
crash kills only the analysis, and several analyses can run at once with different plugins and
parameters. It is the **upgrade path**, not a road not taken:

- It is cheap to reach from here. The control surface and histogram protocol are identical; the two
  designs differ only in which process calls `dlopen`, so the GUI and the CLI would not change.
- `RingBuffer` is flat POD — `T buffer[N]` plus one `std::atomic<unsigned long>`, no pointers, no
  heap — and the builder already takes a `const` pointer, so read-only mapping works as-is.
- What it costs is making `hitRing` a pointer into mapped memory instead of an inline member of
  `Digitizer2Gen`. That is mechanical but it touches the `ReadData()` hot path and every use site,
  and it adds shm lifetime and layout-versioning problems (stale `/dev/shm` after a crash, a reader
  mapped across a daemon restart with a different board count — needs a header with magic, board
  count and run generation).

Paying that now would buy protection against a fault that has not happened yet. Take it if plugin
crashes become a real operational problem; it also constrains the analysis to the DAQ machine,
which is where a server-side service belongs anyway.
