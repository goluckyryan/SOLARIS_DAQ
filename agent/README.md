# Digitizer CLI scripts (VX2745 via the broker)

Drive the digitizer without hand-writing `printf 'connect\n…\nquit\n' | ./solaris-cli`
heredocs. These speak **ZeroMQ to the broker directly** (`DAQ:broker/BrokerProtocol.h`) —
same server, same wire as the CLI, without spawning a process per batch. Every read comes
back **labelled** and every write comes back **read from the board**.

The speed win is **one process spawn saved, not a faster transport** — measured
2026-10-20:

| | 1 read | 40 reads | 200 reads |
|---|---|---|---|
| `solaris-cli` (one spawn) | 234 ms | 232 ms | 218 ms |
| ZMQ, persistent socket | 1 ms | 9 ms | 80 ms |
| **per-read, after the ~150 ms spawn** | — | ~2 ms | ~0.35 ms |

Per-read cost **converges** (~0.35 ms either way) because the broker reads the actual
board on every request: `BrokerServer.cpp` calls `ReadValue(param, false, &status)` and
that `false` is **`verbose`**, not "use the cache" (`ClassDigitizer2Gen.h:121`).
`DigiManager::ReadValueFromCache` exists and the broker never calls it. So no transport
choice makes a read cheap — only the ~150 ms of process startup is removable, and that
is what this is. Corollary: `read_all()` returns 3450 params in 1.2 s, which is the same
0.35 ms/param — it is bulk because it skips parsing, not because it skips the board.

### Requirement: pyzmq

Installed 2026-10-20 into `~/venv` (created with `--system-site-packages`, so the system
numpy 1.26.4 / matplotlib 3.6.3 are still visible to `traces_capture.py`). Run the scripts
with a bare `python3` — `dig.py:_ensure_zmq()` adds `~/venv/lib/python*/site-packages` to
`sys.path`, and re-execs under `~/venv/bin/python` if that is not enough. Nothing to
activate.

If it is ever missing:

```bash
python3 -m venv --system-site-packages ~/venv
~/venv/bin/python -m pip install pyzmq        # 27.2.0, prebuilt cp312 wheel
```

```bash
cd ~/SOLARIS_DAQ/agent
python3 dig_status.py                        # trigger config, ch0 + ch52 + globals
python3 dig_status.py --diff --ch=0          # only params off from factory default
python3 dig_status.py --list ch              # every channel param + allowed values
python3 dig_status.py --counters             # rates (needs ACQ running -- it will say so)

python3 dig_set.py --ch=0 --set=WaveTriggerSource=TRGIN
python3 dig_set.py --board --set=ITLAGateWidth=200 --dry-run
python3 dig_set.py --ch=0 --set=SelfTriggerWidth=1000 --stop-first

python3 dig_trigger.py --show                # live routing, ch0 vs ch52 side by side
python3 dig_trigger.py --apply=planA-and --width=1000

python3 dig_run.py --seconds 10 --ch=0,52    # start, measure, stop, report rates
python3 dig_run.py --seconds 30 --save=agent/data/tf/planA
python3 dig_run.py --stop
```

## What these enforce

Five things that were each got wrong at least once on this bench. None of them are
theoretical — every one has a wrong number attached to it in `AGENT.md`:

1. **A batch of reads has no labels.** `TrgOutMode` and `GlobalTriggerSource` were once
   transcribed as `Disabled`/`TrgIn` when they were `ITLA`/`SwTrg`, from mapping 30
   unlabeled CLI output lines back by position. `reads()` keys its result by the request
   you passed in, so `('TriggerThr', 0)` and `('TriggerThr', 52)` cannot collide and
   ordering never matters.

2. **`OK` from a write means ACCEPTED, not SET.** The board clamps and quantizes
   silently: `ITLAGateWidth=500` lands on **504**; `CoincidenceLengthS=524280` is refused
   with an opaque `WriteValue failed` (that max belongs to the `*_T`/ns variant). Values
   are checked against the device tree **before** writing, and read back **after**. The
   quantization warning deliberately does *not* predict the landing value — the board
   rounds 500→504 (up) where `round()` gives 496, and the rounding rule is undocumented.

3. **ACQ state is a boolean, not `AcquisitionStatus`.** `/par/AcquisitionStatus` reads
   **8 while acquisition is OFF** and its enum is undocumented (`ANSTYPE::BINARY` in
   `DAQ:core/DigiParameters.h`). `acq_on()` uses `REQ_GET_ACQ_STATUS`, which returns a
   real per-digitizer flag — the setinrun guard below depends on it being correct.

4. **`setinrun=false` params need ACQ stopped** — `ITLConnect`, `TrgOutMode`,
   `ITLAGateWidth`, `SelfTriggerWidth`, `ChRecordLengthS`, `StartSource`. Writable-but-
   ignored while acquiring. `dig_set.py` refuses and names them; `--stop-first` stops,
   applies, and restores the ACQ state it found.

5. **`SelfTrgRate` is a per-second readout, not a counter.** It is live even with ACQ
   stopped and reports the channel's **input** population regardless of
   `WaveTriggerSource`. `ChRealtimeMonitor` is frozen when ACQ is off, so counter
   deltas divide by zero. `dig_run.py` reads the monitor first (it is the only thing that
   refreshes the group), uses its delta as the true elapsed time, and reports `Hz(own)`
   and `Wave/s` side by side — **when they disagree, the disagreement is the answer.**

## Files

| file | role |
|---|---|
| `dig.py` | shared engine: ZMQ session, labelled reads, verified writes, device-tree validation, parameter groups |
| `dig_status.py` | read. `--what=trigger\|shape\|counters\|all`, `--diff`, `--list`, `--counters` |
| `dig_set.py` | write one or more params. `--ch=N` / `--board`, `--dry-run`, `--stop-first` |
| `dig_trigger.py` | named routing presets (see below), with full readback |
| `dig_run.py` | start / hold / sample / stop / report rates |
| `traces_capture.py` | plot a capture (2 analog + 4 digital) as one timing diagram. Offline by default; `--sweep` touches the board |

`Session.read_all()` (available, not used by the scripts) pulls ~3450 parameters in one
1.2 s request — but it is **not complete**: 52 of the 62 channel params appear, and the
10 missing ones are exactly the `*_S` sample-form half of every dual-unit pair
(`ChPreTriggers`, `ChRecordLengthS`, `CoincidenceLengthS`, `EnergyFilterRiseTimeS`,
`…FlatTopS`, `…PoleZeroS`, `…PileUpGuardS`, `…BaselineGuardS`, `TimeFilterRetriggerGuardS`,
`TimeFilterRiseTimeS`) plus `WaveDownSamplingFactor`. The dump carries only the `*_T`
form: it reports `ChPreTriggerT = 4000` where the sample form is 500. So it cannot answer
"what is ChRecordLengthS" — those names still need individual reads.

`agent/data/devt_vx2745.json` (2.3 MB, gitignored) is the device tree these validate
against — a snapshot taken 2026-10-14. If it is missing, `dig.py` says so and stops
rather than writing unvalidated values. §15 of `AGENT.md` has the dump recipe.
Node names in that JSON are **lowercase**; the wire is case-insensitive, so display uses
the name you typed and the tree is used for ranges.

## Not provided, on purpose

- **`trace`** — writes `ChEnable=False` to all 64 channels, its restore silently fails,
  and it forces `WaveTriggerSource=ChSelfTrigger`. It destroys the config under test.
  Capture with `dig_run.py --save` and parse the `.sol`.
  It is also **not a broker opcode** — the handler is CLI-side at
  `DAQ:broker/solaris-cli.cpp:815`, so it cannot be reimplemented over ZMQ. Worse, it calls
  `StartACQ(digi, DataFormat::ALL)` at `:849`, and `ALL = 0x00` (`DAQ:core/Hit.h:13`) —
  **`trace` *is* the format-0 path that has killed the broker twice**, which is why `traces_capture.py
  --sweep` runs it four times in a row and prints `TRACE_WARNING` first.
  There is no way around it for this plot: per `DAQ:docs/sol-file-format.md`, only `ALL`
  carries 2 analog + 4 digital probes (`OneTrace` = 1 analog, `NoTrace` = none), and the
  7-column format this tool plots is the CLI's text rendering of `ALL`.
- **`open`** — a fresh open resets `WaveTriggerSource` on every channel. `boot()` refuses
  and prints the command to run deliberately.
- **format 0 (ALL)** — default is 1 (OneTrace). The broker has died twice on `start 0 0`;
  it still accepts TCP after dying, so the symptom is empty responses, not a refused
  connection. `--format=0` is allowed but warns.

## Presets (`dig_trigger.py --apply=`)

| name | what it is | status |
|---|---|---|
| `off` | both channels self-triggered | positive control |
| `internal-and` | AND on ITLA, delivered internally via `GlobalTriggerSource`, cable off | **PROVEN — 10.0 Hz on both channels, no wire.** This is the coincidence result |
| `cable-52-to-0` | ch52 → ITLA → TRGOUT → cable → TRGIN → ch0 | **PROVEN** (362 vs 362 counts / 6 s). 1:1 following, *not* coincidence |
| `planA-and` | `ITLAMainLogic=AND` + widened `SelfTriggerWidth` | **untested** — Test A |
| `planB-dual` | TRGIN primary + own discriminator back on ITLB as the partner | **untested** — Test B |
| `planB-anti` | same wiring, `AntiCoincidenceMask` | never written on this board |
| `internal-itla` | ch52 listens to ITLA directly, no cable | **DEAD** — confirmed again at width 1000. Use `internal-and`: ITLA does not reach `WaveTriggerSource`, but `GlobalTriggerSource` fed by ITLA does |

Each preset prints its own prediction and what would count as a failure — e.g. in
`planA-and`, ~60 Hz means AND silently degenerated to OR, which is a **fail**, not a win.
Reasoning and the eight ruled-out configurations (R1–R8): `agent/trigger_test_plan.md`,
and `AGENT.md` §19.

## Rules these encode

- `ChWaveCnt` ↔ `WaveTriggerSource`; `ChTriggerCnt`/`ChSavedEventCnt` ↔
  `EventTriggerSource`. Using `ChTriggerCnt` to ask a routing question reports success
  for broken configs.
- A rate appearing is not attribution: change one emulator channel's rate and watch only
  its own digitizer channel follow.
- A nonzero rate is never proof of coincidence. The gate has to make the rate **drop**
  to the predicted subset, and removing the partner has to move it back.
- Nothing is committed by these scripts, and none of them should be.
