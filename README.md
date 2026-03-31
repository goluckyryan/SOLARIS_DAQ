# SOLARIS DAQ

Data acquisition system for the SOLARIS (SOLenoid And Resonance Ionization Spectroscopy) detector at FRIB, using CAEN x2730 series digitizers (VX2740, VX2745, VX2730) with DPP-PHA and DPP-PSD firmware.

## Project Structure

```
SOLARIS_DAQ/
├── Makefile              # Top-level: builds GUI + broker
├── core/                 # Shared code (no Qt dependency)
│   ├── ClassDigitizer2Gen.h/cpp   # Digitizer hardware control
│   ├── DigiManager.h/cpp          # Unified interface (standalone/broker)
│   ├── Hit.h                      # Event data structures
│   ├── RawDecoder.h               # Raw binary decoder
│   ├── RingBuffer.h               # Lock-free circular buffer
│   ├── DigiParameters.h           # Register definitions (PHA/PSD)
│   ├── ClassInfluxDB.h/cpp        # InfluxDB client
│   └── macro.h                    # Global constants
├── GUI/                  # Qt6 GUI application
│   ├── SOLARIS_DAQ.pro            # qmake project file
│   ├── mainwindow.h/cpp           # Main window: run control, scalars
│   ├── digiSettingsPanel.h/cpp    # Register editing panel
│   ├── scope.h/cpp                # Oscilloscope waveform display
│   ├── SingleSpectra.h/cpp        # Energy spectra histograms
│   ├── SOLARISpanel.h/cpp         # SOLARIS detector configuration
│   └── ...                        # Custom widgets, plotting
├── broker/               # Digitizer broker (daemon + CLI)
│   ├── BrokerServer.h/cpp         # ZMQ server: manages digitizers
│   ├── BrokerClient.h/cpp         # ZMQ client library
│   ├── BrokerProtocol.h           # Binary protocol definitions
│   ├── solaris-broker.cpp         # Broker daemon entry point
│   └── solaris-cli.cpp            # Command-line interface
└── Aux/                  # Offline tools (EventBuilder, etc.)
```

## Operating Modes

The DAQ supports two modes, selected automatically at startup.

### Standalone Mode

The GUI directly controls the digitizers. Only one GUI instance can run (enforced by DAQ lock file).

```
SOLARIS_DAQ ──── Digitizer Hardware
```

### Broker Mode

A broker daemon (`solaris-broker`) manages the digitizers. The GUI connects over ZMQ (TCP). Multiple GUIs can connect simultaneously from any machine on the network.

```
solaris-broker ──── Digitizer Hardware
     │  (tcp://  REQ/REP commands + PUB/SUB data)
     ├── SOLARIS_DAQ (GUI instance 1, local or remote)
     ├── SOLARIS_DAQ (GUI instance 2)
     └── solaris-cli  (command-line client)
```

### Auto-Detection

On startup, the GUI:

1. Reads `brokerIP` and `brokerCmdPort` from `programSettings.txt`
2. Sends a ZMQ ping to `tcp://{brokerIP}:{brokerCmdPort}` (1-second timeout)
3. If the broker responds: **broker mode** — auto-connects, syncs settings, DAQ lock skipped
4. If no broker found: **standalone mode** — DAQ lock active, manual digitizer open

No manual mode toggle is needed. If the broker is running, the GUI uses it.

## Broker Architecture

### Broker Lifecycle

```
solaris-broker [--ip DIG_IP] [--cmd-port 5555] [--pub-port 5556]
  │
  ├─ Start(): bind ZMQ REP (commands) + PUB (broadcast) sockets
  ├─ Open digitizers from --ip argument (if provided)
  └─ Run(): enter main loop
      │
      ├─ ScalarBroadcastLoop thread (started with Run)
      │   ├─ Every 100ms: publish hit summaries (during ACQ)
      │   ├─ Every 500ms: publish trace snapshots (only when data format includes waveforms)
      │   └─ Every 2s: publish scalars + board status (LED, ACQ, temperatures)
      │
      └─ Command loop: poll ZMQ REP, dispatch commands
          ├─ open/close/list digitizers
          ├─ read/write parameters
          ├─ start/stop ACQ → spawns/joins ReadDataLoop thread per digitizer
          ├─ file control (open/close/save)
          ├─ settings management (read all/save/load)
          ├─ ping (for GUI auto-detection)
          └─ shutdown
```

### Threads

| Thread | Lifetime | Purpose |
|--------|----------|---------|
| **Main** | Process start → exit | Command polling loop (REQ/REP) |
| **ScalarBroadcastLoop** | Run() → Stop() | Publishes hit summaries (100ms), traces (500ms), and scalars (2s) via PUB socket |
| **ReadDataLoop[i]** | StartACQ → StopACQ | Reads hits from digitizer *i* into ring buffers, saves to file |

Thread safety: per-digitizer `digiMutex[i]` protects all hardware access. `ReadDataLoop` yields between reads to allow scalar broadcasts and command handlers to acquire the mutex.

### Data Flow

```
Digitizer Hardware
  │ (CAEN FELib)
  ▼
ReadDataLoop: digi->ReadData() → fills ringBuffer[ch] + traceRingBuffer
  │
  ▼
ScalarBroadcastLoop (independent timers, non-blocking):
  ├─ PUB_HIT_SUMMARY (every 100ms): new hits from ring buffers → ZMQ PUB
  ├─ PUB_TRACE (every 500ms): latest waveform snapshot → ZMQ PUB
  │     (only when data format includes waveforms: ALL or OneTrace)
  └─ PUB_SCALAR (every 2s): trigger rates, accept rates, file size,
     board status (LED, ACQ, temperatures) → ZMQ PUB
  │
  ▼ (network)
BrokerClient::SubscriptionLoop:
  ├─ Pushes hits to client-side ringBuffer[digi][ch]
  ├─ Updates scalarData[digi] (cached, mutex-protected)
  └─ Fires callbacks: onScalarUpdate, onHitSummary, onTraceSnapshot
  │
  ▼
GUI reads from cached data (zero network calls for display)
```

### Parameter Caching (DigiManager)

All GUI components read parameters from a local memory cache, not from the hardware:

| Method | What it does | When to use |
|--------|-------------|-------------|
| `ReadValue(digi, reg, ch)` | Reads from hardware/broker, **updates cache** | Explicit "Read Settings" button, write-readback |
| `ReadValueFromCache(digi, reg, ch)` | Reads from local cache only, **zero network calls** | All UI display updates |
| `WriteValue(digi, reg, val, ch)` | Writes to hardware/broker, **reads back and caches** | User changes a setting |
| `ReadAllSettings(digi)` | Reads all parameters from hardware, **populates entire cache** | On connect, "Refresh Settings" button |

In broker mode, a local dummy `Digitizer2Gen` object stores the cache. On connect, `ReadAllSettings` syncs all values from the broker to the local cache.

## Setting Up Broker Mode

1. Start the broker on the machine connected to the digitizers:
   ```bash
   ./solaris-broker --ip 192.168.0.100
   ```
   The `--ip` flag opens the digitizer immediately. Without it, use the CLI to open later.

2. Optionally use the CLI:
   ```bash
   ./solaris-cli
   > open dig2://192.168.0.100
   > lsdig
   > read 0 /par/ModelName
   ```

3. Configure the GUI's `programSettings.txt` with the broker machine's IP:
   - Line 12: `brokerIP` (default: `localhost`)
   - Line 13: `brokerCmdPort` (default: `5555`)
   - Line 14: `brokerPubPort` (default: `5556`)

4. Start the GUI:
   ```bash
   ./SOLARIS_DAQ
   ```
   It auto-detects the broker, syncs digitizer settings, and is ready to use.

Multiple GUIs on different machines can connect by setting the broker IP in their `programSettings.txt`.

### Broker Behavior

- **Window title**: shows `[Broker : IP]` or `[Standalone]` to indicate current mode
- **GUI "Close Digitizers" button**: closes remote digitizers on the broker
- **GUI window close (X button)**: just disconnects, broker keeps digitizers open
- **Broker Ctrl+C**: gracefully stops all ACQ, closes all digitizers, exits
- **Multiple GUIs**: all see the same scalar/histogram data via PUB/SUB; commands are serialized via REQ/REP
- **DAQ lock**: only active in standalone mode; skipped in broker mode (multiple GUIs allowed)

## Build

### Prerequisites

- Ubuntu 22.04+
- Qt6: `sudo apt install qt6-base-dev libqt6charts6-dev`
- libcurl: `sudo apt install libcurl4-openssl-dev`
- libzmq: `sudo apt install libzmq3-dev`
- CAEN FELib: CAEN_FELib v1.2.2+
- CAEN Dig2: CAEN_DIG2 v1.5.3+
- libreadline: `sudo apt install libreadline-dev` (for solaris-cli)
- ROOT (for EventBuilder only)

### Compile

```bash
make          # builds GUI + broker + CLI
make gui      # GUI only
make broker   # broker + CLI only
make clean    # clean all
```

All executables are placed in the project root:
- `SOLARIS_DAQ` — GUI application
- `solaris-broker` — broker daemon
- `solaris-cli` — command-line client

### Compile Auxiliary Tools

```bash
cd Aux/
make EventBuilder   # requires ROOT
make test           # register and raw decode tests
```

### Using CAENDig2.h

```bash
cp caen_dig2-vXXXX/include/CAENDig2.h /usr/local/include/
```

## Data Formats

| Format | ID | Description |
|--------|----|-------------|
| ALL | 0x00 | All metadata + 2 analog probes + 4 digital probes |
| OneTrace | 0x01 | 1 analog probe + energy/timestamp/flags |
| NoTrace | 0x02 | Energy/timestamp/flags, no waveforms |
| Minimum | 0x03 | Channel/energy/timestamp only |
| MiniWithFineTime | 0x04 | Channel/energy/timestamp/fine_timestamp |
| Raw | 0x0A | Raw binary blob, decoded via RawDecoder |

## Program Settings

The `programSettings.txt` file stores all configuration:

```
Line  0: masterExpDataPath
Line  1: SubFolder/SingleFolder
Line  2: expName
Line  3: IPListStr (e.g., 192.168.0.100,102)
Line  4: analysisPath
Line  5: DatabaseIP
Line  6: DatabaseName
Line  7: DatabaseToken
Line  8: ElogIP
Line  9: ElogUser
Line 10: ElogPWD
Line 11: useBrokerMode (0/1, used as hint for DAQ lock; overridden by auto-detect)
Line 12: brokerIP (default: localhost)
Line 13: brokerCmdPort (default: 5555)
Line 14: brokerPubPort (default: 5556)
```

Lines 11-14 are optional. Old settings files without them use defaults (standalone mode, localhost broker).

## Supported Firmware

Tested with:
- V2745-dpp-pha-1G / V2740-dpp-pha-1G
- V2745-dpp-psd-1G / V2740-dpp-psd-1G
- VX2730 DPP-PSD (firmware 2025052203+)

## Known Issues

- The "Trig." Rate in the Scaler does not include the coincidence condition.
- LVDSTrgMask cannot be accessed.
- CoincidenceLengthT not loaded.
- Sometimes the digitizer halts after `/cmd/armacquisition` (CAEN library issue).
- Event/Wave trigger source cannot be set as SWTrigger.
- After CAEN_FELib v1.2.5 and CAEN_DIG2 v1.5.10, firmware before 202309XXXX not supported.

## Wiki

https://fsunuc.physics.fsu.edu/wiki/index.php/FRIB_SOLARIS_Collaboration
