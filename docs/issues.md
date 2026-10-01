# Code review — the three decisions worth recording

Most of the September 2025 review was mechanical: a confirmed defect, a one-line fix, done. Three
items were not. Each one either deviates from what the review recommended, or changes behaviour in
a way that is not a bug fix. The reasoning is here so it is not re-derived, or worse, "corrected"
back.

The numbering is the review's own (`1.7`, `4.7`, `5.1`).

---

## 1.7 — the 1 GB array in the event builder

**Was:** `int trace[MAX_MULTI][MAX_TRACE_LEN] = {0};` as a file-scope global in
`Aux/EventBuilder.cpp`. With `MAX_MULTI` 50000 and `MAX_TRACE_LEN` 5000 that is exactly
1,000,000,000 bytes — measured on the binary, 1,001,400,320 B of BSS.

**Now:** a flat `std::vector<int> traceBuf` (`:82`) with a `traceRow(m)` accessor (`:83`),
allocated at `:263` **only when `saveTrace` is set**. BSS is 1,400,352 B.

### The stride stays at MAX_TRACE_LEN

The review wanted the stride shrunk, sized from the configured trace length and fed into the
`Form(...)` leaflist. It should not be, for three reasons.

**There is no configured trace length here.** `EventBuilder` is an offline tool over `.sol` files;
its argv is `[outfile] [timeWindow] [saveTrace] [sol-1] ...`. Trace length is a per-hit field,
`hit->traceLenght` (`:102`), and it varies by board and channel. Sizing a stride would mean a
pre-scan of every input for the maximum — which makes the branch shape a function of the data, so
two runs of the same experiment with different trace settings produce trees that no longer
`TChain` together.

**The leaflist string is the on-disk format.** `:265`:

```cpp
tree->Branch("trace", traceBuf.data(), Form("trace[multi][%d]/I", MAX_TRACE_LEN));
```

Every analysis macro that opens one of these trees sees `trace` as `[multi][5000]`. Changing 5000
changes the declared branch shape for all of them.

**Stale-tail semantics are load-bearing.** The write loop fills only `min(traceLen, 5000)` entries
(`:104`) and nothing clears the row between events, so bytes past `traceLen` read as zero on the
first pass and as the previous event's tail afterwards. `assign(..., 0)` reproduces that exactly;
a different stride would not. `traceLen[multi]` is the field consumers should be reading.

### Two properties the branch depends on

Both are noted at the declaration, and both are easy to break by accident:

- the buffer is sized **before** `tree->Branch()` captures its address, and never resized after. A
  reallocation leaves the branch pointing at freed memory, silently.
- the stride is `MAX_TRACE_LEN`, not the trace length.

### What did and did not improve

The win is the **no-trace run**, which is the common one: it previously committed a gigabyte of
process image to touch none of it.

With `saveTrace == 1` the gigabyte is still there, and it is now *more* resident than before.
BSS is zero-fill-on-demand, so the old global only materialised the pages the builder actually
reached — a run with `multi` peaking at 200 cost a few MB of RSS. `assign(n, 0)` writes the whole
allocation, so all 1 GB faults in.

**This is accepted, not an oversight.** The machine has the RAM, and trace-saving runs are
deliberate. If it ever needs to change, the fix is `calloc` rather than a vector: a 1 GB `calloc`
goes to `mmap` and returns pages that are already zero without writing them, restoring the lazy
behaviour while keeping the `saveTrace` gate.

### Verification

`size Aux/EventBuilder` for the BSS figure — **done**, 1,001,400,320 → 1,400,352 B.

The byte-compare of the output ROOT file against a pre-change run over the same input is
**not done, and currently cannot be**: it needs `.sol` input, and `ExpData/` holds only `.gtd*`
files from the older format. See "Outstanding verification" below. This is the one check that
catches a stale-tail regression in the trace branch, so it should happen before anyone trusts a
trace-saving build.

---

## 4.7 — the trigger decoder, and why the mask check starts at `i = 1`

`SOLARISpanel.cpp` used to hash four combo-box strings into one hex nibble word (`0x2211`,
`0x3331`, …) and compare against magic constants. That is now a table, one row per combo entry, at
`:28-71`. Four bugs were hiding in the arithmetic.

### Fixed

| | Was | Now |
|---|---|---|
| `jaja[0]`/`jaja[1]` overlap | A single-channel self-triggering group incremented both buckets, and the selection loop was last-match-wins with no `break` — so **every single-channel detector displayed "Trigger E"**. | `nChInDet > 1` guard at `:752`; first-match-wins `break` at `:760`. |
| Unbounded shift | `1ULL << chID` with a raw channel id off the mapping file. `chID >= 64` is undefined behaviour. | Guarded at `:774`. |
| Empty group | Every count was 0 and so was the size compared against, so it landed on "Others" by arithmetic accident. | Explicit `if( nChInDet == 0 ) continue;` at `:735`. |
| Silent read failure | `GetSettingValueFromMemory` returns the literal `"invalid"`, which matches no mode — a dead board was indistinguishable from a deliberately odd trigger setup. | Reported at `:763-766`. |

"Trigger e" is not a table row. It is the one position-dependent mode — the group's first channel
self-triggers, the rest sit in coincidence on it — so it is an explicit predicate, `IsTriggerE` at
`:65`.

### Deviation: the mask check still starts at `i = 1`

The review flagged this as a bug: "`triggerMap[0]` is never checked against its documented `0`."
**The review is wrong, and the write path settles it.** `case 1` at `:610-625`:

```cpp
if( i == ChStartIndex ){
  digi[digiID]->WriteValue(PHA::CH::CoincidenceMask, "Disabled", chID);
}else {
  digi[digiID]->WriteValue(PHA::CH::CoincidenceMask, "Ch64Trigger", chID);
  ...
  digi[digiID]->WriteValue(PHA::CH::ChannelsTriggerMask, maskStr.toStdString() , chID);
}
```

The group's first channel gets `CoincidenceMask = "Disabled"` and **no `ChannelsTriggerMask` write
at all** — its trigger mask is unused by the hardware in this mode, so the panel deliberately
leaves the register alone. It therefore holds the board default, or a stale value from whatever
mode the group was in last.

The read loop builds `triggerMap` from `h = ChStartIndex` upward (`:711-718`), so `triggerMap[0]`
is exactly that channel. Checking it against 0 would take a correctly configured Trigger-e group
and read it back as "Others", on the strength of a register the panel never writes. That is a
false negative introduced to satisfy a comment.

`i = 1` is correct. The reason is at the site, `:778-782`.

### Verification

Needs hardware. Cycle all five trigger modes and confirm the combo reads back what was written —
**including a single-channel detector group**, which is the case the old code got wrong.

---

## 5.1 — `Histogram1D` one vertex per bin, and the half-bin gutter

**Was:** two x samples per bin edge, the first nudged down by `dX * 1e-6`, with `Fill()` writing
the count into two slots. Every index in the class was spelled `2*bin + 1`. The graph was drawing
at the default `lsLine` and the doubling was faking the staircase.

**Now:** one vertex per bin at the bin centre (`:319-321`), with
`setLineStyle(QCPGraph::lsStepCenter)` where each graph is added (`:35`, `:257`). QCustomPlot has
rendered steps natively all along. `Fill()` is one increment per hit instead of two, on the hottest
path in online histogramming.

### Behaviour change: the first and last half-bin are not drawn

`lsStepCenter` puts each step boundary midway between two vertices, which for a uniform binning is
exactly the bin edge — so every **interior** bin renders identically to before.

The ends do not. The polyline starts at the first vertex and stops at the last, and those sit at
`xMin + 0.5*dX` and `xMax - 0.5*dX`, while the axis range is still `[xMin, xMax]` (`:269-270`).
The result is a blank half-bin gutter at each end of the plot. The counts are right and the
tooltip reports them; they are simply not drawn.

At the 1000-bin cap that is 0.05% of the width and invisible. On a 20-bin quick channel check it
is 2.5% at each end and the end bars look clipped.

**Accepted as is.** If it ever becomes a nuisance, widen the axis by half a bin at each side so the
gutter falls outside the data region:

```cpp
xAxis->setRangeLower(xMin - 0.5*dX);
xAxis->setRangeUpper(xMax + 0.5*dX);
```

The alternative — phantom vertices outside the range — reintroduces the index offset this change
removed. Do not.

### Two bugs fixed in the same pass

Both were unguarded subscripts:

- the mouse-move tooltip indexed `yList` with a bin derived from the cursor position, and the plot
  is zoomable and draggable, so moving the cursor outside the data range read out of bounds.
  Guarded at `:73-75`.
- `Fill()` never range-checked `ID`, though `SetBinContents` always had, and `yList` is a plain
  `[MaxNHist]`. Guarded at `:337`; the bin itself at `:358`.

---

## Outstanding verification

**Nothing in this sweep has been checked against hardware.** Everything builds clean — the GUI,
`Aux`, and `Aux tools`, with zero warnings — and the GUI launches. That is the whole of the
runtime evidence. It says nothing about whether any of the behaviour below is right.

Several of these changes alter what the operator sees *during a run*, so a compile is a
particularly weak signal here:

- the trigger decoder (4.7) was rewritten outright
- scalar rates (4.3) now come through `ReadStat()`, which early-returns under Raw by design
- the SOLARIS panel's Refresh button (1.1) previously never read hardware at all; now it does
- the settings-tab enable/disable sweep (2.1f) no longer re-enables individually-disabled children

### Run the dummy-digitizer pass first

It exercises the dummy/disconnected paths in 1.1, 1.2 and 2.1(c), and **2.1(c) is only reachable
with no hardware** — `coinTime` stays empty, which is the case that used to be UB.

The GUI needs a scrubbed environment; the VS Code snap breaks the loader:

```
env -i HOME=$HOME DISPLAY=$DISPLAY ./SOLARIS_DAQ
```

### Then, on a live board

| Item | Check |
|---|---|
| 1.1 | Change a threshold from the digitizer settings panel, then press Refresh on the SOLARIS panel. The new value must appear — before the fix it never would. |
| 1.2 | With one dummy board ahead of a real one, set a coincidence time. The board behind the dummy must take it. |
| 1.3 | Disable one channel. Only that channel's lamp row may change; all 64 rows used to track channel 0. |
| 1.6 / 4.7 | Cycle all five trigger modes and confirm the combo reads back what was written. **Include a single-channel detector group** — that is the case the old decoder got wrong. |
| 4.3 | Rates and accept rates must match pre-change values on a steady test pulse. Time one `UpdateScalar` tick either side; expect ~3x fewer round-trips. Confirm the GUI stays responsive during acquisition. |
| 4.3 | **Raw format specifically.** `ReadStat()` early-returns under Raw, so confirm rates still come from the decoder's counters and do not read zero. |
| 2.1(f) | On a non-VX2745 board, confirm `bdVGA` stays disabled across an ACQ stop/start cycle. Sweep the tabs for anything else deliberately disabled that must stay so. |
| 2.1(a) | Two boards, ACQ started on one only. The Inquiry/Copy box must be disabled. |

### Blocked

**1.7's ROOT byte-compare.** Needs a `.sol` file; `ExpData/` has only `.gtd*`. Produce one from the
test station, build the tree with the pre-change binary and the current one, and compare. Until
then the trace branch is unverified — the BSS measurement proves the allocation moved, not that
the bytes are the same.

### Note on commit granularity

The review plan called for one commit per stage so a bisect would land on a small change. It did
not work out that way: the work was done as one sweep and the stages are interleaved within files
— `SOLARISpanel.cpp` alone carries 1.2, 1.8, 4.7 and 5.6. Splitting after the fact would have
meant intermediate commits that do not build, which is worse for a bisect than one honest commit.
If something turns up on the beam line, the unit of revert is the whole sweep.
