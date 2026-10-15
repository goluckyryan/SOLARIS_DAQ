# agent/AGENT.md — bench and code findings (SOLARIS DAQ)

**This file is NOT loaded into any context window.** It is a searchable record of
what has been *measured on this bench* and what has been *disproved*, so the next agent can `grep` it
instead of re-deriving it. It is the counterpart to the session md, which is injected every turn and therefore has to stay small.

## Scope — what belongs here

**AGENT.md records what was LEARNED, not what was DONE or EXPLORED.** The hardware, the physics, the
code's behaviour -- not my work session. If a line only makes sense as a step I took on the way to
something, it is a commit message, and `git log` answers it better than this file can.

A reader five months from now, with a different tool and a different task, should find every line here
still worth knowing.

**"What I explored" is worth keeping exactly when it CLOSES a possibility.** "Model X fits this tail,
and here is the residual that kills it" is a learned fact -- it stops someone re-deriving a dead end,
which is why overturned claims stay here marked RETRACTED rather than being deleted. "I wrote a script,
rewrote it, then deleted it" is a sequence of my actions and teaches nothing; the **durable residue** of
that episode -- the parse recipe, the trap it exposed -- belongs here, stated on its own merits and
without the names of the files involved. The test is not "did I do this?" but **"does a future reader
know something false, or waste real time, if I leave this out?"**

The operational form, applied to each candidate paragraph:

1. **Would it still be true after the tool is rewritten?** If it describes a decision in
   `plot_capture.py` / `trapezoid_gui.py`, it is a commit message, not a note. Code is the record —
   `git log` answers it. (This is why "the vertical-line special case is gone" was wrong to put here.)
2. **Would omitting it cause someone to re-derive something FALSE, or waste real time?** A dead
   hypothesis, a trap, a measured number, a hardware quirk: yes. "I fixed a bug": no.
3. **Is it about the instrument or about me?** `FlatTopT clamps to 3000 and returns OK` is a property of
   the board — permanent. `my scratch script divided ns by seconds` is a property of my afternoon — gone.

**Write here:** measurements with numbers and their error bars; hypotheses that died, with the evidence
that killed them; traps that cost time; hardware behaviour that contradicts its own documentation; the
recipe for a parse or a measurement that is easy to get wrong.

**Do NOT write here:** rules of behaviour — those go in the session md, because they must be in context
to be followed, and a file nobody reads cannot regulate anything. Also not: what I changed in our own
tools, commit-by-commit; `BENCH STATE` snapshots (one live copy in the session md, not ten dated ones
here); changelog noise about our own bugs once the fix is in; **the name of a deleted file.**

**A deleted file's name is not a fact, and keeping it costs more than it pays.** "X.py was deleted,
recipe preserved here" tells a future reader nothing they can act on — they cannot un-delete it — and it
leaves an unresolvable reference that reads like a pointer to real code. Strip the name and keep the
**fact** the file embodied: §13e is titled for what it *contains* (the capture-parse recipe, now the only
copy anywhere), not for the script that once held it. Two failure modes this prevents, both of which this
file actually had: a present-tense sentence ("X still reports corr 1.00000") reads as a runnable check
when nothing can run it, and a function kept because "X imports it" silently loses its justification the
moment X is gone — **justify a kept function by what it measures, never by a caller.**

**The exception, and it is the only thing that justifies a section about a tool change:** the *transferable
lesson* inside it. "Pixel-diffing cannot verify a sub-pixel artist; measure path geometry in data
coordinates" outlives the bug entirely. **Distil the lesson into the relevant topic section and delete the
narrative** — do not keep the narrative as a tombstone.

## Index

| § | topic |
|---|---|
| 1 | bench topology (IPs, models, what the web app can/can't do) |
| 2 | emulator FAST vs HDR outputs; source impedance measured by termination; scope geometry and measurement discipline |
| 3 | shaped-pulse measurements on HDR (tau, baseline, 10-90 rise; the 0.8R rule; **the A(1-e^-t/r)e^-t/d pulse model**) |
| 4 | digitizer input params (zin 100 ohm, DIFF, inputrange 4 V) |
| 5 | CAEN_FELib: path format, device-tree dump, bulk channel paths NOT supported, read costs |
| 6 | build state; broker + CLI end-to-end |
| 8 | trace capture from the digitizer ch0; impedance model confirmed to 0.5 % |
| 9 | trapezoid (EnergyFilter) capture; probe settings not restored by design |
| 10 | the 4 digital probes, all 13 sources measured, which are worth a slot |
| 11 | trapezoid filter: criteria for "good", the user's pole-zero rule, what passes and what FAILS; **RETRACTED 2026-10-14: "filter rise >= signal rise" was never a rule -- RiseTimeT is unconstrained by the signal** |
| 12 | trapezoid TUNING sweep: resolution 0.09 %, mean energy is the observable (not FWHM); the ~3 % rise effect is UNATTRIBUTED (not ballistic deficit) |
| 13 | `Aux/trapezoid_gui.py` (still there -- user-facing): offline playground; **the correct filter (corr 1.00000) and what the pole-zero really is**; sampled+quantised pipeline with dt/bits controls; **pedestal/offset boundary fix + the DC leak the board also has**; **defaults now 1000/5000 ns + why a long flat top measures short**; **"exponential tail" was MY DEFAULT not the formula + the user's optimisation criteria tested + **the capture-parse recipe that has no script left, §13e**; **7xr holds as a default; "tune L to fill the energy bits" REFUTED -- DPP normalises by L** |
| 13b | **PHA timing chain RC-CR^2: armed on threshold, TRIGGER on the zero crossing -- reproduces the board's markers to +-1 sample**; +608 ns lag = trigger latency (partly); trigger-level pile-up limit ~1700 ns vs the board's 1528 ns guard; **trapezoid corners are line-fit intersections, not threshold crossings -- and they are referenced to the ARRIVAL, not the trigger**; **`argmax` over the rest of the record merged two pulses 8 us apart** |
| 13c | **LIVE six-signal capture vs the model: the +608 ns lag was 612 ns of REAL PIPELINE LATENCY, not gating** -- all six signals match (armed err 0, trigger +1 sample, corners <=12 ns, resid 0.034 %); Peaking/PileupGuard are NOT trapezoid corners; **the marker table is §13e, which is now the only copy of the parse** |
| 13d | **GUI REWRITTEN to the user's 5-step plan: the delay is DERIVED as (TRIGGER - ARMED) and applied to the INPUT, never fitted**; retires the 612 ns position and the t1=trigger+L rule; **`twinx()` must be created once, not per recompute**; **the 2nd derivative's pre-trigger was not flat -- IIR needs an ANALYTIC steady state, no finite warm block is exact**; x-range bounded to the sample data +-10 % |
| 13e | **THE CAPTURE-PARSE RECIPE, verbatim: regex, constants, probe map, time-filter calibration** -- it exists nowhere else and **no script can run it**; gitignoring the data means a fresh clone cannot re-verify corr 1.00000; **the GUI is a DEMONSTRATION tool, scores removed from it** |
| 13f | **TRAP: `FlatTopT` writes return OK but the board CLAMPS to 3000 ns**; `/max` nodes unreadable, so the limit is only discoverable by write+read-back; clamp confirmed in the waveform by the DC-leak formula AND the peaking marker, not just the register |
| 13g | **Emulator rise 1.0 -> 0.5 -> 0.2 us: RETRACTS the "FAST + cable add fixed rise" model** (falsified on 3 points, best fixed pole 21 % off); **below ~0.5 us the single-pole pulse model BREAKS DOWN** -- r=238 ns with a +-0.4 ns error bar but a 23 mV structured residual; amplitude is rise-invariant (634.3 -> 631.6 mV) |
| 13h | **Emulator CH2 -> digitizer ch52, proven by a CAUSAL control** (changed only ch2's rate, only ch52 followed); `selftrgrate` is live without ACQ, `chtriggercnt` is not; **ch52's probes are NOT the ch0 set**; `set_pulse`'s `polarity` string is INVERTED for ch=1 |
| 13i | **ch52's digital probes are NOT the ch0 set** (D2=EnergyFilterPeakReady, D3=EventPileUp) -- a wrong label looked completely plausible; **RULE: read the probe nodes before labelling any plot but ch0's**; plus verification independent of read-back (ch52 marker moved 5536 -> 2544 ns) |
| 13j | **ch52 vs ch0 EnergyFilter: the FILTERS agree, only amplitude differs** (out ratio 0.9829 == in ratio 0.9827, corr 1.00000, DC leak +1.7/+1.4 mV => both L+M=4000 ns); the 16 ns D2 gap is two different signals, not a filter discrepancy |
| 13k | **`DAQ:agent/` folder** -- log NOW TRACKED (was ignored), `agent/data/` ignored, `plot_capture.py` tracked; **`trapezoid_gui.py` stays in `Aux/` because it is user-facing teaching, not agent scratch**; git-index `AD` hazard when moving a staged file |
| 14 | open items |
| 15 | **WAVEFORM SOURCES, authoritative: 5 analog x 2 slots, 13 digital x 4 slots, identical per slot**; device-tree dump recipe + the lowercase-node-name trap; **INCIDENT: a 2nd FELib Open broke the broker's handle -- recover with `close 0` + `open`** |
| 16 | **4 passes cover all 5 analog + 13 digital sources; ADCInput repeated = the stability measurement: spread 0.04-0.06 % = ~4 LSB**; a capture's columns are anonymous, so record probe names beside the data |
| 17 | **`ChRecordLengthS` is NOT `setinrun` (needs ACQ stopped); trigger sample = `ChPreTriggers + 1`, not `ChPreTriggers`**; the pulse onset is BEFORE the trigger, so "N samples before the trigger" is not a clean baseline; `git ls-files` not `.gitignore` proves tracking |
| 18 | **Verification and measurement discipline -- transferable, tool-independent**: a tight error bar on a wrong model is not precision; verify plots in DATA COORDINATES not pixels (colour collision and colour dilution both defeat pixel-diffing); size axes from drawn artists not requested spans; a label's position decides which thing it belongs to; a mark must not contradict the physics; assert on every patch then re-read the line |

## MANDATORY: every file reference carries a repo tag

**This session works in two repos and both contain a `README.md`, a `docs/`, and an agent file
(`DAQ:agent/AGENT.md` vs `EMU:AGENTS.md`). A bare filename is therefore ambiguous and must not be written.**
Prefix every reference, in both this file and the session md:

| tag | means |
|---|---|
| `DAQ:` | `~/SOLARIS_DAQ` — the DAQ (this file lives at its root) |
| `EMU:` | `~/CAEN_DT5810B_LinuxController` — the DT5810B signal-emulator control repo |
| `LAB:` | `~/caen_signal_emulator` — the lab machine's own tree: vendor DLL + decompiled C#, the superseded old `dt5810.py`, manual PDF. **Absent on this machine.** NOT `scope/`/`experiments/`/`firmware/` — those are gitignored inside `EMU:` |
| `/tmp/...`, `/etc/...` | absolute, unambiguous, no tag needed |

Write `EMU:README.md:201`, not `README.md:201`. Write `DAQ:core/DigiManager.cpp:526`, not
`DigiManager.cpp:526`. When naming a *function*, still give the tagged file. Applies to anything you
add here and to the session md.

**Trust order:** this file > session md > `DAQ:docs/` > `DAQ:README.md`, for anything measured on *this*
bench on *this* machine. For register maps and vendor behaviour, `EMU:README.md` wins.

Conventions: UTF-8 is fine — `grep` handles it, and units are spelled out (`us`, `ohm`, `+-`) in
headings and table keys so an ASCII-only `grep` still finds them. Date every claim.
When you overturn something, keep the old claim and mark it **RETRACTED** — a deleted wrong answer
comes back.

---

## 1. Bench topology

| instrument | address | verified |
|---|---|---|
| digitizer **VX2745** SN 32575, DPP-PHA | `192.168.0.100` (host `enp131s0f1` = 192.168.0.234) | 2026-10-09 |
| scope **Rigol DHO4804** | `192.168.2.200:5555` SCPI (host `enp131s0f0` = 192.168.2.1) | 2026-10-07 |
| emulator **DT5810B** | USB `21e1:000e`, Bus 003 (device index drifts — re-read `lsusb`) | 2026-10-09 |

Digitizer facts (read live via CAEN_FELib, 2026-10-09): 16 bit, 125 Msps, tick2ns 8, 64 channels
(device tree has `ch/0..64` but only 64 are addressable — `/ch/64/...` correctly fails), FPGA version 1,
CupVer 2025012205. Web app "Digitizer 2.0" on :80 is an **SPA with no usable REST API** — `rest/devices`,
`rest/parameters`, `rest/system`, `api` all return the same `index.html`, HTTP 200 on *any* path
(catch-all routing). Do not try to script it. TCP **5340 is closed**; FELib reaches the board anyway
(`dig2://192.168.0.100` opens fine), so do not treat 5340 as a liveness check.

## 2. Emulator analog outputs — FAST vs HDR

Two analog connectors per channel, mutually exclusive by one `analogsel` bit (emulator repo
`EMU:README.md:201`, confirmed from vendor C#):

**HDR = High Dynamic Range** (front-panel label `OUTn HDR`). The same stage is called "High Voltage" in the manual and `ChannelRange.V10` in the vendor enum, so one output carries three vendor names. FAST = the fast/high-speed output. Source for "High Dynamic Range" is `EMU:README.md:201`, which cites the CAEN manual (`LAB:WEB_UM5312_DT5810_Fast_Digital_Detector_Emulator_v5.pdf`, 7.8 MB, not on this machine) — so the expansion is inherited from that citation, not read off the PDF.

| | FAST (`OUTn`) | HDR (`OUTn HDR`) |
|---|---|---|
| vendor enum | `ChannelRange.V2` = 0 | `ChannelRange.V10` = 1 |
| manual name | High Speed | High Voltage |
| rated swing | ±2 V @ 50 Ω, ±4 V high-Z | ±8 V high-Z (manual quotes this 4 ways: ±8, ±10, ±12, ±24 — design to ±8 V) |
| rise, filter OFF / ON | 1 ns / 25 ns | 25 ns / 42 ns |
| front-end | CFA op-amp, >2.5 GHz, 4000 V/µs | "more relaxed electronics", for PMT-like signals |

Same 16-bit DAC and datapath behind both. We run `0xF00000C2 = 0xF` (`EMU:dt5810.py:234`) = filter ON +
analogsel 1 = **HDR connector live**.

**Patching on this bench: HDR -> scope CH1, FAST -> scope CH2.** Stated three times two different
ways; the third is authoritative.

### Source impedance — both outputs are 50 Ω (measured 2026-10-09)

Terminating a channel halves its signal iff the source is 50 Ω.

| output | load | pulse (flat-top plateau) | ratio |
|---|---|---|---|
| HDR / CH1 | 1 MΩ | 2833 mV | — |
| HDR / CH1 | 50 Ω | 1399 mV | **2.024 → HDR source Z ≈ 51 Ω** |

**The "±8 V high-Z" figure is a swing rating, not a preferred load.** Both emulator analog outputs are
50 Ω sources.

**RETRACTION OF MY OWN RETRACTION (2026-10-12) — the FAST numbers are valid again.**
On 2026-10-09 CH2 read noise, and the user confirmed "scope ch2 should be empty", so on 2026-10-09 I
voided the 2026-10-08 CH2 captures as unattributable. **The reason CH2 went quiet is now known: the FAST
line was moved from the scope to the digitizer ch0.** The 2026-10-08 captures were FAST on CH2 all
along.

Independent confirmation, not just the explanation: §8 measured the FAST pulse arriving at the
digitizer's **100 ohm** input at **634 mV**. A 50 ohm source into 100 ohm predicts
`956 x 100/(50+100) = 637 mV` from the 2026-10-08 open-circuit figure. **634 measured vs 637 predicted
— 0.5 % agreement.** So **956 mV (FAST into 1 MΩ)** and the **HDR/FAST = 2.93** ratio are restored as
sound, and the "both connectors carry the same event, scaled 2.96x, skew -4 ns" finding stands.

**Still unresolved, and now better posed:** the emulator repo says FAST and HDR are **mutually
exclusive** (one `analogsel` bit selects the live connector), yet on 2026-10-08 both carried the same
pulse simultaneously with baselines moving together on one GUI field. Either `analogsel` does not mute
the other connector, or it only sets which is correctly ranged. Needs the lab machine's vendor code.

### Scope geometry and how to read it

DHO4804, `*IDN?` = `RIGOL TECHNOLOGIES,DHO4804,HDO4A262900576,00.02.13`. NORM acquisition = **1000
points over 10 divisions**, `:WAV:PRE?` gives `ymult`, `xincr`, `xorig`, `yref` (=128, so code 128 is
0 V). **Quantisation = ymult per code**: 34.1 mV/code at 1 V/div, 6.8 mV/code at 0.2 V/div. ADC rails
at **±4.34 divisions**, i.e. codes 0 and 255 — check clipping from codes, not volts.

Measurement discipline that has actually bitten here:

- **Confirm `:TRIG:STAT?` is `TD`, not `AUTO`,** before trusting any capture. `AUTO` = it never fired
  and you are looking at random windows. A trigger level *below* the baseline can never fire on a
  rising edge.
- **Use the flat-top plateau median, not argmax.** With 19 mV rms noise, argmax is a max-of-noisy-
  samples and is biased up ~1 code. Two captures of the same pulse differed 478 vs 471 mV on argmax
  alone — that spread was measurement, not drift.
- **`:WAV:DATA?` returns whatever `:WAV:SOUR` is set to.** Always `:WAV:SOUR?` and assert it matches
  before parsing; an earlier tool silently returned CH1 data when asked for CH2 and a CH1 trace was
  reported as CH2.
- Baseline figures are **±half a code** regardless of how many samples you average. Do not interpret
  sub-30 mV baseline differences at 1 V/div.
- **`:MEAS:ITEM?` is mostly unusable on this instrument** — `MAXimum`, `MINimum`, `MEAN`, `PKAMplitude`,
  `AMP`, `BASEline`, `PKtoPeak`, `RMS`, `TRATIO` all *time out*; `FREQuency`/`PERiod`/`PWIDth` return the
  `9.9e37` no-data sentinel (though `PWIDth,CHAN2` once returned a plausible 36 µs). Read the samples.
- `:ALL:POIN?`, `ALL:NORM?`, `ALL:WAV?` and `:WAV:DATA? CHAN2` (arg form) all time out. To read another
  channel you must write `:WAV:SOUR` — which is **not** on `EMU:bench_scope.py`'s six-command whitelist.
- SCPI writes return **no reply** (they look like timeouts); verify by readback, never by response.
  Bad tokens land in the error queue as `-100,"Command err"` and can pile up — drain with `:SYST:ERR?`.
- RIGOL impedance token is **`FIFTy`** (`:CHANn:IMP FIFT`), not `FIRSt`.

### Scope writes made during bring-up (all restored, verified by readback)

`:WAV:SOUR` CHAN2/CHAN1 (restored to CHAN1); `:CHAN2:SCAL` 0.2 and 0.05 (back to 1.0);
`:CHAN1:IMP` and `:CHAN2:IMP` → `FIFT` (both **restored to `OMEG`**). Final: `TD`, 5 µs/div, both
channels 1 V/div / 1 MΩ / probe 1×, `WAV:SOUR` CHAN1, error queue clean.

`EMU:bench_scope.py` whitelist is `:TRIG:EDGE:LEV`, `:CHANn:SCAL` (×4), `:TIM:MAIN:SCAL`.
`:WAV:SOUR` and `:CHANn:IMP` are **outside** it and were written only with the user's explicit
approval. If my read-back work is ever folded into `EMU:bench_scope.py`, keep the per-capture
`:WAV:SOUR?` assertion — that is the guard against the silent wrong-channel failure.

### The emulator can silently lose amplitude while config space reports no change
The first sweep reported ADCInput peaks of ~34 mV, then (after the window fix below) **184.3 mV
identical across all four passes**, against 631.6 mV captured at 12:17 the same day. Consistent across
passes, so not noise. Pulse SHAPE was unchanged (r=239.1 vs 238.2 ns, d=50.06 vs 50.20 us) -- only the
amplitude had collapsed to 29 %. Digitizer ruled out (FineGain 1.000, zin 100, dcoffset 50.001,
triggerthr 50, all unchanged; ch52 fed by emulator CH2 still read **620.7 mV**, so it was CH1-specific).
**Reprogramming ch0 with the IDENTICAL parameters restored it: `set_pulse(rate_hz=1000,
amplitude_v=1.0, decay_us=50.0, rise_us=0.2, baseline_v=0.0, ch=0)` -> 630.9 mV.** energy_reg/gain/
offset were the same values as before (27431/1184/-52451), so the register write did not change -- the
emulator's analog output had drifted or lost calibration in some way not visible from config space,
which is write-only (sec.5). **Mechanism UNRESOLVED.**
**RULE: never trust the emulator's amplitude between sessions -- config space cannot be read, and
`set_pulse` returning the same registers does not prove the output is unchanged. Check ADCInput pulse
height against the last capture BEFORE interpreting any sweep.** This is exactly the failure that would
have been reported as "the trapezoid attenuates 70 %".

## 3. Shaped-pulse measurements (HDR channel only — trustworthy)

Pulser set 1 V / 1 µs rise / 50 µs decay / 1 kHz; scope 1 V/div, 1 MΩ, 5 µs/div, 1000 pts NORM.

| quantity | value |
|---|---|
| τ (decay fit over 37.6 µs of tail) | **50.60 ± 0.09 µs** for 50 µs requested |
| pulse height (plateau, peak−baseline) | 2867 mV |
| baseline at GUI `Baseline` = 0 V | **+410 mV** |
| baseline at GUI `Baseline` = −0.15 V | **−68 mV** |
| baseline at GUI 0 V, scope terminated 50 Ω | +239 mV (baseline halves like the signal) |
| 10–90 rise | 1400 ns |
| baseline noise | 19.6 mV rms |
| max slew | 6.8 V/µs |

**τ is correct.** `EMU:pulser.py` applies `DECAY_OFFSET_US = 3.86` properly — measured τ matches the
requested value. Do not "fix" the decay correction.

### The correct pulse model: A(1 - e^-t/r) e^-t/d   (user-specified, 2026-10-12)

**This is the emulator's own shape, not an approximation.** `EMU:pulser.py build_shape` runs an IIR
low-pass (`arr[i] = arr[i-1]*(1-a) + x*a`, `:302`) on a bare exponential `x = exp(-t/decay)` (`:301`).
A single-pole filter driven by a decaying exponential is `(1-e^-t/tau_r) e^-t/d` in the continuous
limit; simulating the actual difference equation reproduces the closed form to **<1 %** (the residual is
discretisation). Fit the **whole pulse** with 4 params (A, r, d, t0), not the tail with 2.

Verified on the digitizer ch0 ADCInput trace: **A=690.5 mV, r=0.818 us, d=51.31 us**, residual
**0.98 mV**. Peak factor 0.9212 matches the documented `y_pk/A = (d/(d+r))(r/(d+r))^(r/d)`. `r` tracks
the emulator's `tau_r = rise_us/2.197` (1 us -> 455 ns) but the digitizer sees r=818 ns -- **the FAST
stage and cable add rise, so the observed edge is not the emulator's pole alone.**

**RETRACTED (same day): "the product fit is 19x better than the tail fit."** Apples to oranges -- 1.05 mV
was the product fit on the whole pulse, 19.7 mV was the **trapezoid flat-top** residual (§11), a
different signal and window. **Fair comparison, same signal (ADCInput) and same window (whole pulse):**
product 0.98 mV vs pure-exponential 44.95 mV = **46x**. But the tail fit **on its own window**
(t > peak + 0.2 us) is perfectly good: 1.52 mV residual, tau 51.609 us. **The tail fit is not broken --
it is only wrong if applied outside the region it models.**

**Fit the WHOLE pulse -- a tail fit's answer depends on where you declare the tail to start** (user's
point 2, 2026-10-12). Measured on one trace, baseline fixed at the pre-trigger median throughout, so
this isolates the window choice alone:

| tail declared to start at | tau |
|---|---|
| peak + 0.2 us | **51.609** +- 0.018 us |
| peak + 1.0 us | 51.293 +- 0.006 us |
| peak + 3.0 us | 51.208 +- 0.004 us |
| peak + 6.0 us | 51.227 +- 0.005 us |

0.8 % swing from window alone. **With the baseline also free it becomes 58.6 -> 52.6 -> 50.9 -> 51.2 us
-- a 15 % swing**, and the quoted +-0.02 us error bar captures none of it. The whole-pulse 4-parameter
form has no such knobs: `r` absorbs the rise, so `d` is set by the decay alone and is stable across
windows. **Any tail-only tau in this file is only as good as its (usually undocumented) window. Prefer
the product fit.**

**Window systematic, small but real:** d = 51.31 us over the full record vs 51.50 us over T>200 ns..25 us
= **0.35 % spread**, comparable to the quoted +-0.012 us statistical error. Quote the window with d.

**Second same-day retraction, a DIFFERENT error from the one above** (that one was a bad comparison;
this one is a bad attribution). **"The pure-exponential tail fit is biased +14.3 % by using the wrong
fit function."** Wrong -- that number came from a *free-baseline* fit, the error I had already retracted in
§11. Refitting the same data three ways, **baseline fixed at the pre-trigger median throughout**:

| fit | d / tau |
|---|---|
| (1) product form, whole pulse | **51.314 +- 0.012 us** |
| (2) pure-exponential tail, baseline FIXED (what I originally used) | 51.609 +- 0.018 us |
| (3) pure-exponential tail, baseline FREE | 58.637 +- 0.303 us |

Fit function accounts for **0.6 %** ((1) vs (2)), not 14 % -- `e^-t/r` is gone by t~4 us, so the tail
genuinely is exponential over the fitted range. **The 14 % was (2) vs (3): the baseline, again.**
**Lesson: when comparing two fits, change ONE thing.** I reused my own retracted number as if it were a
new measurement. **d = 51.3 us stands**, so the §11/§12 pole-zero comparisons were not wrong.

Use the product form regardless: 19x smaller residual, and it reports `r`, which the tail fit cannot.

**10–90 rise is NOT the rise parameter on a shaped pulse.** For r = 1 µs, d = 50 µs the ideal 10–90 is
**1812 ns** (the peak is only 0.904·A and the shape climbs slowly). Measured 1400 ns is consistent, not
a fault. Never use 10–90 as a bandwidth or filter test here; a reflection round-trip on a ~1 m cable
is ~10 ns, sub-sample at 50 ns/pt, so "no visible ringing" is weak evidence either way.

### Baseline field behaviour (2-point calibration only — not a law)

One GUI `Baseline` field drives both outputs (they are the same channel's two taps).

- `Baseline = −0.15 V` zeroes **FAST** (0.0 mV) and leaves HDR at −68 mV.
- To zero **HDR** use ≈ **−0.13 V**.
- The two can never be zero simultaneously — HDR is ~3.2× more sensitive to the field than FAST
  (3.19 vs 0.91 mV out per mV of field).
- Measured DC relation, HDR: `HDR_mV = 3.50 × normal_mV − 68.3`.

**Caveat:** a 2-point calibration (−0.15 V and 0 V), so the slopes are poorly constrained. The
2026-10-08 CH2 data they use **is** valid (see the retraction-of-the-retraction in §2 — FAST was on CH2,
confirmed to 0.5 % by the digitizer's 634 mV reading in §8). The HDR-side figures (−68 mV at −0.15 V,
+410 mV at 0 V) are directly measured.

### `EMU:pulser.py` has no load-impedance term — the real calibration hazard

The vendor C# *asks the user for the load impedance* and divides by it
(`LSBToV` / `ImpedanceToDivison: 0.5 for 50 Ω, 1.0 for high-Z` — the output stage **doubles** when
open-circuit). `EMU:pulser.py` hardcodes one case, high-Z. Consequences:

- On a **50 Ω** load the GUI over-reports amplitude by ~2×, and the baseline law halves too.
- On the **digitizer's 100 Ω** input (§4), 50 Ω source → 100 Ω load = **0.667, not 0.5** → the GUI
  over-reports by ~1.5×. This is the real DAQ condition, so it is not a bench-only curiosity.
- Adding a 50 Ω scope on a tee to that line makes the load 100 ∥ 50 = 33 Ω → 0.4, wrong twice over.
  **Terminate at one point only; a scope tee'd onto a terminated line must be 1 MΩ.**

**Unresolved contradiction — do not "fix" from one side.** `EMU:README.md` §5 states its calibrations
were taken "At gain 1184, **1 MΩ**, CH0" and gives `1.00 V → 1.029 V`, while `EMU:README.md` §3 warns "we have been
running on HDR this whole time" (`analogsel = 1`). Both cannot be true: HDR reads 2867 mV for a
requested 1 V. Either §5's amplitude law is really a FAST measurement, or HDR gain has changed since.
Needs `LAB:` (the decompiled C# holds `HS_LSB_V` / `HV_LSB_V`, absent here) to settle.

## 4. Digitizer input, read live (2026-10-09)

| node | value |
|---|---|
| `/par/zin` | **100 Ω** |
| `/par/inputrange` | **4 V** |
| `/par/inputtype` | **DIFF** (differential) |
| `/par/boardready` | True |
| `/ch/0/par/dcoffset` | 50.001 % (16-bit DAC baseline trim) |
| `/ch/0/par/adctovolts` | 0.000060944 V/count |

**`inputtype = DIFF` while the emulator output is single-ended** — worth checking before any real
acquisition. Flagged, not investigated.

## 5. CAEN_FELib on this machine — how to talk to the board

### Path format (this cost real time)

`CAEN_FELib_GetValue` needs a **leading slash**: `/par/zin`, `/ch/0/par/dcoffset`. Without it
(`par/zin`, `zin`) it returns **−6 Command error**, which looks exactly like a failed connection.
Node names in the device tree are **lowercase**, but `GetValue` is case-insensitive (`/par/LedStatus`
and `/par/ledstatus` both work).

**To discover node names, dump the tree — do not guess:**

```c
int need = CAEN_FELib_GetDeviceTree(handle, NULL, 0);   /* returns byte count */
char* buf = malloc(need + 8);
CAEN_FELib_GetDeviceTree(handle, buf, need + 4);        /* 2.3 MB JSON on the VX2745 */
```

The JSON has every node with `value`, `accessmode`, `datatype`, `uom`, `allowedvalues`,
`description`, and a `multiplevalue` flag. Far better than guessing names.

### Bulk channel paths are NOT supported for reads (tested to destruction, 2026-10-09)

`GetFullPara(ch_index = -1)` (`DAQ:core/DigiParameters.h:64`) builds `/ch/0..63/par/<name>`. On the
VX2745 that **always fails as a read**.

| test | result |
|---|---|
| `/ch/N/par/dcoffset` for all N=0..63 | **1 distinct value** (`50.001`) — perfectly uniform |
| `/ch/0..63/par/dcoffset` on that uniform param | **ERR** |
| the 9 `multiplevalue=true` params (`coincidencemask`, `anticoincidencemask`, `channelstriggermask`, `eventselector`, `eventtriggersource`, `itlconnect`, `wavesaving`, `waveselector`, `wavetriggersource`) in bulk form | **all ERR** |
| same params single-channel | OK (`/ch/0/par/coincidencemask` → `Disabled`) |
| syntax variants `/ch/0..63/`, `/ch/0-63/`, `/ch/{0..63}/`, `/ch/*/`, `/ch/0..63/par`, `/ch/0..6/` | **every one ERR** |
| `/par/zin` (board level, no channel) | OK → 100 |
| `CAEN_FELib.h` | documents **no** range or wildcard path syntax at all |

**Hypothesis tested and REJECTED: "bulk read fails because each channel can hold a different value."**
A provably uniform parameter fails just as hard, and the genuinely list-valued `multiplevalue=true`
params fail too. **The range form is simply not implemented for `GetValue`.**

`SetValue` with the same form **returns success** — now read as a **likely false positive** (unknown
path accepted, nothing applied), not genuine support. **Still unconfirmed**: proving it needs a real
value written across 64 channels. Test built and NOT run: `/tmp/zprobe/wtest.c` (single-write a
distinct value to ch5, bulk-write the original back, read ch5, then restore and sweep all 64).
Needs the user's go-ahead — it writes a non-original value to a real digitizer.

**Iteration mechanism:** the documented way is `CAEN_FELib_GetChildHandles(h, "/ch", handles, n)` →
64 handles, but **relative paths from a child handle also ERR** (`"par/dcoffset"` → −6). Only absolute
`/ch/N/par/<name>` works, so child handles buy nothing. Iterate with explicit indices.

**Not a live bug:** no call site anywhere passes a CH-type `Reg` without an index. All 14 single-arg
`ReadValue(Reg)` sites are `TYPE::DIG` (board-level `/par/...`, which works), and `ReadAllSettings`
uses `ReadValue(chSettings[ch][i], ch)` (`DAQ:core/ClassDigitizer2Gen.cpp:1543`). Latent only — it becomes
a silent failure the moment anyone calls a CH register without an index.

### Read cost is NOT a problem (measured — corrects an earlier guess)

192 individual `/ch/N/par/<name>` reads = **31 ms**; via child handles 47 ms (slower). At the 2 s
scalar interval that is ~1.5 % duty. **RETRACTED: "192 reads is why status feels sluggish."** 31 ms
does not explain it. If status seems stale, suspect the refresh wiring (below), not the read cost.

### Reading the board directly, bypassing the broker

CAEN_FELib works standalone and is handy for one-off reads — but the broker holds an **exclusive**
connection, so a probe must wait for the broker to exit. Throwaway probes live in `/tmp/zprobe/`
(`zin2`, `dtree`, `stat`, `uniform`, `iter`, `wtest`); they are not in the repo.

**Watch out:** `pkill -f solaris-broker` matches the invoking shell's own command line and kills your
shell (exit −15; bit me twice). Use `pgrep -x solaris-broker | xargs kill`.

## 6. Build state (2026-10-09 — all deps installed, all three binaries build)

| artifact | status |
|---|---|
| `solaris-broker` (895,760 B) | builds, runs, verified against real hardware |
| `solaris-cli` (952,968 B) | builds (needs `libreadline-dev`) |
| `SOLARIS_DAQ` GUI (36.6 MB) | builds, links Qt6.4.2 Charts/Widgets/PrintSupport + FELib + zmq; launches and shuts down clean |

All three are `.gitignore`d, so a clean build leaves the tree clean. **`qmake6` = Qt 6.4.2; the old
`qmake` is Qt5.15 and must not be used.** Deps that were needed: `libzmq3-dev`, `qt6-base-dev`,
`qt6-charts-dev`, `libreadline-dev`, CAEN_FELib (installed under `/usr/local`). `make -C broker`
builds `cli` *after* `broker`, so a readline failure makes the whole target look broken even though
the broker linked fine — build `../solaris-broker` explicitly if cli is unavailable.

### Broker + CLI, verified end-to-end

```bash
./solaris-broker --ip 192.168.0.100 &          # ~20 s to open the board, then "Broker running."
printf 'connect\nping\nlist\ninfo 0\nread 0 /par/zin\nquit\n' | ./solaris-cli
```

- **`solaris-cli` does NOT auto-connect.** Every command returns "Not connected" until you send
  `connect`. It is readline-interactive, so pipe commands on stdin.
- Verified: `list` → `0 32575 VX2745 DPP_PHA 64`; `info 0` → tick2ns 8, FPGA ver 1, CupVer 2025012205;
  `read 0 /par/zin` → 100, `/par/inputtype` → DIFF, `/par/inputrange` → 4, `/ch/0/par/dcoffset` → 50.001.
- Broker auto-detect: the GUI pings `tcp://localhost:5555` at startup (`DAQ:GUI/mainwindow.cpp:1955-1975`)
  and picks standalone mode when nothing answers. No broker running → standalone.

## 8. Trace capture from the digitizer — ch0, verified working (2026-10-09)

**Patching corrected: FAST -> digitizer ch0, HDR -> scope CH1.** Scope CH2 is empty *because the FAST
line moved to the digitizer* — which finally explains the 2026-10-08 vs 2026-10-09 discrepancy in §2.
The 2026-10-08 CH2 captures were not bogus; the cable moved afterwards.

**RETRACTED: my "nothing is connected to the digitizer" conclusion (2026-10-09).** I inferred it from
`chtriggercnt`/`chrealtimemonitor`/`chsavedeventcnt` all reading 0 across ch0..ch7. Wrong — those
counters do not advance outside an acquisition. `SelfTrgRate` = 1000 Hz on ch0 was already telling the
truth. **Do not use the counters as evidence of absence; use `SelfTrgRate`, or capture a trace.**

### Scaler scan, all 64 channels (read-only, CAEN_FELib)

`/ch/N/par/selftrgrate` — **ch0 = 1000 Hz, every other channel = 0.** Steady across six 1 s samples.
`triggerthr` = 50 counts = 3.05 mV on every channel (60.94 uV/count), so a ~600 mV pulse triggers
easily. All 64 channels `ChEnable = True`.

### How the trace was taken

`DAQ:solaris-cli` -> `trace 0 0 4200`. It writes `ChEnable` False on all / True on ch0,
`WaveSaving=Always`, `WaveTriggerSource=ChSelfTrigger`, starts ACQ in `DataFormat::ALL` with
**no file saving**, waits <=10 s for a `PUB_TRACE`, stops ACQ, and restores. Verified restored after:
`WaveSaving=Always`, `WaveTriggerSource=ChSelfTrigger`, `ChEnable=True`, `acquisitionstatus=8`
(unchanged from before). **Note it restores `ChEnable=True` on ALL channels without saving per-channel
state** — harmless here (all 64 were already True), but it would silently enable a deliberately
disabled channel. ch0 was already configured for traces before I touched anything.

### The captured pulse: 4000 samples x 8 ns = 32.0 us

| quantity | value |
|---|---|
| baseline | 34152 counts = **2081 mV** (~52 % of the 16-bit range, consistent with `dcoffset` 50.001 %) |
| peak | 44560 counts = 2716 mV at sample 553 = 4.42 us (trigger at sample 250 = 2.00 us) |
| **pulse height above baseline** | 10408 counts = **634 mV** |
| 10-90 rise | **1456 ns** |
| **tau** | **51.55 +- 0.02 us** (fit over 27.6 us of tail; residual 1.56 mV rms) |
| baseline noise | 44 counts = **2.71 mV rms** |
| `analog_probes[1]` (TimeFilter) | bipolar, -360..+1083 — the shaped output, not the raw input |

### The amplitude confirms the impedance model to 0.5 %

FAST is a 50 ohm source (§2). Measured into the scope's 1 MΩ it was 956 mV (2026-10-08). Into the
digitizer's **100 ohm** (§4) it should be `956 x 100/(50+100)` = **637 mV**. **Measured: 634 mV.**
That is the whole 50-ohm-source / 100-ohm-load story checked end to end across two instruments, and it
independently re-validates the 2026-10-08 CH2 numbers I had marked void.

Consequence stands: `pulser.py` reports the **open-circuit** amplitude, so the GUI's "1 V" is really
~634 mV at the digitizer input. Off by 1.5x, not 2x (the 2x figure applies only to a 50 ohm load).

τ = 51.55 us vs 50 us requested and 10-90 = 1456 ns vs the 1812 ns ideal for r=1us/d=50us: both
consistent with the scope measurements of the same pulse on HDR. The digitizer sees the same shape.

## 9. Trapezoid (EnergyFilter) capture — ch0 (2026-10-12)

Answering "have you captured the trapezoid?" -> **no, not before this.** The two probes default to
`ADCInput` / `TimeFilter`; the trapezoid is `EnergyFilter`, which is option **2** of the five
(`ADCInput`, `TimeFilter`, `EnergyFilter`, `EnergyFilterBaseline`, `EnergyFilterMinusBaseline`).

### Setting change needed, and how to undo it

`/ch/0/par/WaveAnalogProbe1 = EnergyFilter` (probe 0 left on `ADCInput`, so one capture shows input and
output together). Record length 4000 samples / 32 us was already enough — see the timing below.

**`DAQ:solaris-cli`'s `trace` command does NOT restore the probe setting.** It saves and restores
`WaveSaving`, `WaveTriggerSource` and `ChEnable` only. `WaveAnalogProbe*` must be restored by hand:
`write 0 /ch/0/par/WaveAnalogProbe1 TimeFilter`. Verified back to `ADCInput`/`TimeFilter` after the
capture, along with `WaveSaving=Always`, `WaveTriggerSource=ChSelfTrigger`, `ChEnable=True`,
`ChRecordLengthS=4000`, `ChPreTriggers=250`, `acquisitionstatus=8`.

`WaveAnalogProbe0/1` and `ChPreTriggers` are `setinrun=true` (changeable during acquisition);
`ChRecordLengthS/T` are **not** -- a record-length change needs ACQ stopped first.

### The board's shaping settings (ch0, read-only)

`EnergyFilterRiseTimeT` 5000 ns · `EnergyFilterFlatTopT` 1000 ns · `EnergyFilterPoleZeroT` 50000 ns ·
`PileUpGuardT` 960 ns · `BaselineAvg` = Medium · `PeakingAvg` = OneShot · `FineGain` 1.000

### Measured trapezoid

| quantity | measured | programmed |
|---|---|---|
| peak | **654 mV** | (input step was 634 mV) |
| rise 10->90 % | **4064 ns** | 5000 ns |
| flat top (>97 % of peak) | **1072 ns** | 1000 ns |
| fall 90->10 % | **4168 ns** | -- |
| total width 10->10 % | 18304 ns | -- |
| baseline after the pulse | **+1.5 mV** | 0 (pole-zero should cancel) |

**Rise/fall are symmetric to within 104 ns** (4064 vs 4168) -- a clean, well-formed trapezoid, not a
slewed one. **The pole-zero works**: the 50 us exponential input tail is fully cancelled, baseline back
to +1.5 mV by the end of the record. Flat top 1072 ns against 1000 ns programmed is the 8 ns/sample
quantisation plus the 97 % threshold choice.

**RETRACTED: "rise reads 4064 vs 5000 ns programmed, a 19 % gap, not chased."** There is no gap.
`EnergyFilterRiseTimeT` is the duration of the **linear ramp**, and for a pure linear ramp the 10-90
crossing is **0.8 x ramp = 4000 ns by definition**, not 5000. Measured 4064 ns = ratio **1.016**. The
filter is behaving exactly as programmed; the "deficit" was my arithmetic. **For a trapezoid always
compare 10-90 to 0.8R, never to R.**

**Probe 1 showed 0.00 mV rms noise on the baseline (a single count value, 4104, across 150 samples).**
That is not a low-noise win -- the EnergyFilter output is a heavily smoothed digital signal, so the
ADC noise is filtered away. Do not read it as "the trapezoid is cleaner than the input" for any purpose
other than SNR of the pulse height.

Plot was `/tmp/solaris_ch0_trapezoid.png`, data `/tmp/trace_trap.txt` -- **both gone, wiped by the
2026-10-12 reboot**. The surviving equivalent capture is `DAQ:agent/data/real_trap.txt`, verified to
reproduce these numbers (pedestal 2081.4 mV, peak 2715.4 mV, height 634.1 mV).

## 10. Digital probes — the four signals, and what each is worth (2026-10-12)

`/ch/N/par/WaveDigitalProbe0..3`, 13 sources each, `setinrun=true` (changeable during ACQ). Defaults on
this board: `Trigger`, `TimeFilterArmed`, `EnergyFilterPeakReady`, `EventPileUp`.

**I ignored the digital probes entirely in the first two captures** (2026-10-09, 2026-10-12). The
`solaris-cli` dump prints D0..D3 as columns and they were in the data the whole time; I looked only at
the analog columns and, when I did print rows, printed the tail of the record where every probe is idle
-- so the visible D columns read `0 0 0 0`. They were never zero. **The 8-640 ns pulses are 1-80
samples out of 4000, so eyeballing a row dump cannot find them. Always compute `d.sum()` and the run
list.**

### Measured widths, ch0, 1 kHz, 32 us record

| source | high window | width | useful? |
|---|---|---|---|
| `Trigger` | 2.008 -> 2.016 us | **8 ns** (1 sample) | marks the trigger instant; too narrow to see by eye |
| `TimeFilterArmed` | 1.376 -> 2.016 us | **640 ns** | the pre-trigger arming window; ends exactly at the trigger |
| `ReTriggerGuard` | 2.016 -> 2.024 us | 8 ns | starts exactly at trigger end -- dead time, but invisible at this rate |
| `EnergyFilterPeaking` | 7.528 -> 7.536 us | 8 ns | marks flat-top start |
| `EnergyFilterPeakReady` | 7.544 -> 7.552 us | 8 ns | 16 ns after Peaking -- **near-duplicate of it, not worth a probe slot** |
| `EnergyFilterPileupGuard` | 7.544 -> 9.072 us | **1528 ns** | the pile-up rejection window; the only wide one, spans flat-top + fall start |
| `EnergyFilterBaselineFreeze` | 1.992 -> **end of record** | >=30 us | at L=5000/M=1000 it never releases within a 32 us record. **UPDATE 2026-10-14: it DOES release -- 6312 ns at RiseTimeT 1000 / FlatTopT 3000.** So the duration tracks the FILTER SETTINGS, it is not a fixed open-loop behaviour; the old reading was an artefact of those defaults plus a short record. |
| `AcquisitionInhibit` | -- | idle | nothing to inhibit at 1 kHz |
| `EventPileUp` (default D3) | -- | idle | no pile-up at 1 kHz |
| `ADCSaturation`, `ADCSaturationProtection`, `PostSaturationEvent` | not tested | -- | would need the input driven into saturation |
| `EnergyFilterSaturation` | **measured 2026-10-14: asserts with NO saturation present** | HIGH **44 % of a 2000-sample record, 56 % of a 4000-sample one**, runs both at the record edge and mid-record (e.g. -4008..-3624 and +6432..+11992 ns rel. trigger) | **INCONCLUSIVE -- do NOT report this as a real saturation event, and do NOT attribute it to the pulse.** ADCInput peaks at only 631 mV of 4 V (68 % of range), and the duty cycle is AMPLITUDE-INDEPENDENT: 44 % at a 185 mV pulse, 56 % at 632 mV. Not a short-record artefact either (both lengths show it). Uninvestigated since; the next test is whether it persists with the input disconnected. |

**Settled on** (user asked for trigger, peak, and something trigger/trapezoid-related):
`D0 Trigger`, `D1 TimeFilterArmed`, `D2 EnergyFilterPeaking`, `D3 EnergyFilterPileupGuard`.
Chose `EnergyFilterPeaking` over `EnergyFilterPeakReady` because the two are 16 ns apart and Peaking is
the one that coincides with the flat-top onset; chose `EnergyFilterPileupGuard` as the only source that
produces a visible window tied to the trapezoid.

**Timing that fell out:** trigger at 2.008 us -> trapezoid flat-top starts 7.528 us -> **5.52 us
latency**, against `EnergyFilterRiseTimeT` = 5000 ns. Consistent. PileupGuard opens 16 ns after the
peaking marker and holds 1528 ns.

### Probe settings persist across captures and are NOT restored

User's standing instruction (2026-10-12): **do not bother restoring probe/trace settings** -- the
`solaris-cli` `trace` command already restores what matters (`WaveSaving`, `WaveTriggerSource`,
`ChEnable`) and probe assignments are deliberately left as configured. Current live state on ch0:
`WaveAnalogProbe0=ADCInput`, `WaveAnalogProbe1=EnergyFilter`, `WaveDigitalProbe0=Trigger`,
`WaveDigitalProbe1=TimeFilterArmed`, `WaveDigitalProbe2=EnergyFilterPeaking`,
`WaveDigitalProbe3=EnergyFilterPileupGuard`.

Plot was `/tmp/solaris_ch0_trapezoid_digital.png` (3 panels), data `/tmp/trace_final.txt` -- **both
gone, wiped by the 2026-10-12 reboot**. This table is reproduced exactly by the surviving
`DAQ:agent/data/trace_six_1013_1308.txt` (widths 8 / 640 / 8 / 1528 ns), so the numbers are
re-verifiable; the figure is not. Regenerate with `python3 DAQ:agent/plot_capture.py`.

## 11. Trapezoid filter -- design rules and whether ch0 satisfies them (2026-10-12)

### What the trapezoid actually is (user-corrected, 2026-10-12)

**A linear running-sum filter, not a charge collector.** The CAEN DPP "FP" form is `-L, M, +L`
convolved with the digital signal: `y1 = a*y1[i-1] + x[i] - x[i-L]` (pole-zero, L = RiseTimeS), then a
moving sum over M = FlatTopS, then a moving sum over L. Output is fully determined by input + three
constants -- **there is no integration window that can "miss" charge**.

Consequences for tuning:
- **For a perfect square input the plateau is independent of rise time.** Rise time is a NOISE and
  timing trade-off, not a signal-amplitude one. Do not read a rise-time dependence as signal loss.
- Output amplitude scales with L and M **by construction** (filter gain). Any "energy changed when I
  changed L" must be checked against the gain before being attributed to the signal.
- `EnergyFilterRiseTimeS` = L, `EnergyFilterFlatTopS` = M; the `...T` forms are the same in ns.
  Defaults here: L=625, M=125 at 8 ns/sample.

### What "good" means -- criteria (cited below; this is the definition)

**One sentence: the longest, flattest-top pulse the input statistics allow, with the flat top landing
where peak-hold samples and the baseline restored before the next event.**

| # | criterion | measured on this bench |
|---|---|---|
| 1 | **energy resolution** FWHM/E -- the objective function | needs a clean monoenergetic line; see noise floor below |
| 2 | **flat top actually flat** (tilt << noise) | -0.04 +- 4.17 mV/us -- INCONCLUSIVE, see below |
| 3 | **peak-hold lands on the flat top** | PASS: Peaking 7.528 us inside 7.46..8.53 us |
| 4 | **pole-zero matched to the real input decay** | 50.00 vs 51.55 us, 3 % off; +1.5 mV residual |
| 5 | **pile-up guard covers the whole excursion** | **FAIL: guard 1528 ns vs pulse width 18.3 us** |
| 6 | ~~no ballistic deficit~~ **NOT A VALID CRITERION** -- the trapezoid is a linear running-sum filter and cannot fail to collect charge (see the definition section) | withdrawn |
| 7 | **baseline restores within the dead time** | untested vs rate |
| 8 | **throughput: filter length << event spacing** | not limiting at 1 kHz |

Criteria 1 and 8 are the ones that *trade against* 2-6 -- which is why you cannot just maximise rise
time. Series noise wants long rise, parallel and 1/f noise want short; flat-top is nearly free so make
it just long enough for trigger jitter. Standard DSP, not repeated at length here -- the bench-specific
consequence is the noise floor at the end of this section.

### User's rule: pole-zero == signal decay
(verified at the 2026-10-12 factory defaults: rise 5000 ns, flat-top 1000 ns, pole-zero 50000 ns)

### RETRACTED, user correction 2026-10-14: "Rule 1: filter rise >= signal rise"

**There is no such rule.** Filter RiseTimeT can be anything; it is not constrained by, and does not
need to be compared against, the signal's rise time. **Rise time controls the trapezoid's own
"energy" -- an amplitude/gain property of the filter -- and has nothing to do with the signal.**
The 3.4x margin I computed and reported as "verified satisfied" was checking a constraint that does
not exist, so it was never evidence of anything.

This is the same error as the withdrawn criterion 6 ("no ballistic deficit") one layer up: I kept
looking for a signal-side justification for a filter parameter. Consistent with the definition section
(output amplitude scales with L and M **by construction**, i.e. filter gain, not signal), and directly
supported by the bench: §13g changed the SIGNAL rise 1.0 -> 0.5 -> 0.2 us and the ADCInput peak moved
634.3 -> 634.1 -> 631.6 mV -- essentially nothing -- while the filter's own L is what sets output
height.

**What survives from the deleted paragraph** (all recorded elsewhere, nothing lost): the trapezoid
10-90 of 4064 ns at L = 5000 ns and the 0.8R rule are in §9 / §13b; the signal's 1456 ns 10-90 is in
§3. Only the *comparison* of the two, and the word "margin", are gone.
**Do not reintroduce any lower or upper bound on RiseTimeT relative to the signal rise.**

Signal decay **51.55 us** vs PoleZero **50.00 us** -> 3 % mismatch. Consequences:

- End-of-record baseline residual: **+1.5 mV** (0.2 % of pulse). Small but nonzero, consistent with the
  mismatch.
- **Flat-top tilt test: INCONCLUSIVE.** Expected tilt for a 50 us filter on a 51.55 us input is
  A*(1/tau_pz - 1/tau_sig) = 654 * 0.000602 = **0.39 mV/us**. Measured **-0.04 +- 4.17 mV/us** -- the
  signal is 10x below the noise. All we can say is |tau_pz - tau_sig| < ~16 us (1 sigma).
  Flat-top residual is **324 counts = 19.7 mV rms**, NOT the 2.71 mV input noise -- because
  `EnergyFilterPeakingAvg = OneShot` (no averaging), so instantaneous noise reaches the flat top.
  **Switching PeakingAvg to LowAVG/MediumAVG/HighAVG would shrink this and make the tilt test usable**
  -- untested, and it trades resolution for the diagnostic.

**The important caveat on rule 2:** the pole-zero must match the **real detector's** decay, not the
emulator's. The emulator is currently *set* to 50 us, so matching it proves only that the filter tracks
what it is fed. Before finalising PoleZero for physics, the actual MUSIC / crystal decay constant is
what matters, and it is not something this bench can tell us.

### Criterion 5 FAILS -- the one actionable defect found
`EnergyFilterPileupGuard` holds **1528 ns**, but the trapezoid's 10-10 % width is **18.3 us**. A pulse
arriving on the flat top or the fall is NOT guarded, so it would be accepted and its energy corrupted.
Harmless at 1 kHz (0.1 % duty); **must be widened before pushing the rate.** `EnergyFilterPileupGuardT`
range is 0..64000 ns in 64 ns steps, so ~18304 ns is available. Set to cover flat-top + fall + tail.

### Noise floor limits what this bench can optimise
Digitizer input noise 2.71 mV rms on a 634 mV pulse -> **FWHM/E floor ~1.0 %** set by series noise
before any filter tuning. If the emulator's own amplitude scatter is comparable, criterion 1
(resolution) cannot be optimised here -- only the surviving mechanism criteria (2-5) can. **Not yet
measured; and see §12, which reached the same verdict by a different route.**

### `hist` gotcha
Energies for this setup sit near **10450** in channel units. `hist 0 0 20 400 2000` reported
`mean=10450.7 over=9391` -- **every event overflowed**, and `under=0` is what tells you. Use
`xmax >= ~12000`, or read `over`/`under` first and treat a non-zero one as an invalid histogram.

## 12. Trapezoid TUNING sweep (2026-10-12) -- what tuning this bench can and cannot do

Objective function: **energy resolution** FWHM/E from `DAQ:solaris-cli hist` on the DT5810B as a
monoenergetic 1 kHz source. Method: de-binned moment sigma -- `sqrt(m2 - binwidth^2/12)`, FWHM =
2.355*sigma. **Do not read FWHM off a coarse histogram**: at 4000 bins the crossing quantised to
exactly 4 bins (7.0 ch) and looked identical at every setting.

### Baseline and a RETRACTED claim
Resolution is **0.09-0.10 %** (FWHM ~9.3-10.4 ADC channels on a mean of ~10350).
**RETRACTED: "FWHM/E floor ~1.0 % set by series noise before any filter tuning."** That applied raw
per-sample noise (2.71 mV rms) to the energy and ignored the point of the filter. Measured filter output
sigma is **0.36 mV = 7.5x lower** than the input noise -- the trapezoid averages broadband noise down,
which IS its job. **Resolution here is set BY the filter, so it is tunable on this bench.** The earlier
claim would have wrongly closed off the whole exercise.

### Node names: `EnergyFilterRiseTime` DOES NOT EXIST
Only `EnergyFilterRiseTimeT` (ns) and `EnergyFilterRiseTimeS` (samples) exist. My first rise sweep wrote
the bare name and produced a flat, meaningless result. **Read the node back after every write.**
Useful distinction found by accident: writing a **nonexistent parameter name** fails loudly
(`Failed: WriteValue failed ... /ch/0/par/EnergyFilterRiseTime = 9999`), whereas writing a **nonexistent
channel range** (`/ch/0..63/...`) returns success (§5). So the silent-success hazard is specific to the
range syntax, not to bad writes in general.

### Rise-time sweep (P-Z left at 62000 -- see confound)
| RiseTimeT | mean ch | FWHM ch | vs default |
|---|---|---|---|
| 2000 ns | 9493 | 13.53 | **+3.60 = 7.7 sigma -- REAL** |
| 3000 | 9928 | 10.78 | 1.8 sigma |
| **5000 (default)** | 10345 | 9.93 | -- |
| 8000 | 10570 | 9.26 | 1.4 sigma |
| 11000 | 10643 | 10.85 | 2.0 sigma |
| 13000 | 10660 | 10.36 | 0.9 sigma |

Run-to-run FWHM scatter is **sigma 0.47 ch** (from 3 repeats at identical settings), vs a statistical
error of 0.03 ch at N=7000 -- **scatter dominates by 15x**, so it is the only error bar that matters.

**RETRACTED (2026-10-12): "everything from 3000 to 13000 ns is within 2 sigma of the default -- there
is NO resolvable resolution optimum."** That verdict used **FWHM, the wrong observable.** Run-to-run
scatter of the **mean energy** is **0.81 ch** (3 repeats at identical settings) while the rise-time
effect is **hundreds of channels**. FWHM scatter (0.47 ch) is 15x the statistical error and hides a
response the mean resolves trivially. **On a monoenergetic source the mean is the sensitive observable;
FWHM is not.**

**Corrected rise-time sweep, at the P-Z default (50000 ns), mean energy:**

| RiseTimeT | x signal-rise (1456 ns) | mean ch | vs 5000 ns |
|---|---|---|---|
| 1600 ns | 1.10 | 9258 | **-11.4 %** |
| 2400 | 1.65 | 9757 | -6.7 % |
| 2900 | 1.99 | 9966 | **-4.7 %** |
| **5000 (default)** | 3.43 | **10455** | -- |

Monotonic and still rising at 5000 ns (saturating: 8000/11000/13000 ns gave +2.1/+2.8/+3.0 % in the
earlier P-Z=62000 sweep).

**RETRACTED (2026-10-12, user correction): calling this "ballistic deficit / incomplete charge
collection" was a physics error.** The trapezoid is **not** a charge collector -- it is a linear
running-sum filter (`-L, M, +L`, see the definition section). **A linear filter cannot fail to collect
charge.** Verified two ways:
- Simulating the FP filter on a **perfect square** input, the plateau scales with L by construction
  (625 -> 121094 counts for L = 25 -> 1000 samples). Output amplitude depending on L is a **gain
  property of the filter**, not a property of the signal. For a square input the rise time is
  arbitrary -- exactly the user's point.
- The measured effect **saturates by ~8000 ns (1000 samples) and totals only 3 %**. Real ballistic
  deficit against a 51.3 us decay would scale on tau_d = 6412 samples = 51300 ns. It does not.

**The residual rise-time dependence is UNATTRIBUTED -- and filter gain does not explain it either.**
Gain proportional to L predicts 1600->5000 ns should raise the energy 3.13x (+213 %); measured is
+13 %. So the DPP is clearly normalising something, and what remains is neither charge collection
(impossible for a linear filter) nor raw gain. **Do not re-label it either way -- it is an open
question, not a settled mechanism.**

### Pole-zero sweep: FWHM is NOT the diagnostic
No minimum at the matched value. 40000 ns -> 9.68, 50000 -> 9.30, **51550 (matched) -> 10.39**, 62000 ->
10.87 ch. All within 2 sigma of each other. The flat-top tilt test also stays inconclusive
(measured -0.42/-0.71/-0.33 +- 4.15 mV/us vs expected +3.72/0/-2.17) -- **no minimum at the match**.
Neither FWHM nor tilt resolves P-Z on this bench. **The P-Z effect that IS clearly visible is the mean
energy: 10586 -> 10351 channels monotonically over 40-62 us** (2.2 %). So P-Z mismatch shows up as an
energy-scale shift, not a resolution change.

### Confounds and caveats -- read before reusing these numbers
- **The rise sweep ran with PoleZeroT = 62000 ns**, left over from the P-Z sweep that preceded it, not
  the 50000 default. Internally consistent, but its absolute means are NOT comparable to the P-Z sweep
  or to the tuned point.
- **Tuned point (8000/51550): mean 10708, higher than the 10408-channel pulse height measured on the
  trace (§8).** Unexplained. Not ADC saturation -- full scale is 65535 counts (3994 mV) and we are at
  16 %. The rise-vs-mean curve plateaus around 10700 and the mechanism is **unestablished**; do not call
  it saturation.
- **`hist` quirk: a second `hist` in the same CLI session returns `total=0`.** One histogram per
  connect/disconnect cycle.
- These are ~7000-count histograms at 15-20 s. Any future claim under ~1 sigma (0.47 ch) needs more
  statistics, not a re-run.

### The 2x rise rule -- SUPERSEDED 2026-10-14; and my "conflict" rested on wrong physics

**There is no 2x cap either.** Recorded here as the user's rule on 2026-10-12, and superseded by the
user's 2026-10-14 correction (§11): **RiseTimeT can be anything -- it controls the trapezoid's own
"energy", a gain property of the filter, and is not related to the signal at all.** So the cap at
~2912 ns derived below should not be applied. What follows is kept because the *noise* trade-off is
still correct physics and the retraction history matters, but **any rise-time bound expressed relative
to the signal rise is dead** -- see §11 for the full retraction.

With the signal's measured 10-90 = 1456 ns that would have capped RiseTimeT at ~2912 ns. I wrote that
obeying it "costs 4.7 % of the pulse height" through incomplete charge collection -- **that argument
was voided on 2026-10-12** (see the retraction above; the trapezoid does not collect charge).

**Correct framing:** for a square input the trapezoid plateau is independent of rise time, so the rule
is about **noise and timing**, not amplitude. Rise time trades series noise (favours long) against
parallel and 1/f noise (favours short); capping it near the signal rise avoids admitting noise the
signal cannot use. **No conflict with anything measured here.** The residual 3 % mean-energy variation
is a filter-gain effect, not a penalty for obeying the rule.

~~**What this bench cannot answer:** the rule needs the **real detector's** rise time.~~ **Moot as of
2026-10-14** -- the rule it was asking about is superseded, so there is nothing left for the real
detector's rise time to resolve.

### Verdict on tuning this bench
**Criterion 1 (resolution) cannot be optimised here** -- the response across the usable parameter range
is flat within noise. What this bench DOES show is that the mean energy depends on rise time by ~3 %
and saturates by 8000 ns -- but that effect is **UNATTRIBUTED**, and it is NOT the "ballistic deficit"
I originally called it (see the retraction). **The filter is already at a good setting by default**; the
honest recommendation is to leave rise at 5000 ns and set P-Z from the real detector's decay constant,
which this bench cannot measure.

**BOARD STATE: restored to defaults and verified** -- `RiseTimeT=5000`, `PoleZeroT=50000`,
`FlatTopT=1000` (re-read after the sweeps). The "don't restore settings" instruction covers trace *probe*
assignments; filter parameters change physics, so they were put back.

Plot `/tmp/solaris_trapezoid_tuning.png`; raw histograms `/tmp/sweep_*_*.txt`, `/tmp/rep_*.txt`.

## 13. Aux/trapezoid_gui.py — offline trapezoid playground (2026-10-12)

`DAQ:Aux/trapezoid_gui.py`. **Offline -- drives no hardware.** PyQt6 + matplotlib.
Run `python3 DAQ:Aux/trapezoid_gui.py`. Controls: pulse A / r / d / offset, up to **3 piled pulses**
(pulse 1 + two extras, own amplitude and delay, sharing r/d), trapezoid RiseTimeT (L) / FlatTopT (M) /
PoleZeroT (tau_pz), 2 plots.
**This section describes the GUI as of 2026-10-12 and is SUPERSEDED on layout and defaults by §13d**
(rewrite to the user's 5-step plan): the **7 presets are DELETED**, the panel count went 3 -> 2, the
"position trim" control and the pedestal-leak subtraction are DELETED, and the defaults are trap
1000 / 5000 ns with A 800 / r 1000 / d 50 us / B 100 mV -- NOT the 5000/1000 and 2081.4 mV pedestal
quoted throughout §13/§13c as the "validated opening state". The FILTER MATHEMATICS below is unchanged
and still correct; only the GUI's shape and defaults moved.

### The filter, correct at last -- and what the pole-zero actually is

**The pole-zero is not an optional extra and not a baseline-restorer. It is stage 1 of the trapezoid,
and its job is to cancel the signal's decay.** From nukephysik101 (2020-03-20 and
2022-05-11 trapezoid-filter-revisit, paper doi:10.1016/0168-9002(94)91011-1):

    1. pole-zero   f[n] = f[n-1] + v[n] - c*v[n-1],   c = exp(-dt/tau_pz)
       For v[n] = A e^{-(n-r)/tau}, the difference v[n] - c*v[n-1] is ZERO everywhere
       except at the rise -> f is a FLAT STEP. The decay is CANCELLED, not shortened.
       tau_pz is meant to EQUAL the signal decay d.
    2. shaping     S[n] = S[n-1] + g[n]
       g[n] = f[n] - f[n-L-1] - f[n-L-M] + f[n-2L-M-1]
       = difference of two running sums (widths L+M and L) -> ramp L, flat M, ramp L.

Output divided by L lands on the input's own mV scale.

**VALIDATED against hardware, essentially exactly:** driven by the MEASURED ADCInput trace from
VX2745 ch0 (L=625, M=125, tau_pz=50 us) and compared with the board's own EnergyFilter output:
**correlation 1.00000, residual 0.05 % of peak, output scale k=1.001 against 1/L.**
Predicted peak 653.2 mV vs board 654.8 mV (0.2 %). Data: `DAQ:agent/data/real_trap.txt`.

### Three wrong models, all of them plausible -- recorded so none returns

| model | corr vs hardware | verdict |
|---|---|---|
| 3-stage Jordan FP (pole-zero, then sums over M then L) | **0.11** | WRONG. Leaves a NONZERO baseline. Its docstring claimed "verified against the reference loop (2e-8)" -- that verified the code against its OWN intended math, never against hardware. |
| pole-zero-free shaper (diff over L, sum over L+M) | **0.96**, resid 3.5 % | NEAR MISS and the dangerous one. Looked validated, was used to "confirm" a claim in this file, and is 30x worse than the truth. |
| **pole-zero then diff/sum (above)** | **1.00000**, resid 0.05 % | correct |

**RETRACTED: "the kernel is zero-DC so no pole-zero is needed."** I wrote that in this file, and it is
why I made PoleZeroT an optional stage **off by default** -- which is exactly backwards. The shaper IS
zero-DC, so it returns the baseline of a *step*; but the real signal is an exponential, and without the
pole-zero cancelling the decay the trapezoid is **asymmetric** (the blog's words: "when the parameter
does not match the decay time, the trapezoid becomes asymmetric. This is also called pole-zero").
**Directly answering the question "why is it weird that with pole-zero = decay the baseline is not
restored": it was my math.** I had `a = exp(-dt/tau_pz)` in a plain high-pass
(`lfilter([1,-1],[1,-a])`), which is NOT the blog's recursion. The correct stage is
`f[n] = f[n-1] + v[n] - c*v[n-1]`. With it, tau_pz = d gives baseline **+0.0 mV**; mismatched gives
+33.6 mV (tau_pz short) or -20.0 mV (tau_pz long) -- and the sign flips with the mismatch direction,
as it must.

**Also retracted: the "flat top 1040 ns vs 1064 ns, rise 4016 vs 4064" agreement quoted for the
0.96 model** was real but coincidental at bench settings -- it hid a 3.5 % residual.

### The signal is SAMPLED, not continuous -- and now there are sampling controls
The pipeline is exactly what a digitiser does: **evaluate the analytic pulse at sample
times -> quantise to integer codes -> run the filter on those codes -> plot, joining the
samples with straight lines.** Nothing is plotted as a continuous curve; the input panel
title says so explicitly, and samples can be shown as dots.

New controls (first group in the panel):
- **sample period, ns/sample** (0.1 .. 1000, default 8 = 125 MS/s, the VX2745 tick2ns).
  Not cosmetic: L = RiseTimeT/dt and M = FlatTopT/dt, and c = exp(-dt/tau_pz), so changing
  dt re-derives the whole filter. Verified: at dt = 4/8/16/40/100 ns the *physical* flat top
  stays ~1056 ns while L/M rescale 1250/625/312/125/50 -- the filter follows the time axis
  correctly. At dt = 1000 ns with RiseTimeT = 5000 ns, L collapses to 5 samples and the
  shaping is visibly coarse, which is the point of exposing it.
- **record length, samples** (16 .. 200000, default 4000). Pre-trigger is a SAMPLE count
  (like ChPreTriggers), so it scales with dt. The truncation warning still fires if the
  record cannot hold 2L+M.
- **ADC resolution (bits) and input range (V full scale)** -- the digitise step.
  16-bit over 4 V gives 0.0610 mV/LSB, matching the board's measured `adctovolts`
  (0.000060944 V/count) to 3 s.f. Default is 16-bit / 4 V, i.e. the VX2745.
  Note what quantisation of a DC level actually produces: a **fixed bias, not scatter**
  (offset 137 mV lands on 137.024 mV at 16-bit, 140.625 mV at 8-bit). A flat baseline
  quantises to one code, so scatter stays 0 -- quantisation noise needs a *moving* signal.
  8-bit also shifts the trapezoid peak (130.4 vs 129.7 mV).

**Caveat on the validation:** `agent/data/real_trap.txt` is the board's own quantised samples,
so the corr-1.00000 check validates the FILTER only, not the sampling/quantisation chain
(which is exact by construction -- the analytic pulse is evaluated at sample times).

### The TAIL is the pole-zero diagnostic -- the stage overlay was removed (2026-10-12)

User's point: checking the trapezoid's tail already tells you whether the pole-zero is right, so
plotting stage 1 separately adds nothing. **Verified, and it is the better metric.** Sweeping tau_pz
through d = 51.3 us:

| tau_pz | tail (mV) | asym (ns) |
|---|---|---|
| 45.0 | +5.52 | +176 |
| 50.0 | +1.03 | +96 |
| **51.3 (= d)** | **0.00** | +80 |
| 53.0 | -1.27 | +56 |
| 60.0 | -5.72 | **0** |

**Both are monotonic in tau_pz. The difference is where each crosses zero.** The **tail crosses exactly
at tau_pz = d (51.30 us, error +0.00)**; **asymmetry crosses at 60 us, 8.7 us HIGH.** So asymmetry is
not usable as a match indicator -- and note this CORRECTS what I wrote above under "vertical lines",
where I called asymmetry "non-monotonic" based on two points. Wrong diagnosis: it is monotonic, just
**misaligned**. Do not tune tau_pz by minimising rise-vs-fall asymmetry.

The GUI now shows the tail as an actionable readout plus a dashed level line:
`POLE-ZERO CHECK: tail = +15.10 mV  tau_pz too SHORT -- raise it toward d` (sign tells the direction).
The "show pole-zero output" overlay and its `show_deriv` state were deleted.

**`f.ptp()` remains a useless flatness metric** -- f is a step, so its ptp is the step height, not a
slope. That is how an earlier test called a 33 mV droop "flat".

### Pre-trigger is now a control -- and it exposed a real failure mode
Pre-trigger samples (0 .. 100000, default 250 like `ChPreTriggers`) is a window control, not a filter
parameter -- it does not change L or M (verified: L/M unchanged across pre = 0 .. 10000).

**But it can be set so the pulse is not in the record at all, and every readout then reports confident
nonsense.** Found by testing: with N = 4000 and pre = 10000 the tool printed "trapezoid peak 0.0 mV at
**-80000 ns**" and a "31992 ns flat top" -- both measured on the flat pre-trigger baseline. Now guarded:
- **NOTHING TO MEASURE** when the pulse starts beyond the record end.
- **RECORD TRUNCATED** when the shaped pulse (2L+M samples) does not fit after the trigger.

### The analog signal before digitisation is now drawn
The input panel shows the **continuous** pulse (8x finer grid than the samples, joined smoothly) under
the digitised samples, so quantisation error is visible as dots sitting off the curve. Without it there
is nothing to be off -- sampled and analog look identical.

Measured (digitised minus analog, pre-trigger region only): at offset 0 the error is exactly 0 at every
bit depth, because 0 is an exact code. At **8-bit with offset 137 mV the error is +3.625 mV** -- 137
rounds to code 9 = 140.625 mV. Consistent with the LSB/2 bound (7.8 mV). Two earlier attempts at this
measurement were wrong: one sliced 200 analog points against 4000 samples (a time-axis mismatch, giving
a spurious 0.000 everywhere), the other tested offset 0 and concluded quantisation was invisible.
**A DC level quantises to a bias, not to scatter** -- see the note above.

### What the two vertical lines and the stage overlay mean (labels added 2026-10-12)
Both vertical lines were drawn WITHOUT legend labels, so they read as decoration. Now labelled:
- **green dotted at t=0 = the trigger instant.** Everything left of it is the pre-trigger window
  (250 samples, like `ChPreTriggers`). It is NOT the peak and not the pulse start.
- **red dashed + triangle = where the trapezoid peaks**, i.e. where the DPP would sample the energy.
  Sits inside the green flat-top band when things are well set -- that alignment is criterion 3.
- **green shaded band = the flat top, from the line-fit corners t1..t2** (was a >97 %-of-peak window;
  removed 2026-10-13 at the user's insistence -- twice. A threshold measure is not a corner).
  Labelled "flat top t1..t2 (N ns vs M P)" so the deviation from the programmed M is visible.
~~"show pole-zero output (stage 1)"~~ REMOVED as redundant -- see the tail diagnostic above (was mislabelled "show shaper stages" -- it shows ONE
  stage, not stages) overlays `f`, peak-matched to the trapezoid. `f` is what the pole-zero does to
  the input: a **flat step** means the decay was cancelled; a sloping `f` means tau_pz != d.

**Caveat found while checking this: the asymmetry signature is weaker and less clean than the blog
implies.** Trapezoid rise vs fall, tau_pz swept against d = 51.3 us:

| tau_pz | rise | fall | asymmetry |
|---|---|---|---|
| 51.3 (= d) | 4064 | 4144 | 80 ns |
| 50.0 | 4064 | 4160 | 96 ns |
| 40.0 | 4080 | 4368 | **288 ns** |
| 62.0 | 4056 | 4040 | **16 ns** |

Non-monotonic: tau_pz = 62 is LESS asymmetric than the 50 ns default. So asymmetry is a usable
signature for a SHORT tau_pz but not a general match indicator -- do not tune tau_pz by minimising
rise-vs-fall asymmetry and expect it to find d.
**Also: `f.ptp()` is a useless flatness metric** -- f is a step, so its peak-to-peak is the step
height, not a slope. My first test of this called a 33 mV droop "flat" for exactly that reason.

### "The tail is exponential, which is clearly wrong -- check the formula" (2026-10-12, user report)

Investigated. **The formula is correct; the exponential tail was real and was MY DEFAULT's fault.**

User's observation was genuine: at the bench default (PoleZeroT 50 us, signal d = 51.3 us) the
trapezoid does have a visible exponential tail. But that is the mismatch doing its job, not a bug.
Long-record test (160 us, L=125/M=625), tail sampled well after the trapezoid has returned:

| tau_pz | tail @+20 us | @+40 | @+80 | @+140 |
|---|---|---|---|---|
| **51.3 us (= d)** | **0.000** | **0.000** | **0.000** | **-0.000** |
| 50.0 us | 1.365 | 0.924 | 0.424 | 0.132 |
| 40.0 us | 14.831 | 10.043 | 4.605 | 1.430 |

**Matched tau_pz gives an exactly flat zero tail. Mismatched gives an exponential.** And the decay
constant of that exponential is the **SIGNAL's d, never tau_pz** -- swept over 6 combinations:

| d | tau_pz | fitted tail tau | = d? | = tau_pz? |
|---|---|---|---|---|
| 30.0 us | 21.0 / 36.0 | 29.99 / 30.05 | YES | no |
| 51.3 us | 35.9 / 61.6 | 51.29 / 51.35 | YES | no |
| 80.0 us | 56.0 / 96.0 | 79.96 / 80.13 | YES | no |

Which is what the algebra says: the pole-zero cancels the decay only when its tau matches; the
residual left over is proportional to the original exponential, so it must fall with d.

**Hardware agrees.** On the real record, parsed per §13e: model tail **+2.07 mV**, board tail
**+2.87 mV**, both FLAT (ptp 13.5 / 27.3 mV of noise). corr 1.00000, resid 0.053 %, peak 654.3 vs
board 654.8 mV. The board's own PoleZeroT is 50 us against d = 51.3 us, so the board has the same
1.3 us mismatch -- and a small positive tail -- exactly as the model predicts.

**Fix:** PoleZeroT now defaults to d and a **"PoleZeroT = d (match)"** checkbox slaves it (changing d
moves it; unchecking frees it). Presets pin tau_pz explicitly, so `_preset()` releases the match
first -- otherwise the auto-match re-slaves tP and a deliberate-mismatch preset never reaches the
filter (found and fixed; presets 5/6 now correctly show +33.6 / -20.0 mV). The "bench" preset keeps
the board's real 50 us and therefore still honestly reports the 1.3 us mismatch.

### Ad-hoc check scripts lied three times -- so ONE canonical check was written

While chasing the above, throwaway scripts produced **mutually contradictory** results: one reported
a model tail of **-58 mV** (flat, contradicting the +2.07 mV from another), another printed
`corr=-9.000 lag=0 k=0.0000` because a `continue` rejected every lag and the sentinel won, and a
third showed a shape mismatch from `n` not being recomputed. None was a real finding. The -58 mV
one nearly convinced me the formula was broken.

So the parse, the column identities, the lag search and the tail window were consolidated into **one
saved script** with assertions (`assert a.shape[1] == 7`, `assert cc > 0`, NaN/inf checks, printed
column names). It also documented the trap: `Analog0` = ADCInput, `Analog1` = EnergyFilter, and
swapping them is the easiest way to "discover" a broken filter.
**The parse regex and every constant are in §13e below -- read THAT instead of rewriting the parse
by hand.**

**Meta-lesson for this file: a diagnostic script that cannot fail loudly is worse than no script.
Any check whose failure mode is a plausible-looking number should assert, not print.**

### The user's trapezoid OPTIMISATION criteria, tested (2026-10-12)

Stated as: **L doesn't really matter (just the slope shape); M must be long enough that the top is
flat but not so long it blocks the next pulse; PoleZeroT = signal decay.** All three check out.

1. **L does not set the peak** -- verified, tau_pz matched, M fixed: peak varies only **0.64 %**
   across L = 1000..16000 ns (683.8 -> 679.4 mV at M=5000). The ~3 % rise-time effect in §12 is the
   board's gain, not this. L sets the slope, as the user says.
2. **M sets the usable flat top, and "long enough" means long enough to outlast a settling TIME**
   (law: see "The usable flat top is `M - C(L, r)`" below -- cite by TITLE, sections move). At L=1000, r=818 ns a nominal M=5000 ns yields only ~770 ns truly flat
   -- so **M=5000 does NOT give a flat 5000 ns top on this signal.**
3. **Blocking cost is the total width** -- new readout `filter OCCUPANCY 2L+M` (default 7000 ns),
   the longest a next pulse can be blocked. This is the quantity §11's failing criterion 5
   (PileupGuardT 1528 ns vs trapezoid width) should be compared against.
4. **PoleZeroT = d** -- now the default; see "The tail is exponential, which is clearly wrong" above.

**SECOND RETRACTION on the rounded top (same section, one day).** First I blamed "filter rise close
to signal rise" -- wrong (3000/3000 is 3.7x and still rounded). I then wrote "it is the SIGNAL's r,
not the filter's L" -- **also incomplete**: measured, a LONGER filter rise makes the top FLATTER
(M=5000, r=818: L=1000 -> 752 ns, L=5000 -> 1800, L=8000 -> 2184 ns of true flat). A longer L
averages more of the pole-zero's exponential creep. Correct statement: the top rounds because the
pole-zero output approaches its step exponentially over ~r. My "grows with M" was ALSO wrong -- see
"The usable flat top is `M - C(L, r)`" below (cite by TITLE, sections move). That section carries
the THIRD retraction -- including my "shortfall grows with M" claim implied here.

### GUI defaults changed to rise 1000 ns / flat 5000 ns (2026-10-12, user request)

`self.trap_rise_ns = 1000.0`, `self.trap_flat_ns = 5000.0`. This is the **inverse ratio** of the
board's ch0 factory settings (5000 / 1000, §9), which remain one click away as the **"bench"
preset**. Total width L+M is 6000 ns either way, so the DC pedestal leak is unchanged (0.1202).

**Two consequences flagged in the GUI rather than silently accepted:**
1. **FlatTopT 5000 ns is above the VX2745's max of 3000 ns** (device tree: RiseTimeT 32..13000 ns,
   FlatTopT 32..3000 ns, §13). The tool is a sandbox and does NOT clamp -- the user asked for any
   positive value -- but it now prints "NOT WRITABLE ON VX2745". Constants `hw_max_rise_ns` /
   `hw_max_flat_ns` on the Window. Fires on startup by design: showing an unwritable shape without
   saying so is how a sandbox quietly misleads.
2. **The flat top measures 2840 ns, not 5000 ns** -- law in "The usable flat top is `M - C(L, r)`," cite by TITLE (the section immediately below is partly superseded by it).

### A long flat top measures SHORTER than nominal -- rounded top, and my first explanation was wrong

> **PARTLY SUPERSEDED** by "The usable flat top is `M - C(L, r)`" below. The shortfall table here uses
> the `>=97% of peak` window, which is a FRACTION of the peak and so scales with M -- that is why this
> section says the shortfall "grows with M". The slope-based measure in the later section is correct:
> the deficit is a TIME and is constant in M. Read both; trust the later one.

Measured with tau_pz matched to d, >=97% width vs nominal M (ns):

| filter L/M | nominal M | r=10 | r=200 | r=818 | r=2000 | r=5000 |
|---|---|---|---|---|---|---|
| 1000/5000 | 5000 | **5040** | 4728 | 2872 | 1136 | 912 |
| 3000/3000 | 3000 | 3168 | 3048 | 1824 | 1192 | 1464 |
| 5000/1000 | 1000 | 1280 | 1240 | 1056 | 1384 | 1824 |

The shortfall **grows with BOTH the signal rise r and the nominal M**, and vanishes as r -> 0.
Mechanism: the pole-zero's output is not an instantaneous step -- it still rises over the signal's
own r -- so the trapezoid *creeps* up to its plateau instead of arriving flat, and a 97 % threshold
then catches only the middle of it.

**RETRACTED, caught by the data: "filter rise close to the signal rise lets the rise leak through."**
My first wording of the GUI note said exactly that. It is wrong: at **3000/3000 the filter rise is
3.7x the signal rise and the top is still rounded** (1808 ns of a nominal 3000). The control is the
*SIGNAL's* r, not the filter's L. Wording now matches the table.

**Also wrong, same pass: a units bug in my own message.** `dt` in `_recompute()` is **ns/sample**
(only the filter call converts to seconds), so `M * dt * 1e9` printed "5000000000000 ns". The note
now multiplies without the factor and carries a comment saying so.

**Dead control, recorded so it is not retried: feeding a PURE STEP to test "is it the filter?"** A
step has no decay to cancel, so the pole-zero ramps it and the plateau comes out 730/758 mV instead
of 690 -- it measures the pole-zero's response to DC (the leak above), not the flat top. Only a
pulse with r -> 0 and tau_pz matched is a valid control.

### User's tuning heuristics: "flat top = 7 x signal rise" and "L sets the peak, so tune L to fill
### the energy bits" (2026-10-12) -- one holds, one is refuted by the board

**Heuristic 1, M = 7r: reasonable DEFAULT, not a constant.** Re-measured 2026-10-13 with the
**line-fit corners** (flat = t2 - t1) after the user rejected the >=97 % threshold measure a second
time -- the earlier table on this point used that measure and is superseded. Flat width as a share of
nominal M, r = 818 ns:

| filter L | M=3r | 5r | **7r** | 10r |
|---|---|---|---|---|
| 1000 ns | 73 % | 81 % | **86 %** | 90 % |
| 2000 ns | 91 % | 92 % | **94 %** | 96 % |
| 5000 ns | 105 % | 100 % | **100 %** | 99 % |

The line-fit corners are much more forgiving than the threshold window said (63 % -> 86 % at 7r,
L=1000), because they measure the ramp's intersection rather than a slice of a rounded top. **L
matters more than the multiplier**: at L = 5000 ns even 3r delivers its full nominal flat top, so 7r
is a SAFE default rather than a requirement.
**Not covered by the 2026-10-14 retraction, and do not confuse the two:** that retraction removes bounds
on the FILTER RISE (L) relative to the signal. This heuristic sets the **FLAT TOP (M)** from the signal
rise, which is a different parameter -- M is about trigger jitter and guard timing, not filter gain.
**The GUI button is GONE:** `Aux/trapezoid_gui.py` had `_set_7r` / the "flat top = 7 x signal rise"
button removed in the 2026-10-13 rewrite (user's 5-step plan), verified absent 2026-10-14 (`grep -c
"_set_7r" -> 0`). Earlier text here cited it as live.

**Heuristic 2, "trapezoid rise time L sets the peak size, so tune L to fill the energy word":
REFUTED on this board -- the DPP normalises by L.** The user is right that the RAW filter gain scales
as L (verified on the model: raw peak 85470 -> 1698334 for L = 125 -> 2500, ratio 19.87 for 20x,
exactly linear). But the board divides it back out. Three independent lines:

1. **The captured record, in the histogram's own count units.** Model /L peak = **10736 counts**;
   board EnergyFilter peak = **10744 counts** -- agreement to 0.07 %. Without the /L the board would
   have had to output **409 V** on a 4 V range.
2. **The board's own rise sweep** (corrected table, P-Z = 50000). RiseTimeT 1600 -> 13000 ns is a
   6.5x change in raw gain, but the mean energy moves only **+12 %**:

| RiseTimeT | hw mean % of 5000 | model /L % of 5000 | diff | raw gain would say |
|---|---|---|---|---|
| 1600 | 88.55 % | 92.92 % | +4.4 | 32 % |
| 2400 | 93.32 % | 95.83 % | +2.5 | 48 % |
| 2900 | 95.32 % | 97.03 % | +1.7 | 58 % |
| 5000 | 100.00 % | 100.00 % | 0.0 | 100 % |

3. **The GUI already divides by L** and validates at corr 1.00000 -- so the /L convention was
   implicitly confirmed all along; this makes it explicit.

**What to tell the user instead:** to fill the energy word use **input range / FineGain / signal
amplitude**, NOT RiseTimeT. L is a noise-and-timing knob (and, per the M - C law, a flatness knob).

**This LARGELY RESOLVES the §12 open item "the ~3 % rise effect is UNATTRIBUTED" -- but not fully.**
The model's /L peak reproduces the hardware sweep to **+1.7 .. +4.4 percentage points**, so the
effect is the finite signal rise interacting with the shaper (the pole-zero output creeps with
r||d, and the shaper averages a still-creeping signal), NOT raw gain and NOT ballistic deficit.
Supporting evidence: with tau_pz matched, the spread of the /L peak over L = 1000..20000 ns is
**0.36 % at r = 10 ns, 8.98 % at r = 818 ns, 38.61 % at r = 2000 ns** -- the L-dependence is entirely
a function of the signal's finite rise. **The residual (up to 4.4 points, growing as L shortens) is
still unexplained -- do not mark the open item closed.**

**Energy word width -- the user assumed 12 bit; this board is NOT running 12 bit.**
`DAQ:docs/sol-file-format.md:83` gives `energy` = **2 bytes**, and `DAQ:broker/solaris-cli.cpp:781`
reads it as `uint16_t`. The captured line sits at mean **10455 counts**, which already exceeds 4095
(12 bit max), so **>= 14 bits are in use**. What the DPP's internal fill width actually is (14 or 16)
could NOT be established: no `EnergyBits`/`TraceBits`/`Gain` node resolves (all `ReadValue failed`),
and the only 16383 in the tree is `AbsoluteBaseline` -- a **PSD baseline max, not an energy width**,
so it must not be read as evidence for 14-bit energy. Left as an open question; the GUI exposes an
"energy word" bits control (2..16, default 16) so the user can model whichever it turns out to be.

GUI readout added: `energy peak ~ N counts of a K-bit word = P% of full scale`, with an OVERFLOWS
warning above 100 % and a "bits wasted" hint below 50 %. At bench defaults 11210 counts = 17 % of a
16-bit word (3 bits wasted), 68 % of a 14-bit word, 274 % of a 12-bit word.

### The usable flat top is `M - C(L, r)` -- a settling TIME, not a fraction (2026-10-12)

Mechanism, verified exactly: the pole-zero output `f` is NOT an instant step. It approaches its step
value exponentially with **tau_f = r || d** (fit over +3..+12 us, tau_pz matched):

| signal r | fitted tau_f | r\|\|d |
|---|---|---|
| 818 ns | 805.2 ns | 805.2 |
| 2000 ns | 1925.0 ns | 1925.0 |
| 5000 ns | 4555.9 ns | 4556.0 |

The shaper averages this creeping `f`, so the trapezoid top creeps too, amplitude ~ tau_f / L.
Measured true-flat width (walk out from the peak while slope < 0.02 mV/sample), r = 818 ns:

| filter L | M=5000 | M=10000 | M=20000 | M=30000 | M=40000 | **C = M - flat** |
|---|---|---|---|---|---|---|
| 1000 ns | 752 | 5752 | 15752 | 25752 | 35752 | **4248 ns** |
| 5000 ns | 1800 | 6792 | 16792 | 26792 | 36792 | **3208 ns** |
| 8000 ns | 2184 | 7184 | 17184 | 27184 | 37184 | **2816 ns** |

**C is EXACTLY CONSTANT in M** (4248, 4248, 4248 ... to the sample) and **shrinks as L grows**. At
r = 10 ns, C ~ 0 (4936 flat out of a nominal 5000). So

    usable flat top ~= M - C(L, r);   C falls with L, rises with r, -> 0 as r -> 0

**THIRD RETRACTION, and why it took three tries:** the GUI's `>=97% of peak` window is the WRONG
MEASURE for this claim. It is a *fraction of the peak*, so as M grows the window narrows and C
appears to grow with M -- which is exactly what I wrote first. The settling deficit is a **TIME**, so
it must be measured as a run of near-zero *slope*. The GUI note now computes the slope-based true-flat
run for the C it prints, and labels the 97 % figure separately. Verified after the change: C = 4232 ns
at L=1000 for BOTH M=5000 and M=10000 (constant), 3208 at L=5000, 2816 at L=8000.

**Consequence for the user's criterion:** "flat top long enough to be flat" is NOT satisfied by M
alone. With r = 818 ns and L = 1000 ns a nominal 5000 ns flat top delivers ~770 ns. Either raise M
above C (needs M ~ 9.2 us at L=1000, ~8.2 us at L=5000) or accept a rounded top. Since C falls only
~1/L while occupancy is 2L+M, that is a real trade and not a free lunch.

### The pedestal was mishandled at the record boundary -- FIXED (2026-10-12, user-reported)

User: *"when offset is there the trapezoid is wrong; I think you assume no-data = 0 -- a mis-handled
boundary. Or you need to patch no-data as same as the offset."* **Correct, and it was both halves.**

**Half 1 -- zero-padded history.** `delay()` (and `lfilter`'s initial state) fill the pre-record region
with ZEROS. That asserts "before this record there was no signal", which is true only for a 0 mV
baseline. For a pedestal it is a step of **-offset at sample 0**, which the shaper integrates into a
ramp. Measured pre-trigger excursion, default L/M/dt:

| offset (mV) | pre-trigger y, cold history | with warm history |
|---|---|---|
| 100 | 0.16 .. 40.80 (ptp 40.6) | 12.02 .. 12.02 |
| 500 | 0.80 .. 203.98 | 60.09 .. 60.09 |
| 2081.4 | 3.33 .. 849.14 | 250.15 .. 250.15 |

**Half 2 -- DC is not rejected by this filter.** A constant input does NOT come out at zero: the
pole-zero ramps it and the shaper leaves a **constant leak**. Closed form, verified exact to 2e-12
over 28 combinations of (L, M, tau_pz):

    ybase / V0 = (1 - e^(-dt/tau_pz)) * (L+1)(L+M)/L  ~=  (L+M)*dt / tau_pz
                                                    =  (RiseTimeT + FlatTopT) / PoleZeroT

i.e. **the DC leak fraction is the filter's total width divided by the pole-zero tau** -- the
pole-zero is only *partially* cancelled, and what survives is the ratio of two times. At bench
settings (5000 + 1000) / 50000 ns = **0.12** exactly, measured 0.120182. The small-angle form is
0.15 % off at dt/tau = 1.6e-4 and degrades as L shrinks (2.6 % at L=37, 17 % at L=5), so use the
exponential form. The board does the same thing -- on
`DAQ:agent/data/real_trap.txt` a **2081.4 mV ADCInput pedestal sits under a 263.8 mV EnergyFilter
baseline** (ratio 0.12673 vs the model's 0.12018). Same mechanism, **5 % apart** -- the model is not
reproducing the board's DC path exactly (a baseline restorer, or a pedestal that is not pure DC), but
the discrepancy is confined to DC: the pulse shape still matches at corr 1.00000. So the offset must be
*both* fed correctly *and* subtracted at the output -- which is precisely what the board's
`EnergyFilterMinusBaseline` option is for.

**Why it mattered for the readout:** the tail diagnostic added the previous turn was measuring
`y[0.8n:]`.mean() on the RAW curve, so the pedestal's leak went straight into it -- +1.39 mV became
+13.4 at 100 mV offset and +251.5 at 2081 mV, and the sign flipped the "raise/lower tau_pz" advice.
The diagnostic was right; the input to it was contaminated.

**Fix:** `trapezoid()` / `trapezoid_fast()` take `v0=` (the level that existed before the record).
They prepend a warm block of `2L+M+2` samples at `v0`, run the filter, strip the block, and return
`(f, y, ybase)` where `ybase` is the settled leak -- known **analytically**, not estimated from a few
pre-trigger samples. The GUI feeds `v0 = offset` and plots `y - ybase`; the raw curve is drawn as a
dotted grey trace when the leak exceeds 0.05 mV.

**Validated both ways, identical:** (A) pedestal subtracted from the input, cold history -- the
original route; (B) pedestal fed in with warm history + `ybase` removed -- the new route.
**Both: corr 1.00000, k = 1.0012, residual 0.053 % of peak, peak 654.3 mV.** Leaving the leak in
gives corr 1.00000 but **k = 0.52 and peak 904 mV** -- the shape is right, the scale is destroyed,
which is why correlation alone never caught this.
GUI now reports `peak`, `tail`, `flat top` and `asym` identically at offset 0, 50, 137, 500, 1000,
2081.4 and **-300 mV**, plus a `pre-trigger baseline flat to X mV OK/NOT FLAT` self-check.

### Three latent bugs the v0 work exposed -- in the REFERENCE loop, not the fast path
Adding `v0` made the two implementations disagree, and chasing it found all three. Each was harmless
before only because the validation fed **baseline-subtracted** data, i.e. zeros at the record start --
which is exactly the case that hides them.

| bug | effect | why it hid |
|---|---|---|
| guard `if i >= L+M+1` should be `i >= L+M` (off-by-one) | first 1375 samples wrong by up to 500 mV | only shows when v[0] != 0 |
| `f[0]` left at 0 instead of `v[0]` | single-sample impulse at record start | subtracted away by baseline removal |
| `n` not recomputed after prepending the warm block | `IndexError` (read 1377 past the end) | no caller used `v0` |

Also fixed in the same pass: with `tau_pz <= 0` (c = 0) the slow loop took the wrong branch.
Now `trapezoid` and `trapezoid_fast` agree to 1e-9 relative over **every** combination of
`v0 in {None, 500, 0, -250}` x `tau_pz on/off` x `(L,M) in {(625,125),(37,11)}`.

**Generalise: a self-consistency check between two implementations only tests the input region they
both were written for. Non-zero boundary values are the classic untested region -- when adding a
feature, test the boundary case explicitly, not just the default.**

### Why (flat-top start - trigger) != RiseTimeT -- and the >=97 % measure is gone for good
### (2026-10-13, user asked, and repeated the request)

Two separate things in one message; both were right to ask.

**1. The question is a real trap in the numbers, and the answer is a decomposition.**
At the GUI defaults: trigger->t1 = **1239 ns**, but RiseTimeT = 1000 ns. Not equal, for two
independent physical reasons that pull in OPPOSITE directions:

    trigger->t1  =  (t1 - t0)  -  (trigger - t0)
                  =  measured rise    minus   how far the trigger landed into the ramp

- **RiseTimeT is a ramp DURATION (t1 - t0), not a delay from the trigger.** The trigger fires on the
  time filter's zero crossing, which lands *mid-ramp* -- 663 ns after t0 at defaults. So it eats into
  the rise. This alone would make trigger->t1 SMALLER than the rise.
- **The measured rise is not always L either.** The shaper integrates the pole-zero output, which is
  still creeping up, so with L comparable to the signal rise the ramp stretches. At defaults:
  1902 ns measured against L = 1000 ns.

Net: 1902 - 663 = 1239 ns.

**RETRACTED the closed form I first put in the GUI text, "measured rise = L + r||d".** It looked
clean and it is WRONG. Measured excess over L (r = 818 ns): **+902 at L=1000, +335 at L=2000, +1 at
L=5000, -100 at L=8000, -197 at L=16000**. It is not a constant added on -- the ramp is L-dominated
once L >> r, and the excess even goes slightly negative there. Full grid of excess (ns) over L:

| signal r | r\|\|d | L=1000 | 2000 | 5000 | 8000 | 16000 |
|---|---|---|---|---|---|---|
| 10 ns | 10 | +7 | +6 | -4 | -22 | -92 |
| 200 | 199 | +37 | +4 | -13 | -28 | -96 |
| 818 | 805 | +902 | +335 | +1 | -100 | -197 |
| 2000 | 1925 | +2804 | +1964 | +488 | +65 | -375 |

So: **excess ~ 0 when r is small; grows with r; shrinks as L grows.** No closed form claimed. The GUI
now prints the measured rise and states which regime it is in, with a conditional wording -- it says
"= L, as expected when L >> r" instead of falsely claiming a departure at L=5000/r=818 where they
agree to 1 ns. (That conditional was itself a bug: the first version hard-coded the departure text.)

**A second bug caught while writing this: the same seconds-vs-ns confusion, again.** My first
decomposition script printed `L (RiseTimeT) = 0 ns` because it multiplied a seconds-valued `dt` by
nothing. Now `DT` is named in seconds and every print carries an explicit `*1e9`.

**2. "Don't need flat top >= 97 %" -- you said this twice; it is now removed, not just demoted.**
Previously I had kept it as a secondary figure. Removed everywhere it was still a *measure*:
- `flat_top_mask()` **deleted** (dead code once its only caller went).
- the green shaded band became `t1..t2` from the line-fit corners, labelled `flat top t1..t2 (4187 ns
  vs M 5000)` -- so the deviation from programmed M is visible instead of a threshold slice.
- the rounded-top note no longer prints its "(N ns within 3% of peak)" parenthetical.
- the 7x rule table above re-measured with line-fit corners (it had been computed with the 97 %
  window -- that table was superseded, and the new numbers are much more forgiving: 63 % -> 86 % at
  7r, L=1000).
- verified externally at defaults: `"97" in w.info.text()` is False. (No assert in the code -- if
  the measure ever comes back, nothing will fail loudly. That is a gap, not a guard.)

The remaining "97 %" strings in the GUI source are comments explaining WHY it was removed.
**Lesson: when the user rejects a measure, delete it -- do not keep it as a "secondary" readout.**
I demoted it once and had to be told again.

### Legend-growth bug in the time-filter panel (2026-10-13, user-reported, fixed)

User: *"there is a bug in the legend of the time filter plot -- if I check and uncheck the show
samples as dots, the legend keeps expanding."* Reproduced immediately.

**Cause:** `_recompute()` cleared only `ax_in` and `ax_tr`. The middle panel `ax_tf` was added when the
RC-CR^2 chain went in and was **never added to the clear list**, so each redraw appended a whole new
legend set. Toggling "show samples as dots" forces a redraw, which is why that checkbox looked like
the culprit -- it was just the easiest way to trigger the leak.

**Fix:** clear all three. Verified by counting `get_legend().get_texts()` across redraws:
startup 3/4/8 (input/timefilter/trapezoid) -> 10 dot-toggles 3/4/8 -> 21 redraws via presets x
checkboxes 3/4/8. Stable.

**Not a leak, checked because it looked like one:** the trapezoid panel goes 8 -> 9 when the pedestal
is non-zero. That is the conditional "raw output, incl. pedestal leak" trace gaining its legend entry;
it returns to 8 at offset 0. Counts also drop at spinbox extremes (fewer markers when no trigger fires
or the corners cannot be fitted) -- both are the conditional-label design working, not growth.

**Generalise: every added axis must be added to the `clear()` call, and a legend-entry count across
repeated redraws is a cheap regression test for it.** Matplotlib `cla()`/`clear()` is not automatic.

### GUI behaviour worth knowing
- **Ranges are deliberately uncapped** (RiseTimeT/FlatTopT 8 ns .. 1e8 ns, PoleZeroT 0.001 .. 1e5 us) --
  FlatTopT is NOT limited to the hardware's 3000 ns max; it is a sandbox. Verified tracking M over
  5 decades: 8 ns -> 928 ns, 1000 -> 1056, 3000 -> 2280, 50000 -> 48664 ns measured flat top.
- **Record length now follows L and M** (was fixed at 4000 samples = 32 us, which silently truncated
  any wide pulse -- the first "470440 ns flat top" readings at M=1e6 and M=1e8 were IDENTICAL, which is
  how the truncation was spotted). Above the 60000-sample cap it prints an explicit
  "RECORD TRUNCATED" warning rather than a confidently wrong number.
- **Presets are self-contained** (each pins d, L, M, tau_pz). They were previously incremental, so
  "P-Z mismatch" inherited d=400 us from the square preset and showed a 180 mV artifact instead of the
  intended demo.
- Worst-case recompute (L=M=1e8 ns, capped record) is 63 ms; 200000 samples is 30 ms. Interactive.
- `trapezoid_fast` (lfilter+cumsum) and the reference loop `trapezoid` agree to 1e-9 relative over all
  `v0` x `tau_pz` x `L/M` combinations (2026-10-12 -- this test found three boundary bugs, above).
- **A non-zero `offset` is now handled correctly** at the record boundary; peak/tail are pedestal-free
  via an analytically-known `ybase`. See the two sections above -- the pre-fix version ramped the
  pre-trigger window by up to 846 mV and poisoned the tail diagnostic.

**Lesson for the whole file: a model can be internally consistent, self-verified, and 30x wrong.
Validate against the instrument. And when something looks "weird", suspect your math first -- here it
was, twice.**

## 13b. PHA timing chain: RC-CR^2 2nd derivative, armed on threshold, TRIGGER on the
## return through zero (2026-10-13, user-specified, validated to +-1 sample)

User's instruction: stop guessing where the trapezoid starts -- model what the firmware does.
**Signal -> RC-CR^2 (2nd derivative) -> armed while it is above threshold -> TRIGGER when it
crosses back through zero -> peaking time and all trapezoid corners measured from that trigger.**

**Implemented in `DAQ:Aux/trapezoid_gui.py`** as `second_derivative()` + `find_trigger()` /
`find_all_triggers()`, a third plot panel, and tau RC / tau CR / threshold controls
(140 ns / 220 ns / 35 mV).

**VALIDATED against the board's own measured digital-probe markers (§10), on the real captured
ADCInput: armed = sample 172 vs board 172 (EXACT), trigger = 252 vs board 251 (+1 sample = 8 ns).**
GUI default pulse reproduces it: armed window 624 ns vs board 640 ns; trigger 824 ns after onset
vs board 848 ns.

### What the RC-CR^2 is FOR, and why the trapezoid cannot do this job
It is the fastest thing that is insensitive to BOTH amplitude and baseline: a step or pedestal gives
a transient that dies out, only a real RISE holds it above threshold. That is why the DPP uses one
filter for WHEN (TimeFilter, fast, ~296 ns) and another for HOW MUCH (EnergyFilter, slow, 5000 ns).
Verified: a pure step gives max |d2| = 151 mV transient that decays -- it does not sustain an arm.

### tau RC / tau CR are a CALIBRATION, not a firmware claim
`TimeFilterRiseTimeT` = 296 ns was read live from the board. It does NOT map cleanly onto the taus:
(148,148) -- the obvious "296 = 148+148" split -- misses the trigger by 19 samples, while
(140,220) hits it exactly. **The zero-crossing time is also strongly DEGENERATE in (tau_RC, tau_CR):**
six different pairs from (140,220) to (600,60) all put the crossing on sample 251. So the trigger
TIME is robust but the individual taus are not identifiable from it. Do not present these numbers as
the firmware's internal structure.

### The +608 ns validation lag IS trigger latency -- partly confirmed, NOT fully resolved
The hardware validation has fitted a +608 ns lag for days. Testing the hypothesis that the board
ENABLES the energy filter at the trigger: running the trapezoid from the trigger sample instead of
the record start **halves the lag to +288 ns**, and the model's ramp onset then lands 32 ns from the
board's and its peak 48 ns from the board's. **But shape fidelity gets 12x WORSE** (residual 0.642 %
vs 0.053 %, corr 0.99983 vs 1.00000). So the board's energy filter is NOT gated at the trigger -- it
runs continuously, and the lag is a time-REFERENCE offset, not an enable. Trigger-referencing aligns
the corners; continuous running matches the shape. The GUI keeps the continuous filter (validated
corr 1.00000) and uses the trigger purely as a marked time reference. **Why 288 ns remains is open.**

### Trigger-level pile-up limit -- a NEW diagnostic, and it agrees with the board's guard
With two equal pulses the second one **never arms** below a certain gap, because its 2nd-derivative
lobe rides on the first's tail and never reaches the threshold:

| pulse gap | max d2 after the first trigger | threshold 35 mV | result |
|---|---|---|---|
| 1200 ns | 29.9 mV | not reached | second pulse NEVER ARMS |
| 1600 ns | 33.6 mV | not reached | second pulse NEVER ARMS |
| 1800 ns | 35.5 mV | reached | second trigger fires |
| 2000 ns | 37.1 mV | reached | second trigger fires |

So this bench cannot resolve two pulses closer than **~1700 ns at the trigger level** -- strikingly
close to the board's measured `EnergyFilterPileupGuard` window of **1528 ns** (§10). The GUI now
prints the event count, so a pile-up that the trigger silently misses is visible.

### Corner measurement: line fits, not thresholds
`RiseTimeT`/`FlatTopT` are **ramp durations**, so the quantity wanted is where the ramp *would meet*
the flat top if both were extended -- the intersection of two fitted lines. A `>= 97 % of peak` window
instead reports a value that depends on how rounded the top is: it read **1072 ns on hardware
programmed to 1000 ns**, and shrank as M grew. Fit straight lines to the rise and fall between
**25 % and 80 %** of peak (clear of both the rounded corner and the baseline noise) and to the plateau
above **95 %**, then intersect. Thresholds choose the fit windows only; they never touch the reported
corner. Validated against the board's own corners: **rise 4833 vs 4832 ns, flat 1450 vs 1444, fall
4902 vs 4901** -- within ~6 ns. A shape with no usable ramps must return "not measurable", never a
number.

**A corner is referenced to the pulse ARRIVAL, not the trigger.** At defaults the true trigger sits
~824 ns after arrival, so labelling a trigger->corner interval "peaking time" overstates it by 824 ns.
State which of the two a corner-to-x number is measured from -- they are not interchangeable.

### `find_trigger` merged two pulses 8 us apart -- `argmax` over the rest of the record
The lobe search ran to the END OF THE RECORD, so a later, taller pulse's lobe was attributed to an
earlier trigger: **two pulses 8 us apart silently became one event.** Bound any lobe search to the
current lobe (stop at the first downward zero crossing). The real-record result was unchanged (172,
252) -- which is exactly why this survived: it only fires when a second pulse is TALLER.

### Boundary bug caught again, third time in this file
`second_derivative()` initially had no warm history: with offset 500 mV the pedestal looked like a
step at the record start and **fired a false trigger 1632 ns BEFORE the pulse arrived**. Fixed with
the same `v0=` warm-start as `trapezoid()`. This is the same class as the §13 pedestal bug -- any
IIR filter given a record with non-zero first samples needs its history warmed or it invents an edge.

## 13c. LIVE SIX-SIGNAL CAPTURE vs the GUI -- the +608 ns lag was REAL PIPELINE
## LATENCY, and the user was right that the GUI trapezoid must be delayed (2026-10-13)

> **POSITION: superseded** by "POSITION RULE" below. The 612 ns figure here is a fitted
> *shape-alignment* constant for one capture. The GUI no longer uses it to place the curve:
> the flat top is now placed at **trigger + RiseTimeT** exactly, by the user's stated rule.
> Do not re-apply 612 ns as a position. The shape measurements in this section stand.

Captured ch0 with all six signals in one trace: Analog0 ADCInput, Analog1
EnergyFilter, D0 Trigger, D1 TimeFilterArmed, D2 EnergyFilterPeaking,
D3 EnergyFilterPileupGuard. Data: `DAQ:agent/data/trace_all_1013_1230.txt`
(4000 x 8 ns, board at RiseTimeT 5000 / FlatTopT 1000 / PoleZeroT 50000).
The parse used for this comparison is in §13e. Do not re-derive it.

**User asserted: "the start of trap is at trigger, so the trap in the GUI should be delayed."**
**Right about the delay. Half right about the mechanism -- and the half matters.**

### The measured answer: a pure 612 ns DELAY, not a gate
Fitted by scanning a sub-sample shift of the model against the board's EnergyFilter. Residual,
as a share of the board's own peak:

| delay | 0 ns | 600 | 608 | **612** | 616 | 624 |
|---|---|---|---|---|---|---|
| residual | 6.82 % | 0.137 % | 0.054 % | **0.034 %** | 0.059 % | 0.143 % |

**76.5 clock cycles at 125 MS/s.** Sharp minimum, and 0 ns is 200x worse, so the latency is real
and not a fitting artefact. Sub-sample matters: rounding to 76 or 77 samples costs a factor 1.6.

### Why "gated at the trigger" is the WRONG mechanism, even though it also halves the lag
Enabling the shaper at the trigger sample was tested directly: lag drops 608 -> 288 ns, but the
**shape gets 12x WORSE (residual 0.644 % vs 0.054 %)**. Two independent proofs the board's energy
filter runs CONTINUOUSLY:

1. **It moves 200 ns BEFORE the trigger.** First sustained departure at sample 226; the trigger is
   251; EF is already at 4.39 mV and rising when D0 fires. A gated filter would be exactly 0 there.
2. **The pre-onset region is dead flat (std 0.0000 counts) while ADCInput carries 0.42 mV of noise**
   -- which is CONSISTENT with a running filter, not against it. A running sum over L+M = 750
   samples attenuates noise by ~sqrt(750) = 27, so 0.42 mV / 27 = 0.016 mV lands inside ONE 16-bit
   bin. My first reading of that flatness as evidence of gating was wrong.

A delay leaves the shape identical and moves all four corners together -- exactly what the board
does. So: **the trapezoid is delayed past the trigger, not started by it.**

### ALL SIX signals now match the model
Times relative to each side's own trigger:

| quantity | board | model | err |
|---|---|---|---|
| armed rises | sample 172 | 172 | **0** |
| trigger | 251 | 252 | +1 (8 ns) |
| armed window | 640 ns | 640 ns | 0 |
| t0 trapezoid start | trigger+488 | +484 | -3 ns |
| t1 flat top | +5320 | +5316 | -4 |
| t2 fall | +6770 | +6759 | -11 |
| t3 end | +11671 | +11660 | -12 |
| rise / flat / fall | 4832 / 1450 / 4902 | 4832 / 1442 / 4901 | 0 / -7 / -1 |
| shape | -- | -- | **corr 1.00000, resid 0.034 %, k=1.0011, lag 0 ns** |

**Open item 5c CLOSED** (see the supersede note there): the "why does 288 ns remain" question was
an artefact of testing the wrong mechanism.

### Two board markers are NOT trapezoid corners -- do not confuse them
- **EnergyFilterPeaking (D2) fires 200 ns AFTER t1**, the line-fit flat-top start. It is not "start
  of flat top". §10 said it "coincides with the flat-top onset" -- close (200 ns on an 18.3 us
  trapezoid) but not equal, and it is 16 ns before PileupGuard rather than a near-duplicate of it.
- **EnergyFilterPileupGuard (D3) opens 216 ns after t1** and holds 1528 ns.

### Defaults set to the capture's settings, so the GUI opens IN the validated state (2026-10-13)

User asked the GUI defaults to match the tested configuration. Now, at startup:
RiseTimeT **5000** / FlatTopT **1000** / PoleZeroT **50 us**, pulse A 690 / r 818 / d 51.3,
pedestal **2081.4 mV**, dt 8 ns, N 4000, pre-trigger 250, 16 bit over 4 V, energy path delay
**612 ns**. All thirteen verified equal to the values read back from the board before capture.
The "bench" preset is now identical to the defaults (its pedestal reset changed 0 -> 2081.4).

**PoleZeroT default is 50.0 us, NOT the matched 51.3, and `chkPzMatch` now starts OFF.** Deliberate:
the point of these defaults is to reproduce the board, and the board is configured 1.3 us short.
So the tail diagnostic honestly reports **+1.41 mV / "tau_pz too SHORT"** at startup -- that is the
board's real behaviour, not a GUI defect. Checking the box moves it to 51.3 and the tail to 0.00.

The previous defaults (1000/5000) were the inverse ratio; still reachable from the controls, and
still the best demo of a rounded top. Side effect: the "NOT WRITABLE ON VX2745" flag and the
"TOP IS ROUNDED" note are both now correctly ABSENT at startup (5000/1000 are inside the board max,
and L=5000 >> r=818).

### Latent bug the non-zero pedestal default exposed: `ybase` subtracted TWICE

Making the pedestal non-zero by default dropped the readout from 10702 to **6604 counts** (10 % of
full scale instead of 16 %). `_dynrange()` computed `np.max(y) - ybase`, but the caller had already
passed `y = y_raw - ybase`. **Completely invisible at offset 0**, which is what every previous test
used. Fixed to `np.max(y)`; now 10702 / 10703 / 10703 counts at pedestals 2081.4 / 0 / 500 mV
(board's captured EnergyFilter peak: 10744 counts, so 0.4 % low).

**Same lesson as the §13 boundary bugs, one class wider: a default of 0 was hiding an arithmetic
error in a dependent readout.** Changing a default to a realistic non-zero value is itself a test --
do it deliberately, not only as an afterthought.

### GUI change
New control **"energy path delay" (ns, default 612)**, applied as a sub-sample `np.interp` shift to
the trapezoid AND to the pedestal-leak raw trace (plotting an unshifted raw beside a shifted one
would show a 612 ns step that is not physically there). With it the GUI reproduces the board's
corner timings to <=12 ns. The shape check (§13e) applied it too and reported **lag +0 ns,
residual 0.033 %** instead of absorbing the latency into a fitted lag.

**Consequence for the trigger->t1 decomposition in §13:** with the delay applied the trigger lands
BEFORE t0, not after it -- 512 ns before at the current 5000/1000 defaults, 51 ns before at the
former 1000/5000 ones (it was 663 ns AFTER before the delay existed). The wording is now sign-aware;
a hardcoded "AFTER" printed the nonsense "-512 ns AFTER t0" the first time. Numbers quoted here are
setting-dependent -- re-derive them with the §13e recipe rather than trusting the numbers here.

### POSITION RULE (user, 2026-10-13, stated as fact): the flat top starts RiseTimeT
### after the trigger -- the GUI now ENFORCES this, and the 612 ns latency is superseded

User, stated plainly and not to be re-litigated: **"the flat top start is trap rise time
after the trigger. This is the truth. don't need to verify."** Accepted as the firmware's
definition of where the trapezoid sits.

Implemented as a construction, not a measurement: `_recompute()` fits the model's current
flat-top start, then shifts the whole curve sub-sample so that it lands on `trigger + L`.
**The shape is untouched -- only its position on the time axis.** The readout now says
`PEAKING TIME trigger->t1 = 5000 ns (rule: = RiseTimeT 5000 ns, set by the rule)` and is
exact by construction at every setting tested (1000/5000 -> 998, 5000/1000 -> 5000,
2000/2000 -> 2000 ns; the 2 ns is the sub-sample fit of a line intersection).

**What this supersedes:** the `energy path delay` control, which defaulted to 612 ns of
empirically fitted latency (§13c). Renamed **"position trim"**, default **0**, range now
+-100000 ns. The 612 ns number is not deleted from this file -- it was a real measurement
of one capture -- but as a POSITION rule it is now replaced by the exact statement
`t1 = trigger + L`. Note the two disagree by a few hundred ns: the fitted latency put t1
at trigger+5311 ns, the rule says 5000 ns. **The rule wins**, per the user.

**This also retires my §13 explanation of why trigger->t1 "!= RiseTimeT".** That whole
decomposition (measured rise vs L, trigger landing mid-ramp, the sign-aware wording) was
describing an artefact of where MY model happened to put the curve. Under the rule they
are equal by definition. The underlying measurements in §13 are still valid data; the
interpretation that they implied a mismatch was the wrong frame.

**Unchanged and unaffected:** the shape check (§13e) reported **corr 1.00000, lag 0 ns,
residual 0.033 %** -- a shift cannot change shape, and that script did its own alignment,
so the shape validation never depended on the GUI's positioning. It used its own
`LATENCY_NS = 612` internally (in §13e); a fit constant there, not the
GUI's position rule. **If the two are ever reconciled, do not assume they agree.**

### Two bugs found while implementing the rule

1. **I made the pre-trigger self-check lie.** `pre_ptp` was measured on the SHIFTED curve;
   the rule moves the pulse relative to the pre-trigger window, so preset 3 reported
   "baseline flat to 7.79 mV NOT FLAT" -- a false alarm about boundary handling, caused by
   geometry. Fixed by measuring `pre_ptp` BEFORE the shift, which is what the check was ever
   for (catching pedestal/zero-padding mishandling). All 7 presets now pass.
2. **Preset 4 reports "no usable trapezoid" and that is PRE-EXISTING, not from the shift.**
   Verified by rebuilding preset 4's curve with no shift at all: the corner fit returns
   "not measurable" because a second pulse at +9500 ns genuinely defeats a single-trapezoid
   line fit. Correct behaviour -- it says so instead of inventing corners.

**And a third time in this session: my own test script divided ns by seconds** (`L` came out
625 million and numpy tried to allocate 10 TiB). `DT_NS` / `DT_S` named explicitly now. The
GUI itself is safe; this is about scratch scripts, which keep failing the same way.

### RECONCILIATION: 0.053 % and 0.033 % are the SAME fit at two different latencies
Three places quote `resid 0.053 %` (twice in §13, once in §13b) and several quote `0.033 %`/`0.034 %`,
which reads as a contradiction. It is not one: §13c's scan table gives residual as a function of the applied delay --
**608 ns -> 0.054 %, 612 ns -> 0.034 %** (minimum). The 0.053 % quotes are the pre-612-ns fits and the
0.033/0.034 % quotes are post-612-ns. Both are correct for their latency; the earlier sections simply
predate the latency fit and do not state which value they used. **When quoting a residual here, always
say which delay it was fitted at** -- the difference is a factor 1.6 and looks like drift otherwise.

### Removed from the trapezoid panel (user request, 2026-10-13)
The **trapezoid-peak vertical line and its triangle marker**, and the **green t1..t2
flat-top band**. Verified: 0 patches, 0 triangle markers. The corner lines (flat top starts
/ ends) are kept -- they are what shows the rule being satisfied. The peak line was redundant
anyway: with the flat top placed by the rule, the peak sits at trigger + L + M/2.

### Re-captured and plotted 2026-10-13 (second capture) -- everything reproduces

`agent/plot_capture.py` renders a capture as a 6-lane timing diagram (ADCInput, EnergyFilter, and the
four digital markers as separate lanes, with the digital HIGH windows shaded behind the analog
lanes). Image: `DAQ:agent/data/plot_six_1013_1308.png`. Data: `DAQ:agent/data/trace_six_1013_1308.txt`.

**This was a NEW capture, so it is a genuine reproducibility test, not a re-run of the same file.**
Markers identical to the 12:30 capture: armed 172->252 (640 ns), trigger 251, peaking 941, pile-up
guard 943->1134 (1528 ns). Against the model: **armed err 0, trigger +2 samples, corners within
19 ns, corr 1.00000, residual 0.035 %, lag 0 ns.**

**Trigger error moved from +1 sample (first capture) to +2 (this one)** while the armed edge stayed
exactly at 172 both times. So the +/-1-2 sample trigger agreement is RUN-TO-RUN JITTER on a ~43 mV
threshold crossing, not a systematic offset -- do not quote "trigger matches to 8 ns" as if it were
exact; ~8-16 ns is the reproducibility floor here.

Naming: the user asked for "peaking holdoff" and clarified **pile-up guard**. There is no "holdoff"
source on this board -- the 13 `WaveDigitalProbe` sources are Trigger, TimeFilterArmed,
ReTriggerGuard, EnergyFilterBaselineFreeze, EnergyFilterPeaking, EnergyFilterPeakReady,
EnergyFilterPileUpGuard, EventPileUp, ADCSaturation, ADCSaturationProtection, PostSaturationEvent,
EnergyFilterSaturation, AcquisitionInhibit (`DAQ:core/DigiParameters.h:514-565`). "ReTriggerGuard" is
the closest thing to a holdoff and is a DIFFERENT signal from the pile-up guard; §10 measured it at
8 ns starting exactly at the trigger.

## 13d. REWRITTEN to the user's 5-step plan -- the delay is DERIVED, not fitted
## and not fitted (2026-10-13)

`DAQ:Aux/trapezoid_gui.py` was rewritten from the user's explicit plan (turn 65). The old file is
backed up at `DAQ:agent/data/trapezoid_gui.spec0.py` (and `.prev.py`). The pipeline is now fixed and
linear, and this ORDER is the whole point:

    1. signal A(1-e^-t/r)e^-t/d + B  ->  2. digitise  ->  3. RC-CR^2 gives ARMED and TRIGGER
    ->  4. SHIFT the digitised signal by (TRIGGER - ARMED), THEN apply the trapezoid
    ->  5. label trigger / flat-top start+stop / trap end / peaking

**Step 4 is the change that matters, and it RETIRES the §13c position rule.** The delay is no longer
fitted to a capture (612 ns) and no longer imposed as a rule (`t1 = trigger + L` by shifting the
*output*). It is derived from the time filter as `trigger - armed`, applied to the *input*.

**The physics block is byte-identical to the validated one** (`diff` clean over lines 84-383).
The shape check (§13e) reported **corr 1.00000, lag 0 ns, resid 0.033 %** and the marker comparison
reproduced the board (**armed err 0 samples, trigger +1, corners within 12 ns**). Both were re-run
after the rewrite. **Those checks no longer exist as code -- §13e holds the recipe and is the only copy;
rebuild from it rather than re-deriving the parse.**

### What the derived delay actually gives, at the spec defaults
A 800 mV, r 1000 ns, d 50 us, B 100 mV, trap L 1000 / M 5000 ns, tau RC 140 / CR 220 ns, thr 35 mV:

    ARMED sample 276  (arrival +208 ns)      TRIGGER sample 358  (arrival +864 ns)
    SHIFT = trigger - armed = 82 samp = 656 ns
    t0 = trigger -208 ns   t1 = +792   t2 = +5792   t3 = +6792

**So `trigger -> t1` is 792 ns, NOT the 1000 ns the rule demanded.** This is not a bug and I did not
"fix" it: the shift moves the *pulse* to the trigger by the armed-window amount, but ARMED itself sits
208 ns AFTER the arrival (the 2nd derivative needs that long to climb through the threshold), so the
trap ramp lands 208 ns before the trigger and t1 lands L - 208 = 792 ns after it. The readout states
the gap and its cause inline rather than hiding it. **The two mechanisms cannot both be right** --
the user's (trigger - armed) derivation and the earlier "t1 = trigger + L" rule differ by exactly that
208 ns -- and the derivation is what the user specified last, so it is what the code does.
The 656 ns shift is close to the independently measured 640 ns armed window and to the 612 ns
fitted latency; all three are the same physical pipeline delay seen three ways.

### Consequence worth knowing: the time filter is AMPLITUDE-SENSITIVE at fixed threshold
With 3 pulses (800 / 600 / 400 mV at 0 / 9000 / 16000 ns) the d2 lobe peaks are 42.6 / 32.0 / 21.2 mV
-- linear in amplitude -- so at threshold 35 mV **only the first pulse arms**. `find_all_triggers`
reporting 1 event is correct, not a missed trigger. This is the real DPP behaviour (fixed threshold,
so small pulses are timing-invisible); do not "fix" it by lowering the threshold in code.

### `DAQ:agent/plot_capture.py` is now ONE axes -- both analogs + all four digital overlaid (2026-10-13)
User: "can combine all plot into one." Done: single set of axes, single time base. Legitimate because
both traces are baseline-subtracted so they share a zero line and mV scale, and their peaks are close
(634 vs 654 mV) -- directly comparable, which is the useful case. Digital band occupies the top
(peak*1.32 .. peak*2.12 of a ylim to peak*2.2); both plateaus verified below the band base.
**Trigger is now BLACK, not red** -- red is the EnergyFilter curve and they would be confusable in one
axes; the trigger is the reference instant every other number is measured from, so neutral reads right.
Verified positions: armed HIGH 1.376..2.016 us, guard HIGH 7.544..**9.072** us (= 7.544 + 1528 ns; an
earlier note here said 8.768 us, which was MY arithmetic error, not a rendering one), vlines 2.008 /
7.528 us. Marker table unchanged.

**matplotlib trap found, cost three wrong diagnoses:** `set_yticks()` EXPANDS the axes view to include
every tick it is handed. My tick filter dropped ticks above the band but kept the locator's first tick
(-200 mV), which sat BELOW the intended ylo (-20.5) -- so set_yticks silently widened the bottom and
burned ~15 % of the panel height. `set_ylim` before it is not enough. Correct order: set_ylim ->
set_yticks filtered on BOTH bounds -> **re-assert set_ylim** -> set_autoscaley_on(False). Also:
annotate() and legend() each fire autoscale_view, so ylim must come after every artist exists.
Gotcha from the previous step still stands: `set_title(..., loc='left')` leaves `get_title()` returning
'' -- use `get_title(loc='left')`.

### `DAQ:agent/plot_capture.py` restructured -- 6 rows -> 2, digital as colour overlay (2026-10-13)
User: remove the four digital lanes, overlay the waveforms on the two analog plots. Done: each analog
panel now carries all four markers stacked in a band across its TOP (y-limits raised to peak*2.2, band
occupies peak*1.32..peak*2.12, so the trapezoid plateau is never covered -- verified no overlap).
Positions verified in data coordinates on the rendered figure: armed HIGH spans 1.376..2.016 us
(expected 1.376 -> 1.984 + one 8 ns step), guard 7.544..8.768 us, vlines at 2.008 (trigger) and 7.528
(peaking) us -- all exact. Marker table unchanged.
**Two of the four are NOT drawn as fills and must not be "fixed":** Trigger and EnergyFilterPeaking are
8 ns wide = 1.5 px on a 7.7 us window, so a filled step is invisible; they are vertical lines at the
same scale, labelled as such in the legend. The two wide ones (armed 640 ns, guard 1528 ns) are true
`fill_between(step='post')` waveforms.
The old 6-row PNG at `DAQ:agent/data/plot_six_1013_1308.png` is overwritten by the new layout.
Gotcha for future edits: `ax.set_title(..., loc="left")` populates `_left_title`, so `ax.get_title()`
(with no loc) returns '' -- that produced a false "titles are missing" reading here. Use
`get_title(loc='left')`.

### The GUI is a DEMONSTRATION tool, not a comparison harness (user, 2026-10-13)
"can delete corr. the trapezoid_gui.py is not for compare, it is for demonstration." Removed from the
file: the corr 1.00000 / residual 0.033 % claims in the module and `trapezoid()` docstrings, the
board-vs-model corner table from the corner-fitting helper, and the corr 0.11 / 0.96 dead-end scores (kept as a
pointer to sec.11, which still lists the two wrong models and their numbers). Nothing was ever
*displayed* -- all four were docstring prose -- but the framing mattered: a demo tool should not carry
a validation score, and corr specifically is a misleading metric to quote alone (it is blind to scale
AND offset: a curve 1000x too big still scores 1.0000; that is why the deleted validator printed k and
residual alongside it). **All the evidence remains here in sec.11/13/13e -- only the GUI's copy of the
scores is gone.** Do not add comparison metrics back into the GUI; if the filter formula needs
re-verifying, rebuild the check from sec.13e as a separate script.

### The 2nd derivative's pre-trigger was NOT flat -- a 4th-instance boundary bug, fixed ANALYTICALLY
User asked "why the 2nd derivative t < 0 is not flat". It was not: ptp over the pre-trigger window was
**2.1378 mV** on a 100 mV pedestal, decaying toward zero with ratio 0.9741 per sample.

**Root cause: `second_derivative` warmed its history with a finite block of `6*max(tau)` samples, and
for an IIR filter no finite warm-up block can be exact.** Contrast the trapezoid, whose memory is a
finite 2L+M+1 taps -- there a warm block of that length IS exact (verified: ptp 0.000e+00 at three
L/M pairs). The RC-CR^2 stages have INFINITE memory, so the block only approximates the steady state.
The residual is the filter's own step response sampled at k=W, which is why it decays with the true
single-pole tau_CR (measured tau from the residual: 220.0 ns, exactly tau_CR).

Measured residual vs warm-block length (100 mV pedestal): 6 tau -> 2.14e-2 of pedestal; 11 tau ->
3.8e-4; 18 tau -> 5.0e-7; 36 tau -> 1.3e-14. So 6 tau was off by ~2 % of the pedestal -- and
**at the board's real 2081.4 mV pedestal that is 35.6 mV against a 35 mV threshold: a FALSE TRIGGER.**
The bug was invisible at the new 100 mV default and lethal at the old one. Lengthening the block was
not an option: at dt = 0.1 ns with tau_CR = 100 us, a 40*tau block is 4e7 samples = 320 MB.

**Fix is analytic, not longer.** Seed the RC with its exact steady state at v0 (`zi = a*v0` -- zi is
the filter's internal STATE, so a*v0, not v0; on a constant pedestal it then outputs v0 to the last
bit), then feed the two CR stages the DEVIATION from v0 with zero state. The CRs are DC-blocking, so
"deviation was zero forever" filters to exactly zero. Pre-trigger is now **0.000e+00, all samples
bit-identical to zero** -- not approximately flat. Same trick as `trapezoid(..., v0=)` but exact where
that one is merely finite.

Verified no collateral damage: armed/trigger at the GUI defaults unchanged (276/358);
the marker comparison gave **armed err 0, trigger +1, corners within 12 ns** and the shape check
**corr 1.00000, resid 0.033 %** (§13e). Old-vs-new difference was 2.14 mV at sample 0, falling below 1 uV past
sample 400, i.e. ONLY the first ~1300 ns changed -- the pre-trigger window, which is what was wrong.
Edge cases rc=cr=1 ns and rc=cr=1e6 (b underflow) stay finite and flat. `v0=None` still shows a large
startup transient, correctly: that means the signal really stepped up from 0.

### `twinx()` must be created ONCE, not per recompute -- the "old 2nd derivative does not clean up" bug
User report: changing the pre-trigger length left stale 2nd-derivative curves on screen. Reproduced
exactly: **axis count and line count grew by +1 and +3 on EVERY recompute, unbounded** (3 -> 8 axes over
five changes). Cause was mine, in the rewrite: `ax2 = ax.twinx()` sat inside `_recompute`, and each
`twinx()` call creates a NEW overlay axes object. `ax_in.clear()` does **not** clear a twin -- it is a
separate object in `fig.axes` -- so every previous d2 curve stayed drawn on its own orphan axes.
Fix: create the twin once in `__init__` as `self.ax_tf`, and clear all three axes in `_recompute`.
Verified: 3 axes / 16 lines stable across 30 recomputes, dt changes and pile-up toggles; legend label
sets stay exactly 5 / 1 / 7 with no growth; sharex survives the repeated clears (both panels read
-5199.2 .. 33191.2 ns, and the twin would otherwise have autoscaled to its own data).
**Same class of bug the old 3-panel file warned about in a comment** ("ALL THREE panels must be cleared.
Forgetting the middle one made its legend grow by one full set") -- the rewrite dropped that comment
along with the panel, then reintroduced the bug in a worse form. When panels become overlays, the
clear-list grows with them.
Note: a 15th line at dt = 2 ns is NOT a leak -- t3 = 8168 ns genuinely falls outside the 8000 ns
record, so its vline is correctly skipped.

### X range: bounded by the SAMPLE data, +-10 % -- and the bug that motivated it (2026-10-13)
The user rejected the time axis outright ("the time range is terrible"). Cause was a bug I introduced
in the rewrite: the fine analog grid was built as `(np.arange(n*SUB) - pre) * dt_s` instead of
`(np.arange(n*SUB) / SUB - pre) * dt_s`, so the "continuous signal" curve ran to 32000 samples of dt
= **256 us** and dragged the autoscaled xlim out to -14800 .. 266792 ns. The old file had it right;
the rewrite broke it. Fixed, and verified by interpolating the analog curve onto the sample times:
**max difference 0.0000 mV** against the digitised values (1 LSB = 0.0610 mV) -- exact alignment.

Both panels now share an explicit `set_xlim`: sample extent `ms[0] .. ms[-1]` plus 10 % of that span
on each side, so **-5199 .. 33191 ns** at the defaults (4000 samples x 8 ns, pre 250). Not autoscaled:
the analog curve and the long trapezoid tail would otherwise decide the window. Record length and
pre-trigger remain the user's only control over how much baseline is visible, which is the honest
behaviour. Y is left autoscaled -- it reads 63..861 mV and -27..839 mV, i.e. the pedestal no longer
inflates the vertical scale.

### Removed (all user requests; do not reintroduce)
Tail-level measurement and the pole-zero diagnostic line; pedestal-leak correction (`ybase`
subtraction); "position trim" control; preset `QComboBox`; trapezoid peak vline + triangle; green
t1..t2 flat-top band; the >= 97 %-of-peak flat-top measure; `_dynrange`; the NOT-WRITABLE-on-board
and ROUNDED-TOP warnings; the corner-readout call in `_recompute` (the helper itself was kept -- it is
the only implementation of the line-fit corner measurement that produced the §13 numbers, and §13e's
recipe expects it if that check is ever rebuilt). Panel count 3 -> 2: the time-filter curve is now
overlaid on the input-pulse panel on a twin axis.

**A kept helper, and why it stays.** `Aux/trapezoid_gui.py` still defines a corner-fitting helper that
nothing in the live GUI calls (grep finds only its own `def`). **That is deliberate and it is not dead
code to clean up:** it is the last implementation of a measurement method whose numbers are cited in
§13/§13b. But note how it got here -- it was kept because an external checker imported it, and that
checker no longer exists, so the original reason is gone. **Justify keeping a helper by what it
measures, never by who calls it; a caller can be deleted and the reason silently dies with it.**

### Still present and correct
`v0 = B` warm history on BOTH filters is how "patch t < first sample = baseline" is honoured -- the
signal is assumed to exist from t = -infinity while the digitiser sees a finite window. Without it the
pedestal reads as a step and fires a false trigger (measured once: 1632 ns early). `second_derivative`
sizes its own warm block from its taus, NOT from L/M -- it must not inherit the trapezoid's span.

## 13e. THE CAPTURE-PARSE RECIPE -- the only copy. Do not re-derive the parse by hand.

**This is a bench fact, not a history note:** the VX2745 trace transcript has no parser in the repo. The
format is a six-command CLI transcript, not a table, and getting it wrong is the single biggest failure
mode of this project -- three ad-hoc parses produced three mutually contradictory answers, one of them
inventing a **-58 mV tail that does not exist**. So the regex, the constants and the probe map are
recorded verbatim below. Rebuild from THIS if a check is ever needed; do not re-derive it from a sample.

Every number in §13/§13b/§13c was produced by a script using this recipe. **Those scripts are gone and
were never committed, so no number below can be re-verified without rebuilding from this recipe.**

```python
# Data: agent/data/real_trap.txt -- `solaris-cli trace 0 0 4200`, VX2745 ch0,
#   WaveAnalogProbe0=ADCInput, WaveAnalogProbe1=EnergyFilter.
#   Board at capture: RiseTimeS=625, FlatTopS=125, PoleZeroS=6250.
DT       = 8e-9        # s/sample, 125 MS/s
ADCTOMV  = 0.060944    # mV/count, the board's measured adctovolts
L, M     = 625, 125    # RiseTimeS, FlatTopS read back from the board
TAU_PZ   = 6250 * DT   # PoleZeroS = 6250 samples = 50 us
LATENCY_NS = 612.0     # shape-fit constant, NOT a position -- see 13d. 0 ns -> 6.82 % residual,
                       # 612 -> 0.034 % (minimum). Gating instead of delaying: 12x worse shape.
# Six-command CLI transcript, NOT a table. Data lines have exactly 7 integer columns:
#   Sample Analog0 Analog1 D0 D1 D2 D3
# Analog0 = ADCInput, Analog1 = EnergyFilter. Swapping those two is how people "discover"
# the filter is broken. Digital columns: D0 Trigger, D1 TimeFilterArmed,
#   D2 EnergyFilterPeaking, D3 EnergyFilterPileupGuard.
pat = re.compile(r"^\s*(\d+)\s+(-?\d+)\s+(-?\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s*$")
#   -> skip to the line starting "Sample" (assert it names Analog0 and Analog1), match every
#      later line, keep the matches, then: assert a.shape[1] == 7 and len(a) == 4000.
#      adc_mv = col1 * ADCTOMV ; ef_mv = col2 * ADCTOMV
```

Time-filter calibration used against that record: **tau_RC 140 ns, tau_CR 220 ns, threshold 35 mV**
(now the GUI's own defaults, so nothing is orphaned). Last recorded output of the check that used this
recipe, and nothing can reproduce it now: `corr=1.00000 lag=+0 ns k=1.0012 residual 0.033 %`;
board markers Trigger 251->252, Armed 172->252 (640 ns), Peaking 941, PileupGuard 943->1134; model
armed err **+0 samples**, trigger err **+1**, corners within **12 ns**.

**Consequence of gitignoring `agent/data/`:** the captures that validate the filter -- `real_trap.txt`,
`trace_all_1013_1230.txt`, `trace_six_1013_1308.txt` -- are now untracked. They survive on this disk
(a reboot does not wipe them; that is why they were moved out of /tmp), but a fresh clone cannot
re-verify corr 1.00000. `agent/plot_capture.py` is kept and still reads `agent/data/` by default.

## 13f. TRAP: FlatTopT writes return **OK** but the board CLAMPS to 3000 ns (2026-10-14)
User asked to set ch0 RiseTimeT 1000 ns / FlatTopT 5000 ns and capture. **RiseTimeT took; FlatTopT did
not, and the CLI said OK anyway:**

    write 0 /ch/0/par/EnergyFilterRiseTimeT 1000   -> OK   read back 1000  (= RiseTimeS 125 x 8 ns) OK
    write 0 /ch/0/par/EnergyFilterFlatTopT  5000   -> OK   read back 3000  (= FlatTopS   375 x 8 ns) CLAMPED

**This is the silent-success hazard from §5 extended:** §5 established that writing a nonexistent
*channel range* returns success. Now: writing an **out-of-range value to a real node also returns
success** and the firmware stores its own maximum instead. `OK` from the CLI means "the write was
accepted", NEVER "the value is set". **Read back after every write -- no exceptions.** The device-tree
`/max` and `/min` sub-nodes are NOT readable through this firmware (`ReadValue failed for
/ch/0/par/EnergyFilterFlatTopT/max`), so the limit cannot be queried -- it can only be discovered by
writing and reading back. The documented FlatTopT 32..3000 ns (§13) is confirmed still correct.

### The clamp is confirmed in the WAVEFORM, not just the read-back
Three independent measurements of the captured record agree on M = 3000 ns, so the read-back is not the
thing that lies -- the filter really is running 3000 ns:

| evidence | value | if M=3000 | if M=5000 |
|---|---|---|---|
| EnergyFilter pedestal (DC leak `V0*(L+M)/PZ`, V0=2081.4 mV, PZ=50 us) | **168.2 mV** | 166.5 ✓ | 249.8 ✗ |
| Peaking marker after trigger | **+2528 ns** | L+M/2 = 2500 ✓ | 3500 ✗ |
| FlatTopS read-back | 375 samples | 375 ✓ | 625 ✗ |

The DC-leak prediction lands within 1.7 mV of measured -- independent confirmation from a formula that
knows nothing about markers.

### Capture at RiseTimeT 1000 / FlatTopT 3000 (clamped), ch0, 4000 x 8 ns
`DAQ:agent/data/trace_L1000_F3000_1014_1201.txt`, plotted by
`python3 DAQ:agent/plot_capture.py` -> `plot_L1000_F3000_1014_1201.png`.
Markers rel. trigger: Armed -624..+8 (632 ns), Trigger +0..+8, Peaking +2528..+2536, PileupGuard
+2544..+5064 (2520 ns). EF peak 674.3 mV, 10-90 rise ~1840 ns against L=1000 programmed (consistent
with the 4064 ns measured at L=5000 -- the ramp is linear, so the 10-90 fraction scales).
PileupGuard width grew 1528 -> 2520 ns and now covers the whole flat top + fall, so the §14 item-1
"guard too short" defect is IMPROVED at these settings, not worse.
`/ch/0/par/PileUpGuardT` is NOT readable (note the capital U in the board's own name; §9 records it as
`PileUpGuardT` 960 ns) -- it appears to have moved or renamed; do not assume 960 ns is still there.

**What the board is set to right now is the session file's live table, not repeated here (§Scope).**
The durable point: the board is NOT at factory defaults and nothing has been restored, so never infer a
setting from "it must be the default". And a restore is only done once you have READ BACK (§13f).

## 13g. Emulator rise 1.0 -> 0.5 us, recaptured: RETRACTS the "FAST + cable add fixed rise"
## model in sec.2 (2026-10-14)

Programmed `set_pulse(rate_hz=1000, amplitude_v=1.0, decay_us=50.0, rise_us=0.5, baseline_v=0.0, ch=0)`
-- identical to the sec.6 reference EXCEPT rise_us. Config space is write-only (sec.5), so the previous
state was confirmed by FITTING the existing capture first: A=691.5 mV, r=819.0 ns, d=51.22 us,
resid 1.06 mV -- reproduces the documented A=690.5 / r=818.0 / d=51.31 / resid 0.98. That fit is the
only way to know the emulator's prior state, and it validated the method before using it.

Whole-pulse 4-parameter fit of the new capture (`DAQ:agent/data/trace_rise05_1014_1211.txt`):

| | rise_us=1.0 | rise_us=0.5 |
|---|---|---|
| r observed | **819.0 ns** | **483.7 ns** |
| A (asymptote) | 691.5 mV | 671.0 mV |
| **ADCInput peak** | **634.3 mV** | **634.1 mV** |
| d | 51.22 us | 50.36 us |
| fit residual | 1.06 mV | 0.57 mV |
| EnergyFilter peak | 674.3 mV | 670.9 mV |
| EF pedestal | 168.2 mV | 167.7 mV |
| armed window | 632 ns | **600 ns** |
| peaking / guard | +2528 / 2520 ns | +2528 / 2520 ns (unchanged) |

**Amplitude did NOT change** (634.3 -> 634.1 mV). A dropped 20.5 mV but the peak factor
`(d/(d+r))(r/(d+r))^(r/d)` rose 0.9210 -> 0.9472, cancelling exactly. Consistent with the emulator
holding energy_reg=27431 and amp_predicted=1.0000 V: the peak is what the DPP measures, and it is
untouched by rise -- an independent confirmation of the §14 item-4 conclusion by a different route.

### RETRACTED: "the FAST stage and cable add rise, so the observed edge is not the emulator's pole
### alone" (sec.2, 2026-10-12) -- falsified by the second data point
That claim inferred a fixed extra pole from ONE measurement (tau_r=455 ns programmed vs r=818 observed).
Two points now over-determine it and it fails both candidate forms:
* **Adding in quadrature:** X = sqrt(r_obs^2 - tau_r^2) = 681 ns at rise 1.0 but 427 ns at rise 0.5.
* **Adding linearly:** X = r_obs - tau_r = 364 ns vs 256 ns.
A fixed extra pole must give a CONSTANT X. It does not. Tested properly by cascading the emulator pole
with one fixed pole and fitting tau_f to the rise=1.0 point: best tau_f=350 ns reproduces r=836 ns
(2 % error) but then PREDICTS 597.6 ns at rise 0.5 against 483.7 observed -- **23 % off**, and no tau_f
fits both (150 ns: 24 %/19 %; 250 ns: 11 %/2 %; 450 ns: 15 %/45 %).
(My first quadrature test here was itself invalid -- it treated 10-90 rise times as adding in
quadrature, which is not how cascaded single poles combine. The cascade simulation above is the
correct test.)
**Status: the observed r tracks the emulator's rise_us but NOT as emulator-pole + fixed-pole. Mechanism
UNRESOLVED -- do not re-derive it from one point again.** Practical upshot stands: rise_us=0.5 gives
r~484 ns at the digitizer, rise_us=1.0 gives r~819 ns.

### Third point: emulator rise 0.2 us -- the single-pole pulse model BREAKS DOWN here, and the
### fixed-extra-pole question is closed (2026-10-13)
`set_pulse(rate_hz=1000, amplitude_v=1.0, decay_us=50.0, rise_us=0.2, baseline_v=0.0, ch=0)`.
Capture `DAQ:agent/data/trace_rise02_1014_1217.txt` (plot `plot_rise02_1014_1217.png`).

| emulator rise_us | 1.0 | 0.5 | **0.2** |
|---|---|---|---|
| **10-90 % rise, model-free** | 1463 ns | 919 ns | **491 ns** |
| 20-80 % rise, model-free | 954 ns | 595 ns | 297 ns |
| fitted r (single-pole) | 819.1 +-0.4 | 483.8 +-0.2 | 238.2 +-0.4 |
| **ADCInput peak** | 634.3 mV | 634.1 mV | **631.6 mV** |
| EnergyFilter peak | 674.3 | 670.9 | 656.7 mV |
| armed window | 632 ns | 600 ns | **552 ns** |
| single-pole fit residual | 1.06 mV | 0.57 mV | **1.99 mV** |
| shape RAM `ram_rise` | 40 pt | (2-region) | **18 pt @ 10 ns** |

**THE PULSE IS NOT A SINGLE POLE AT 200 ns.** The 4-parameter `A(1-e^-t/r)e^-t/d` fit still reports a
tight r (+-0.4 ns) but its residual is now STRUCTURED, not noise: over the rising edge it runs
-6.3, -6.8, -6.5, +7.6, +19.0, +23.2, +19.1, +16.1, +11.2 ... -- a 23 mV hump at 10.8 mV RMS, against
1.64 mV RMS over the decay region and a 0.061 mV ADC LSB. That is model error of ~350 LSBs on the
edge, not measurement noise. **The +-0.4 ns error bar on r does NOT mean r is well determined -- it
only says the single free parameter that best matches a wrong shape is 238 ns. Do not quote it as the
rise time without the residual.** This is the same class of error as the §3 "tail-only fit" trap: the
number looks precise and the model is wrong.
Practical consequence for the GUI: its signal model IS `A(1-e^-t/r)e^-t/d`, so below ~0.5 us emulator
rise it will mis-shape the input in ways the GUI cannot show. At rise_us >= 0.5 the residuals
(1.06, 0.57 mV) are consistent with a single pole.

**The fixed-extra-pole model is now dead on THREE points, not two** (see the retraction above): best
tau_f = 175 ns gives max error **21 %** across the three (646.6 / 416.5 / 273.8 predicted vs
819.1 / 483.8 / 238.2 observed); tau_f = 200 ns gives 25 %. No single fixed pole fits all three.
Also note the 10-90 the single-pole model PREDICTS from its own fitted r (2.197 x r = 1799 / 1063 / 523
ns) EXCEEDS what is measured (1463 / 919 / 491) at every rise -- so even where the fit residual is
small, the edge is already faster-than-single-pole at the top. The shape is multi-pole throughout;
r=819 ns was never "the emulator pole plus a cable".
**Status: mechanism UNRESOLVED, and now bounded by three data points. Do not re-propose a fixed extra
pole, and do not re-derive the edge from one capture.**

`d` fitted 51.16 / 50.33 / 50.20 us -- declining, but r and d are strongly anti-correlated in this fit
and the model is wrong on the edge at 0.2 us, so treat the d trend as FIT COVARIANCE, not a measured
change in the emulator decay. The emulator was programmed d=50.0 us (compensated to 46.14) in all three.

### Also measured: the time filter's armed window SHRINKS with faster rise
632 -> 600 ns for the same threshold, because the 2nd derivative climbs through it sooner and the lobe
ends sooner. So `trigger - armed` (the GUI's derived shift, sec.13d) is NOT a constant -- it depends on
signal rise time. At rise 0.5 us the derived shift would be 600 ns, not the 632 ns the rise-1.0 capture
gives. Worth remembering before treating the shift as board-invariant.

## 13h. Emulator ch2 -> digitizer ch52, established by a CAUSAL control (2026-10-14)
Programmed the emulator's second channel identically to the first (`set_pulse(rate_hz=1000,
amplitude_v=1.0, decay_us=50.0, rise_us=0.2, baseline_v=0.0, ch=1)`; ch=1 IS the front-panel CH2) and
asked which digitizer channel receives it. **Answer: ch52.**

**Method -- the user's hint was right, no DAQ needed.** `/ch/N/par/selftrgrate` is live outside an
acquisition (it is what §8's retraction established: `chtriggercnt`/`chrealtimemonitor`/
`chsavedeventcnt` all read 0 with no run and MUST NOT be used as evidence of absence). Full 64-channel
scan: BEFORE, only ch0 = 1000 Hz, all 63 others 0. AFTER programming ch2, ch0 = 1000 and **ch52 = 1000**.

**A rate appearing is NOT attribution** (§8's own lesson), so the control was causal, not correlational:
changing **only** ch2's rate 1000 -> 1700 Hz made ch52 read **1700** while ch0 stayed **1000**. Restored
to 1000 Hz after. That is the proof. (An amp=0 control was attempted first and correctly REFUSED by
`auto_gain` -- the 0.30 V floor guard, §EMU known-limits -- which is the guard doing its job.)

### ch52 is NOT configured like ch0 -- do not assume the ch0 probe set
Only the rows that are still true and still the point are tabulated; **mutable state (filter settings,
record length, and ch52's Analog1 -- written TimeFilter -> EnergyFilter in §13j) is NOT here**, because
a table that contradicts the session file's live table is worse than no table. What is durable is the
**asymmetry of the probe names**:
| | ch0 | ch52 |
|---|---|---|
| WaveDigitalProbe2 (D2) | EnergyFilterPeaking | **EnergyFilterPeakReady** |
| WaveDigitalProbe3 (D3) | EnergyFilterPileupGuard | **EventPileUp** |
| dcoffset | 50.001 % | 50.001 % |
| triggerthr | 50 counts = 3.05 mV | same |

At §13h time ch52's WaveAnalogProbe1 was TimeFilter while ch0's was EnergyFilter; that row was removed
on 2026-10-14 once §13j changed it. `WaveAnalogProbe0` is ADCInput on both -- and on every channel
and slot (§15).

`solaris-cli trace` sets only ChEnable / WaveSaving / WaveTriggerSource -- **never the probes**
(`DAQ:broker/solaris-cli.cpp:815-832`), which are per channel. So a ch52 capture silently carries a
2nd-derivative lobe in the Analog1 column while a plot labels it "the trapezoid". That is the §13e
column-identity trap one step worse, because the wrong label is PRINTED on the figure. Confirmed with
`read 0 /ch/52/par/WaveAnalogProbe1` before believing anything.

### Pulse reached ch52 correctly, and the two channels agree
ch52: ADCInput peak 620.7 mV, 10-90 rise **492 ns** (ch0 measured 491 ns -- same emulator rise, so the
rise is reproduced through the second output stage), single-pole fit r=238.9 ns, d=50.51 us, resid
2.02 mV (the same structured-edge failure as §13g). Pulse height 620.7 mV vs ch0's 631.6 mV -- **1.7 %
lower** at identical energy_reg 27431 and gain 1184, because the offset registers differ per channel
(ch0 -52451, ch1 +58371) and each channel has its own v_per_count (3.234e-5 vs 3.401e-5). Per-channel
amplitude calibration, not a fault. Both channels at the same rate are PHASE-LOCKED (`EMU:AGENTS.md:105`),
consistent with onsets 153 (ch0) and 158 (ch52).

### BUG in EMU:pulser.py `set_pulse` return value: the `polarity` label is INVERTED for ch=1
It reported `polarity: negative` for ch1 while the captured pulse is measurably POSITIVE-going
(max +620.7 mV above baseline, min -0.5). Cause: `pulser.py:644` does `"positive" if invert else
"negative"`, but `invert_for_ch` returns the channel's NATIVE bit, and `CH_OFFSET[1]['invert']=0` is
documented at `pulser.py:234-237` as ch2's *positive-going* setting -- the bit means the opposite on
that channel. **The hardware behaved correctly; only the string lied.** Do not trust `result['polarity']`
for ch1; check the capture. Not fixed in the emulator repo (that repo forbids commits without approval,
and it is not this session's file to change).

### `DAQ:agent/plot_capture.py` hardened for non-ch0 captures
It used to assume the ch0 probe set: `--probe=A0,A1` and `--ch=N` were added, absent markers are
skipped (analog still plots fine), and the title no longer says "ch0" unconditionally -- it falls back
to a filename `chNN` token, else `ch?`. Verified: ch52 titles "VX2745 ch52 capture" and labels its
second trace "TimeFilter" (not "the trapezoid"); ch0 still titles from its filename and keeps
"= the trapezoid".
**CORRECTED in 13i:** the `IndexError` quoted here was raised by a THROWAWAY analysis script, not by
this tool -- the tool already skipped the absent marker correctly. And ch52's D3 is not "Guard
configured to nothing": it is `EventPileUp`, a different signal that simply produced 0 HIGH samples in
that record. `--dprobe=` was added in 13i to name D0..D3 per channel.

## 13i. ch52's digital probes are NOT the ch0 set -- and how to verify a write without trusting
## read-back (2026-10-14)
Read back from the board: `/ch/52/par/WaveDigitalProbe2` = **EnergyFilterPeakReady** (ch0 slot:
EnergyFilterPeaking) and `WaveDigitalProbe3` = **EventPileUp** (ch0: EnergyFilterPileupGuard).
Different signals, same column positions, similar firing times -- so a ch52 figure rendered with ch0's
labels **looked plausible while being wrong**. `DAQ:agent/plot_capture.py` now takes
`--dprobe=D0,D1,D2,D3` alongside `--probe=` and `--ch=`; colour is keyed to the COLUMN, so a renamed
probe still lands in the same lane of the overlay band.

**RULE -- same shape as §13e for the analog columns: the seven columns of a capture carry no identity.
For ANY channel other than ch0, read `WaveAnalogProbe0/1` and `WaveDigitalProbe0..3` before labelling a
plot.** Four-plus nodes, all per channel.

**Verification independent of read-back.** §13f established that `OK` proves nothing, so setting ch52's
trapezoid was confirmed by the filter's own timing responding rather than by a register echo: its D2
marker moved **trigger+5536 ns -> +2544 ns**, against ch0's +2528 ns at the same settings. Armed width
stayed 552 ns on both channels before and after -- correct, since the time filter does not depend on the
trapezoid settings. FlatTopT 3000 is **AT** the board max, not above it, so the §13f clamp did not fire;
that is consistent with §13f, which truncates values ABOVE max. Capture
`DAQ:agent/data/trace_ch52_L1000_F3000.txt`. **The channels' current values are the session file's live
table -- deliberately not repeated here (§Scope).**

## 13j. ch52's EnergyFilter vs ch0's: the two channels' FILTERS agree, only amplitude differs
## (2026-10-14)
With ch52 Analog1 set to EnergyFilter (probes are `setinrun=true`, §9 -- no ACQ stop needed) the column
really is a trapezoid, and it matches ch0's:

- ch52 EF peak 645.5 mV vs ch0 656.7 mV, ratio **0.9829**; ADCInput peaks 620.7 vs 631.6 mV, ratio
  **0.9827**. Output ratio equals input ratio to 4 s.f., so **the filters agree and only the pulse
  amplitude differs** (per-channel offset / v_per_count, §13h). **corr between the two EF traces =
  1.00000.**
- Cross-checked by the DC leak, which knows L+M but nothing about markers: ch0 input pedestal
  2081.4 mV -> EF baseline 168.2 mV, predicted `V0*(L+M)/PZ` = 166.5 (**+1.7 mV**); ch52 2030.7 ->
  163.8, predicted 162.5 (**+1.4 mV**). Same 0.08 leak ratio => **both are L+M = 4000 ns**. Both show
  width-at-50% = 4000 ns (2L+M would be 5000; the top rounds, §11).
- Marker geometry identical: armed 552 ns, Trigger +0, D2 +2544 (ch52 PeakReady) vs +2528 (ch0
  Peaking) -- **a 16 ns difference between two DIFFERENT signals (§13i); do not read it as a filter
  discrepancy.**

Capture `DAQ:agent/data/trace_ch52_EF_1014_1256.txt`.

## 13k. Repo layout: agent/ folder (2026-10-14)
The user asked for one folder holding everything an agent authored. Settled as:
- **`DAQ:agent/`** -- `agent/AGENT.md` (this log) and `agent/data/` (all CLI trace captures, model
  backups `trapezoid_gui.prev.py` / `trapezoid_gui.spec0.py`, renderings). Originally BOTH were
  gitignored; **the log's ignore rule was removed on 2026-10-14 and it is now TRACKED** (commit
  `28d428a`), so the findings travel with the tools that cite them. `agent/data/` is still ignored.
  Plus `DAQ:agent/plot_capture.py`, which IS tracked: it is analysis tooling, not scratch.
- **`DAQ:Aux/trapezoid_gui.py` stays in `Aux/`** -- user's call: it is a **user-facing teaching tool**
  about what a trapezoid filter is, not agent scratch. Do not move it into `agent/` again.
`Aux/data/` no longer exists; every `Aux/data/...` reference anywhere now means `agent/data/...`.
Consequence unchanged from 13e: `agent/data/` is machine-local, so a fresh clone cannot re-verify the
filter against hardware. `~/real_trap.txt.bak` was folded into `agent/data/real_trap.txt.bak`.
**git index note:** `Aux/trapezoid_gui.py` had been STAGED by someone outside this session (no reflog
entry; I never ran `git add`). Moving it out left the index in `AD` state -- a commit would have
resurrected the old path. Resolved by `git rm --cached` on the old path + `git add` at `Aux/`, so the
index is back to `A Aux/trapezoid_gui.py` as the user had left it.
**UPDATE 2026-10-14: the user then committed it themselves** (`28d428a "added agent and
trapezoid_gui.py"`: .gitignore, Aux/trapezoid_gui.py, agent/AGENT.md, agent/plot_capture.py,
core/DigiManager.cpp). I never ran `git commit`. That commit also DELETED the `agent/AGENT.md` line
from `.gitignore`, which is how a tracked log became possible -- and it invalidated four statements in
this file and the session file that said the log was ignored. All corrected here.
**Lesson: a `.gitignore` rule does not un-track a file, and removing the rule is the only way to
commit an ignored path. If the log's status matters, check `git ls-files`, not `.gitignore`.**

## 14. Open items

**Actionable (trapezoid work):**
1. **Criterion 5 FAILS -- widen `EnergyFilterPileupGuardT`.** Guard holds 1528 ns vs a trapezoid
   10-10 % width of 18.3 us; a pulse on the flat top or fall is accepted with corrupted energy. Harmless
   at 1 kHz, **must be fixed before raising the rate**. Range 0..64000 ns in 64 ns steps. (See §11.)
   **UPDATE 2026-10-14: setting-dependent.** At RiseTimeT 1000 / FlatTopT 3000 the guard is **2520 ns**
   and covers the whole flat top plus the fall (§13f) -- improved at the values the bench now runs, and
   it still FAILS at the 5000/1000 default. Also `/ch/0/par/PileUpGuardT` did not read back on
   2026-10-14 (`ReadValue failed`); §9 recorded 960 ns there, so the node may have moved or been
   renamed -- do not assume 960 ns is still available.
2. **RE-OPENED, not resolved** -- was marked RESOLVED on 2026-10-12 after restoring 5000 / 50000 /
   1000, but the board has since been changed again and is **NOT restored**: ch0 and ch52 are both
   RiseTimeT **1000** / FlatTopT **3000** / PoleZeroT 50000 (§13f/13i), ch0 record length **2000** /
   pre-trigger **500** (defaults 4000/250, §17), and ch52's `WaveAnalogProbe1` is `EnergyFilter` where
   the default is `TimeFilter` (§13j). Probe assignments need not be restored (standing instruction),
   but the **filter and record-length** values are a real deviation. Restoring needs ACQ stopped for
   ChRecordLengthS (not `setinrun`, §9) and **read-back after every write** -- FlatTopT above 3000
   returns OK and silently clamps (§13f).
3. **Re-run the rise sweep with `PoleZeroT` at the 50000 default.** The §12 sweep ran with P-Z left at
   62000 ns from the preceding sweep, so its absolute means are not comparable to the P-Z sweep or the
   tuned point.
4. **Mostly RESOLVED: the energy is the TRAPEZOID peak, not the input pulse height.** Across the P-Z
   sweep the histogram mean tracks the measured trapezoid peak with a **constant ~17 mV offset**
   (645/664, 636/653, 631/646 mV), both falling monotonically with P-Z. So the 10708-vs-10408 gap is
   largely a category error -- comparing shaped peak height to input pulse height (634 mV).
   **Still unexplained: the constant ~17 mV itself**, and why rise-vs-mean plateaus near 10700.
   Not ADC saturation (16 % of full scale).
5. **What causes the residual ~3 % rise-time dependence of the mean energy?** -- **LARGELY explained
   (2026-10-12), residual open.** See §13 "Heuristic 2". The /L model reproduces the hardware sweep
   to +1.7 .. +4.4 percentage points, so the effect is the **finite signal rise interacting with the
   shaper** (the pole-zero output creeps with r||d and the shaper averages a still-creeping signal),
   NOT raw gain (which the board divides out) and NOT ballistic deficit. Supporting: with tau_pz
   matched, the /L peak spread over L = 1000..20000 ns is 0.36 % at r = 10 ns, 8.98 % at r = 818,
   38.61 % at r = 2000 -- purely a function of the signal rise.
   **Still open: the up-to-4.4-point residual, which grows as L shortens.** Do NOT close this item.
   Also note the earlier "flat within 2 sigma" framing was wrong -- by the MEAN these points are
   143-1187 sigma apart, but that sigma is run-to-run scatter (0.81 ch), a systematic floor, not
   statistical precision. (See §12.)
5c. **Why does +288 ns of the old 608 ns validation lag remain** after trigger-referencing the
   energy filter? The board's EF ramp starts 136 ns after its own trigger; the trigger-referenced
   model starts 104 ns after. Trigger-referencing halves the lag but makes shape 12x worse, so the
   energy filter is not gated -- something else shifts the reference. (See §13b.)
   **SUPERSEDED 2026-10-13: RESOLVED -- see §13c.** The answer is 612 ns of pure pipeline
   latency, not gating. Gating halves the lag but wrecks the shape (12x worse); the board's EF
   moves 200 ns BEFORE its trigger, so it runs continuously. With the delay applied: corr 1.00000,
   residual 0.034 %, lag 0 ns, all four corners within 12 ns of the capture.
5d. **Are tau RC=140 / tau CR=220 ns meaningful, or just a fit?** The zero crossing is degenerate
   across at least six (tau_RC, tau_CR) pairs, and the obvious 296=148+148 split from
   `TimeFilterRiseTimeT` misses by 19 samples. Needs the CAEN timing-filter description or a
   TimeFilter probe capture to settle. (See §13b.)
5b. **What is the DPP energy word's actual fill width?** The `sol` field is 2 bytes
   (`DAQ:docs/sol-file-format.md:83`) and the bench line sits at ~10455 counts, so >= 14 bits are in
   use -- NOT the 12 the user assumed. But no `EnergyBits`/`TraceBits`/`Gain` node resolves (all
   `ReadValue failed`) and `AbsoluteBaseline` max 16383 is a PSD baseline, not an energy width.
   Needs the CAEN manual or a device-tree dump to settle. (See §13.)
6. **`EnergyFilterPeakingAvg = OneShot` is why the flat-top tilt test was inconclusive** (324 counts
   residual vs 2.71 mV input noise). LowAVG/MediumAVG/HighAVG should make the tilt diagnostic usable --
   it trades resolution for the diagnostic, so measure both.
7. **Pole-zero must be set from the REAL detector decay, not the emulator's.** The emulator is currently
   *set* to 50 us, so matching it proves only that the filter tracks what it is fed. Get the MUSIC /
   crystal constant before finalising. (The parallel caveat about "rule 1"'s 3.4x rise margin is VOID --
   that rule was retracted 2026-10-14; there is no rise-time margin to lose.)

**Older:**
8. **Bulk `SetValue` false positive** — is `/ch/0..63/par/<name>` accepted-but-ignored? Needs approval
   because it writes a real value to 64 channels. **The test program is GONE: `/tmp/zprobe/wtest.c`
   was wiped by the 2026-10-12 reboot** -- the exact hazard this file warns about, so it must be
   REWRITTEN, not just re-run. §5's evidence stands (every bulk READ fails, even on a provably uniform
   parameter, while bulk `SetValue` returns success). **§13f strengthens the hypothesis**: an
   out-of-range write to a REAL node also returns OK while doing something else (clamp), so "OK" is
   demonstrably not a receipt anywhere on this board.
9. **§5-vs-§3 calibration contradiction** (1 MΩ claim vs `analogsel=1`) — needs `LAB:`'s decompiled C#
   for `HS_LSB_V` / `HV_LSB_V`.
10. **`EMU:pulser.py` has no load-impedance term** — should take the load and divide, like the vendor.
   Matters most for the real 100 Ω digitizer load (§4), where it is off by 1.5×.
11. **`inputtype = DIFF` vs single-ended emulator output** — unexamined.
12. **HDR amplitude** — 2867 mV for a requested 1 V. Not explained by anything on this machine.
13. Not started: `DAQ:docs/plan-broker-analysis.md` (event builder + analysis into the broker),
    `DAQ:docs/plan-dummy-generator.md` (dummy digitizers as signal sources).

**RESOLVED (do not re-open):**
- ~~Why CH2 had a clean pulse on 2026-10-08 and nothing on 2026-10-09~~ — the FAST line moved to the
  **digitizer ch0**; scope CH2 is empty by design. The 2026-10-08 numbers were valid, confirmed to
  0.5 % by §8. (§2)
- ~~Is standalone board status blank in standalone mode?~~ — fixed; the fix is committed in
  `DAQ:core/DigiManager.cpp` (`GetScalarSnapshot`'s standalone branch now reads the LED / ACQ /
  temp node groups with `ParseCounter`, mirroring broker mode). Confirmed on the bench. The
  former §7 was deleted 2026-10-14 as redundant once the fix landed -- the code is the record.
  If status ever goes blank again, `git log -- core/DigiManager.cpp` recovers the diff.
- ~~Do the digital probes read zero?~~ — never did; see the `d.sum()` warning. (§10)
- ~~Is resolution floored at ~1 % by digitizer noise, making tuning pointless?~~ — no, measured 0.09 %.
  The filter averages noise down 7.5x. Tuning is still flat, but for a different reason. (§12)
- ~~What is the emulator-noise-vs-digitizer-floor question?~~ — moot for tuning; see item 5.

## 15. Waveform probe sources -- AUTHORITATIVE COUNT: 5 analog, 13 digital, per
## channel, identical for every channel and every slot (2026-10-14, from the device tree)
Answered from the device tree, not by guessing names (§5's own rule). Dumped read-only with ctypes:

```python
import ctypes; L=ctypes.CDLL("/usr/local/lib/libCAEN_FELib.so")
L.CAEN_FELib_Open.argtypes=[ctypes.c_char_p, ctypes.POINTER(ctypes.c_uint64)]
# CAEN_FELib_OpenTCP does NOT exist -- only CAEN_FELib_Open(url). URL form: dig2://192.168.0.100
# GetDeviceTree(handle, None, 0) returns the byte count (2,326,299 = 2.3 MB), then call again with
# a buffer of size+4. Saved: DAQ:agent/data/devt_vx2745.json
```
**In the tree, node names are LOWERCASE** (`waveanalogprobe0`), while the CLI/`GetValue` names are
case-insensitive -- so a `grep WaveAnalogProbe` on the JSON finds NOTHING and looks like the nodes
don't exist. They do. `allowedvalues` is a dict of numeric-string keys -> {value}, plus a `value`
key holding the COUNT.

**ANALOG (`WaveAnalogProbe0/1`) -- 5 sources, same list in both slots:**
`ADCInput` (default), `TimeFilter`, `EnergyFilter`, `EnergyFilterBaseline`, `EnergyFilterMinusBaseline`.
So the DC-leak discussion in §13 has a purpose-built partner: `EnergyFilterMinusBaseline` is the board's
own version of the `ybase` subtraction that was removed from the GUI.

**DIGITAL (`WaveDigitalProbe0..3`) -- 13 sources, same list in all four slots:**
`Trigger`, `TimeFilterArmed`, `ReTriggerGuard`, `EnergyFilterBaselineFreeze`, `EnergyFilterPeaking`,
`EnergyFilterPeakReady`, `EnergyFilterPileupGuard`, `EventPileUp`, `ADCSaturation`,
`ADCSaturationProtection`, `PostSaturationEvent`, `EnergyFilterSaturation`, `AcquisitionInhibit`.
**Confirms §10's "13 sources each" was right**, and §10's table is missing nothing -- it measured 9 of
the 13 and left the 4 saturation ones untested (they need the input driven into saturation).
**Defaults are NOT uniform across channels**: ch0 was set to Trigger/TimeFilterArmed/Peaking/
PileupGuard by us; ch52's factory defaults are Trigger/TimeFilterArmed/EnergyFilterPeakReady/
EventPileUp -- which is exactly why ch52 mislabelled a plot (§13h/13i).
Tree has 65 `ch` entries for a 64-channel board (0..63 plus one aggregate node).

### INCIDENT, mine: opening a second FELib connection BROKE THE BROKER'S HANDLE
To dump the tree I called `CAEN_FELib_Open("dig2://192.168.0.100")` from a Python process while the
broker held the board. It returned rc=0, the dump succeeded, I called `Close`. **Afterwards every
`read` through the broker failed** (`ReadValue failed` for selftrgrate, dcoffset, EnergyFilterRiseTimeT)
while `ping` and `list` still worked -- the broker process was alive with its cached device list, only
its board handle was stale. A fresh ctypes Open in the same Python process read all three values
fine, proving **the board was healthy and the broker was the broken side**.
**FIXED without restarting anything: `close 0` then `open dig2://192.168.0.100` through the CLI** --
re-established as digitizer 0, reads verified working (selftrgrate 1000, RiseTimeT 1000). Board state
untouched; no writes were involved.
**RULE: the broker's CAEN_FELib connection is EXCLUSIVE for a reason. Do not open a second handle to
the digitizer from another process -- it silently invalidates the broker's handle, and the failure
looks like a bad parameter name rather than a lost connection. If a device-tree dump is needed again:
stop the broker first, or use `close 0`/`open` afterwards to recover.** `list`/`ping` succeeding while
`read` fails is the signature of exactly this.

## 16. Multi-source capture: 4 passes cover all 5 analog + 13 digital sources (2026-10-14)

The board exposes 2 analog + 4 digital probe SLOTS but 5 + 13 SOURCES (§15), so no single capture can
show them all. Four passes suffice: hold `ADCInput` in analog slot 0 and `Trigger` in D0 every pass
while the other slots walk the remainder (3 analog passes < 4 digital). Holding `Trigger` in D0 also
gives every pass a common alignment reference.

**Repeating `ADCInput` in every pass is the stability measurement, not redundancy** -- its spread across
passes is the noise floor for everything overlaid after it. Measured at ch0, 2000 samples, 1 kHz,
rise 0.2 us: peaks 631.1 / 631.5 / 631.1 / 631.4 mV, **spread 0.24-0.37 mV = 0.04-0.06 %**, i.e. ~4 LSB
of a 16-bit 4 V range. That is the bench's real repeatability and it is what makes cross-pass overlay
legitimate.

**A capture's columns are anonymous, so record the probe names alongside the data.** `solaris-cli trace`
restores `ChEnable`/`WaveSaving`/`WaveTriggerSource` but **never the probes** (§13h), and they are
per-channel -- so any multi-pass capture must save and restore all six itself, and read them back before
trusting the write (§13f). The implementation is `DAQ:agent/plot_capture.py` (`--sweep`, `--replot`,
`.json` sidecars); `git log` covers its history, this section is only why the pass structure is what it is.
## 17. Record length and pre-trigger: what is changeable, and what to check (2026-10-14)

`ChRecordLengthS/T` is **NOT `setinrun`** (§9) -- changing it requires ACQ stopped. `acquisitionstatus=8`
IS the stopped state, so no explicit stop is needed when nothing is acquiring. `ChPreTriggers` **is**
`setinrun`. Both accept writes and read back honestly (unlike `FlatTopT`, which clamps silently -- §13f).
Board defaults are 4000 / 250 samples; the values the bench currently runs are in the session md, not
here, because they change.

**The trigger sample is `ChPreTriggers + 1`, not `ChPreTriggers`** (500 -> trigger at sample 501). Any
code that assumes the trigger sits exactly `pre` samples into the record is off by one sample.

**A short record truncates the pulse, and the plot must know it.** With 500 pre-trigger the pulse onset
lands ~89 samples before the trigger, so "N samples before the trigger" is NOT a clean baseline region
at any pre-trigger setting -- the baseline must be taken from before the PULSE ONSET, which is a
different sample. Taking it from the window head silently absorbed part of the rising edge.

**`git ls-files`, not `.gitignore`, is the check for whether a file is tracked.** An ignore rule is
inert for a path already in the index, so a `.gitignore` that lists a tracked file reads as "ignored"
and is wrong. Verify with `git ls-files <path>` plus `git status --ignored`.
## 18. Verification and measurement discipline (2026-10-14)

**Transferable, tool-independent.** These outlive any specific bug, tool or capture. They were
originally buried inside sections about the incidents that produced them, which is exactly why they
got never re-found. New lessons of this kind belong HERE, not in the section about whatever tool was
being edited at the time.

### About numbers and models

**Check what a zero means before dividing by it.** An early pass reported "board EF noise sigma =
0.000 mV" and used it as a threshold, making the answer meaningless-by-luck. The number was right (the
pre-onset column really is one constant count); the framing was wrong -- the filter attenuates the
0.42 mV input noise below one output LSB.

**A tight error bar on a wrong model is not precision.** The 4-parameter pulse fit
`A(1-e^-t/r)e^-t/d` can return a rise constant with a sub-nanosecond error bar while its residual is a
large STRUCTURED hump, not noise. The error bar only says which single parameter best matches a wrong
SHAPE. Quote a model-free 10-90 instead, and report the residual's STRUCTURE, not just its size.
Worked case: §13g.

**An unverified belief about who wrote code silently decides what you are willing to change.** I
assumed a helper was the user's, treated it as untouchable, and built readouts on top of it instead of
simplifying it. The assumption was never checked and it was wrong. **Provenance is cheap to verify
(git history, a prior capture, asking) and expensive to assume** -- and once acted on, the error is
invisible because everything built on top of it looks deliberate.

**A rate appearing on an instrument is not attribution.** Prove causation by changing ONE source and
watching exactly one destination follow; a coincident value proves nothing. Worked case: §13h.

**A number can be right and the conclusion still wrong.** "Board EF noise 0.000 mV" was accurate and
its use was not. Before treating a measurement as evidence, state what else could produce it.

### About plots and figures

**Verify a plot in data coordinates, not pixels.** Pixel-diffing failed three different ways on the
same question (does a 0.78 px fill rasterise): pure-colour match, alpha-blend prediction, and
nearest-pixel-in-lane, which returned the panel's black reference line and grey grid as the closest
match. Two failure modes, neither fixable by tuning a threshold: **colour COLLISION** (the filter also
matches another artist in that colour) and **colour DILUTION** (antialiasing spreads a sub-pixel
rectangle below any threshold). The reliable check is the artist itself:
`collection.get_paths()[0].vertices`, mask to the top row, measure the span.

**Count artists, not pixels, when asking "is every source drawn?"** Answer it with `ax.get_lines()` /
`ax.collections` and their labels; posed in pixels it returns counts inflated by every artist sharing
that colour.

**Size every axis from the artists actually added, never from the requested window.** A requested span
is silently truncated by the data extent and the remainder shows up as empty axis. Same class of error
as anchoring a zoom on an assumed probe column: **any geometry rule that assumes what a column
contains is unsafe**, because probe identity varies per channel and per capture.

**Reusing a colour across panels defeats any colour-based verification.** If lane colours are projected
as lines onto a panel that also draws traces in those colours, a pixel check measures the traces.
Choose disjoint palettes per panel.

**A plot mark asserts a claim about the signal, so the mark must not contradict the physics.** A
one-clock-tick pulse drawn as a vertical line asserts *an instant*; the signal is a PULSE of measurable
width. When a mark is too thin to see, fix the rendering (opacity, minimum width, a zoomed inset) --
do not change what it asserts. Special-casing narrow runs into a different mark type is the same error
as clipping an axis to make a curve look flat.

**A label's position decides which thing it belongs to.** A marker label placed ABOVE its trace reads as
belonging to the lane above, even when horizontally aligned with its own. Centre it on the lane it
names; where a fill may already be HIGH beneath it, give it a translucent backing box.

**Repeating a source across captures is a measurement feature; repeating it in the legend is a display
bug.** Decouple them -- deduplicate the drawing while still collecting the value from every pass, which
is what a spread/stability number needs. "Fixing" it by dropping the repetition destroys the measurement.

### Matplotlib facts that cost hours

**`set_yticks()` EXPANDS the axes view to include every tick it is handed.** So filtering a tick list
and then calling `set_yticks()` can silently undo a `set_ylim()` placed before it -- the surviving
out-of-range tick widens the axis. Order that survives: `set_ylim` -> `set_yticks` filtered on BOTH
bounds -> re-assert `set_ylim` -> `set_autoscaley_on(False)`.

**`annotate()` and `legend()` each fire `autoscale_view`**, so any `set_ylim` before them is replaced.
Set limits after every artist exists.

**`set_title(..., loc='left')` populates `_left_title`, so a bare `get_title()` returns `''`.** Read it
back with `get_title(loc='left')` or a verification script reports "titles are missing" on a correct
figure.

**A 0.78 px fill is not a solid colour anywhere in the raster.** A sub-pixel rectangle straddles two
columns at ~39 % coverage each, so no pixel approaches the nominal colour -- which is why pixel tests
report a correctly-drawn feature as absent.
