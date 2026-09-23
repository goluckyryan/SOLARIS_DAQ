# Online analysis — design decisions

The README says how the online event builder and analyzer *work*. This says **why they are shaped
the way they are**, including the alternatives that were tried and rejected, so the next person to
touch them does not have to rediscover the reasoning — or undo it by accident.

Read the README first. This is the layer underneath it.

---

## The pipeline

```
ReadDataThread  ->  hitRing (per board)  ->  OnlineEventBuilder  ->  EventRing  ->  Analysis
   one per board       4 MiB, lock-free       builder thread         8 MiB        analysis thread
```

Every boundary is a lock-free ring with a single producer. There is no mutex anywhere in the steady
state, and that is deliberate: the DAQ read path is the highest-priority thread in the process, and
nothing downstream may ever be able to block it.

---

## Why one hit ring per board, not per channel

A board already emits **all of its channels interleaved in timestamp order** — `format_RAW.md`,
Aggregate Header: *"interleaved events (all channels, sorted by timestamp)"*. This was not taken on
faith: it was checked against ~23 million consecutive hits of real MUSIC data (VX2745 PHA,
`DataFormat::Minimum`), across 35 interleaved channels, with **zero** backwards steps.

That one fact is load-bearing for three separate decisions:

- **one ring per board instead of 64.** Splitting per channel would turn a 4-way merge into a
  256-way one and buy nothing.
- **no priority queue** (see below).
- **the build horizon can be per board**, which is what makes a quiet *channel* harmless.

The evidence covers PHA / no-waveform / decoded-endpoint only. It has **not** been verified for PSD
multi-channel or the raw-blob path. `OnlineEventBuilder::monotonicityViolations` is the tripwire: if
it is ever non-zero on real data, this assumption has broken and the events are not trustworthy.
The fallback is per-channel rings plus a heap, confined to `PickEarliestBoard()`.

Memory also improves. 64 per-channel rings statically partition the buffer — one hot channel is
capped while 63 idle ones hold megabytes of zeros. One per-board ring pools the depth where the rate
actually is.

## Why a linear scan and not a priority queue

FSUDAQ's `MultiBuilder` merges 20 boards x 64 channels = 1280 sources, so it needs a heap. Per-board
sorting cuts that to **one source per board** — 2-4 typically, 20 at most.

Measured on 2M hits across 4 sorted streams, merge only:

| strategy | per hit |
|---|---|
| linear scan, one front per stream | **2.9 ns** |
| heap, one entry per stream | 8.8 ns |
| heap, every hit pushed in | 83.5 ns |

The third row is the one worth internalising. Dumping every hit into a `std::priority_queue`
re-sorts data that is *already sorted*: `O(H log H)` on hits instead of `O(H log B)` on boards, with
a heap depth of ~21 instead of 2 and cache-hostile sift access. It is 29x slower than the scan.

But speed is the lesser argument. The scan is **simpler to keep correct**. A heap needs side state
tracking which sources are currently in it, and that state must be re-synchronised every time a
source runs dry and refills — which online is the *normal* case, not the exception. With a scan, the
`hasFront` flag is the entire state and nothing can desynchronise. A heap also stores a *copy* of
the timestamp, which goes stale when a ring laps and the front becomes a different hit; the scan
re-reads it every time and cannot have that bug.

For context, the whole builder costs **11.6 ns/hit** end to end — ring push, merge, `BuiltHit`
construction and the event callback — or about 1% of one core at 1 MHz.

## The build horizon

The central problem: when is it safe to close an event? Emit at t=500, then a slower board delivers
t=520, and you have silently turned a coincidence into two singles.

Because each board emits in non-decreasing order, once it has delivered time `t` it can never
deliver anything earlier. So the minimum over boards of *newest timestamp delivered* is a floor
under every hit still to come. An event is final once its whole window sits below that, plus
`guardTime`.

Three things differ from FSUDAQ, and all three matter only online:

**A board that has delivered nothing yet has an *unknown* floor, not an absent one.** FSUDAQ skips
such a source. Offline that is right — a file with no data never acquires any. Online it lets the
horizon run ahead of a board that is merely slow to start, and every hit it then delivers arrives
too late. So the horizon is forced to 0 until every live board has spoken.

**A board that stops delivering must not pin the horizon forever.** After a wall-clock stall timeout
it is dropped from the minimum. This is the one place data can be lost: a hit it later delivers
below an already-settled boundary is counted in `totalHitsLate` and discarded. A bounded, *counted*
loss in exchange for liveness — the counter is the point, silent loss would not be acceptable.

**Wall-clock, not timestamps, for the stall check.** A stalled board's timestamps are by definition
not advancing, so using them to detect the stall cannot work.

Also: the horizon comparison is written to be safe against unsigned wraparound. FSUDAQ
(`MultiBuilder.cpp:197`) and the offline `Aux/EventBuilder.cpp:306` both compute that difference
unguarded, so a backwards hit wraps to ~1.8e19 and force-closes the event.

**Window bound is exclusive**, `[seed, seed + timeWindow)`, matching `Aux/EventBuilder`. FSUDAQ is
inclusive and disagrees. Every existing SOLARIS analysis was written against the offline one, and
mismatching it costs a day of chasing boundary-coincident hits.

## Why the event ring is separate from the builder

`Aux/testOnlineBuilder.cpp` constructs `OnlineEventBuilder` on the **stack** in five places. An
8 MiB member — `memset` by `RingBuffer`'s constructor — would overflow the default stack. The
builder holds a non-owning `EventRing *` instead.

That also keeps the ring independently testable and lets a future out-of-process host place it in
shared memory without touching the builder.

**Flat hit ring plus index ring, not fixed-size event slots.** A fixed `MaxHitsPerEvent` pays for
the cap on every event when real multiplicity is 1-3, and truncates during exactly the noise bursts
and common-mode events the multiplicity plot exists to diagnose. There is no defensible constant —
20 boards x 64 channels is 1280 hits before pile-up. The absence of a good constant *is* the
argument for the flat layout.

Sized to hold as many hits as one board's `hitRing`, so a stalled consumer starts losing events at
the same point the raw hits would have been lost anyway. A shallower ring would be pointless.

**No epoch counter.** An earlier design had one, to avoid clearing a ring while a consumer held a
stale cursor. It became unnecessary once the analyzer's enable toggle was the only run boundary:
both workers are stopped at that point, so the ring can simply be cleared and the reader re-anchored
together. Simpler and stronger.

## Why two worker threads

If the builder and the analysis shared a thread, a slow analysis would stall the builder, which
stops draining the per-board hit rings, and **raw hits** are lost. With the ring between them, a
slow analysis loses only events.

Losing events is recoverable. Losing raw hits is not, and it would be this window's fault.

The realistic stall is not today's `Coincidence` analysis — three histogram fills against an
11.6 ns/hit builder. It is a future heavy analysis doing waveform work or fitting. This is insurance
with a known premium.

## Why no hand-rolled thread fence

`SingleSpectra` uses `isFillingHistograms` + `suspendFilling` + `WaitForFillToDrain()`, because its
GUI thread mutates widgets directly while a worker fills them.

The Analyzer does not need any of it. Every GUI-initiated change is a
`Qt::BlockingQueuedConnection` into the worker that **owns** that state, so the change executes on
that worker thread between two `ProcessEvent()` calls. **The marshalling is the fence.** That is why
an analysis can keep plain `int` parameter members with no atomics, and why there is no 200 ms
give-up loop — which in `SingleSpectra` is a correctness hole, not a safety net.

The cost is that a blocking call stalls the GUI until the worker finishes its current batch, so the
drain is bounded per tick. That bound is wanted anyway.

## Why a worker must never call `Histogram*::Fill()`

This was wrong in an early draft of the design and is worth stating bluntly.

`Histogram1D::Fill()` assigns a `QString` into a `QCPItemText` on **every** call. The refcount is
atomic; the d-pointer store is not. A worker can free the buffer the GUI thread is reading inside
`draw()`. `Histogram2D::Fill()` additionally walks `cutList` while the operator may be drawing a
cut — and drawing a cut on a live plot is a thing operators do.

Both are use-after-free, not a cosmetic smear.

So `QtHistRegistry` accumulates into `std::atomic<uint32_t>` arrays on the worker, and only the GUI
thread publishes them into the widgets via `SetBinContents` / `SetCellContents`. An analysis never
sees a widget, so it cannot get this wrong.

`SingleSpectra` still fills from its worker. That predates this work and should eventually move to
the same discipline. **Do not copy it.**

## Why the analysis interface has no Qt

`Analysis` and `HistRegistry` are pure abstract, and no STL type crosses the boundary — only
`const char *`, primitives and POD structs. Consequence, measured: an analysis compiles into a
17.8 KB shared object in 0.23 s with **zero undefined symbols from the host**. No `-rdynamic`,
nothing linked from the DAQ, no Qt, no CAEN.

That is what makes `dlopen` work, and it is the same property an out-of-process host would need.
`BuiltHit` lives in its own header for the same reason: a plugin should not depend on the builder's
internals, and changing them should not oblige anyone to rebuild their analyses.

**Never `dlclose`.** Unloading is where hot-reload crashes come from — static destructors,
thread-locals, `atexit` handlers running against code about to be unmapped. Leaking ~17 KB per
reload removes the entire failure class.

**Load a uniquely named copy every time.** `dlopen` keys on path, so re-opening the same path after
a rebuild silently returns the *old* handle. That is an hour of debugging phantom behaviour.

**One analysis per `.so`**, since the entry points are `extern "C"` with fixed names. The
`ANALYSIS_ENTRY` macro emits those exports only under `-DANALYSIS_PLUGIN_BUILD`; the guard is
load-bearing, because the test harness links every `analyzers/*.cpp` into one binary and would
otherwise get duplicate symbols as soon as a second analysis exists.

## Rate and cost, measured

| | |
|---|---|
| builder, end to end | 11.6 ns/hit — 86 M hits/s |
| merge only, 4 boards | 2.9 ns/hit |
| plugin compile | 0.23 s, 17.8 KB |
| analysis edit + rebuild + relink | ~0.9 s |
| `hitRing` | 4 MiB/board — 262 ms at 1 MHz |
| `EventRing` | 8 MiB total |

At 1 MHz the whole online chain is about 1% of one core. Copying is not the bottleneck and will not
become one; 16 bytes/hit at 1 MHz is 16 MB/s against ~10 GB/s of memory bandwidth.

---

## Known gaps

- **Nothing has run on hardware.** The GUI path, the plugin reload loop, the two worker threads
  under a live `ReadDataThread`, and clearing rings mid-run are all unverified.
- **Per-board timestamp ordering is verified for PHA only.** Watch `monotonicityViolations` on the
  first PSD or raw-mode run.
- **`EnDataReduction` firmware mode is incompatible** with any timestamped builder: single-word
  events carry a 32-bit reduced timestamp that wraps every ~34 s, and `RawDecoder` passes it up as
  if it were the full 48-bit value.
- **`ANALYSIS_ABI_VERSION` is bumped by hand.** `sizeof(BuiltHit)` is `static_assert`ed, but adding
  a virtual to `HistRegistry` shifts the vtable with no size change and no assert. The
  `analyzers/Makefile` ABI dependency means a stale `.so` realistically only arrives by being copied
  in from elsewhere, which is what the version check backstops.
- **A bad plugin still takes the DAQ down.** In-process `dlopen` was chosen over an out-of-process
  host for a quarter of the work; crash isolation is the deferred step, and nothing here blocks it.
