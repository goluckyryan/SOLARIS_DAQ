#!/usr/bin/env python3
"""Apply a named trigger ROUTING configuration to dig:ch0 / dig:ch52, and verify it landed.

    python3 dig_trigger.py --show                     # what is live now
    python3 dig_trigger.py --apply=cable-52-to-0      # the working path (ch52 -> ch0)
    python3 dig_trigger.py --apply=planA-and          # SelfTriggerWidth as the AND window
    python3 dig_trigger.py --apply=planB-dual         # ITLA out / ITLB partner back
    python3 dig_trigger.py --apply=off                # both channels self-triggered
    python3 dig_trigger.py --apply=planA-and --width=1000
    python3 dig_trigger.py --apply=planA-and --dry-run

The point is ordering and verification, not brevity. A routing config spans both the
board-global `/par/` namespace and two channels, and the parameters are split across
`setinrun=true` (writable live) and `setinrun=false` (needs ACQ stopped) -- so the naive
sequence of writes half-applies silently and the board then reports a rate that looks
like a result. `dig.py:write()` refuses that, and every preset here declares the stop
it needs. Each write is read back and compared; the summary line says which values did
not land as asked.

Presets encode what agent/trigger_test_plan.md PROVED, including the dead ends:
`internal-itla` is offered so the retraction can be re-demonstrated, not because it works.
See DAQ:agent/AGENT.md §19 and the plan's R1-R8 before believing any rate.

Nothing here calls `trace` (it disables all 64 channels and forces ChSelfTrigger) and
nothing here `open`s a digitizer (a fresh open resets WaveTriggerSource on all 64).
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dig import Session, DigiError, canon, spec, TRIGGER_CH, TRIGGER_BOARD          # noqa: E402

# A preset is {'note': str, 'board': {param: value}, 'ch0': {...}, 'ch52': {...}}
# Values of None mean "leave whatever is there". Width-style knobs are placeholders
# filled in from --width where the preset is parameterised.

PRESETS = {
    'off': {
        'note': 'Positive control. Both channels self-triggered; cable and ITL logic idle.\n'
                '        Expect ch0 Wave/s == its own input (~15 Hz with emu ch0=5 + ch2=10)\n'
                '        and ch52 Wave/s == ~60 Hz. Any other numbers mean the bench is not\n'
                '        what this file thinks it is.',
        'board': {'TrgOutMode': 'Disabled', 'ITLAMainLogic': 'OR',
                  'ITLAGateWidth': 0, 'GlobalTriggerSource': 'TrgIn'},
        'ch0': {'WaveTriggerSource': 'ChSelfTrigger',
                'EventTriggerSource': 'ChSelfTrigger',
                'ITLConnect': 'Disabled', 'CoincidenceMask': 'Disabled',
                'AntiCoincidenceMask': 'Disabled', 'CoincidenceLengthT': 0,
                'SelfTriggerWidth': 8},
        'ch52': {'WaveTriggerSource': 'ChSelfTrigger',
                 'EventTriggerSource': 'ChSelfTrigger',
                 'ITLConnect': 'Disabled', 'CoincidenceMask': 'Disabled',
                 'AntiCoincidenceMask': 'Disabled', 'CoincidenceLengthT': 0,
                 'SelfTriggerWidth': 8},
    },
    'cable-52-to-0': {
        'note': 'The PROVEN path: ch52 self-triggers, joins ITLA, ITLA goes out TRGOUT,\n'
                '        the cable lands on TRGIN, ch0 listens there. ch0 then runs at\n'
                '        ch52\'s rate (~60 Hz) no matter what ch0\'s own input is doing.\n'
                '        This is 1:1 FOLLOWING, not coincidence -- it does not require ch0\n'
                '        to have fired. Verified: ch0 362 vs ch52 362 counts over 6 s.',
        'board': {'TrgOutMode': 'ITLA', 'ITLAMainLogic': 'OR', 'ITLAGateWidth': 0},
        'ch0': {'WaveTriggerSource': 'TRGIN', 'EventTriggerSource': 'TRGIN',
                'ITLConnect': 'Disabled', 'CoincidenceMask': 'Disabled',
                'AntiCoincidenceMask': 'Disabled', 'CoincidenceLengthT': 0,
                'SelfTriggerWidth': 8},
        'ch52': {'WaveTriggerSource': 'ChSelfTrigger',
                 'EventTriggerSource': 'ChSelfTrigger', 'ITLConnect': 'ITLA',
                 'SelfTriggerWidth': 8},
    },
    'planA-and': {
        'note': 'Test A: make the ITL AND actually intersect two self-triggers by widening\n'
                '        SelfTriggerWidth past the ~47 ns ch2 injection skew. Default 8 ns\n'
                '        cannot overlap at all, which is the accepted explanation for why\n'
                '        ITLAMainLogic=AND returned exactly 0 (plan R7).\n'
                '        Predicted: width 8 -> ch0 Wave/s 0 (control). If AND works, some\n'
                '        width -> ch0 Wave/s ~10 Hz (only ch2 lands on both channels).\n'
                '        If it degenerates to OR you get ~60 Hz -- that is a FAIL, not a win.\n'
                '        RISK: ch0 both feeds ITLA and listens via TRGIN. Under AND it cannot\n'
                '        self-sustain; under a broken AND it becomes a trigger loop.',
        'board': {'TrgOutMode': 'ITLA', 'ITLAMainLogic': 'AND', 'ITLAGateWidth': 0},
        'ch0': {'WaveTriggerSource': 'TRGIN', 'EventTriggerSource': 'ChSelfTrigger',
                'ITLConnect': 'ITLA', 'CoincidenceMask': 'Disabled',
                'AntiCoincidenceMask': 'Disabled', 'CoincidenceLengthT': 0,
                'SelfTriggerWidth': 'WIDTH'},
        'ch52': {'WaveTriggerSource': 'ChSelfTrigger',
                 'EventTriggerSource': 'ChSelfTrigger', 'ITLConnect': 'ITLA',
                 'SelfTriggerWidth': 'WIDTH'},
    },
    'planB-dual': {
        'note': 'Test B: the coincidence window has only ever been tested with the partner\n'
                '        on the SAME bus as the primary, so the partner was never\n'
                '        independent -- the hole in R4-R6. Here ch0 is triggered by TRGIN\n'
                '        (partner-free) and its OWN discriminator returns on ITLB, so\n'
                '        CoincidenceMask=ITLB is a genuine second input.\n'
                '        Predicted: ~10 Hz if the window gates, ~60 Hz if it is inert. Those\n'
                '        are 6x apart, so no judgement call is needed.\n'
                '        Strongest control: turn emulator ch2 off -> a live gate drops ch0\n'
                '        toward 0 Hz, without touching the digitizer at all.',
        'board': {'TrgOutMode': 'ITLA', 'ITLAMainLogic': 'OR', 'ITLAGateWidth': 0},
        'ch0': {'WaveTriggerSource': 'TRGIN', 'EventTriggerSource': 'ChSelfTrigger',
                'ITLConnect': 'ITLB', 'CoincidenceMask': 'ITLB',
                'AntiCoincidenceMask': 'Disabled', 'CoincidenceLengthT': 'WIDTH'},
        'ch52': {'WaveTriggerSource': 'ChSelfTrigger',
                 'EventTriggerSource': 'ChSelfTrigger', 'ITLConnect': 'ITLA',
                 'SelfTriggerWidth': 8},
    },
    'planB-anti': {
        'note': 'Same wiring as planB-dual but the veto version: AntiCoincidenceMask=ITLB\n'
                '        should PASS ch0\'s non-coincident events, i.e. ~60 Hz minus the 10 Hz\n'
                '        subset. AntiCoincidenceMask has never been written on this board, so\n'
                '        it is an independent probe of the same logic block.',
        'board': {'TrgOutMode': 'ITLA', 'ITLAMainLogic': 'OR'},
        'ch0': {'WaveTriggerSource': 'TRGIN', 'ITLConnect': 'ITLB',
                'CoincidenceMask': 'Disabled', 'AntiCoincidenceMask': 'ITLB',
                'CoincidenceLengthT': 'WIDTH'},
        'ch52': {'WaveTriggerSource': 'ChSelfTrigger', 'ITLConnect': 'ITLA'},
    },
    'internal-and': {
        'note': 'COINCIDENCE WITH NO CABLE. The AND is computed on ITLA and delivered\n'
                '        internally through GlobalTriggerSource -- the one consumer of the\n'
                '        ITL bus that actually reaches WaveTriggerSource. TrgOutMode=Disabled\n'
                '        so nothing external can contribute.\n'
                '        Predicted: ch0 AND ch52 both ~10 Hz (the ch2 population), against own\n'
                '        inputs of 15 and 60 Hz. Measured 2026-10-20: 10.0 / 10.0 Hz, 0% deadtime.\n'
                '        Controls: MainLogic AND->OR gives 65.1 Hz (= 15+60-10, the union);\n'
                '        SelfTriggerWidth 1000->8 gives 0.0 Hz; emulator ch2 off gives 0.0 Hz.\n'
                '        SelfTriggerWidth MUST exceed the inter-channel skew (~47 ns here) or the\n'
                '        AND is empty -- that is why every earlier internal attempt read zero.',
        'board': {'TrgOutMode': 'Disabled', 'ITLAMainLogic': 'AND',
                  'GlobalTriggerSource': 'ITLA', 'ITLAGateWidth': 0},
        'ch0': {'WaveTriggerSource': 'GlobalTriggerSource',
                'EventTriggerSource': 'GlobalTriggerSource',
                'ITLConnect': 'ITLA', 'SelfTriggerWidth': 'WIDTH',
                'CoincidenceMask': 'Disabled', 'AntiCoincidenceMask': 'Disabled',
                'CoincidenceLengthT': 0},
        'ch52': {'WaveTriggerSource': 'GlobalTriggerSource',
                 'EventTriggerSource': 'GlobalTriggerSource',
                 'ITLConnect': 'ITLA', 'SelfTriggerWidth': 'WIDTH',
                 'CoincidenceMask': 'Disabled', 'AntiCoincidenceMask': 'Disabled',
                 'CoincidenceLengthT': 0},
    },
    'internal-itla': {
        'note': 'RETRACTED -- offered only so the dead end can be re-demonstrated, not used.\n'
                '        ch52 listens to ITLA directly, no cable. Measured ChWaveCnt = 0 in\n'
                '        both directions, at every mask / window / gate width tried (R1).\n'
                '        If a future run shows nonzero here, §19 is wrong and should be\n'
                '        corrected -- that is the reason this preset exists.',
        'board': {'TrgOutMode': 'Disabled', 'ITLAMainLogic': 'OR'},
        'ch0': {'WaveTriggerSource': 'ChSelfTrigger', 'ITLConnect': 'ITLA',
                'CoincidenceMask': 'Disabled'},
        'ch52': {'WaveTriggerSource': 'ITLA', 'EventTriggerSource': 'ITLA',
                 'ITLConnect': 'Disabled', 'CoincidenceMask': 'Disabled'},
    },
}


def resolve(preset, width):
    out = {}
    for scope, d in (('board', preset.get('board', {})),
                     ('ch0', preset.get('ch0', {})),
                     ('ch52', preset.get('ch52', {}))):
        out[scope] = {k: (width if v == 'WIDTH' else v) for k, v in d.items() if v is not None}
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Apply a verified trigger routing preset",
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--apply', choices=sorted(PRESETS), help='preset to write')
    ap.add_argument('--show', action='store_true', help='read the live routing config')
    ap.add_argument('--width', default='1000',
                    help='value for the parameterised knob: SelfTriggerWidth in planA, '
                         'CoincidenceLengthT in planB (default 1000)')
    ap.add_argument('--dry-run', action='store_true')
    ap.add_argument('--no-verify', action='store_true',
                    help='skip the post-apply full readback table')
    ap.add_argument('--digi', type=int, default=0)
    args = ap.parse_args(argv)

    if not args.apply and not args.show:
        args.show = True

    try:
        s = Session(digi=args.digi)
        s.boot()
        print(f"acq: {'ON' if s.acq_on() else 'OFF'}")

        if args.show or args.apply:
            live = s.reads([(n, 0) for n in TRIGGER_CH] + [(n, 52) for n in TRIGGER_CH])
            gb = s.reads(TRIGGER_BOARD, scope='board')
            print("\n--- live routing ---")
            for n in TRIGGER_CH:
                a, b = live[(n, 0)], live[(n, 52)]
                flag = '' if a == b else '   (differ)'
                print(f"  {n:<28s} ch0={a:<18s} ch52={b:<18s}{flag}")
            print()
            for n in TRIGGER_BOARD:
                print(f"  {n:<28s} board={gb[n]}")

        if not args.apply:
            return 0

        p = PRESETS[args.apply]
        want = resolve(p, args.width)
        print(f"\n=== preset '{args.apply}' ===\n{p['note']}\n")

        needs_stop = any(
            spec(canon(k, 'board' if scope == 'board' else 'ch'),
                 'board' if scope == 'board' else 'ch')['setinrun'] == 'false'
            for scope, d in want.items() for k in d)
        print(f"writes: {sum(len(d) for d in want.values())} parameters"
              + ("  [NEEDS ACQ STOPPED]" if needs_stop else "  [all live]"))
        if args.dry_run:
            for scope, d in want.items():
                for k, v in d.items():
                    print(f"  {scope:>5s} {k:<28s} = {v}")
            print("\ndry run -- nothing written.")
            return 0

        was_on = s.acq_on()
        if needs_stop and was_on:
            print("stopping ACQ for the setinrun=false writes...")
            s.stop()
            was_on = True   # remember to put it back

        for scope, d in want.items():
            ch = None if scope == 'board' else (0 if scope == 'ch0' else 52)
            scp = 'board' if scope == 'board' else 'ch'
            for k, v in d.items():
                n = canon(k, scp)
                old, new, note = s.write(n, v, ch=ch, scope=scp, check_acq=True)
                mark = '  ' if new == str(v) else '!!'
                where = 'board' if ch is None else f'ch{ch}'
                print(f"  {mark} {where:>5s} {n:<28s} {old:>18s} -> {new:<18s} {note}")

        if needs_stop and was_on:
            print("restarting ACQ (it was running before)...")
            s.start(fmt=1)

        if not args.no_verify:
            print("\n--- readback of every parameter this preset touches ---")
            # Batched into two calls (one board-scope, one channel-scope) rather than
            # one per parameter: each read is a REQ/REP round trip, and the loop form
            # was written when every read meant spawning solaris-cli.
            got_board = s.reads(list(want['board']), scope='board')
            got_ch = s.reads([(k, 0 if sc == 'ch0' else 52)
                              for sc in ('ch0', 'ch52') for k in want[sc]])
            bad = 0
            # Index by the key we ASKED with: reads() keys its result by the request,
            # and canon() returns the tree's LOWERCASE spelling -- mixing the two is a
            # KeyError waiting to happen (it happened).
            for k, v in want['board'].items():
                got = got_board[k]
                ok = got == str(v); bad += (not ok)
                print(f"  {'ok' if ok else 'MISMATCH':>8s} board {k:<28s} "
                      f"want={str(v):<18s} got={got}")
            for sc in ('ch0', 'ch52'):
                ci = 0 if sc == 'ch0' else 52
                for k, v in want[sc].items():
                    got = got_ch[(k, ci)]
                    ok = got == str(v); bad += (not ok)
                    print(f"  {'ok' if ok else 'MISMATCH':>8s} ch{ci:<4d} {k:<28s} "
                          f"want={str(v):<18s} got={got}")
            print(f"\n{'all values landed as asked' if bad == 0 else str(bad) + ' MISMATCH(ES)'}"
                  f" | ACQ {'ON' if s.acq_on() else 'OFF'}")
        print("\nnext: python3 dig_run.py --seconds 10 --ch=0,52")
    except DigiError as e:
        print(f"\nERROR: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
