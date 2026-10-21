#!/usr/bin/env python3
"""Read digitizer parameters back, LABELLED, in one call.

    python3 dig_status.py                       # trigger config on ch0 + ch52 + globals
    python3 dig_status.py --ch=0 --what=trigger
    python3 dig_status.py --ch=52 --what=shape
    python3 dig_status.py --counters            # rates, refreshed properly
    python3 dig_status.py --param=WaveTriggerSource --ch=0
    python3 dig_status.py --list ch             # every channel param the board has
    python3 dig_status.py --diff                # board vs factory defaults

This exists because reading a batch of parameters through `solaris-cli` by hand returns
one bare value per line with nothing naming it, and you map them back by position. That
is how `TrgOutMode` and `GlobalTriggerSource` got transcribed as `Disabled`/`TrgIn` when
they were actually `ITLA`/`SwTrg`. Every read here goes out with a marker line in front
of it and comes back keyed by name, so position never matters.

Counters: the group (`ChWaveCnt`, `ChTriggerCnt`, `ChSavedEventCnt`, `ChDeadtimeMonitor`)
is only refreshed when `ChRealtimeMonitor` is read, and `ChRealtimeMonitor` is itself a
free-running clock in ns. So `--counters` reads monitor-first, waits, reads again, and
divides the deltas by the monitor delta. A single read tells you nothing -- outside a run
they all read 0, which is NOT evidence of absence.

Nothing here writes. `trace` is not offered at all: it disables all 64 channels and
forces WaveTriggerSource=ChSelfTrigger, destroying the config under test (§19).
"""

import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dig import (Session, DigiError, meta, COUNTERS, TRIGGER_CH, TRIGGER_BOARD,   # noqa: E402
                 SHAPE_CH)

WHAT = {
    'trigger': TRIGGER_CH,
    'shape': SHAPE_CH,
    'counters': COUNTERS,
}


def show(title, pairs, width=28, diff=False, scope='ch'):
    print(f"--- {title} ---")
    from dig import meta
    m = meta()[scope]
    for k, v in pairs.items():
        star = ''
        if diff:
            s = m.get(k.lower())
            if s and s['default'] is not None and str(s['default']) != str(v):
                star = f"   <- default was {s['default']}"
        print(f"  {k:<{width}} {v}{star}")


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Read VX2745 parameters via the broker (labelled, never positional)",
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--ch', default='0,52',
                    help='channel(s): "0", "0,52", "all" (default 0,52)')
    ap.add_argument('--what', default='trigger', choices=sorted(WHAT) + ['all'],
                    help='which parameter group (default trigger)')
    ap.add_argument('--param', action='append', default=[],
                    metavar='NAME', help='read just these (repeatable); overrides --what')
    ap.add_argument('--board', action='store_true',
                    help='globals ONLY: skip the per-channel blocks')
    ap.add_argument('--no-board', action='store_true', help='skip the global block')
    ap.add_argument('--counters', action='store_true',
                    help='sample rates over an interval (monitor-first, then difference)')
    ap.add_argument('--interval', type=float, default=6.0, help='seconds (default 6)')
    ap.add_argument('--list', choices=['ch', 'board'], metavar='SCOPE',
                    help='dump every parameter name + description from the device tree')
    ap.add_argument('--diff', action='store_true',
                    help='show only parameters that differ from the factory default')
    ap.add_argument('--digi', type=int, default=0)
    args = ap.parse_args(argv)

    if args.list:
        m = meta()[args.list]
        print("# SNAPSHOT from agent/data/devt_vx2745.json (taken 2026-10-14) -- the"
              " 'value' column is"
              "\n# the board's state AT THAT TIME, not live. Read live values with"
              " --param / --what.")
        for k in sorted(m, key=lambda x: m[x]['name']):
            s = m[k]
            av = (' [' + ', '.join(s['allowed']) + ']') if s['allowed'] else \
                (f" [{s['min']}..{s['max']} step {s['inc']}]" if s['max'] else '')
            print(f"{s['name']:<30s} {str(s['value']):>12s}  "
                  f"{'RO' if s['access'] == 'READ_ONLY' else 'RW'}"
                  f"{'  !stop-ACQ' if s['setinrun'] == 'false' else ''}"
                  f"{' *' if s['default'] is not None and str(s['default']) != str(s['value']) else ''}"
                  f"{av}")
            if s['desc']:
                print(f"{'':30s}    {s['desc'][:96]}")
        print("\n'*' = snapshot value differs from the factory default."
              "\n'!stop-ACQ' = setinrun=false, needs acquisition stopped to write."
              "\nREAD_ONLY rows carry the snapshot reading and are meaningless as state --"
              "\n  e.g. selftrgrate shows whatever it was on 2026-10-14. Read it live.")
        return 0

    chans = (list(range(64)) if args.ch.strip().lower() == 'all'
             else [int(x) for x in args.ch.split(',') if x.strip() != ''])
    if args.param:
        names = args.param
    elif args.what == 'all':
        names = TRIGGER_CH + SHAPE_CH
    else:
        names = WHAT[args.what]

    try:
        s = Session(digi=args.digi)
        rows = s.boot()
        conn, sn, model, fpga, nch = rows[s.digi]
        print(f"broker   : alive | digitizer {s.digi}: SN {sn} {model} {fpga} {nch} ch"
              f" | acq {'ON' if s.acq_on() else 'OFF'}")

        if args.counters:
            print(f"\n--- counters over {args.interval:g} s ---"
                  "\n  (ChRealtimeMonitor is read FIRST each round: it is the only thing"
                  "\n   that refreshes the counter group. It is also a free-running ns"
                  "\n   clock, so the monitor DELTA is the true elapsed time.)")
            keys = [(n, c) for c in chans for n in COUNTERS]
            t0 = s.reads(keys)
            time.sleep(args.interval)
            t1 = s.reads(keys)
            frozen = all(
                int(t1[('ChRealtimeMonitor', c)]) == int(t0[('ChRealtimeMonitor', c)])
                for c in chans)
            if frozen:
                print("\n  ChRealtimeMonitor did NOT advance over the interval, so elapsed"
                      "\n  time is unknown and every counter reads 0. That is NOT evidence"
                      "\n  of absence: the counter group only moves while ACQ is RUNNING."
                      "\n  Measure with: dig_run.py --seconds 10 --ch=0,52"
                      "\n  (it starts ACQ, samples around the run, and stops). Raw second read:")
                for c in chans:
                    print(f"    ch{c:<3d} " + '  '.join(
                        f'{n}={t1[(n, c)]}' for n in COUNTERS))
                return 0
            hdr = f"  {'ch':>4s} {'Hz(own)':>9s} {'Wave/s':>9s} {'Trig/s':>9s} {'Saved/s':>9s} {'DT%':>6s}  {'raw Wave':>9s}"
            print(hdr)
            print('  ' + '-' * (len(hdr) - 2))
            for c in chans:
                dt = int(t1[('ChRealtimeMonitor', c)]) - int(t0[('ChRealtimeMonitor', c)])
                dt_s = dt / 1e9
                def rate(n):
                    """Cumulative counters -> per-second. ChRealtimeMonitor steps by
                    524288 ns, so dt is quantised to ~0.52 ms -- irrelevant at 6 s."""
                    try:
                        return (int(t1[(n, c)]) - int(t0[(n, c)])) / dt_s
                    except (ValueError, KeyError, ZeroDivisionError):
                        return float('nan')
                # SelfTrgRate is a per-second READOUT ("triggers in the previous
                # second", reset every second) -- read it, NEVER difference it.
                try:
                    own = float(t1[('SelfTrgRate', c)])
                except (ValueError, KeyError):
                    own = float('nan')
                wv, tg, sv = rate('ChWaveCnt'), rate('ChTriggerCnt'), rate('ChSavedEventCnt')
                try:
                    d0 = int(t0[('ChDeadtimeMonitor', c)]); d1 = int(t1[('ChDeadtimeMonitor', c)])
                    dtp = 100.0 * (d1 - d0) / dt if dt else 0.0
                except (ValueError, KeyError):
                    dtp = float('nan')
                print(f"  {c:>4d} {own:>9.1f} {wv:>9.1f} {tg:>9.1f} {sv:>9.1f} "
                      f"{dtp:>6.1f}  {t1[('ChWaveCnt', c)]:>9s}")
            print("\n  Hz(own)  = SelfTrgRate: the channel's INPUT pulse population, regardless"
                  "\n             of WaveTriggerSource. Use it to attribute a rate to the emulator."
                  "\n  Wave/s   = ChWaveCnt  <- WaveTriggerSource.  The trigger-routing observable."
                  "\n  Trig/s   = ChTriggerCnt <- EventTriggerSource. BLIND to WaveTriggerSource;"
                  "\n             it reports success for broken configs. Never use it as proof."
                  "\n  Saved/s  = ChSavedEventCnt <- EventTriggerSource (time-energy events)."
                  "\n  DT%      = ChDeadtimeMonitor delta / elapsed.")
            return 0

        # parameter dump. Keys are the (NAME, ch) tuples we asked for -- a bare
        # name would collide across channels, which is exactly the bug that makes
        # positional reads unsafe.
        if not args.board:
            req = [(n, c) for c in chans for n in names]
            vals = s.reads(req)
            for c in chans:
                show(f'ch{c}', {n: vals[(n, c)] for n in names},
                     diff=args.diff, scope='ch')
        if not args.no_board:
            bn = names if args.param else TRIGGER_BOARD
            show('global /par/', s.reads(bn, scope='board'),
                 diff=args.diff, scope='board')
    except DigiError as e:
        print(f"\nERROR: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
