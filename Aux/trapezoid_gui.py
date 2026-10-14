#!/usr/bin/env python3
"""Trapezoid filter playground -- offline, drives NO hardware.

The pipeline is fixed and runs in this order, once per recompute:

  1. SIGNAL      v(t) = A(1-e^{-(t-t0)/r}) e^{-(t-t/d}) + B   (per pulse)
  2. DIGITISE    sample at dt, quantise to an integer ADC code over the
                 input range.  t < first sample is the BASELINE B: the
                 signal is assumed to have existed from t = -inf, the
                 digitiser only captured a finite window.
  3. TIME FILTER digital RC-CR^2 (2nd derivative) on the digitised
                 samples.  ARMED   = its rising edge through the threshold.
                 TRIGGER = its falling edge back through ZERO after ARMED.
  4. SHIFT, then SHAPE
                 the digitised signal is shifted right by (TRIGGER - ARMED)
                 and ONLY THEN is the trapezoid applied.  The delay is
                 derived from the time filter, never fitted.
  5. LABEL       trigger, flat-top start and stop, end of the trapezoid,
                 peaking time.

This is a DEMONSTRATION tool, not a comparison harness: it shows what the
filter does to a pulse.  It is not scored against the board here.  The
formula WAS verified against a real VX2745 capture, and the evidence, the
constants and the dead-end models that look right but are not all live in
agent/AGENT.md (sec.11 and sec.13e) -- read those before changing the filter.

Usage:  python3 Aux/trapezoid_gui.py
"""

import sys
import numpy as np
from scipy.signal import lfilter

from PyQt6.QtCore import Qt
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QGridLayout, QGroupBox, QHBoxLayout,
    QDoubleSpinBox, QCheckBox, QLabel, QFrame)
from matplotlib.figure import Figure
from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg as FigureCanvas

DT_DEFAULT = 8.0        # 125 Msps, the VX2745 tick
PRETRIG_SAMPLES = 250   # like ChPreTriggers: a SAMPLE count, scales with dt


def _wants_arg(fn):
    import inspect
    try:
        return len(inspect.signature(fn).parameters) > 0
    except (TypeError, ValueError):
        return False

# ----------------------------------------------------------------- physics ---
def pulse(t, t0, A, r_s, d_s):
    """A(1-e^{-(t-t0)/r})e^{-(t-t0)/d}, zero before t0. All times in s."""
    x = np.clip(t - t0, 0.0, None)
    out = np.zeros_like(t)
    m = t >= t0
    if r_s <= 0:  # instantaneous rise limit
        out[m] = A * np.exp(-x[m] / d_s)
    else:
        out[m] = A * (1.0 - np.exp(-x[m] / r_s)) * np.exp(-x[m] / d_s)
    return out


def delay(v, n):
    """v shifted right by n, with the vacated head filled with ZEROS.

    That zero-fill is a boundary assumption: it says "before this record there
    was no signal". For a baseline of 0 mV that is true. For any non-zero
    pedestal it is FALSE, and it injects a step of -offset at the record start
    -- a spurious transient the real board never sees, because the board has
    been running with its pedestal present long before the trigger. Use
    trapezoid(..., v0=baseline) to warm-start instead of zero-padded history.
    """
    n = int(n)
    if n <= 0:
        return np.zeros_like(v)
    return np.concatenate([np.zeros(min(n, len(v))), v[:max(0, len(v) - n)]])


def trapezoid(v, L, M, tau_pz_s, dt_s, v0=None):
    """The CAEN DPP trapezoid, exactly as derived in
    nukephysik101.wordpress.com/2022-05-11/trapezoid-filter-revisit (paper eqs
    28/30/31, doi:10.1016/0168-9002(94)91011-1).  Two stages:

      1. POLE-ZERO  f[n] = f[n-1] + v[n] - c*v[n-1],   c = exp(-dt/tau_pz)
         Turns the exponential pulse into a FLAT STEP: the difference
         v[n] - c*v[n-1] is zero everywhere except at the rise, so the decay
         is cancelled rather than merely shortened.  THIS is the pole-zero --
         it is not an optional extra, it is stage 1 of the filter, and
         tau_pz is meant to equal the signal decay d.

      2. TRAPEZOID  S[n] = S[n-1] + g[n], with
         g[n] = f[n] - f[n-L-1] - f[n-L-M] + f[n-2L-M-1]
         which is the difference of two running sums of width L+M and L:
         ramps up over L, flat for M, ramps down over L.

    This is the CAEN DPP form, reproduced from the measured VX2745 record
    (agent/AGENT.md sec.11, which also lists the two plausible-looking models that
    are WRONG for this hardware: the 3-stage Jordan "FP" cascade, and a
    pole-zero-free shaper).

    Output is divided by L so the trapezoid peak sits on the input's own mV
    scale.

    Set tau_pz <= 0 to skip stage 1 (pure shaper, no decay cancellation) --
    useful for seeing what the pole-zero actually does.

    v0 = the signal level that existed BEFORE the record started (the pedestal
    / DC offset). The board's filter has been running forever at that level, so
    its history is v0, not 0. Feeding zeros instead is the boundary bug: it is
    equivalent to a step of -v0 at the record start, which the shaper integrates
    into a ramp. Passing v0 makes the pre-trigger region flat at the settled
    pedestal-leak level, and lets the caller subtract a baseline that is known
    analytically rather than measured from a few samples. v0=None keeps the old
    zero-padded behaviour.
    """
    n = len(v)
    W = 0
    if v0 is not None:
        W = 2 * int(L) + int(M) + 2          # >= the filter's full span
        v = np.concatenate([np.full(W, float(v0)), v])
        n = len(v)
    c = np.exp(-dt_s / tau_pz_s) if tau_pz_s and tau_pz_s > 0 else 0.0
    Lp, Mp = int(L), int(M)
    f = np.zeros(n)
    S = np.zeros(n)
    # f[-1] = v[-1] = 0: the same implicit initial state scipy's lfilter uses
    # in the fast path (it starts with zero filter state). The warm block above
    # is what carries the pedestal, not this term -- so leave it at zero here
    # or the two implementations disagree at i = 0.
    v_prev = 0.0
    for i in range(n):
        f[i] = v[i] if not c else (f[i - 1] if i else 0.0) + v[i] - c * v_prev
        v_prev = v[i]
        g = f[i]
        if i >= Lp + 1:          g -= f[i - Lp - 1]
        if i >= Lp + Mp:         g -= f[i - Lp - Mp]
        if i >= 2 * Lp + Mp + 1: g += f[i - 2 * Lp - Mp - 1]
        S[i] = (S[i - 1] if i else 0.0) + g
    S = S / float(L)
    # Same 3-tuple as trapezoid_fast(): ybase is the settled pedestal leak.
    ybase = S[W - 1] if W else 0.0
    return (f[W:] if W else f), (S[W:] if W else S), ybase


def trapezoid_fast(v, L, M, tau_pz_s, dt_s, v0=None):
    """Vectorised equivalent of trapezoid() -- 4000 samples in ~1 ms instead of
    a Python loop, so the sliders stay interactive.  Verified identical to
    trapezoid() to 1e-9 relative over the whole record, with and without v0.

    Returns (f, y, ybase).  ybase is the settled output level the pedestal
    produces (0 when v0 is 0/None), i.e. the baseline the caller should remove
    before reading a peak or a tail.

    ybase is not empirical -- a DC level V0 leaks through as
        ybase = V0 * (1 - exp(-dt/tau_pz)) * (L+1)*(L+M)/L
              ~= V0 * (RiseTimeT + FlatTopT) / PoleZeroT
    because the pole-zero only PARTIALLY cancels DC: what survives is the
    filter's total width over the pole-zero tau. Verified exact to 2e-12 over
    28 (L, M, tau_pz) combinations. The board does the same thing -- see
    agent/AGENT.md sec.13 (2081 mV ADCInput pedestal under a 264 mV EnergyFilter
    baseline in agent/data/real_trap.txt) -- which is why it offers the
    EnergyFilterMinusBaseline output.
    """
    W = 0
    if v0 is not None:
        W = 2 * int(L) + int(M) + 2          # >= the filter's full span
        v = np.concatenate([np.full(W, float(v0)), v])
    if tau_pz_s and tau_pz_s > 0:
        c = np.exp(-dt_s / tau_pz_s)
        f = lfilter([1.0, -c], [1.0, -1.0], v)
    else:
        f = v.copy()
    g = f - delay(f, L + 1) - delay(f, L + M) + delay(f, 2 * L + M + 1)
    y = np.cumsum(g) / float(L)
    ybase = y[W - 1] if W else 0.0
    return (f[W:] if W else f), (y[W:] if W else y), ybase


def landmarks(y, dt_s, base=0.0):
    """The four CORNERS of the trapezoid, found by fitting the ramps and the flat
    top with straight lines and intersecting them -- NOT by a threshold.

    Why not a threshold: RiseTimeT/FlatTopT are ramp DURATIONS, so the quantity
    you want is where the ramp would meet the flat top if both were extended.
    A `>= 97% of peak` window instead reports a value that depends on how rounded
    the top is, which is why it read 1072 ns on hardware programmed to 1000 ns
    and shrank as M grew. The line fits are threshold-free at the corners;
    thresholds are used only to choose the fit windows.

      t0  start of trapezoid        rise line meets the baseline
      t1  end of rise / start of flat top     rise line meets the flat line
      t2  end of flat top / start of fall     flat line meets the fall line
      t3  end of fall               fall line meets the baseline

    Returns a dict; values are nan if the shape has no usable ramps.
    """
    n = len(y)
    out = dict(t0=np.nan, t1=np.nan, t2=np.nan, t3=np.nan, peak=np.nan,
               rise=np.nan, flat=np.nan, fall=np.nan, total=np.nan,
               integ=np.nan, ok=False)
    if n < 16:
        return out
    pk = int(np.argmax(y))
    mx = y[pk]
    if mx - base <= 1e-9:
        return out

    def cr(level, after):
        """First index walking away from the peak where y crosses `level`."""
        i = pk
        if after:
            while i < n - 1 and y[i] > level:
                i += 1
        else:
            while i > 0 and y[i] > level:
                i -= 1
        return i

    def fit(l, r):
        if r - l < 4:
            return None
        xs = np.arange(l, r, dtype=float)
        yy = y[l:r]
        # A flat segment has no slope to fit; guard against a zero-variance window.
        if np.ptp(yy) < 1e-9:
            return [0.0, float(yy.mean())]
        return np.polyfit(xs, yy, 1)

    # flat line from the plateau; ramps fitted between 25% and 80% of the peak,
    # which stays clear of the rounded corner AND of the baseline noise.
    lo, hi = cr(0.95 * mx, False), cr(0.95 * mx, True)
    flat = fit(lo, hi + 1)
    rise = fit(cr(0.25 * mx, False), cr(0.80 * mx, False))
    fall = fit(cr(0.80 * mx, True), cr(0.25 * mx, True))
    if flat is None or rise is None or fall is None:
        return out
    if abs(rise[0]) < 1e-12 or abs(fall[0]) < 1e-12:
        return out

    def inter(a, b):
        d = a[0] - b[0]
        return (b[1] - a[1]) / d if abs(d) > 1e-12 else np.nan

    t0 = inter(rise, [0.0, base])
    t1 = inter(rise, flat)
    t2 = inter(flat, fall)
    t3 = inter(fall, [0.0, base])
    ts = [t0, t1, t2, t3]
    if not all(np.isfinite(ts)) or not (t0 < t1 <= t2 < t3):
        return out
    a0 = max(0, int(round(t0)))
    a3 = min(n, int(round(t3)))
    out.update(t0=t0, t1=t1, t2=t2, t3=t3, peak=mx - base,
               rise=(t1 - t0) * dt_s * 1e9, flat=(t2 - t1) * dt_s * 1e9,
               fall=(t3 - t2) * dt_s * 1e9, total=(t3 - t0) * dt_s * 1e9,
               integ=float(np.trapz(y[a0:a3] - base, dx=dt_s)), ok=True)
    return out


def second_derivative(v, tau_rc_s, tau_cr_s, dt_s, v0=None):
    """The DPP-PHA TIME filter: RC then CR^2, i.e. an approximate 2nd derivative.

        RC   y[n] = a*y[n-1] + (1-a)*x[n],   a = exp(-dt/tau_rc)
        CR   y[n] = b*(y[n-1] + x[n] - x[n-1]),  b = exp(-dt/tau_cr)   (twice)

    Why the firmware uses this for timing and NOT the trapezoid: it is the
    fastest thing that is insensitive to amplitude AND to baseline. A step or a
    pedestal produces a transient that dies out; only a genuine RISE makes it go
    positive and stay there long enough to arm. The two crossings are the two
    hardware markers (both MEASURED on the board, see agent/AGENT.md sec.10/13):

        rising edge through the threshold  -> TimeFilterArmed goes HIGH
        falling edge back through ZERO     -> Trigger

    v0 = the level the signal had BEFORE this record started (the pedestal B).
    The board's filter has been running forever at that level, so its state is
    the STEADY STATE AT v0, not zero. Feeding zeros instead makes the pedestal
    look like a step at the record start and the filter FIRES A FALSE TRIGGER
    there (measured once: offset 500 mV triggered 1632 ns before the pulse).

    This is done ANALYTICALLY, not with a warm-up block, and that matters:
    unlike the trapezoid (whose memory is a finite 2L+M+1 taps, so a warm block
    of that length IS exact), these are IIR stages with INFINITE memory -- no
    finite warm block can ever be exact. A 6*tau block left -2.14 mV of residual
    on a 100 mV pedestal, which at the board's real 2081 mV pedestal is 35.6 mV
    -- right at the 35 mV threshold, i.e. a false trigger. Instead:
      * the RC is seeded with its exact steady state at v0 (zi = a*v0), so on a
        constant pedestal it outputs v0 to the last bit;
      * the two CR stages are DC-BLOCKING, so they are fed the DEVIATION from
        v0 with zero state. That is precisely "the signal existed from
        t = -infinity at level v0": the deviation was 0 forever, so its
        filtered value is exactly 0. Pre-trigger is then flat to 0.0, not
        approximately flat.

    Returns the filter output, in the input's units.
    """
    a = np.exp(-dt_s / tau_rc_s) if tau_rc_s > 0 else 0.0
    base = float(v0) if v0 is not None else 0.0
    if a:
        # zi is the filter's internal state, so it is a*v0 (state), NOT v0.
        y, _ = lfilter([1.0 - a], [1.0, -a], v, zi=np.array([a * base]))
    else:
        y = v.copy()
    b = np.exp(-dt_s / tau_cr_s) if tau_cr_s > 0 else 0.0
    z = y - base                       # deviation from the pre-record level
    for _ in range(2):
        z = lfilter([b, -b], [1.0, -b], z) if b else np.zeros_like(z)
    return z


def _lobe(d2, start):
    """Peak and end of the lobe that begins at `start`.

    The lobe ENDS at the first downward zero crossing after `start`. The peak
    must be searched only WITHIN the lobe: searching to the end of the record
    makes a later pulse's taller lobe get attributed to this trigger, which is
    what silently merged two pulses 8 us apart into one event.
    Returns (peak_index, end_index); end_index is None if d2 never returns to 0.
    """
    z = np.nonzero((d2[start:-1] > 0) & (d2[start + 1:] <= 0))[0]
    end = int(z[0]) + start + 1 if len(z) else len(d2) - 1
    pk = start + int(np.argmax(d2[start:end + 1]))
    return pk, (int(z[0]) + start + 1 if len(z) else None)


def find_trigger(d2, threshold):
    """Locate the two hardware markers on the 2nd-derivative waveform.

    Returns (armed, trigger) sample indices, or None for either.
    armed   = first upward crossing of +threshold
    trigger = the lobe's downward crossing of ZERO -- the board's trigger instant.
    """
    above = np.nonzero(d2 >= threshold)[0]
    if not len(above):
        return None, None
    armed = int(above[0])
    _, trig = _lobe(d2, armed)
    return armed, trig


def find_all_triggers(d2, threshold):
    """All (armed, trigger) pairs -- with piled pulses the filter fires again,
    and seeing WHERE it re-fires is the point of the pile-up presets."""
    arms = np.nonzero((d2[:-1] < threshold) & (d2[1:] >= threshold))[0] + 1
    events = []
    i = 0
    while i < len(arms):
        a = int(arms[i])
        _, t = _lobe(d2, a)
        events.append((a, t))
        stop = t if t is not None else len(d2)
        while i + 1 < len(arms) and arms[i + 1] < stop:
            i += 1          # same lobe, not a new event
        i += 1
    return events



# -------------------------------------------------------------------- GUI ----
class Window(QMainWindow):
    """Two panels: (1) the digitised pulse with the time-filter curve overlaid,
    (2) the trapezoid that the shifted signal produces. Nothing else."""

    def __init__(self):
        super().__init__()
        self.setWindowTitle("Trapezoid playground (offline -- drives no hardware)")

        # --- state defaults (user's spec, 2026-10-13) ---
        # trap: RiseTimeT 1000 ns / FlatTopT 5000 ns.
        # signal: A 800 mV, r 1000 ns, d 50 us, offset B 100 mV.
        self.pulses = [(True, 800.0, 0.0), (False, 800.0, 3000.0),
                       (False, 800.0, 6000.0)]
        self.dt_ns = DT_DEFAULT
        self.r_ns = 1000.0
        self.d_us = 50.0
        self.offset = 100.0
        self.trap_rise_ns = 1000.0
        self.trap_flat_ns = 5000.0
        self.trap_pz_us = 50.0

        root = QWidget()
        lay = QHBoxLayout(root)
        lay.addWidget(self._build_controls())
        self.fig = Figure(figsize=(8.2, 7.0), tight_layout=True)
        self.canvas = FigureCanvas(self.fig)
        self.canvas.setMinimumWidth(760)
        lay.addWidget(self.canvas, stretch=1)
        self.setCentralWidget(root)

        self.ax_in = self.fig.add_subplot(211)
        self.ax_tr = self.fig.add_subplot(212)
        # The 2nd-derivative curve lives on a twin of panel 1 (the user wants it
        # overlaid on the input pulse, not in its own panel). The twin MUST be
        # created ONCE here: calling ax.twinx() inside _recompute makes a NEW
        # overlay axes every time, and the previous one stays in fig.axes with
        # its old curve still drawn -- so changing pre-trigger left a stack of
        # stale 2nd-derivative traces behind (measured: +1 axis and +3 lines per
        # recompute, unbounded). Clearing ax_in does NOT clear a twin.
        self.ax_tf = self.ax_in.twinx()
        self._recompute()

    # ---------------------------------------------------------- controls ---
    def _dsb(self, lo, hi, val, step, dec, suffix, cb=None):
        w = QDoubleSpinBox()
        w.setRange(lo, hi)
        w.setValue(val)
        w.setSingleStep(step)
        w.setDecimals(dec)
        w.setSuffix(suffix)
        w.setKeyboardTracking(False)
        if cb:
            w.valueChanged.connect(cb)
        return w

    def _chk(self, text, val, cb):
        w = QCheckBox(text)
        w.setChecked(val)
        w.stateChanged.connect(lambda state: (cb(), None)[1] if _wants_arg(cb) else cb())
        return w

    def _build_controls(self):
        box = QGroupBox("pulse  /  trapezoid")
        box.setMaximumWidth(340)
        g = QGridLayout(box)
        r = 0

        g.addWidget(QLabel("<b>Sampling</b>"), r, 0, 1, 2); r += 1
        self.eDT = self._dsb(0.1, 1000.0, self.dt_ns, 1, 3, " ns/sample", self._recompute)
        g.addWidget(QLabel("sample period"), r, 0); g.addWidget(self.eDT, r, 1); r += 1
        self.eN = self._dsb(16, 200000, 4000, 100, 0, " samples", self._recompute)
        g.addWidget(QLabel("record length"), r, 0); g.addWidget(self.eN, r, 1); r += 1
        self.ePre = self._dsb(0, 100000, PRETRIG_SAMPLES, 50, 0, " samples", self._recompute)
        g.addWidget(QLabel("pre-trigger"), r, 0); g.addWidget(self.ePre, r, 1); r += 1

        g.addWidget(QLabel("<b>Digitiser</b>"), r, 0, 1, 2); r += 1
        self.eBits = self._dsb(1, 32, 16, 1, 0, " bits", self._recompute)
        g.addWidget(QLabel("ADC resolution"), r, 0); g.addWidget(self.eBits, r, 1); r += 1
        self.eFS = self._dsb(0.002, 20, 4.0, 0.5, 3, " V full scale", self._recompute)
        g.addWidget(QLabel("input range"), r, 0); g.addWidget(self.eFS, r, 1); r += 1
        self.eEBits = self._dsb(2, 16, 16, 1, 0, " bits", self._recompute)
        g.addWidget(QLabel("energy word"), r, 0); g.addWidget(self.eEBits, r, 1); r += 1

        self._sep(g, r); r += 1
        g.addWidget(QLabel("<b>Input pulse</b>  A(1-e<super>-t/r</super>)e<super>-t/d</super>+B"),
                    r, 0, 1, 2); r += 1
        self.eA = self._dsb(0, 4000, self.pulses[0][1], 10, 1, " mV", self._recompute)
        g.addWidget(QLabel("A (pulse 1)"), r, 0); g.addWidget(self.eA, r, 1); r += 1
        self.eR = self._dsb(0, 20000, self.r_ns, 50, 1, " ns", self._recompute)
        g.addWidget(QLabel("r  (rise)"), r, 0); g.addWidget(self.eR, r, 1); r += 1
        self.eD = self._dsb(0.1, 1000, self.d_us, 1, 2, " µs", self._pz_matched)
        g.addWidget(QLabel("d  (decay)"), r, 0); g.addWidget(self.eD, r, 1); r += 1
        self.eO = self._dsb(-1000, 3000, self.offset, 10, 1, " mV", self._recompute)
        g.addWidget(QLabel("offset B"), r, 0); g.addWidget(self.eO, r, 1); r += 1

        self._sep(g, r); r += 1
        g.addWidget(QLabel("<b>Pile-up</b>  (extra pulses, same r/d)"), r, 0, 1, 2); r += 1
        self.pchk, self.pA, self.pT = [], [], []
        for i in (1, 2):
            c = self._chk(f"pulse {i+1}", self.pulses[i][0], self._recompute)
            a = self._dsb(0, 4000, self.pulses[i][1], 10, 1, " mV", self._recompute)
            t = self._dsb(16, 30000, self.pulses[i][2], 100, 0, " ns", self._recompute)
            g.addWidget(c, r, 0); g.addWidget(a, r, 1); self.pA.append(a); r += 1
            g.addWidget(QLabel("  dt after pulse 1"), r, 0); g.addWidget(t, r, 1)
            self.pT.append(t); self.pchk.append(c); r += 1

        self._sep(g, r); r += 1
        g.addWidget(QLabel("<b>Trapezoid</b>  (-L, M, +L running sums)"), r, 0, 1, 2); r += 1
        self.tR = self._dsb(8, 1e8, self.trap_rise_ns, 100, 0, " ns", self._recompute)
        g.addWidget(QLabel("RiseTimeT  (L)"), r, 0); g.addWidget(self.tR, r, 1); r += 1
        self.tF = self._dsb(8, 1e8, self.trap_flat_ns, 50, 0, " ns", self._recompute)
        g.addWidget(QLabel("FlatTopT  (M)"), r, 0); g.addWidget(self.tF, r, 1); r += 1
        self.tP = self._dsb(0.001, 1e5, self.trap_pz_us, 1, 3, " µs", self._recompute)
        g.addWidget(QLabel("PoleZeroT"), r, 0); g.addWidget(self.tP, r, 1); r += 1
        self.chkPzMatch = self._chk("PoleZeroT = d (match)", False, self._pz_matched)
        g.addWidget(self.chkPzMatch, r, 0, 1, 2); r += 1

        self._sep(g, r); r += 1
        g.addWidget(QLabel("<b>Time filter</b>  (RC-CR^2, 2nd derivative)"), r, 0, 1, 2); r += 1
        self.eTRC = self._dsb(1, 100000, 140.0, 10, 1, " ns", self._recompute)
        g.addWidget(QLabel("tau RC"), r, 0); g.addWidget(self.eTRC, r, 1); r += 1
        self.eTCR = self._dsb(1, 100000, 220.0, 10, 1, " ns", self._recompute)
        g.addWidget(QLabel("tau CR"), r, 0); g.addWidget(self.eTCR, r, 1); r += 1
        self.eThr = self._dsb(0.01, 5000, 35.0, 5, 1, " mV", self._recompute)
        g.addWidget(QLabel("threshold"), r, 0); g.addWidget(self.eThr, r, 1); r += 1
        self.ePeak = self._dsb(0, 1e8, 5520.0, 100, 0, " ns", self._recompute)
        g.addWidget(QLabel("peaking time (after trigger)"), r, 0)
        g.addWidget(self.ePeak, r, 1); r += 1

        self.chkDots = self._chk("show samples as dots", True, self._recompute)
        g.addWidget(self.chkDots, r, 0, 1, 2); r += 1

        self.info = QLabel("")
        self.info.setWordWrap(True)
        self.info.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
        g.addWidget(self.info, r, 0, 1, 2)
        g.setRowStretch(r + 1, 1)
        return box

    @staticmethod
    def _sep(g, r):
        line = QFrame(); line.setFrameShape(QFrame.Shape.HLine)
        g.addWidget(line, r, 0, 1, 2)

    def _pz_matched(self):
        """When checked, PoleZeroT is slaved to the signal decay d."""
        on = self.chkPzMatch.isChecked()
        self.tP.setEnabled(not on)
        if on:
            self.tP.setValue(self.eD.value())
        self._recompute()

    # ------------------------------------------------------------ pipeline ---
    def _recompute(self):
        dt = self.eDT.value()                       # ns / sample
        dt_s = dt * 1e-9
        L = max(1, int(round(self.tR.value() / dt)))
        M = max(1, int(round(self.tF.value() / dt)))
        n = int(self.eN.value())
        pre = int(self.ePre.value())
        # t = 0 at the pulse ARRIVAL (sample `pre`). The TRIGGER is elsewhere --
        # step 3 finds it -- so nothing below assumes the two coincide.
        t = (np.arange(n) - pre) * dt_s

        # ---- 1. SIGNAL: A(1-e^-t/r)e^-t/d + B, defined from t = -infinity ----
        B = self.eO.value()
        pulses = [(True, self.eA.value(), 0.0)] + \
                 [(c.isChecked(), a.value(), tt.value())
                  for c, a, tt in zip(self.pchk, self.pA, self.pT)]
        x = np.full(n, B)
        for en, A, t_ns in pulses:
            if not en or A == 0:
                continue
            x = x + pulse(t, t_ns * 1e-9, A * 1e-3,
                          max(self.eR.value(), 1e-6) * 1e-9, self.eD.value() * 1e-6) * 1e3

        # ---- 2. DIGITISE: integer codes over the input range ----
        lsb = self.eFS.value() / (2.0 ** self.eBits.value())
        x = np.round(x / lsb) * lsb                                  # mV

        # ---- 3. TIME FILTER: RC-CR^2 -> ARMED, then TRIGGER ----
        # ARMED   = 2nd derivative's rising edge through the threshold
        # TRIGGER = its falling edge back through ZERO, after ARMED
        d2 = second_derivative(x, self.eTRC.value() * 1e-9,
                               self.eTCR.value() * 1e-9, dt_s, v0=B)
        events = find_all_triggers(d2, self.eThr.value())
        armed, trig = events[0] if events else (None, None)

        # ---- 4. SHIFT by (TRIGGER - ARMED), THEN the trapezoid ----
        # The delay is DERIVED here from the time filter; it is not fitted and
        # not a free parameter. v0 = B warms the filter history, which is how
        # "t < first sample == baseline" is honoured (a zero-padded history
        # would read the pedestal as a step and fire a false trigger).
        shift = float(trig - armed) if trig is not None and armed is not None else 0.0
        xs = np.interp(np.arange(n) - shift, np.arange(n), x,
                       left=B, right=float(x[-1]))
        _, y, _ybase = trapezoid_fast(xs, L, M, self.tP.value() * 1e-6, dt_s, v0=B)

        # ---- 5. GEOMETRY, referenced to the trigger ----
        # The trapezoid is applied to the SHIFTED signal, so its ramp starts
        # where the shifted pulse arrives: t0 = pre + shift.
        if trig is not None:
            t0 = pre + shift
            geo = dict(t0=t0, t1=t0 + L, t2=t0 + L + M, t3=t0 + 2 * L + M,
                       ok=True)
        else:
            geo = dict(ok=False)

        pk = int(np.argmax(y))

        # ---------------- readouts ----------------
        lines = [
            f"L={L} M={M} samp   {dt:g} ns ({1000.0/dt:.1f} MS/s)   "
            f"{self.eBits.value():.0f}-bit / {self.eFS.value():g} V = "
            f"{lsb*1e3:.3e} mV/LSB",
        ]
        if trig is None:
            lines.append("NO TRIGGER: the 2nd derivative never reached the "
                         "threshold. Lower it or raise A.")
        else:
            aft = lambda i: (i - trig) * dt          # ns after the TRIGGER
            lines += [
                f"\u23f1 ARMED   sample {armed}   = arrival {(armed-pre)*dt:+7.0f} ns"
                f"   = trigger {aft(armed):+7.0f} ns",
                f"\u23f1 TRIGGER sample {trig}   = arrival {(trig-pre)*dt:+7.0f} ns"
                f"   ({len(events)} event(s) fired)",
                f"   SHIFT applied = TRIGGER - ARMED = {trig-armed} samp = "
                f"{(trig-armed)*dt:.0f} ns",
                f"   t0 trap start    {aft(geo['t0']):+8.0f} ns after trigger",
                f"   t1 flat top START {aft(geo['t1']):+8.0f} ns after trigger"
                f"   (= t0 + L, L = {L*dt:.0f} ns)"
                f"   [rule says +{L*dt:.0f}; the {aft(geo['t1'])-L*dt:+.0f} ns gap is"
                f"    ARMED sitting {(armed-pre)*dt:+.0f} ns off the arrival]",
                f"   t2 flat top STOP  {aft(geo['t2']):+8.0f} ns after trigger"
                f"   (= t1 + M, M = {M*dt:.0f} ns)",
                f"   t3 trap END       {aft(geo['t3']):+8.0f} ns after trigger"
                f"   (= t2 + L; total 2L+M = {(2*L+M)*dt:.0f} ns)",
                f"   PEAKING           {self.ePeak.value():+8.0f} ns after trigger"
                f"   (where the DPP samples the energy)",
                f"   trapezoid peak {y[pk]:.1f} mV at trigger {aft(pk):+.0f} ns",
            ]
        self.info.setText("\n".join(lines))

        # ---------------- draw ----------------
        ms = t * 1e9
        # X RANGE = the digitised samples' own extent, plus 10 % each side. Not
        # autoscaled: the analog curve and the trapezoid tail would otherwise
        # decide the window, and a long record would push the event into a sliver.
        # The record (N, pre-trigger) is the user's control over how much
        # baseline is on screen, so the axis follows it exactly.
        xlo, xhi = ms[0] - 0.1 * (ms[-1] - ms[0]), ms[-1] + 0.1 * (ms[-1] - ms[0])
        # ALL THREE axes must be cleared -- ax_tf is a separate axes object and
        # ax_in.clear() leaves it untouched.
        self.ax_in.clear()
        self.ax_tf.clear()
        self.ax_tr.clear()

        # panel 1: the digitised pulse, with the time-filter curve overlaid on
        # its own axis (the user asked for these to share one plot).
        ax = self.ax_in
        SUB = 8
        tf = (np.arange(n * SUB) / SUB - pre) * dt_s
        xa = np.full(n * SUB, B)
        for en, A, t_ns in pulses:
            if not en or A == 0:
                continue
            xa = xa + pulse(tf, t_ns * 1e-9, A * 1e-3,
                            max(self.eR.value(), 1e-6) * 1e-9,
                            self.eD.value() * 1e-6) * 1e3
        ax.plot(tf * 1e9, xa, color="#7fb3ff", lw=1.0, alpha=.9,
                label="analog signal")
        if self.chkDots.isChecked():
            ax.plot(ms, x, ".", ms=3, color="#333", label="digitised samples")
        else:
            ax.plot(ms, x, color="#333", lw=1.2, label="digitised samples")
        ax2 = self.ax_tf
        ax2.plot(ms, d2, color="#9467bd", lw=1.1,
                 label="RC-CR$^2$ (2nd deriv)")
        ax2.axhline(self.eThr.value(), color="#9467bd", ls="--", lw=.8, alpha=.7)
        ax2.axhline(0, color="#9467bd", lw=.6, alpha=.4)
        ax2.set_ylabel("2nd deriv [mV]", color="#9467bd", fontsize=8)
        ax2.tick_params(axis="y", labelcolor="#9467bd", labelsize=8)
        if armed is not None:
            ax.axvline(ms[armed], color="#9467bd", ls=":", lw=1.2, label="ARMED")
        if trig is not None:
            ax.axvline(ms[trig], color="#d62728", ls="-", lw=1.2, label="TRIGGER")
        ax.axvline(0, color="g", ls=":", lw=1, label="arrival t=0")
        ax.set_xlim(xlo, xhi)
        ax.set_ylabel("mV")
        ax.set_title("1. signal -> digitise -> RC-CR$^2$ gives ARMED and TRIGGER")
        ax.legend(fontsize=7, loc="upper left")
        ax2.legend(fontsize=7, loc="upper right")
        ax.grid(alpha=.3)

        # panel 2: the trapezoid of the SHIFTED signal, with every timing label.
        ax = self.ax_tr
        ax.plot(ms, y, color="#1f77b4", lw=1.6, label="trapezoid")
        if trig is not None:
            ax.axvline(ms[trig], color="#d62728", ls="-", lw=1.2, label="TRIGGER")
            for key, col, lab in (("t0", "#2ca02c", "t0 start"),
                                  ("t1", "#ff7f0e", "t1 flat start"),
                                  ("t2", "#ff7f0e", "t2 flat stop"),
                                  ("t3", "#8c564b", "t3 trap end")):
                i = geo[key]
                if 0 <= i < n:
                    ax.axvline(ms[0] + i * dt, color=col, ls="--", lw=.9,
                               alpha=.85, label=lab)
            pi = min(max(int(round(trig + self.ePeak.value() / dt)), 0), n - 1)
            ax.axvline(ms[pi], color="#e377c2", ls="-", lw=1.3, alpha=.95,
                       label="PEAKING")
        ax.axhline(0, color="k", lw=.6, alpha=.5)
        ax.set_xlim(xlo, xhi)
        ax.set_xlabel("ns relative to pulse arrival")
        ax.set_ylabel("mV")
        ax.set_title("4/5. shift by (TRIGGER - ARMED), then trapezoid")
        ax.legend(fontsize=7, loc="upper left")
        ax.grid(alpha=.3)

        self.canvas.draw_idle()


def main():
    app = QApplication(sys.argv)
    w = Window()
    w.resize(1400, 900)
    w.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
