# SOLARIS Digitizer Broker

## Overview

The CAEN digitizer library (`CAEN_FELib`) allows only one connection per digitizer. The broker solves this by holding the exclusive hardware connection and exposing a ZeroMQ interface. Multiple clients (GUI, CLI, Python scripts, AI agents) connect to the broker instead of the hardware directly.

```
         CAEN Digitizer(s)
              |  CAEN_FELib (exclusive, one connection only)
              |
      +-------v--------+
      |  solaris-broker |
      |  - owns digi[] |
      |  - ReadData    |
      |  - SaveToFile  |
      |  - scalars     |
      +--+----------+--+
    REP  |          | PUB
  :5555  |          | :5556
    +----+          +----+
    |                    |
+---v---+          +----v----+
|  GUI  |          |  CLI /  |
|  (Qt) |          |  Python |
+-------+          +---------+
```

## Key Concepts

### Digitizer Index

Every digitizer opened by the broker is assigned a 0-based integer index. This index is the **only way** to refer to a specific digitizer in all commands and protocol messages. It is NOT the serial number, NOT the IP address. The index is assigned in the order digitizers are opened (first opened = 0, second = 1, etc.). Use `list` (CLI) or `LIST_DIGI` (protocol) to get the index-to-digitizer mapping. The special index `0xFF` (255) means "all digitizers" in START_ACQ and STOP_ACQ.

### Data Saving

The broker handles all data saving. Clients never write `.sol` files. When starting acquisition with `saveData=1`, the broker creates output files, writes events, and handles 2 GB file rollover automatically. Clients receive status updates (file sizes, rates) via the PUB socket.

### Two ZMQ Sockets

| Socket | ZMQ Pattern | Default Endpoint (server) | Default Endpoint (client) | Purpose |
|--------|-------------|---------------------------|---------------------------|---------|
| Command | REQ/REP | `tcp://*:5555` | `tcp://localhost:5555` | Client sends one request, broker sends one reply. Strictly alternating. |
| Publish | PUB/SUB | `tcp://*:5556` | `tcp://localhost:5556` | Broker broadcasts data to all connected subscribers. One-way. |

## Prerequisites

```bash
sudo apt install libzmq3-dev
```

## Build

```bash
cd broker
make
```

Produces:
- `solaris-broker` -- the daemon
- `solaris-cli` -- interactive command-line client

## solaris-broker

```
Usage: solaris-broker [options]

Options:
  --ip <ip1,ip2,...>       Comma-separated digitizer IPs to open on startup
  --cmd-port <port>        Command (REQ/REP) port          [default: 5555]
  --pub-port <port>        Publish (PUB/SUB) port           [default: 5556]
  --scalar-interval <sec>  Scalar broadcast interval        [default: 2.0]
  --help                   Show help
```

Examples:
```bash
./solaris-broker --ip 192.168.0.254,192.168.0.253
./solaris-broker --ip 192.168.0.254 --cmd-port 6000 --pub-port 6001
./solaris-broker   # start with no digitizers, open them later via CLI
```

The broker:
- Opens each digitizer via `CAEN_FELib_Open("dig2://<ip>")`
- Listens on the REP socket for commands
- Runs a per-digitizer data-reading thread during acquisition
- Handles all data saving to `.sol` / `.sol_raw` files (2 GB auto-rollover)
- Broadcasts scalar updates, hit summaries, and trace snapshots on the PUB socket every `--scalar-interval` seconds
- Shuts down cleanly on SIGINT (Ctrl-C), SIGTERM, or the SHUTDOWN command

### Thread Model

```
Main thread           zmq_poll() loop, dispatches REQ/REP commands
ReadDataLoop[i]       one std::thread per digitizer (only while ACQ is running)
                      calls digi->ReadData() in a loop, calls digi->SaveDataToFile() if saving enabled
ScalarBroadcastLoop   one std::thread, runs continuously while broker is alive
                      reads scalar values (trigger rate, saved count, real time) from each digitizer
                      publishes PUB_SCALAR, PUB_HIT_SUMMARY, PUB_TRACE messages
```

All threads accessing the same digitizer are serialized by a per-digitizer `std::mutex`.

---

## solaris-cli

Interactive command-line client.

```bash
./solaris-cli                              # start, then type "connect"
./solaris-cli --connect localhost 5555 5556 # auto-connect on startup
```

### Commands

`<digi>` in every command below is the **digitizer index** (integer, 0-based). Use `list` to see the mapping:

```
> list
Index SN       Model      FPGA       NCh
0     12345    VX2740     DPP_PHA    64
1     12346    VX2730     DPP_PHA    64
```

Then use `0` or `1` as `<digi>` in subsequent commands.

#### Connection

| Command | Description |
|---------|-------------|
| `connect [host] [cmd_port] [pub_port]` | Connect to broker. Defaults: `localhost 5555 5556`. |
| `disconnect` | Disconnect from broker. |
| `ping` | Returns `PONG` if broker is alive. |

#### Digitizer Management

| Command | Description |
|---------|-------------|
| `list` | Print table of all digitizers with index, serial number, model, FPGA type, channel count. |
| `info <digi>` | Print detailed info for one digitizer (SN, model, FPGA, nChannels, tick2ns, firmware versions). |
| `open <url>` | Open a new digitizer. URL format: `dig2://<ip>`. Returns the assigned index. |
| `close <digi>` | Close a digitizer and release its CAEN connection. |

#### Parameter Access

Parameters use CAEN path syntax. Board-level: `/par/<name>`. Per-channel: `/ch/<N>/par/<name>`. Commands: `/cmd/<name>`.

| Command | Description |
|---------|-------------|
| `read <digi> <param>` | Read a parameter. Returns the value as a string. |
| `write <digi> <param> <value>` | Write a parameter value. |
| `cmd <digi> <command>` | Send a command (e.g. `/cmd/Reset`, `/cmd/armacquisition`). |

Examples:
```
read 0 /par/SerialNum
read 0 /ch/3/par/SelfTrgRate
write 0 /ch/0/par/ChTriggerThreshold 100
cmd 0 /cmd/Reset
```

#### Acquisition Control

| Command | Description |
|---------|-------------|
| `format <digi> <fmt>` | Set data format before starting ACQ. `<fmt>` is an integer (see Data Format Codes). |
| `start <digi\|all> <fmt> [save <path>]` | Start acquisition. `<fmt>` = data format code. If `save <path>` is given, broker saves data to files at that path. Use `all` instead of an index to start all digitizers. |
| `stop <digi\|all>` | Stop acquisition. Closes output files if saving. |
| `status` | Print ACQ on/off status for every digitizer. |

Examples:
```
start 0 2 save /data/exp01_run001      # start digi 0, NoTrace format, save to file
start all 4                             # start all digis, Minimum format, no save
stop all
```

#### File Management

| Command | Description |
|---------|-------------|
| `file-status <digi>` | Print total file size and current file size in MB. |
| `save-settings <digi> [file]` | Save current digitizer settings to a text file. |
| `load-settings <digi> [file]` | Load settings from file and program the digitizer. |

#### Monitoring

| Command | Description |
|---------|-------------|
| `subscribe [seconds]` | Listen and print scalar updates (trigger rates, accept rates, file sizes) for N seconds. Default: 10. |

#### Lifecycle

| Command | Description |
|---------|-------------|
| `shutdown` | Tell the broker to shut down. The broker will stop all ACQ, close all files, close all digitizers, and exit. |
| `quit` / `exit` | Exit the CLI (does not affect the broker). |

### Data Format Codes

| Code | Name | Contents |
|------|------|----------|
| 0 | ALL | All event fields + 2 analog traces + 4 digital traces |
| 1 | OneTrace | Event fields + 1 analog trace |
| 2 | NoTrace | Event fields only (channel, energy, timestamp, flags). No waveforms. |
| 3 | MiniWithFineTime | Channel, energy, timestamp, fine timestamp only |
| 4 | Minimum | Channel, energy, timestamp only |
| 5 | Raw | Raw binary blob from digitizer endpoint |

---

## Binary Protocol Specification

This section fully specifies the wire format. An AI agent or script can use this to construct and parse messages directly over ZMQ without using the C++ BrokerClient library.

### Encoding Rules

- **Byte order**: All multi-byte integers are **little-endian**.
- **Header**: Every message starts with exactly 2 bytes: `[uint8 msgType, uint8 flags]`. `flags` is reserved and must be set to `0x00`.
- **String encoding**: `[uint16 length][length bytes of UTF-8 data]`. No null terminator.
- **Float**: IEEE 754 32-bit, little-endian (same as `struct.pack("<f", val)` in Python).
- **Boolean**: `uint8`, where `0x00` = false, `0x01` = true.

### Request Messages (client sends on REQ socket)

After sending a request, the client MUST wait for exactly one response before sending another request. This is enforced by ZMQ REQ/REP.

#### PING (0xF0)
```
Bytes: [0xF0, 0x00]
Response: PONG
```

#### OPEN_DIGI (0x01)
```
Bytes: [0x01, 0x00, string url]
  url: e.g. "dig2://192.168.0.254"
Response: OK with payload [uint8 assigned_index], or ERROR
```

#### CLOSE_DIGI (0x02)
```
Bytes: [0x02, 0x00, uint8 digi_index]
Response: OK or ERROR
```

#### LIST_DIGI (0x03)
```
Bytes: [0x03, 0x00]
Response: DIGI_LIST
```

#### READ_VALUE (0x10)
```
Bytes: [0x10, 0x00, uint8 digi_index, string param]
  param: CAEN parameter path, e.g. "/par/SerialNum" or "/ch/0/par/SelfTrgRate"
Response: VALUE (string) or ERROR
```

#### WRITE_VALUE (0x12)
```
Bytes: [0x12, 0x00, uint8 digi_index, string param, string value]
Response: OK or ERROR
```

#### SEND_COMMAND (0x14)
```
Bytes: [0x14, 0x00, uint8 digi_index, string command]
  command: e.g. "/cmd/Reset" or "/cmd/armacquisition"
Response: OK or ERROR
```

#### START_ACQ (0x20)
```
Bytes: [0x20, 0x00, uint8 digi_index, uint8 data_format, uint8 save_data, string file_name_base]
  digi_index: 0-based index, or 0xFF for all digitizers
  data_format: 0-5 (see Data Format Codes)
  save_data: 0x00 = no save, 0x01 = save to file
  file_name_base: path prefix for output files (ignored if save_data=0, but string must still be present -- use empty string "")
Response: OK or ERROR
Side effects: starts ReadDataLoop thread, begins broadcasting PUB_HIT_SUMMARY and PUB_TRACE
```

#### STOP_ACQ (0x21)
```
Bytes: [0x21, 0x00, uint8 digi_index]
  digi_index: 0-based index, or 0xFF for all
Response: OK or ERROR
Side effects: stops ReadDataLoop thread, closes output files
```

#### SET_DATA_FORMAT (0x22)
```
Bytes: [0x22, 0x00, uint8 digi_index, uint8 format]
Response: OK or ERROR
```

#### GET_ACQ_STATUS (0x23)
```
Bytes: [0x23, 0x00]
Response: ACQ_STATUS
```

#### OPEN_FILE (0x30)
```
Bytes: [0x30, 0x00, uint8 digi_index, string file_name]
Response: OK or ERROR
```

#### CLOSE_FILE (0x31)
```
Bytes: [0x31, 0x00, uint8 digi_index]
Response: OK or ERROR
```

#### SET_SAVE_DATA (0x32)
```
Bytes: [0x32, 0x00, uint8 digi_index, uint8 on_off]
Response: OK or ERROR
```

#### GET_FILE_STATUS (0x33)
```
Bytes: [0x33, 0x00, uint8 digi_index]
Response: FILE_STATUS
```

#### READ_ALL_SETTINGS (0x40)
```
Bytes: [0x40, 0x00, uint8 digi_index]
Response: OK or ERROR
Note: reads all parameters from digitizer hardware into broker memory cache
```

#### SAVE_SETTINGS_FILE (0x41)
```
Bytes: [0x41, 0x00, uint8 digi_index, string file_name]
  file_name: can be empty string to use default path
Response: OK or ERROR
```

#### LOAD_SETTINGS_FILE (0x42)
```
Bytes: [0x42, 0x00, uint8 digi_index, string file_name]
  file_name: can be empty string to use default path
Response: OK or ERROR
Note: loads settings from file, writes them to digitizer hardware
```

#### GET_DIGI_INFO (0x50)
```
Bytes: [0x50, 0x00, uint8 digi_index]
Response: DIGI_INFO or ERROR
```

#### SHUTDOWN (0xFF)
```
Bytes: [0xFF, 0x00]
Response: OK (broker will exit after sending this reply)
```

### Response Messages (broker sends on REP socket)

#### OK (0x80)
```
Bytes: [0x80, 0x00]
  or for OPEN_DIGI: [0x80, 0x00, uint8 assigned_index]
```

#### ERROR (0x81)
```
Bytes: [0x81, 0x00, string error_message]
```

#### DIGI_LIST (0x82)
```
Bytes: [0x82, 0x00, uint8 n_digi, <repeated n_digi times:>
  uint8 is_connected   (0 or 1)
  uint16 serial_number
  string model_name    (e.g. "VX2740")
  string fpga_type     (e.g. "DPP_PHA")
  uint16 n_channels
]
```

#### VALUE (0x83)
```
Bytes: [0x83, 0x00, string value]
  value is always a string (even for numeric parameters)
```

#### DIGI_INFO (0x84)
```
Bytes: [0x84, 0x00,
  uint16 serial_number,
  string model_name,
  string fpga_type,
  uint16 n_channels,
  uint16 tick_to_ns,
  uint32 fpga_version,
  uint32 cup_version
]
```

#### ACQ_STATUS (0x85)
```
Bytes: [0x85, 0x00, uint8 n_digi, <repeated n_digi times:>
  uint8 acq_on   (0 = off, 1 = on)
]
```

#### FILE_STATUS (0x86)
```
Bytes: [0x86, 0x00, uint64 total_file_size_bytes, uint32 current_file_size_bytes]
```

#### PONG (0xF0)
```
Bytes: [0xF0, 0x00]
```

### Broadcast Messages (broker sends on PUB socket)

These are sent periodically by the broker while acquisition is running. Clients subscribe by connecting a ZMQ SUB socket and subscribing to all messages (`zmq_setsockopt(sub, ZMQ_SUBSCRIBE, "", 0)`).

#### PUB_SCALAR (0xC0)

Sent every `--scalar-interval` seconds (default 2.0) for each digitizer with ACQ running.

```
Bytes: [0xC0, 0x00,
  uint8  digi_index,
  uint16 serial_number,
  uint8  n_channels,
  <repeated n_channels times:>
    uint32 trigger_rate,        (counts per second, read from hardware)
    uint64 saved_count,         (total events saved since ACQ start)
    float32 accept_rate,        (computed events/sec accepted, based on delta since last broadcast)
    uint64 real_time_ns,        (channel real time in nanoseconds)
  uint64 total_file_size_bytes, (total bytes written across all output files)
  uint8  acq_on                 (0 or 1)
]
```

#### PUB_HIT_SUMMARY (0xC1)

Sent every scalar interval. Contains new hit energy values accumulated since last broadcast. Used by clients to fill histograms.

```
Bytes: [0xC1, 0x00,
  uint8  digi_index,
  uint16 n_hits,
  <repeated n_hits times:>
    uint8  channel,
    uint16 energy,
    uint16 energy_short    (non-zero only for DPP_PSD firmware)
]
```

Each hit is exactly 5 bytes. Max ~1000 hits per channel per broadcast.

#### PUB_TRACE (0xC2)

Sent when a new waveform trace is available (during ACQ with trace-enabled data formats).

```
Bytes: [0xC2, 0x00,
  uint8  digi_index,
  uint8  channel,
  uint32 trace_length,         (number of samples)
  <trace_length x int32>      analog_probe_0 samples,
  <trace_length x int32>      analog_probe_1 samples,
  <trace_length x uint8>      digital_probe_0 samples,
  <trace_length x uint8>      digital_probe_1 samples,
  <trace_length x uint8>      digital_probe_2 samples,
  <trace_length x uint8>      digital_probe_3 samples
]
```

#### PUB_STATUS_CHANGE (0xC3)

Sent when a state change occurs.

```
Bytes: [0xC3, 0x00, uint8 event_code, uint8 digi_index]

Event codes:
  0x01 = ACQ_STARTED
  0x02 = ACQ_STOPPED
  0x03 = FILE_OPENED
  0x04 = FILE_CLOSED
  0x05 = DIGI_OPENED
  0x06 = DIGI_CLOSED
```

#### PUB_LOG_MESSAGE (0xC4)

Free-form text log from the broker.

```
Bytes: [0xC4, 0x00, string message]
```

---

## Common CAEN Parameter Paths

These are the parameter paths used with READ_VALUE and WRITE_VALUE. The broker passes them directly to `CAEN_FELib_GetValue` / `CAEN_FELib_SetValue`.

### Board-level parameters (path format: `/par/<name>`)

| Path | Description | Type |
|------|-------------|------|
| `/par/SerialNum` | Serial number | read-only |
| `/par/ModelName` | Model name (VX2740, VX2730, etc.) | read-only |
| `/par/FwType` | Firmware type (DPP_PHA, DPP_PSD) | read-only |
| `/par/NumCh` | Number of channels | read-only |
| `/par/ADC_SamplRate` | ADC sampling rate in MHz | read-only |
| `/par/ClockSource` | Clock source (Internal, FPClkIn) | read-write |
| `/par/StartSource` | Start source (SWcmd, SIN, LVDS, etc.) | read-write |
| `/par/GlobalTriggerSource` | Global trigger source | read-write |
| `/par/TestPulsePeriod` | Test pulse period in ns | read-write |
| `/par/TestPulseWidth` | Test pulse width in ns | read-write |

### Channel-level parameters (path format: `/ch/<N>/par/<name>`)

Replace `<N>` with channel number (0-63), or use `0..63` for all channels.

| Path | Description | Type |
|------|-------------|------|
| `/ch/<N>/par/ChEnable` | Channel enable (true/false) | read-write |
| `/ch/<N>/par/ChTriggerThreshold` | Trigger threshold (ADC counts) | read-write |
| `/ch/<N>/par/ChDCOffset` | DC offset (percentage, 0-100) | read-write |
| `/ch/<N>/par/SelfTrgRate` | Self-trigger rate (counts/sec) | read-only |
| `/ch/<N>/par/ChannelRealtime` | Real time in ns since ACQ start | read-only |
| `/ch/<N>/par/ChannelSavedCount` | Total saved event count | read-only |
| `/ch/<N>/par/TimeFilterRiseTime` | Energy filter rise time | read-write |
| `/ch/<N>/par/EnergyFilterRiseTime` | Energy filter rise time | read-write |
| `/ch/<N>/par/EnergyFilterFlatTop` | Energy filter flat top | read-write |
| `/ch/<N>/par/EnergyFilterPoleZero` | Pole-zero correction | read-write |

### Commands (path format: `/cmd/<name>`)

| Path | Description |
|------|-------------|
| `/cmd/Reset` | Software reset |
| `/cmd/armacquisition` | Arm the acquisition (clear buffers) |
| `/cmd/swstartacquisition` | Software start trigger |
| `/cmd/SwStopAcquisition` | Software stop trigger |
| `/cmd/disarmacquisition` | Disarm the acquisition |

Note: START_ACQ and STOP_ACQ protocol messages handle arming/disarming automatically. Use SEND_COMMAND for these only if you need manual control.

---

## Python Client Example

Complete example showing how to connect, read parameters, start acquisition, and receive scalar updates:

```python
import zmq
import struct
import time

def pack_string(s: str) -> bytes:
    """Encode string as [uint16 length][utf-8 bytes]"""
    b = s.encode("utf-8")
    return struct.pack("<H", len(b)) + b

def unpack_string(data: bytes, offset: int) -> tuple[str, int]:
    """Decode string, return (value, new_offset)"""
    slen = struct.unpack_from("<H", data, offset)[0]
    offset += 2
    value = data[offset:offset + slen].decode("utf-8")
    return value, offset + slen

class BrokerClient:
    def __init__(self, host="localhost", cmd_port=5555, pub_port=5556):
        self.ctx = zmq.Context()
        self.cmd = self.ctx.socket(zmq.REQ)
        self.cmd.setsockopt(zmq.SNDTIMEO, 5000)
        self.cmd.setsockopt(zmq.RCVTIMEO, 5000)
        self.cmd.connect(f"tcp://{host}:{cmd_port}")
        self.sub = self.ctx.socket(zmq.SUB)
        self.sub.connect(f"tcp://{host}:{pub_port}")
        self.sub.subscribe(b"")

    def _request(self, data: bytes) -> bytes:
        self.cmd.send(data)
        return self.cmd.recv()

    def ping(self) -> bool:
        reply = self._request(bytes([0xF0, 0x00]))
        return reply[0] == 0xF0

    def list_digitizers(self) -> list[dict]:
        reply = self._request(bytes([0x03, 0x00]))
        if reply[0] != 0x82:
            return []
        off = 2
        n = reply[off]; off += 1
        result = []
        for i in range(n):
            connected = reply[off]; off += 1
            sn = struct.unpack_from("<H", reply, off)[0]; off += 2
            model, off = unpack_string(reply, off)
            fpga, off = unpack_string(reply, off)
            nch = struct.unpack_from("<H", reply, off)[0]; off += 2
            result.append({"index": i, "connected": bool(connected),
                           "sn": sn, "model": model, "fpga": fpga, "nch": nch})
        return result

    def read_value(self, digi: int, param: str) -> str:
        msg = bytes([0x10, 0x00, digi]) + pack_string(param)
        reply = self._request(msg)
        if reply[0] == 0x83:
            val, _ = unpack_string(reply, 2)
            return val
        return ""

    def write_value(self, digi: int, param: str, value: str) -> bool:
        msg = bytes([0x12, 0x00, digi]) + pack_string(param) + pack_string(value)
        reply = self._request(msg)
        return reply[0] == 0x80

    def start_acq(self, digi: int, fmt: int, save: bool, path: str = "") -> bool:
        msg = bytes([0x20, 0x00, digi, fmt, int(save)]) + pack_string(path)
        reply = self._request(msg)
        return reply[0] == 0x80

    def stop_acq(self, digi: int) -> bool:
        reply = self._request(bytes([0x21, 0x00, digi]))
        return reply[0] == 0x80

    def shutdown(self):
        self._request(bytes([0xFF, 0x00]))

    def recv_pub(self, timeout_ms=1000) -> tuple[int, bytes] | None:
        """Receive one PUB message. Returns (msg_type, raw_bytes) or None on timeout."""
        if self.sub.poll(timeout_ms):
            data = self.sub.recv()
            return data[0], data
        return None


# Usage example:
if __name__ == "__main__":
    client = BrokerClient()

    if client.ping():
        print("Broker is alive")

    digis = client.list_digitizers()
    for d in digis:
        print(f"  Digi {d['index']}: SN={d['sn']} Model={d['model']} FPGA={d['fpga']} NCh={d['nch']}")

    if digis:
        sn = client.read_value(0, "/par/SerialNum")
        print(f"  Serial number: {sn}")

        # Start ACQ with NoTrace format, saving to /data/run001
        client.start_acq(0, 2, True, "/data/run001")

        # Listen for scalar updates for 10 seconds
        end = time.time() + 10
        while time.time() < end:
            msg = client.recv_pub(timeout_ms=500)
            if msg and msg[0] == 0xC0:  # PUB_SCALAR
                digi_idx = msg[1][2]
                print(f"  Scalar update for digi {digi_idx}")

        client.stop_acq(0)
```

---

## File Structure

```
broker/
  BrokerProtocol.h     Message type enums, binary pack/unpack inline helpers, endpoint constants
  BrokerServer.h/.cpp  Broker daemon: ZMQ sockets, command dispatch, ReadDataLoop, ScalarBroadcastLoop
  BrokerClient.h/.cpp  C++ client library: command sending, PUB subscription, local ring buffers
  solaris-broker.cpp   Daemon entry point (CLI arg parsing, signal handling)
  solaris-cli.cpp      Interactive CLI client
  Makefile             Build system (depends on libzmq, libCAEN_FELib, libcurl)
  README.md            This file
```
