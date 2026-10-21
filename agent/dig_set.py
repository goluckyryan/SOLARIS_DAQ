#!/usr/bin/env python3
"""Set digitizer parameters on the VX2745 through the broker, and prove they landed.

    python3 dig_set.py --ch=0 --set=WaveTriggerSource=TRGIN
    python3 dig_set.py --ch=0 --set=CoincidenceLengthT=1000 --set=CoincidenceMask=ITLB
    python3 dig_set.py --board --set=TrgOutMode=ITLA --set=ITLAMainLogic=AND
    python3 dig_set.py --ch=0 --set=SelfTriggerWidth=1000 --stop-first
    python3 dig_set.py --ch=0 --set=TriggerThr=50 --dry-run

Why this exists instead of typing `write` at the CLI: **`OK` from a write means
ACCEPTED, not SET.** The board clamps and quantizes silently -- `ITLAGateWidth=500`
lands on 504, and a value above a max is refused with an opaque `WriteValue failed`.
So every write here is validated against the device tree first (`agent/data/
devt_vx2745.json`), and read back afterwards; the report prints OLD -> NEW and flags
any value that did not land as asked.

ACQ handling: parameters with `setinrun=false` in the tree (`ITLConnect`, `TrgOutMode`,
`ITLAGateWidth`, `SelfTriggerWidth`, `ChRecordLengthS`, `StartSource`, ...) cannot be
changed while acquiring -- the write is accepted and silently does not take. This tool
REFUSES when ACQ is on and names the parameters that need a stop. `--stop-first` stops,
applies, and restarts in the same invocation so the state is never left ambiguous.

`--set=NAME=VALUE` may be repeated. Channel params need `--ch=N`; `--board` selects the
global `/par/` namespace. Names are case-insensitive.

Nothing here calls `trace`, and nothing here opens a digitizer -- a fresh `open` resets
WaveTriggerSource on all 64 channels (DAQ:agent/AGENT.md §19).
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dig import Session, DigiError, spec, validate, canon          # noqa: E402


def parse_sets(pairs):
    out = []
    for p in pairs:
        if '=' not in p:
            raise DigiError(f"--set needs NAME=VALUE, got '{p}'")
        name, value = p.split('=', 1)
        out.append((name.strip(), value.strip()))
    if not out:
        raise DigiError("nothing to do: pass at least one --set=NAME=VALUE")
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Set VX2745 parameters via the broker (validated + read back)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="Names are case-insensitive. See DAQ:agent/AGENT.md §19 for which sources work.")
    ap.add_argument('--ch', type=int, default=None, help='channel 0..63')
    ap.add_argument('--board', action='store_true', help='target /par/ (global) params')
    ap.add_argument('--set', action='append', default=[], metavar='NAME=VALUE',
                    help='repeatable; e.g. --set=WaveTriggerSource=TRGIN')
    ap.add_argument('--digi', type=int, default=0, help='digitizer index (default 0)')
    ap.add_argument('--stop-first', action='store_true',
                    help='stop ACQ, apply, restart ACQ (leaves it running as found)')
    ap.add_argument('--dry-run', action='store_true',
                    help='validate against the device tree, write nothing')
    ap.add_argument('--force', action='store_true',
                    help='skip the setinrun=false guard (NOT recommended: silent no-op)')
    args = ap.parse_args(argv)

    try:
        sets = parse_sets(args.set)
    except DigiError as e:
        print(f"refused: {e}", file=sys.stderr)
        return 2

    if args.ch is not None and args.board:
        print("refused: --ch and --board are mutually exclusive", file=sys.stderr)
        return 2
    if args.ch is None and not args.board:
        print("refused: say --ch=N or --board", file=sys.stderr)
        return 2
    scope = 'board' if args.board else 'ch'

    # ---- validate everything BEFORE touching hardware ----
    print(f"== validating {len(sets)} write(s) against the device tree ==")
    bad = False
    for name, value in sets:
        try:
            n = canon(name, scope)
            ok, note = validate(n, value, scope)
        except DigiError as e:
            print(f"  {name:26s} UNKNOWN  {e}")
            bad = True
            continue
        flag = 'ok  ' if ok else 'BAD '
        print(f"  {name:26s} = {value:22s} {flag} {note}")
        if not ok:
            bad = True
    if bad:
        print("\nrefused: fix the values above. Nothing was written.", file=sys.stderr)
        return 2
    if args.dry_run:
        print("\ndry run -- nothing written.")
        return 0

    try:
        s = Session(digi=args.digi)
        s.boot()
        was_on = s.acq_on()
        if args.stop_first and was_on:
            print("\n-- stopping ACQ --")
            s.stop()
        elif was_on and any(spec(canon(n, scope), scope)['setinrun'] == 'false'
                            for n, _ in sets):
            names = [canon(n, scope) for n, _ in sets
                     if spec(canon(n, scope), scope)['setinrun'] == 'false']
            print(f"\nrefused: ACQ is running and these need it stopped "
                  f"(setinrun=false):\n  {', '.join(names)}\n"
                  f"re-run with --stop-first", file=sys.stderr)
            return 3

            print("\n== writing (each value read back) ==")
        for name, value in sets:
            n = canon(name, scope)
            shown = name if not name.startswith('/') else n
            ch = None if args.board else args.ch
            old, new, note = s.write(n, value, ch=ch, scope=scope,
                                     check_acq=not args.force, force=args.force)
            mark = '  ' if new == str(value) else '!!'
            print(f"  {mark} {shown:26s} {old:>22s} -> {new:<22s} {note}")

        if args.stop_first and was_on:
            print("\n-- restarting ACQ (was running before) --")
            s.start(fmt=1)
        print(f"\nACQ now: {'ON' if s.acq_on() else 'OFF'}")
    except DigiError as e:
        print(f"\nERROR: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
