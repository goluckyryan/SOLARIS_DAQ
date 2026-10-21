#!/usr/bin/env python3
"""Shared engine for the digitizer CLI scripts (VX2745 via the broker).

Talks ZeroMQ to the broker directly (`DAQ:broker/BrokerProtocol.h`) instead of spawning
`solaris-cli` and parsing its stdout.

Why direct, in order of real value:
  * **Typed responses.** Opcode 0x83 is a value, 0x81 is an error carrying the broker's
    own message. The subprocess version had to classify output by string prefix
    ("Error:"/"Failed:"), which is guesswork about a format, and a legitimate value
    could in principle start with either.
  * **No process spawn per batch.** ~150 ms of exec + readline init + connect/teardown,
    once per invocation. This is the ENTIRE speed difference -- see the next bullet.
  * **One persistent connection**, so a run of reads shares a socket.

What it does NOT buy: a faster read. The broker hits the hardware for every request
(`BrokerServer.cpp` passes `false` as `verbose`, not as a cache flag -- see
`ClassDigitizer2Gen.h:121`; `DigiManager::ReadValueFromCache` exists and is never called
by the broker). Measured per-read cost converges to ~0.35 ms either way, and
`read_all()`'s 3450 params in 1.2 s is the same 0.35 ms/param. The board round trip over
TCP to 192.168.0.100 is the floor, and no transport choice changes it.

Needs `pyzmq`, installed into `~/venv` (2026-10-20). If this file is run by a python that
cannot import zmq, `_ensure_zmq()` re-execs it under `~/venv/bin/python` so the scripts
work with a bare `python3 dig_status.py` -- no activation step to forget. The venv was
created with `--system-site-packages`, so the system numpy/matplotlib are still there for
`traces_capture.py` and friends.

Five problems this module exists to solve. Each is a mistake already made on this bench:

1. **A read needs a label.** Reading 30 parameters by position is how `TrgOutMode` and
   `GlobalTriggerSource` got transcribed as `Disabled`/`TrgIn` when they were
   `ITLA`/`SwTrg`. `reads()` keys its result by the request you passed in, so
   `('TriggerThr', 0)` and `('TriggerThr', 52)` cannot collide and order never matters.

2. **`OK` from a write means ACCEPTED, not SET.** The board clamps and quantizes
   silently: `ITLAGateWidth=500` lands on **504**; `CoincidenceLengthS=524280` is refused
   because that max belongs to the `*_T`/ns variant. So values are checked against the
   device tree BEFORE writing and read back AFTER. The quantization warning deliberately
   does not predict the landing value -- the board rounds 500 up to 504 where `round()`
   gives 496, and the rounding rule is not documented anywhere.

3. **`setinrun=false` params need ACQ stopped.** `ITLConnect`, `TrgOutMode`,
   `ITLAGateWidth`, `SelfTriggerWidth`, `ChRecordLengthS`, `StartSource` and others are
   accepted-then-ignored while acquiring. `write()` refuses when ACQ is on and names the
   parameters, rather than producing a config that silently half-applied.

4. **ACQ state comes from `REQ_GET_ACQ_STATUS`, not `/par/AcquisitionStatus`.** That
   parameter reads **8 while acquisition is OFF** and its enum is undocumented (declared
   `ANSTYPE::BINARY` in `DAQ:core/DigiParameters.h`). The status request returns a real
   per-digitizer boolean, which is what the setinrun guard depends on.

5. **Never auto-`open` a digitizer.** A fresh open resets per-channel
   `WaveTriggerSource` (DAQ:agent/AGENT.md §19) and would destroy the config under test.
   `boot()` refuses and prints the command to run deliberately.

Deliberately absent: `trace`. It writes `ChEnable=False` to all 64 channels, its restore
silently fails, and it forces `WaveTriggerSource=ChSelfTrigger`. Capture with
`dig_run.py --save` and parse the `.sol`.

Param names are case-insensitive on the wire; the canonical spelling from the device tree
is used so printed output greps cleanly.
"""

import json
import os
import struct
import sys
import time

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TREE_PATH = os.path.join(REPO_ROOT, 'agent', 'data', 'devt_vx2745.json')
DEFAULT_URL = 'dig2://192.168.0.100'
VENV_PY = os.path.expanduser('~/venv/bin/python')

# --- message types (DAQ:broker/BrokerProtocol.h) ---
REQ_OPEN_DIGI, REQ_CLOSE_DIGI, REQ_LIST_DIGI = 0x01, 0x02, 0x03
REQ_READ_VALUE, REQ_WRITE_VALUE, REQ_SEND_COMMAND = 0x10, 0x12, 0x14
REQ_START_ACQ, REQ_STOP_ACQ, REQ_SET_DATA_FORMAT, REQ_GET_ACQ_STATUS = 0x20, 0x21, 0x22, 0x23
REQ_READ_ALL_SETTINGS = 0x40
REQ_GET_DIGI_INFO = 0x50
REQ_PING, REQ_SHUTDOWN = 0xF0, 0xFF
RSP_OK, RSP_ERROR, RSP_DIGI_LIST, RSP_VALUE = 0x80, 0x81, 0x82, 0x83
RSP_DIGI_INFO, RSP_ACQ_STATUS, RSP_FILE_STATUS, RSP_SETTINGS = 0x84, 0x85, 0x86, 0x87
RSP_PONG = 0xF0

# REQ/REP is strictly lockstep: if a recv() times out the socket is stuck in EFSM and
# every later send fails. A dead broker is exactly that shape -- it keeps accepting TCP
# while answering nothing (observed twice, both after `start 0 0`). So the socket is
# rebuilt after any timeout rather than reused, and the error text says what to do.
_REEXEC_FLAG = 'SOLARIS_DIG_REEXEC'


def _venv_site_dirs():
    import glob
    return sorted(glob.glob(os.path.expanduser('~/venv/lib/python*/site-packages')))


def _ensure_zmq():
    """Make `import zmq` work, however this file was launched.

    Three tiers, in order of least disruption:

    1. already importable -> done.
    2. add ~/venv/lib/python*/site-packages to sys.path. This is the common case and it
       works for EVERY launch mode, because it does not replace the process.
    3. re-exec under ~/venv/bin/python -- only possible when we have a real script path.
       `python3 -c "..."` and `python3 - <<PY` put no usable path in sys.argv (argv is
       literally ['-c']), so reconstructing argv there silently loses the program. That
       was a real bug: the -c form came back with "Argument expected for the -c option".
    """
    try:
        import zmq                                    # noqa: F401
        return
    except ImportError:
        pass

    for d in _venv_site_dirs():
        if os.path.isdir(d) and d not in sys.path:
            sys.path.append(d)
    try:
        import zmq                                    # noqa: F401
        return
    except ImportError:
        pass

    if os.environ.get(_REEXEC_FLAG):
        raise SystemExit(
            "pyzmq is not importable even after adding ~/venv site-packages.\n"
            f"  tried: {', '.join(_venv_site_dirs()) or '(none found)'}\n"
            f"  also:  {VENV_PY}\n"
            "  fix:   ~/venv/bin/python -m pip install pyzmq")
    if not os.path.exists(VENV_PY):
        raise SystemExit(
            "pyzmq is missing and ~/venv does not exist. Create it:\n"
            "  python3 -m venv --system-site-packages ~/venv\n"
            "  ~/venv/bin/python -m pip install pyzmq")

    script = None
    main = sys.modules.get('__main__')
    cand = getattr(main, '__file__', None) or (sys.argv[0] if sys.argv else None)
    if cand and cand not in ('-c', '-') and not cand.startswith('-') \
            and os.path.isfile(os.path.abspath(cand)):
        script = os.path.abspath(cand)

    if script is None:
        raise SystemExit(
            "pyzmq is not importable and this launch mode cannot be re-executed\n"
            f"  (no script path in argv: {sys.argv!r}). Run the script by filename, or\n"
            f"  use {VENV_PY} explicitly.")

    env = dict(os.environ, **{_REEXEC_FLAG: '1'})
    os.execve(VENV_PY, [VENV_PY, script] + sys.argv[1:], env)


class DigiError(Exception):
    """Anything that should stop the script before it writes to hardware."""


# --------------------------------------------------------------------- device tree

def load_tree(path=TREE_PATH):
    if not os.path.exists(path):
        raise DigiError(
            f"device-tree dump missing: {path}\n"
            "  A 2.3 MB snapshot of the board's own parameter table, taken 2026-10-14.\n"
            "  Without it nothing here can validate a value or know which params need ACQ\n"
            "  stopped. Dump recipe: DAQ:agent/AGENT.md §15.")
    with open(path) as f:
        return json.load(f)


def _v(node, key):
    """Tree sub-keys are themselves dicts carrying 'value'; tolerate a plain value too."""
    x = node.get(key)
    if isinstance(x, dict):
        x = x.get('value')
    return x


def _allowed(node):
    a = node.get('allowedvalues')
    if not isinstance(a, dict):
        return None
    keys = [k for k in a if k.isdigit()]
    if not keys:
        return None
    return [a[k]['value'] for k in sorted(keys, key=int)]


_META = None


def meta():
    """{'ch': {...}, 'board': {...}} from the device tree.

    Channel params are read from /ch/0/ and assumed identical on all 64 -- true here
    (same 63 entries per channel), and the tree is one board's firmware snapshot.
    """
    global _META
    if _META is None:
        t = load_tree()
        out = {'ch': {}, 'board': {}}
        for scope, node in (('ch', t['ch']['0']['par']), ('board', t['par'])):
            for name, n in node.items():
                if not isinstance(n, dict) or 'handle' not in n:
                    continue
                out[scope][name.lower()] = {
                    'name': name,
                    'type': _v(n, 'datatype'),
                    'value': _v(n, 'value'),          # SNAPSHOT, 2026-10-14 -- not live
                    'default': _v(n, 'defaultvalue'),
                    'access': _v(n, 'accessmode'),
                    'setinrun': _v(n, 'setinrun'),
                    'min': _v(n, 'minvalue'),
                    'max': _v(n, 'maxvalue'),
                    'inc': _v(n, 'increment'),
                    'uom': _v(n, 'uom'),
                    'allowed': _allowed(n),
                    'desc': _v(n, 'description'),
                }
        _META = out
    return _META


def canon(name, scope='ch'):
    """Canonical spelling of a parameter name, case-insensitively.

    The tree dump stores node names lowercase, so this returns lowercase. Callers that
    print a name should echo what the user typed, not this.
    """
    m = meta()[scope]
    k = name.lower().replace('/', '').replace('-', '_')
    if k in m:
        return m[k]['name']
    last = name.rstrip('/').split('/')[-1]           # accept a full path
    if last.lower() in m:
        return m[last.lower()]['name']
    raise DigiError(f"unknown {scope} parameter '{name}'. "
                    f"List them with: dig_status.py --list {scope}")


def spec(name, scope='ch'):
    return meta()[scope][canon(name, scope).lower()]


def validate(name, value, scope='ch'):
    """Check a proposed value against the tree. Returns (ok, note); note may warn."""
    s = spec(name, scope)
    v = str(value)
    if s['access'] == 'READ_ONLY':
        return False, f"{name} is READ_ONLY -- {s['desc']}"
    if s['allowed'] is not None:
        if v not in s['allowed']:
            return False, (f"{name}='{v}' is not an allowed value. "
                           f"Allowed: {', '.join(s['allowed'])}")
        return True, ''
    if s['type'] == 'NUMBER':
        try:
            x = float(v)
        except ValueError:
            return False, f"{name} needs a number, got '{v}'"
        lo, hi = s['min'], s['max']
        if lo is not None and x < float(lo):
            return False, f"{name}={v} is below the minimum {lo}"
        if hi is not None and x > float(hi):
            hint = ''
            if name.lower().endswith('s') and s['uom'] in ('undefined', 'sample'):
                hint = ("  (that larger max belongs to the *_T / ns variant of the SAME"
                        " register -- CoincidenceLengthS and CoincidenceLengthT are one"
                        " register in two units)")
            return False, f"{name}={v} is above the maximum {hi}{hint}"
        inc = s['inc']
        if inc and float(inc) > 1 and abs(x % float(inc)) > 1e-9:
            return True, (f"{v} is not a multiple of increment {inc} -- the board will"
                          f" quantize it (rounding direction undocumented; the read-back"
                          f" below is the authority)")
    return True, ''


# --------------------------------------------------------------------- wire codec

def _s(b):
    if isinstance(b, str):
        b = b.encode()
    return struct.pack('<H', len(b)) + b


class _Reader:
    def __init__(self, buf):
        self.b, self.o = buf, 0
        self.type = buf[0]
        self.o = 2                              # skip msgType + flags

    def u8(self):
        v = self.b[self.o]; self.o += 1; return v

    def u16(self):
        v = struct.unpack_from('<H', self.b, self.o)[0]; self.o += 2; return v

    def u32(self):
        v = struct.unpack_from('<I', self.b, self.o)[0]; self.o += 4; return v

    def st(self):
        n = struct.unpack_from('<H', self.b, self.o)[0]; self.o += 2
        v = self.b[self.o:self.o + n].decode('utf-8', 'replace'); self.o += n
        return v


# --------------------------------------------------------------------- session

class Session:
    """One persistent REQ socket to the broker; many requests.

    Public API is unchanged from the CLI-parsing version, so dig_set/dig_status/
    dig_trigger/dig_run do not care which transport is underneath.
    """

    def __init__(self, digi=0, url=DEFAULT_URL, timeout=10000, quiet=False):
        _ensure_zmq()
        import zmq
        self._zmq = zmq
        self.digi = digi
        self.url = url
        self.timeout = timeout
        self.quiet = quiet
        self.ctx = zmq.Context.instance()
        self.sock = None
        self._connect()

    def _connect(self):
        zmq = self._zmq
        if self.sock is not None:
            try:
                self.sock.setsockopt(zmq.LINGER, 0)
            except zmq.ZMQError:
                pass
            self.sock.close()
        self.sock = self.ctx.socket(zmq.REQ)
        self.sock.setsockopt(zmq.LINGER, 0)
        self.sock.setsockopt(zmq.SNDTIMEO, self.timeout)
        self.sock.setsockopt(zmq.RCVTIMEO, self.timeout)
        try:
            self.sock.connect('tcp://localhost:5555')
        except zmq.ZMQError as e:
            raise DigiError(f"cannot connect to the broker: {e}")

    def _call(self, payload, want=None, retries=1):
        """Send one request; return a _Reader. Rebuilds the socket on timeout.

        A dead broker presents as an empty response while still accepting TCP, so a
        timeout is retried once after a reconnect before being reported.
        """
        zmq = self._zmq
        for attempt in range(retries + 1):
            try:
                self.sock.send(payload)
                raw = self.sock.recv()
            except zmq.error.Again:
                self._connect()
                if attempt < retries:
                    time.sleep(0.5)
                    continue
                raise DigiError(
                    "broker accepted the connection but did not answer "
                    f"(opcode 0x{payload[0]:02x} timed out after {retries + 1} tries).\n"
                    "  This is what a dead broker looks like -- it keeps the port open.\n"
                    "  It has died twice on `start 0 0` (format ALL). Recovery:\n"
                    f"    cd {REPO_ROOT} && nohup ./solaris-broker >agent/data/broker.log 2>&1 &\n"
                    "  wait ~6 s, then: printf 'connect\\nopen dig2://192.168.0.100\\nquit\\n' | ./solaris-cli\n"
                    "  WARNING: a fresh `open` resets WaveTriggerSource on all 64 channels (§19).")
            r = _Reader(raw)
            if r.type == RSP_ERROR:
                msg = r.st() if len(raw) > 2 else 'unspecified error'
                raise DigiError(f"broker error (opcode 0x{payload[0]:02x}): {msg}")
            if want is not None and r.type != want:
                raise DigiError(f"expected response 0x{want:02x}, got 0x{r.type:02x}")
            return r
        raise DigiError("unreachable")

    # ---------------- lifecycle ----------------

    def ping(self):
        return self._call(bytes([REQ_PING, 0]), want=RSP_PONG).type

    def list(self):
        """[(connected, serial, model, fpga, nch), ...]"""
        r = self._call(bytes([REQ_LIST_DIGI, 0]), want=RSP_DIGI_LIST)
        n = r.u8()
        out = []
        for _ in range(n):
            conn = r.u8(); sn = r.u32(); model = r.st(); fpga = r.st(); nch = r.u16()
            out.append((conn, sn, model, fpga, nch))
        return out

    def boot(self, need_digi=True):
        """Verify the broker is alive and a digitizer is already open. Never opens one."""
        self.ping()
        rows = self.list()
        if need_digi and not any(c for c, *_ in rows):
            raise DigiError(
                "no digitizer is open on the broker, and this tool will not `open` one for\n"
                "  you: a fresh open resets WaveTriggerSource on every channel and would\n"
                "  destroy the configuration under test (§19). Run it deliberately:\n"
                f"    cd {REPO_ROOT} && printf 'connect\\nopen {self.url}\\nquit\\n' | ./solaris-cli")
        return rows

    # ---------------- values ----------------

    def read_value(self, path, digi=None):
        d = self.digi if digi is None else digi
        r = self._call(bytes([REQ_READ_VALUE, 0, d]) + _s(path), want=RSP_VALUE)
        return r.st()

    def write_value(self, path, value, digi=None):
        d = self.digi if digi is None else digi
        self._call(bytes([REQ_WRITE_VALUE, 0, d]) + _s(path) + _s(str(value)), want=RSP_OK)
        return True

    @staticmethod
    def _path(key, scope):
        if isinstance(key, (tuple, list)):
            return f'/ch/{key[1]}/par/{key[0]}'
        return f'/par/{key}' if scope == 'board' else key

    def reads(self, keys, scope='ch'):
        """keys: NAME (global, with scope='board') or (NAME, ch). Returns {key: value}.

        Keyed by exactly what was passed in, so the same parameter on two channels does
        not collide and the caller never reasons about ordering.
        """
        return {k: self.read_value(self._path(k, scope)) for k in keys}

    def read(self, name, ch=None, scope='ch'):
        """One value as a string. read('TrgOutMode', scope='board'), read('ITLConnect', ch=52)."""
        key = name if scope == 'board' else (name, 0 if ch is None else ch)
        return self.reads([key], scope=scope)[key]

    def write(self, name, value, ch=None, scope='ch', check_acq=True, force=False):
        """Write one parameter and READ IT BACK. Returns (old, new, note)."""
        s = spec(name, scope)
        ok, note = validate(name, value, scope)
        if not ok:
            raise DigiError(f"refusing to write {name}={value}: {note}")
        if check_acq and s['setinrun'] == 'false' and not force and self.acq_on():
            raise DigiError(
                f"{name} has setinrun=false and ACQ is RUNNING -- the write would be\n"
                "  accepted and silently not take. Stop ACQ first (dig_run.py --stop)\n"
                "  or pass --stop-first / force=True if you are sure.")
        key = name if scope == 'board' else (name, ch)
        path = self._path(key, scope)
        old = self.read_value(path)
        self.write_value(path, value)
        new = self.read_value(path)
        if new != str(value):
            extra = (f"CLAMPED/QUANTIZED: asked {value}, board holds {new}"
                     + (f" (increment {s['inc']})" if s['inc'] else ''))
            note = (note + '; ' if note else '') + extra
        return old, new, note

    # ---------------- acquisition ----------------

    def acq_on(self):
        """Boolean from REQ_GET_ACQ_STATUS -- NOT /par/AcquisitionStatus (reads 8 when off)."""
        r = self._call(bytes([REQ_GET_ACQ_STATUS, 0]), want=RSP_ACQ_STATUS)
        n = r.u8()
        flags = [r.u8() for _ in range(n)]
        if self.digi >= len(flags):
            raise DigiError(f"digitizer {self.digi} not in ACQ status ({n} reported)")
        return bool(flags[self.digi])

    def start(self, fmt=1, save=None):
        """StartACQ. fmt: 0=ALL, 1=OneTrace, 2=NoTrace. **Use 1.**

        Format 0 (ALL = 2 analog + 4 digital probes x 64 channels) has killed the broker
        twice (§19), and a dead broker keeps TCP open while answering nothing.
        """
        payload = bytes([REQ_START_ACQ, 0, self.digi, fmt, 1 if save else 0]) + _s(save or '')
        self._call(payload, want=RSP_OK)
        if not self.acq_on():
            raise DigiError(
                f"start was accepted (fmt={fmt}) but the broker still reports ACQ OFF.\n"
                "  Check `dig_status.py` -- if the broker died, list will come back empty.")
        return True

    def stop(self):
        self._call(bytes([REQ_STOP_ACQ, 0, self.digi]), want=RSP_OK)
        time.sleep(1.0)                      # let the FPGA settle before the next write
        return True

    def cmd(self, name):
        """Raw device-tree command (sendswtrigger, armacquisition, reset, ...).

        NOT for start/stop -- those carry a data-format argument and are their own
        opcodes. See start()/stop().
        """
        self._call(bytes([REQ_SEND_COMMAND, 0, self.digi]) + _s(name), want=RSP_OK)
        return True

    # ---------------- bulk ----------------

    def read_all(self):
        """REQ_READ_ALL_SETTINGS: the broker pulls EVERY parameter from hardware and
        returns ~3450 (path, value) pairs in ~1.2 s. One request instead of 3450.

        CAVEAT, measured 2026-10-20: 52 of 62 channel params appear. The 10 missing are
        exactly the *_S (sample) half of every dual-unit pair -- ChPreTriggers,
        ChRecordLengthS, CoincidenceLengthS, EnergyFilterRiseTimeS/FlatTopS/PoleZeroS/
        PileUpGuardS/BaselineGuardS, TimeFilterRetriggerGuardS, TimeFilterRiseTimeS --
        plus WaveDownSamplingFactor. Only the *_T form is carried: it reports
        /ch/0/par/ChPreTriggerT = 4000 where the sample form is 500. So this cannot
        answer "what is ChRecordLengthS"; those names still need individual reads.
        """
        r = self._call(bytes([REQ_READ_ALL_SETTINGS, 0, self.digi]), want=RSP_SETTINGS,
                       retries=0)
        n = r.u32()
        out = {}
        for _ in range(n):
            k = r.st(); v = r.st()
            out[k] = v
        return out

    def close(self):
        try:
            self.sock.setsockopt(self._zmq.LINGER, 0)
            self.sock.close()
        except Exception:
            pass


# ------------------------------------------------------------------ parameter groups
# Names must match the device tree's canonical spelling.

TRIGGER_CH = ['WaveTriggerSource', 'EventTriggerSource', 'CoincidenceMask',
              'AntiCoincidenceMask', 'CoincidenceLengthS', 'CoincidenceLengthT',
              'ChannelsTriggerMask', 'ITLConnect', 'TriggerThr', 'SelfTriggerWidth',
              'TimeFilterRetriggerGuardT', 'ChannelVetoSource', 'ADCVetoWidth',
              'ChPreTriggers', 'ChRecordLengthS', 'ChEnable', 'EventSelector']

# ITLAMask / ITLBMask are deliberately NOT in any preset: they are the per-channel
# enable bitmask for the ITL buses and are the SAME state as each channel's
# ITLConnect -- writing a mask rewrites ITLConnect on all 64 channels, and writing
# ITLConnect sets that channel's bit. Manage it through ITLConnect, one channel at a
# time. Measured 2026-10-20.
TRIGGER_BOARD = ['TrgOutMode', 'GlobalTriggerSource', 'ITLAMainLogic',
                 'ITLAMajorityLev', 'ITLAPairLogic', 'ITLAPolarity',
                 'ITLBMainLogic', 'ITLBMajorityLev', 'ITLBPairLogic', 'ITLBPolarity',
                 'ITLAMask', 'ITLBMask', 'ITLAGateWidth', 'ITLBGateWidth',
                 'SyncOutMode', 'BusyInSource', 'BoardVetoSource', 'StartSource',
                 'ClockSource', 'EnStatEvents']

SHAPE_CH = ['EnergyFilterRiseTimeT', 'EnergyFilterFlatTopT', 'EnergyFilterPoleZeroT',
            'EnergyFilterPileUpGuardT', 'EnergyFilterBaselineGuardT',
            'EnergyFilterPeakingPosition', 'DCOffset', 'GainFactor']

COUNTERS = ['ChRealtimeMonitor', 'ChWaveCnt', 'ChTriggerCnt', 'ChSavedEventCnt',
            'ChDeadtimeMonitor', 'SelfTrgRate']


def ch_paths(names, ch):
    return [(n, ch) for n in names]
