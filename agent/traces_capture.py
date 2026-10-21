#!/usr/bin/env python3
"""Plot one capture as ONE timing diagram: both analog signals, with the
4 digital markers overlaid as colour in the same axes.

    python3 agent/traces_capture.py [agent/data/trace_six_1013_1308.txt] [out.png]
    python3 agent/traces_capture.py --replot                 # no board, from saved .txt
    python3 agent/traces_capture.py --sweep --ch=52          # 4 passes, touches the board

Three modes, only one of which writes to the board:
  MODE 1  (default)  plot one saved capture file. Offline, no broker needed.
  MODE 2  --replot   plot saved sweep captures. Offline. No probe writes, so a bad
                     plot is fixable without going near the bench.
  MODE 3  --sweep    set probes and capture 4 passes. THIS ONE TOUCHES HARDWARE --
                     read TRACE_WARNING before using it.

Parameter access goes through dig.Session (ZeroMQ to the broker). The single
exception is `trace`, which only exists inside solaris-cli and is driven as a child
process -- see cli().

Needs a capture whose probes are the settled ch0 set (../AGENT.md sec.10 -- the findings log):
Analog0=ADCInput, Analog1=EnergyFilter, D0=Trigger, D1=TimeFilterArmed,
D2=EnergyFilterPeaking, D3=EnergyFilterPileupGuard.

**The probe names are PER CHANNEL and this script cannot know them** -- a capture
is just seven columns of integers. They are therefore arguments (see --probe),
defaulting to the ch0 set. Do not trust the defaults for any other channel:
ch52 was found with Analog1=TimeFilter while ch0 had EnergyFilter, so plotting
it with the defaults labels a narrow 2nd-derivative lobe as "the trapezoid".
That is the same column-identity trap as agent/AGENT.md sec.13e, one step worse,
because the wrong label looks authoritative on the printed figure. Confirm
with `python3 agent/dig_status.py --param=WaveAnalogProbe1 --ch=N` before plotting a
new channel.

Layout history: this used to be six rows -- two analog lanes plus one lane per
digital marker -- then two rows with the digital band repeated on each. Both
wasted the reader's effort, because the whole point is reading a marker's
start/width/end AGAINST the analog signal it belongs to, and that comparison
was being made by eye across separate axes. Now everything is in one set of
axes at one time base.

Both analog traces are baseline-subtracted, so they share a zero line and a
mV scale honestly (peaks 634 and 654 mV here -- close enough to compare
directly, which is the useful case). The digital markers are stacked in a band
across the top, in data coordinates, so they stay put under zoom and never
cover the plateau.

Every marker is drawn the same way -- filled step plus outline -- including the
8 ns (one clock tick) ones. They used to be special-cased into vertical lines
because a 1-sample run is under a pixel wide at normal zoom; that was wrong on
substance, not just appearance: a line says "an instant", while the signal really
is a 1-tick PULSE. Narrow runs are drawn with an OPAQUE fill so the sliver does
not wash out. Trigger is BLACK, not red: red is the EnergyFilter curve, and the
trigger is the reference instant every other number on this plot is measured
from, so a neutral colour reads correctly.
"""
import os
import re
import sys

import time

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

DT = 8e-9          # s/sample
ADCTOMV = 0.060944  # mV/count (the board's measured adctovolts)
PAT = re.compile(r"^\s*(\d+)\s+(-?\d+)\s+(-?\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s*$")

# A run this many samples wide is "narrow": drawn with the SAME filled step as
# everything else (user, 2026-10-14 -- the old vertical-line special case is
# gone), but with an OPAQUE fill instead of a translucent one, because a 1-clock-
# tick run is under a pixel wide and a 30 %-alpha sliver of it washes out.
NARROW_MAX = 12

# `trace` exists ONLY in solaris-cli -- it is not a broker opcode. It has to be driven as
# a child process. Everything else goes through dig.Session (ZeroMQ to the broker).
# Neither path ever opens a second CAEN_FELib handle, which is what would break the
# broker (AGENT.md sec.15): the broker owns the one handle, and both routes are clients.
CLI = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
        "solaris-cli")

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dig import Session, DigiError, canon, validate          # noqa: E402


SWEEP_FLAG = '--sweep'
REPLOT_FLAG = '--replot'


def load(path):
    lines = open(path).read().splitlines()
    hdr = next(i for i, l in enumerate(lines) if l.startswith("Sample"))
    A = np.array([[int(x) for x in m.groups()]
                  for m in (PAT.match(l) for l in lines[hdr + 1:]) if m], float)
    assert A.shape[1] == 7, A.shape          # Sample Analog0 Analog1 D0..D3
    return A


def runs(col):
    """Contiguous HIGH stretches as (start, stop) sample pairs."""
    out, i, n = [], 0, len(col)
    while i < n:
        if col[i] > 0:
            k = i
            while k < n and col[k] > 0:
                k += 1
            out.append((i, k))
            i = k
        else:
            i += 1
    return out


def overlay_digital(ax, t, dig, dg, top_mv, base_mv):
    """Stack the digital waveforms in a band across the top of `ax`.

    Each marker gets its own sub-lane of (top_mv - base_mv)/len(dig), drawn in
    DATA coordinates so it stays put under zoom. `base_mv` is the bottom of the
    band, i.e. the top of the analog region it must not be drawn over.
    """
    slot = (top_mv - base_mv) / len(dig)
    for k, (nm, col, cc) in enumerate(dig):
        lo = base_mv + k * slot
        hi = lo + slot * 0.82
        if not dg[nm]:
            continue                    # probe not configured on this channel
        widths = ", ".join(f"{(b - a) * DT * 1e9:.0f} ns" for a, b in dg[nm])
        # ONE drawing rule for every marker, wide or 1-sample-narrow (user,
        # 2026-10-14). These used to be special-cased into vertical lines on the
        # grounds that an 8 ns run is under a pixel and a fill would be
        # invisible -- but a line is a different claim from a waveform: it says
        # "an instant", while the signal really is a 1-clock-tick PULSE. Draw it
        # as the pulse it is and let the width be what it is.
        # The one concession to visibility: at 0.78 px a 28 %-alpha sliver washes
        # out against white, so narrow runs get an opaque fill. Same shape, same
        # lane, just not faint.
        narrow = all((b - a) <= NARROW_MAX for a, b in dg[nm])
        step = (col > 0).astype(float)
        ax.fill_between(t, lo, np.where(step > 0, hi, lo), step="post",
                        color=cc, alpha=.9 if narrow else .28, lw=0)
        ax.step(t, lo + step * (hi - lo), where="post", color=cc, lw=1.1,
                label=f"{nm} {widths}")
    return base_mv


# D0..D3 as configured on ch0. Other channels differ -- ch52 was found with
# D2=EnergyFilterPeakReady and D3=EventPileUp, which are DIFFERENT signals from
# EnergyFilterPeaking/EnergyFilterPileupGuard even though they sit in the same
# columns and fire at similar times. Passing the wrong names here prints a
# confident wrong label on the figure: confirm with
# `read 0 /ch/N/par/WaveDigitalProbe0..3` before plotting a new channel.
DPROBE_CH0 = ("Trigger", "TimeFilterArmed", "EnergyFilterPeaking",
              "EnergyFilterPileupGuard")


def plot_file(path, outpath, probes=("ADCInput", "EnergyFilter"), chan=None,
            dprobes=DPROBE_CH0):
    A = load(path)
    n = len(A)
    t = np.arange(n) * DT * 1e6                      # us
    adc = (A[:, 1] - np.median(A[:, 1][:150])) * ADCTOMV
    ef = (A[:, 2] - np.median(A[:, 2][:150])) * ADCTOMV
    # Colour is keyed to the COLUMN, not the name, so a renamed probe still
    # lands in the same lane of the overlay band.
    dig = [(dprobes[1], A[:, 4], "#9467bd"),
           (dprobes[0], A[:, 3], "#000000"),
           (dprobes[2], A[:, 5], "#2ca02c"),
           (dprobes[3], A[:, 6], "#8c564b")]
    dg = {nm: runs(c) for nm, c, _ in dig}
    # Digital probes are PER CHANNEL. A capture from a channel nobody configured
    # (e.g. `trace 0 52 4000` after setting the probes on ch0) has all four
    # columns zero, and indexing dg["Trigger"][0] then dies with an IndexError.
    # The analog traces are still perfectly good, so fall back to them.
    have = {nm: dg[nm] for nm, _, _ in dig if dg[nm]}
    trig = dg[dprobes[0]][0][0] if dg[dprobes[0]] else int(np.argmax(adc > 0.5 * adc.max()))

    if dg[dprobes[1]] and dg[dprobes[3]]:
        # Zoom to where the action is: a little before arming to past the trapezoid
        # end. Anchor on min(armed, trigger), NOT on D1 alone: D1 is only
        # "TimeFilterArmed" on channels configured like ch0, and a pass whose D1
        # is a POST-trigger marker (e.g. EnergyFilterPeaking) pushed lo past the
        # trigger and cut the reference line off the plot entirely.
        lo = max(0.0, min(t[dg[dprobes[1]][0][0]], t[trig]) - 0.6)
        hi = max(t[dg[dprobes[3]][0][1]], t[trig]) + 3.0
    else:
        # No digital markers: frame on the analog pulse itself.
        onset = int(np.argmax(adc > 0.05 * adc.max()))
        hi = min(t[-1], t[onset] + 12.0)
        lo = max(0.0, t[onset] - 2.0)

    fig, ax = plt.subplots(figsize=(13.5, 7.6), constrained_layout=True)
    ax.set_xlim(lo, hi)
    ax.grid(alpha=.25)
    ax.set_xlabel("time (us)")
    ax.set_ylabel("mV  (baseline-subtracted)")

    peak = max(adc.max(), ef.max())
    # Top ~40 % of the axes belongs to the digital band, so the plateau of
    # either trace is never hidden behind a fill.
    band_lo = peak * 1.32
    overlay_digital(ax, t, dig, dg, top_mv=peak * 2.12, base_mv=band_lo)

    an1 = probes[1] + (" = the trapezoid" if probes[1] == "EnergyFilter" else "")
    ax.plot(t, adc, color="#1f77b4", lw=1.4, label=f"{probes[0]} (raw input)", zorder=5)
    ax.plot(t, ef, color="#d62728", lw=1.6, label=an1, zorder=5)



    if have:
        parts = [f"{k.replace('EnergyFilter', '').replace('TimeFilter', '')} @ "
                 f"{(dg[k][0][0] - trig) * DT * 1e9:+.0f} ns" for k in have]
        ax.annotate("trigger is the reference: " + ", ".join(parts),
                    (t[trig], peak * 1.18), fontsize=7.5, color="#333")
    else:
        ax.annotate("no digital probes configured on this channel -- analog only",
                    (t[trig], peak * 1.18), fontsize=7.5, color="#333")

    ax.legend(fontsize=7.5, loc="lower right", ncol=2)

    # ylim goes LAST, after every artist exists: annotate() and legend() both
    # trigger autoscale_view, which silently replaces a ylim set earlier (here
    # it widened the bottom to -200 mV against a -0.5 mV data minimum, burning
    # ~15 % of the panel height). Then switch autoscaling off so it sticks.
    ylo = min(adc.min(), ef.min()) - 20
    ax.set_ylim(ylo, peak * 2.2)
    # Ticks inside the digital band would read as mV levels of the analog
    # traces, which they are not -- drop them. Filter on BOTH bounds: the
    # default locator's first tick (-200) sits below ylo, and set_yticks
    # EXPANDS the view to include every tick it is handed, which silently
    # undid the set_ylim above.
    ax.set_yticks([y for y in ax.get_yticks() if ylo <= y <= band_lo])
    ax.set_ylim(ylo, peak * 2.2)        # re-assert, after set_yticks
    ax.set_autoscaley_on(False)
    # The channel is NOT knowable from the seven integer columns, so it comes
    # from --ch= or, failing that, a "chNN" token in the filename. Never just
    # print "ch0": that mislabelled a ch52 capture here.
    if chan is None:
        m = re.search(r"ch(\d+)", os.path.basename(path))
        chan = f"ch{m.group(1)}" if m else "ch?"
    elif not str(chan).startswith("ch"):
        chan = f"ch{chan}"        # normalise --ch=52 as well as --ch=ch52
    fig.suptitle(f"VX2745 {chan} capture -- {os.path.basename(path)}   |   "
                 f"{n} samples x {DT * 1e9:.0f} ns = {n * DT * 1e6:.1f} us   |   "
                 f"window {lo:.2f}..{hi:.2f} us\n"
                 f"one axes, one time base: coloured steps across the top are the digital "
                 f"markers, drawn against the analog traces below them",
                 fontsize=10)
    fig.savefig(outpath, dpi=110)
    print(f"wrote {outpath}")

    print("\nmarker times relative to the TRIGGER:")
    if not have:
        print("  (none -- digital probes are per channel and were not set on this one)")
    for nm, _, _ in dig:
        if not dg[nm]:
            continue
        a, b = dg[nm][0]
        print(f"  {nm:26s} {(a - trig) * DT * 1e9:+7.0f} .. {(b - trig) * DT * 1e9:+7.0f} ns"
              f"   (width {(b - a) * DT * 1e9:.0f} ns)")


# ============================================================== MODE 2: SWEEP ==
# The board offers 5 analog and 13 digital waveform sources (AGENT.md sec.15)
# but only 2 analog slots and 4 digital slots per capture, so no single capture
# can show them all. Sweep mode walks the combinations and plots the union.
#
#   analog  slots=2  sources=5  -> 3 passes (ADCInput held in slot 0 every pass)
#   digital slots=4  sources=13  -> 4 passes (Trigger held in D0 every pass)
#
# 3 < 4, so FOUR captures cover both axes. ADCInput is repeated in every pass,
# which is not waste: it is the stability check the mode exists for. If the
# signal is stable the four ADCInput traces must sit on top of one another, and
# their spread is then the noise floor of everything plotted after it.
ANALOG_SOURCES = ("ADCInput", "TimeFilter", "EnergyFilter",
                  "EnergyFilterBaseline", "EnergyFilterMinusBaseline")
DIGITAL_SOURCES = ("Trigger", "TimeFilterArmed", "ReTriggerGuard",
                   "EnergyFilterBaselineFreeze", "EnergyFilterPeaking",
                   "EnergyFilterPeakReady", "EnergyFilterPileupGuard",
                   "EventPileUp", "ADCSaturation", "ADCSaturationProtection",
                   "PostSaturationEvent", "EnergyFilterSaturation",
                   "AcquisitionInhibit")
# Colour per source, so a source looks the same wherever it appears.
ANA_COLOR = {"ADCInput": "#1f77b4", "TimeFilter": "#9467bd",
             "EnergyFilter": "#d62728", "EnergyFilterBaseline": "#8c564b",
             "EnergyFilterMinusBaseline": "#e377c2"}
DIG_COLOR = {n: c for n, c in zip(
    DIGITAL_SOURCES,
    ["#000000", "#9467bd", "#2ca02c", "#8c564b", "#d62728", "#e377c2", "#1f77b4",
     "#ff7f0e", "#bcbd22", "#17becf", "#7f7f7f", "#aec7e8", "#98df8a"])}


def cli(digi, *cmds, timeout=300):
    """Run commands through the broker CLI as a child process.

    Only `trace` genuinely needs this: it is implemented in solaris-cli.cpp, not as a
    broker opcode, and it streams a PUB snapshot into the CLI's own ring buffer. There
    is no way to ask the broker for it over ZMQ without reimplementing that whole
    sequence. Do NOT add other commands here -- use dig.Session.
    """
    import subprocess
    inp = "connect\n" + "".join(c + "\n" for c in cmds) + "quit\n"
    p = subprocess.run([CLI], input=inp, capture_output=True, text=True,
                       timeout=timeout)
    return p.stdout + p.stderr


PROBE_PARAMS = (["WaveAnalogProbe0", "WaveAnalogProbe1"]
                + [f"WaveDigitalProbe{i}" for i in range(4)])


def read_probes(s, ch):
    """The six probe names on one channel, in D0..D3 / A0..A1 order.

    Was: parse the CLI's echoed `> read` lines with startswith("> read") and take the
    following line. That depended on readline echo being OFF, which depends on whether
    stdin is a tty -- and a silent shift to the wrong column labels is exactly the
    failure this script exists to avoid (sec.13h: a wrong probe label looks plausible on
    the printed figure). dig.Session keys each read by the request, so position cannot
    drift.
    """
    return [s.read(p, ch=ch) for p in PROBE_PARAMS]


def write_probes(s, ch, names):
    """Set the six probes, each validated against the device tree and read back.

    validate() checks the allowed list, so a typo like "EnergyFilterPeak" is refused
    here instead of being accepted by the board and silently leaving the old source in
    place -- which would mislabel every column of the capture.
    """
    for p, want in zip(PROBE_PARAMS, names):
        ok, note = validate(p, want)
        if not ok:
            raise DigiError(f"refusing probe {p}={want}: {note}")
        old, got, rb = s.write(p, want, ch=ch)
        if got != want:
            raise DigiError(f"probe {p} did not take: asked {want}, board holds {got}"
                            f"  [{rb}]")


def write_meta(save_to, ana, dig, ch):
    """Sidecar recording WHICH SOURCE each column is.

    A capture file is seven integer columns with no identity -- that ambiguity
    has caused most of the wrong findings in this project (AGENT.md sec.13e,
    13h). The sweep knows the names, so it writes them down next to the data
    instead of leaving the next reader to guess or re-query the board.
    """
    import json
    json.dump({"ch": ch, "analog": list(ana), "digital": list(dig),
               "dt_s": DT, "mv_per_count": ADCTOMV},
              open(save_to + ".json", "w"), indent=1)


def meta_of(path):
    """(analog_names, digital_names) for a saved capture, or None."""
    import json
    try:
        m = json.load(open(path + ".json"))
        return tuple(m["analog"]), tuple(m["digital"])
    except Exception:
        return None


def sweep_capture(s, digi, ch, samples, ana, dig, save_to):
    """One pass: set the six probes, capture, return the 7-column array.

    Probes are `setinrun=true` (sec.9) so no ACQ restart is needed. They are written
    through dig.Session -- validated against the device tree and read back individually
    (sec.13f: OK means accepted, not set).

    The capture itself still goes through the CLI, and that is NOT laziness: `trace` is
    implemented in solaris-cli.cpp, not as a broker opcode, so it cannot be requested
    over ZMQ. See the warning in TRACE_WARNING about what that call does to the board.
    """
    write_probes(s, ch, list(ana) + list(dig))
    out = cli(digi, f"trace {digi} {ch} {samples}")
    if "No trace captured" in out:
        raise DigiError(
            f"trace on ch{ch} returned no data. Check ChEnable and that the channel is"
            " actually triggering (dig_run.py --seconds 5).")
    open(save_to, "w").write(out)
    write_meta(save_to, ana, dig, ch)
    return load(save_to), out


TRACE_WARNING = """
  !! `trace` runs StartACQ(digi, DataFormat::ALL) -- format 0.
     ALL is the format the broker has DIED TWICE on (AGENT.md sec.19): 2 analog +
     4 digital probes on 64 channels. --sweep does it four times in a row.
     It also writes ChEnable=False to /ch/0..63/ and WaveTriggerSource=ChSelfTrigger,
     so it cannot capture an externally-triggered channel and it destroys any routing
     config you are testing. NEVER use --sweep while a trigger test is in flight.
     If the CLI hangs or dies, the broker is probably gone: check
       pgrep -x solaris-broker && printf 'connect\\nlist\\nquit\\n' | ../solaris-cli
"""

def align_to_trigger(A):
    """Return (trigger_sample, runs_of_D0). All passes share the trigger instant
    so their traces can be overlaid; without this, a pass captured a few hundred
    samples later would sit visibly offset and look like a shape difference."""
    r = runs(A[:, 3])
    return r[0][0] if r else None


def sweep_main():
    argv = [a for a in sys.argv[1:] if not a.startswith("--")]
    ch, digi, samples, out = 0, 0, 2000, None
    for a in sys.argv[1:]:
        if a.startswith("--ch="):
            ch = int(a.split("=", 1)[1].replace("ch", ""))
        elif a.startswith("--digi="):
            digi = int(a.split("=", 1)[1])
        elif a.startswith("--samples="):
            samples = int(a.split("=", 1)[1])
        elif a.startswith("--out="):
            out = a.split("=", 1)[1]
    here = os.path.dirname(os.path.abspath(__file__))
    stamp = re.sub(r"\D", "", time.strftime("%m%d_%H%M"))
    out = out or os.path.join(here, "data", f"plot_ch{ch}_sweep_{stamp}.png")

    s = Session(digi=digi)
    s.boot()

    print(TRACE_WARNING, flush=True)

    # Save the channel's probe settings so a failed run cannot leave the board
    # mislabelled for the next person.
    try:
        orig = read_probes(s, ch)
    except DigiError as e:
        sys.exit(f"could not read the current probes on ch{ch}: {e}\n"
                 "Is the broker up?  pgrep -x solaris-broker")
    if len(orig) != 6 or any(not v for v in orig):
        sys.exit(f"probes on ch{ch} did not read cleanly: {orig}")
    print(f"ch{ch} probes as found: analog {orig[:2]}  digital {orig[2:]}")

    # trace forces WaveTriggerSource=ChSelfTrigger and disables every other channel,
    # so warn loudly if the board is mid-experiment rather than at defaults.
    wts = s.read("WaveTriggerSource", ch=ch)
    if wts != "ChSelfTrigger":
        print(f"  !! ch{ch} WaveTriggerSource is {wts!r} -- `trace` will overwrite it"
              " with ChSelfTrigger and the restore is known to fail.\n"
              "     Aborting rather than destroy the config under test (sec.19).")
        sys.exit(2)
    others = [c for c in range(64) if c != ch and s.read("ChEnable", ch=c) != "True"]
    if others:
        print(f"  note: {len(others)} other channels are currently ChEnable=False;"
              " trace re-enables all 64 as a side effect.")

    # 4 passes. Analog slot 0 is ADCInput every time; slot 1 walks the other four.
    # D0 is Trigger every time; D1..D3 walk the other twelve.
    passes = []
    ana_rest = list(ANALOG_SOURCES[1:])
    dig_rest = list(DIGITAL_SOURCES[1:])
    for i in range(4):
        a1 = ana_rest[i] if i < len(ana_rest) else ANALOG_SOURCES[0]
        d = ["Trigger"] + list(dig_rest[i * 3:(i + 1) * 3])
        while len(d) < 4:
            d.append(d[-1])
        passes.append(((ANALOG_SOURCES[0], a1), tuple(d)))

    data = []
    try:
        for k, (ana, dig) in enumerate(passes):
            f = os.path.join(here, "data", f"trace_ch{ch}_sweep{k}_{stamp}.txt")
            print(f"pass {k+1}/4  analog={ana[0]},{ana[1]}  digital={','.join(dig[1:])}",
                  flush=True)
            A, _ = sweep_capture(s, digi, ch, samples, ana, dig, f)
            tr = align_to_trigger(A)
            if tr is None:
                print(f"  !! no Trigger in this capture -- pass skipped from the "
                      f"overlay (cannot be aligned to the others)")
            data.append((ana, dig, A, tr, f))
            print(f"  ok: {len(A)} samples, trigger at sample {tr}")
    finally:
        # Restore whatever was there, even if a pass blew up halfway. Read back so a
        # failed restore is reported rather than assumed (sec.13f).
        try:
            write_probes(s, ch, orig)
            print(f"probes restored to {orig[:2]} / {orig[2:]}")
        except DigiError as e:
            print(f"  !! PROBE RESTORE FAILED: {e}\n"
                  f"     ch{ch} may be left mislabelled -- restore by hand or the"
                  " columns in any later capture will not match their names.")

    return plot_sweep(ch, data, out, stamp)


def plot_sweep(ch, data, out, stamp):
    """Panel 1: every analog source on one mV scale. Panel 2: all 13 digital
    markers as lanes -- 13 cannot be overlaid readably, which is why mode 1
    overlays four and this does not."""
    good = [d for d in data if d[3] is not None]
    if not good:
        sys.exit("no pass contained a Trigger; nothing can be aligned.")
    n = min(len(d[2]) for d in good)

    # WINDOW, per pass, on the BASELINE-SUBTRACTED trace.
    # Two traps here, both hit in development:
    #  * comparing RAW counts (pedestal ~2080 mV) against 0.9*max (2038 mV) is
    #    true from sample 0 -- that collapsed the window to its floor.
    #  * the pulse ARRIVES BEFORE THE TRIGGER (onset ~sample 153, trigger ~251,
    #    i.e. ~1 us earlier). So "PAD samples before the trigger" is NOT a clean
    #    baseline: it sits mid-rise, and subtracting its median ate ~150 mV of
    #    pulse, reporting a 34 mV peak for a 184 mV pulse. The baseline must be
    #    taken from before the PULSE ONSET, and the window must start there too.
    def onset_of(A, tr):
        y = (A[:, 1] - np.median(A[:100, 1])) * ADCTOMV
        pk = y.max()
        if pk <= 0:
            return max(0, tr - 60)
        i = np.nonzero(y > 0.05 * pk)[0]
        return int(i[0]) if len(i) else max(0, tr - 60)

    onsets = [onset_of(A, tr) for _, _, A, tr, _ in good]
    PRE = max(tr - o for (_, _, A, tr, _), o in zip(good, onsets)) if good else 60
    PRE = int(min(n - 1, max(60, (tr0 := good[0][3]) - min(onsets)) + 40))

    def pulse_end(A, tr):
        # The decay is ~50 us and the record is 32 us, so the pulse NEVER returns
        # to 2 % of peak -- fall back to the end of the record rather than
        # pretending a 2 % crossing was found.
        y = (A[:, 1] - np.median(A[:100, 1])) * ADCTOMV
        return int(np.argmax(y)) + int(3 * 50e-6 / DT)      # ~3 tau after peak

    span = int(min(n - 1, max(400, max(pulse_end(A, tr) - tr for _, _, A, tr, _ in good))))

    def win(A, tr):
        # Time origin = the TRIGGER, which is what both panels' labels claim. It
        # was NOT: the axis ran from the record start, so the trigger sat at
        # 2.14 us while the digital panel was captioned "relative to the
        # TRIGGER" -- every marker width read off that axis was out by 2.14 us.
        st = max(0, tr - PRE)
        en = min(len(A), tr + span)
        return A[st:en], -(tr - st) * DT * 1e6

    fig, axs = plt.subplots(2, 1, figsize=(13.5, 9.6), constrained_layout=True,
                            gridspec_kw={"height_ratios": [1, 2]})
    ax, axd = axs
    for a in ax, axd:
        a.grid(alpha=.25)

    # ---- analog ----
    peaks, tps, xmax = [], [], []
    drawn_base = False
    for (ana, dig, A, tr, f), o in zip(good, onsets):
        w, t0 = win(A, tr)
        # Baseline from the record's OWN pre-onset samples, not from the head of
        # the display window. The window starts only PRE = (tr - onset) + 40
        # samples back, so its first 60 samples straddle the onset at the short
        # pre-trigger settings and the median absorbs part of the rising edge --
        # at 500 pre-trigger samples that ate ~10 mV off every trace.
        nb = max(5, min(200, o - 5))
        base = np.median(A[:nb, :], axis=0)
        for j, nm in enumerate(ana):
            # ADCInput is deliberately repeated in every pass as the stability
            # check, so slot 0 must be DRAWN only once -- plotting it four times
            # produced four identical-looking curves and a legend that repeated
            # it. Its peaks are still collected from every pass, below, which is
            # what the stability number actually measures.
            if j == 0 and ana[0] == ANALOG_SOURCES[0] and drawn_base:
                continue
            y = (w[:, 1 + j] - base[1 + j]) * ADCTOMV
            ax.plot(t0 + np.arange(len(y)) * DT * 1e6, y, lw=1.3,
                    color=ANA_COLOR[nm], label=nm)
            if j == 0:
                drawn_base = True
        tps.append(t0)
        xmax.append(t0 + len(w) * DT * 1e6)      # end of what THIS pass drew
        peaks.append(((w[:, 1] - base[1]) * ADCTOMV).max())
    # ADCInput appears in EVERY pass: its spread across passes is the stability
    # measurement this mode exists for, and the floor for reading anything else.
    spread = max(peaks) - min(peaks)
    print(f"\nADCInput peak per pass: " + ", ".join(f"{p:.1f}" for p in peaks) + " mV")
    print(f"  spread {spread:.2f} mV = {spread / max(peaks) * 100:.2f} % of peak"
          f"  -> {'STABLE' if spread < 0.02 * max(peaks) else 'NOT stable: overlaying passes is questionable'}")
    print(f"  (16-bit LSB is {ADCTOMV:.4f} mV, so a spread near that is quantisation, not noise)")
    print(f"  this spread is the NOISE FLOOR for every other trace overlaid here.")

    # ---- digital: 13 lanes ----
    # Compute the shared x window FIRST: axd.get_xlim() before anything is drawn
    # returns the autoscale placeholder (0, 1), which silently threw every label
    # off the left edge of the panel.
    # Both panels get the SAME x range explicitly. Leaving the analog panel to
    # autoscale made it span -0.52..33.6 us while the digital panel spanned
    # 1.03..33.02, so a marker could not be read against a trace -- which is the
    # entire reason the two panels are stacked instead of separate files.
    # Size the axis from what is ACTUALLY DRAWN, not from the requested span.
    # The window is [tr-PRE, tr+span) truncated at the record end, so with a
    # 500-sample pre-trigger on a 2000-sample record only ~1628 samples exist
    # past the window start: the data stopped at 11.98 us while the axis ran to
    # 15.99 us, leaving 4.008 us of empty plot on the right.
    TLO = min(tps) if tps else 0.0        # negative: PRE samples before trigger
    THI = max(xmax) if xmax else 0.0      # furthest sample actually drawn
    seen = {}
    for ana, dig, A, tr, f in good:
        w, t0 = win(A, tr)
        tt = t0 + np.arange(len(w)) * DT * 1e6
        for i, nm in enumerate(dig):
            if nm not in seen or len(runs(w[:, 3 + i])) > len(seen[nm][1]):
                seen[nm] = (tt, runs(w[:, 3 + i]), w[:, 3 + i])
    LANE = 2.0                    # lane pitch; the HIGH level is base + 1.5
    for k, nm in enumerate(DIGITAL_SOURCES):
        base = k * LANE
        if nm in seen:
            tt, rs, col = seen[nm]
            # Same single rule as mode 1: fill every marker, however narrow.
            narrow = rs and all((b - a) <= NARROW_MAX for a, b in rs)
            axd.fill_between(tt, base, np.where(col > 0, base + 1.5, base),
                             step="post", color=DIG_COLOR[nm],
                             alpha=.9 if narrow else .35, lw=0)
            axd.step(tt, base + 1.5 * (col > 0), where="post",
                     color=DIG_COLOR[nm], lw=1.0)
            txt = ", ".join(f"{(b - a) * DT * 1e9:.0f} ns" for a, b in rs[:3])
            lab = txt or "never high in this record"
        else:
            lab = "never offered by this firmware"
        # Label sits at HALF THE LANE SPACING (the lane's mid-height), not
        # crammed into the 0.3 gap above the trace at base+1.7. At the old size
        # and position the 13 names were unreadable, and being above the trace
        # each label visually belonged to the lane ABOVE it.
        # va='center' + a translucent white box so it stays legible where a
        # marker is already HIGH at the left edge (BaselineFreeze is).
        axd.text(TLO, base + LANE / 2, f"{nm}  [{lab}]", fontsize=9,
                 color=DIG_COLOR[nm], va="center", ha="left", zorder=6,
                 bbox=dict(facecolor="white", alpha=.78, edgecolor="none", pad=1.2))
        # ---- project EVERY edge of this lane onto the analog panel above ----
        # Only the trigger used to get a line up there, so a marker's start or
        # end had to be read off the lane and then chased across by eye -- the
        # exact cross-panel eyeballing that stacking was meant to remove. Same
        # colour as the lane, so the correspondence needs no legend.
        # Six edge pairs here land 8-16 ns apart (1-2 px), e.g. Trigger vs
        # ReTriggerGuard, so alpha<1 and a thin line keep them separable instead
        # of merging into one blob.
        if nm in seen:
            tt, rs, _ = seen[nm]
            for a, b in rs:
                for idx in (a, min(b, len(tt) - 1)):
                    x = tt[idx]
                    if TLO <= x <= THI:
                        # zorder 1.5: above the grid, BELOW the analog traces
                        # (default 2), so 18 lines frame the pulse without
                        # drawing over it. The trigger's own reference line stays
                        # on top at full opacity because it is the one instant
                        # every number here is measured from.
                        ax.axvline(x, color=DIG_COLOR[nm], lw=1.0, alpha=.55,
                                   zorder=1.5)
    axd.set_yticks([])
    axd.set_xlim(TLO, THI)
    axd.set_ylim(-0.6, len(DIGITAL_SOURCES) * LANE)
    axd.set_xlabel("time (us) relative to the TRIGGER")
    axd.set_title("all 13 digital sources (each from the pass that carried it; "
                  "Trigger is in every pass as the alignment reference)",
                  loc="left", fontsize=9)

    ax.set_xlim(TLO, THI)
    for a in (ax, axd):
        a.axvline(0, color="#000000", lw=1.2, alpha=.8)
    ax.set_ylabel("mV (baseline-subtracted)")
    ax.legend(fontsize=7.5, loc="upper right", ncol=2)
    ax.set_title(f"all analog sources from {len(good)} passes, overlaid on the trigger "
                 f"(ADCInput repeated as the stability check: spread {spread:.2f} mV)",
                 loc="left", fontsize=9)
    fig.suptitle(f"VX2745 ch{ch} sweep -- {len(good)} captures of a repeating signal, "
                 f"{n} samples x {DT*1e9:.0f} ns each\n"
                 f"the board has 2 analog + 4 digital probe SLOTS but 5 + 13 SOURCES "
                 f"(AGENT.md sec.15); this walks them and plots the union",
                 fontsize=10)
    fig.savefig(out, dpi=110)
    print(f"\nwrote {out}")
    print("captures kept: " + ", ".join(os.path.basename(d[4]) for d in data))
    return 0


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    argv = [a for a in sys.argv[1:] if not a.startswith("--")]
    chan, dprobes = None, DPROBE_CH0
    probes = ("ADCInput", "EnergyFilter")
    for a in sys.argv[1:]:
        if a.startswith("--probe="):
            probes = tuple(a.split("=", 1)[1].split(","))
            if len(probes) != 2:
                sys.exit("--probe wants two comma-separated names, "
                         "e.g. --probe=ADCInput,TimeFilter")
        elif a.startswith("--ch="):
            chan = "ch" + a.split("=", 1)[1].replace("ch", "")
        elif a.startswith("--dprobe="):
            dprobes = tuple(a.split("=", 1)[1].split(","))
            if len(dprobes) != 4:
                sys.exit("--dprobe wants four comma-separated names in D0..D3 order")

    rep = [a for a in sys.argv[1:] if a.startswith(REPLOT_FLAG)]
    if rep:
        # Re-plot saved sweep captures: no board, no probe writes, so a bad
        # plot is fixable without re-capturing (and without touching the bench).
        import glob as _g
        pat = rep[0].split("=", 1)[1] if "=" in rep[0] \
            else os.path.join(here, "data", "trace_ch*_sweep*.txt")
        files = sorted(_g.glob(pat))
        if len(files) < 2:
            sys.exit(f"--replot wants >=2 saved captures matching {pat!r} (found {len(files)})")
        c = re.search(r"ch(\d+)_sweep", os.path.basename(files[0]))
        data = []
        for f in files:
            A = load(f)
            md = meta_of(f)
            if md is None:
                sys.exit(f"{os.path.basename(f)} has no .json sidecar naming its "
                         f"probes; refusing to guess (AGENT.md sec.13e). Re-run "
                         f"{SWEEP_FLAG}, or make the sidecar by hand.")
            data.append((md[0], md[1], A, align_to_trigger(A), f))
        sys.exit(plot_sweep(int(c.group(1)) if c else 0, data,
                                   argv[1] if len(argv) > 1 else os.path.join(
                                       here, "data",
                                       f"plot_ch{c.group(1) if c else '?'}_sweep_replot.png"),
                                   "replot"))

    if any(a == SWEEP_FLAG or a.startswith(SWEEP_FLAG + "=") for a in sys.argv[1:]):
        sys.exit(sweep_main())          # MODE 2: needs the broker and the board

    src = argv[0] if argv else os.path.join(here, "data", "trace_six_1013_1308.txt")
    base = os.path.basename(src).replace(".txt", "")
    dst = argv[1] if len(argv) > 1 else os.path.join(
        here, "data", base.replace("trace_", "plot_") + ".png")
    plot_file(src, dst, probes, chan, dprobes)   # MODE 1: offline, one capture
