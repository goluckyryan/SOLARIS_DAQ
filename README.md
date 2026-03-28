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

The DAQ supports two modes, selected automatically at startup:

### Standalone Mode

The GUI directly controls the digitizers. Only one GUI instance can run (enforced by DAQ lock file).

```
GUI (SOLARIS_DAQ) ──── Digitizer Hardware
```

### Broker Mode

A broker daemon (`solaris-broker`) manages the digitizers. The GUI connects to the broker over ZMQ (TCP). Multiple GUI instances can connect simultaneously.

```
solaris-broker ──── Digitizer Hardware
     │
     ├── GUI instance 1 (local or remote)
     ├── GUI instance 2
     └── solaris-cli (command line)
```

### Auto-Detection

On startup, the GUI:

1. Reads `brokerIP` and `brokerCmdPort` from `programSettings.txt`
2. Tries to ping the broker at `tcp://{brokerIP}:{brokerCmdPort}`
3. If the broker responds: **broker mode** (auto-connects, DAQ lock skipped)
4. If no broker found: **standalone mode** (DAQ lock active)

No manual mode toggle is needed. If the broker is running, the GUI uses it.

### Setting Up Broker Mode

1. Start the broker on the machine connected to the digitizers:
   ```bash
   ./solaris-broker
   ```
2. Open digitizers from the broker CLI:
   ```bash
   ./solaris-cli
   > open dig2://192.168.0.100
   ```
3. Configure the GUI's `programSettings.txt` with the broker machine's IP:
   - `brokerIP`: IP address of the broker machine (default: `localhost`)
   - `brokerCmdPort`: command port (default: `5555`)
   - `brokerPubPort`: publish port (default: `5556`)
4. Start the GUI — it auto-detects the broker and connects.

Multiple GUIs (on different machines) can connect to the same broker by setting the broker IP in their `programSettings.txt`.

### What Works in Each Mode

| Feature | Standalone | Broker |
|---------|-----------|--------|
| Open/Close digitizers | Yes | Yes |
| Start/Stop ACQ | Yes | Yes |
| Scalar display | Yes | Yes |
| Energy spectra | Yes | Yes |
| Scope (waveforms) | Yes | Yes |
| Digitizer settings panel | Yes | Yes (reads/writes via broker) |
| SOLARIS panel | Yes | Yes (reads/writes via broker) |
| Data file saving | Local | On broker machine |
| Multiple GUI instances | No (locked) | Yes |
| Remote access | No | Yes (over network) |

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

### Compile Everything

```bash
make          # builds GUI + broker + CLI
```

All executables are placed in the project root:
- `SOLARIS_DAQ` — GUI application
- `solaris-broker` — broker daemon
- `solaris-cli` — command-line client

### Compile Individual Targets

```bash
make gui      # GUI only
make broker   # broker + CLI only
make clean    # clean all
```

### Compile Auxiliary Tools

```bash
cd Aux/
make EventBuilder   # requires ROOT
make test           # register and raw decode tests
```

### Using CAENDig2.h

The CAENDig2.h header is not installed to the system include path by default:

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
Line 11: useBrokerMode (0/1, overridden by auto-detect)
Line 12: brokerIP (default: localhost)
Line 13: brokerCmdPort (default: 5555)
Line 14: brokerPubPort (default: 5556)
```

Lines 11-14 are optional for backward compatibility. Old settings files without these lines will use defaults (standalone mode, localhost broker).

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
