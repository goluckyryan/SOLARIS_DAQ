#!/usr/bin/env python3
"""Plot one ch0 capture as ONE timing diagram: both analog signals, with the
4 digital markers overlaid as colour in the same axes.

    python3 agent/plot_capture.py [agent/data/trace_six_1013_1308.txt] [out.png]

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
with `solaris-cli read 0 /ch/N/par/WaveAnalogProbe1` before plotting a new
channel.

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

Two of the four markers are NOT drawn as filled steps and must not be "fixed":
Trigger and EnergyFilterPeaking are 8 ns wide, which is ~1.5 pixels on a 7.7 us
window, so a fill would be invisible. They are vertical lines at the same
scale, and the legend says so. Trigger is BLACK, not red: red is the
EnergyFilter curve, and the trigger is the reference instant every other
number on this plot is measured from, so a neutral colour reads correctly.
"""
import os
import re
import sys

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

DT = 8e-9          # s/sample
ADCTOMV = 0.060944  # mV/count (the board's measured adctovolts)
PAT = re.compile(r"^\s*(\d+)\s+(-?\d+)\s+(-?\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s*$")

# A run this many samples wide (or narrower) cannot be shown as a filled step:
# it is under ~2 pixels at any sane zoom, so it gets a vertical line instead.
NARROW_MAX = 12


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
        if all((b - a) <= NARROW_MAX for a, b in dg[nm]):
            # 8 ns is ~1.5 px: a fill cannot be seen, so mark the instant.
            for a, b in dg[nm]:
                ax.axvline(t[a], color=cc, lw=1.3, alpha=.95)
            ax.plot([], [], color=cc, lw=1.3, label=f"{nm} {widths} [line: only 8 ns wide]")
        else:
            step = (col > 0).astype(float)
            ax.fill_between(t, lo, np.where(step > 0, hi, lo), step="post",
                            color=cc, alpha=.28, lw=0)
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


def main(path, outpath, probes=("ADCInput", "EnergyFilter"), chan=None,
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
        # Zoom to where the action is: a little before arming to past the trapezoid end.
        lo = max(0.0, t[dg[dprobes[1]][0][0]] - 0.6)
        hi = t[dg[dprobes[3]][0][1]] + 3.0
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


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    argv = [a for a in sys.argv[1:] if not a.startswith("--")]
    chan = None
    dprobes = DPROBE_CH0
    probes = ("ADCInput", "EnergyFilter")
    for a in sys.argv[1:]:
        if a.startswith("--probe="):
            probes = tuple(a.split("=", 1)[1].split(","))
            if len(probes) != 2:
                sys.exit("--probe wants two comma-separated names, e.g. --probe=ADCInput,TimeFilter")
    src = argv[0] if argv else os.path.join(here, "data", "trace_six_1013_1308.txt")
    base = os.path.basename(src).replace(".txt", "")
    dst = argv[1] if len(argv) > 1 else os.path.join(
        here, "data", base.replace("trace_", "plot_") + ".png")
    for a in sys.argv[1:]:
        if a.startswith("--ch="):
            chan = "ch" + a.split("=", 1)[1]
        if a.startswith("--dprobe="):
            dprobes = tuple(a.split("=", 1)[1].split(","))
            if len(dprobes) != 4:
                sys.exit("--dprobe wants four comma-separated names in D0..D3 order")
    main(src, dst, probes, chan, dprobes)
