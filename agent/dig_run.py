#!/usr/bin/env python3
"""Run the digitizer: start ACQ, hold, sample counters, stop, report rates.

    python3 dig_run.py --seconds 10 --ch=0,52
    python3 dig_run.py --seconds 30 --save=agent/data/tf/planA
    python3 dig_run.py --seconds 10 --ch=0,52 --format=1
    python3 dig_run.py --start-only            # leave it running
    python3 dig_run.py --stop                  # and nothing else

One call replaces the `printf ... | ./solaris-cli` heredocs that every measurement here
used to need, and it enforces three things that were each got wrong at least once:

* **format 1 (OneTrace), not 0 (ALL).** The broker has died twice on `start 0 0` -- ALL
  means 2 analog + 4 digital probes on 64 channels. It still accepts TCP after dying, so
  the symptom is empty responses and an empty `list`, not a refused connection.
* **counters are sampled around the run, not after it.** `ChWaveCnt`/`ChTriggerCnt`/
  `ChSavedEventCnt` only refresh when `ChRealtimeMonitor` is read, and that monitor is a
  free-running ns clock -- so the monitor DELTA is the true elapsed time. Outside a run
  the monitor is frozen and everything reads 0, which is NOT evidence of absence.
* **`SelfTrgRate` is read, never differenced.** It is a READ_ONLY per-second readout
  ("triggers in the previous second, reset every second") and it is live even with ACQ
  stopped. It reports the channel's INPUT pulse population regardless of
  WaveTriggerSource -- which makes it the right column for attributing a rate to the
  emulator, and the wrong one for asking "did the routing work". For that, use Wave/s.

The two rate columns are deliberately side by side: Wave/s follows
`WaveTriggerSource`, Hz(own) follows the input. When they disagree, the disagreement is
the answer.
"""

import argparse
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dig import Session, DigiError, COUNTERS                                    # noqa: E402


def sample(s, chans):
    keys = [(n, c) for c in chans for n in COUNTERS]
    return s.reads(keys)


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Start/stop ACQ and measure rates over the interval",
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--seconds', type=float, default=10.0, help='run length (default 10)')
    ap.add_argument('--ch', default='0,52', help='channel(s) to report: "0,52" or "all"')
    ap.add_argument('--format', type=int, default=1, choices=(0, 1, 2),
                    help='data format 1=OneTrace (default, SAFE). '
                         '0=ALL has killed the broker twice. 2=NoTrace.')
    ap.add_argument('--save', metavar='PATH', default=None,
                    help='broker-side save; the CLI appends _<digi>_<sn>_<n>.sol')
    ap.add_argument('--start-only', action='store_true', help='do not stop')
    ap.add_argument('--stop', action='store_true', help='just stop and exit')
    ap.add_argument('--no-counters', action='store_true')
    ap.add_argument('--digi', type=int, default=0)
    args = ap.parse_args(argv)

    chans = (list(range(64)) if args.ch.strip().lower() == 'all'
             else [int(x) for x in args.ch.split(',') if x.strip() != ''])

    try:
        s = Session(digi=args.digi)
        s.boot()
        if args.stop:
            s.stop()
            print("ACQ stopped.")
            return 0

        if s.acq_on():
            raise DigiError(
                "ACQ is already running. Stop it first (dig_run.py --stop) -- starting a\n"
                "  second run on top of one makes the counter deltas meaningless.")

        # The save path is opened BY THE BROKER, so a relative path resolves against
        # the BROKER's cwd, not ours -- and if that directory does not exist, fopen
        # fails inside Digitizer2Gen::OpenOutFile, prints to the broker's stdout, and
        # the run proceeds with no file at all. StartACQ still returns RSP_OK, so
        # nothing on this side can see the failure. Measured 2026-10-20: a 60 s run
        # with --save=data/tf/x produced 10 Hz of counters and zero bytes on disk.
        if args.save and not os.path.isabs(args.save):
            cwd = None
            try:
                pid = subprocess.run(['pgrep','-x','solaris-broker'],
                                     capture_output=True, text=True, timeout=10).stdout.split()
                if pid:
                    cwd = os.readlink(f'/proc/{pid[0]}/cwd')
            except Exception:
                pass
            raise DigiError(
                f"--save must be an ABSOLUTE path (got {args.save!r}).\n"
                "  The broker opens the file itself, so a relative path is resolved against"
                " ITS cwd\n"
                + (f"  (currently {cwd}) " if cwd else "")
                + "and a missing directory fails silently: the run completes, the counters"
                " climb,\n  and no file is written. StartACQ still answers OK.")
        if args.format == 0:
            print("  WARNING: format 0 (ALL) has killed the broker twice this session."
                  " Consider --format=1.")
        s.start(fmt=args.format, save=args.save)

        t0 = None
        if not args.no_counters:
            t0 = sample(s, chans)
        time.sleep(args.seconds)
        t1 = sample(s, chans) if not args.no_counters else None

        if not args.start_only:
            s.stop()

        if t0 is not None:
            print(f"\n  {'ch':>4s} {'Hz(own)':>9s} {'Wave/s':>9s} {'Trig/s':>9s}"
                  f" {'Saved/s':>9s} {'DT%':>6s} {'elapsed':>9s}")
            print('  ' + '-' * 62)
            dts = []
            for c in chans:
                dt = int(t1[('ChRealtimeMonitor', c)]) - int(t0[('ChRealtimeMonitor', c)])
                dt_s = dt / 1e9
                def rate(n, c=c):
                    try:
                        return (int(t1[(n, c)]) - int(t0[(n, c)])) / dt_s
                    except (ValueError, KeyError, ZeroDivisionError):
                        return float('nan')
                try:
                    own = float(t1[('SelfTrgRate', c)])
                except (ValueError, KeyError):
                    own = float('nan')
                try:
                    dtp = 100.0 * (int(t1[('ChDeadtimeMonitor', c)]) -
                                   int(t0[('ChDeadtimeMonitor', c)])) / dt
                except (ValueError, KeyError, ZeroDivisionError):
                    dtp = float('nan')
                dts.append(dt_s)
                print(f"  {c:>4d} {own:>9.1f} {rate('ChWaveCnt'):>9.1f}"
                      f" {rate('ChTriggerCnt'):>9.1f} {rate('ChSavedEventCnt'):>9.1f}"
                      f" {dtp:>6.1f} {dt_s:>8.3f}s")
            print("\n  Hz(own) = SelfTrgRate, the channel's INPUT population (emulator side;"
                  "\n            ignores WaveTriggerSource; live even with ACQ off)."
                  "\n  Wave/s  = ChWaveCnt <- WaveTriggerSource.  THIS is routing/coincidence."
                  "\n  Trig/s  = ChTriggerCnt <- EventTriggerSource. BLIND to WaveTriggerSource."
                  "\n  Elapsed = ChRealtimeMonitor delta per channel (shown above);"
                  f" run was requested at {args.seconds:g} s wall.")
            print("\n  NOTE the monitor steps in 524288 ns quanta, so a run shorter than"
                  "\n  ~0.5 s x (a few samples) gives a coarse elapsed time. 6 s or more"
                  "\n  is the minimum for a rate you intend to quote.")
        print(f"\nACQ now: {'ON' if s.acq_on() else 'OFF'}")
    except DigiError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("\ninterrupted -- stopping ACQ", file=sys.stderr)
        try:
            Session(digi=args.digi).stop()
        except Exception as e:
            print(f"  could not stop cleanly: {e}\n  run: dig_run.py --stop", file=sys.stderr)
        return 130
    return 0


if __name__ == '__main__':
    sys.exit(main())
