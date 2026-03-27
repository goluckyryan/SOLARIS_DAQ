# SOLARIS DAQ

Data acquisition system for the SOLARIS (SOLenoid And Resonance Ionization Spectroscopy) detector at FRIB, using CAEN x2730 series digitizers (VX2740, VX2745, VX2730) with DPP-PHA and DPP-PSD firmware.

## Architecture

The core digitizer control classes are independent from the Qt UI classes.

### Core Classes

| File | Description |
|------|-------------|
| ClassDigitizer2Gen.h/cpp | Digitizer control: connection, configuration, data readout, file I/O |
| Hit.h | Event data structure for decoded hits |
| RawDecoder.h | Decoder for raw endpoint binary blob into individual hits |
| DigiParameters.h | Register definitions for DPP-PHA and DPP-PSD firmware |
| RingBuffer.h | Lock-free circular buffer for per-channel energy histograms |

### UI Classes (Qt6)

| File | Description |
|------|-------------|
| main.cpp | Application entry point |
| mainwindow.h/cpp | Main window: digitizer management, run control, scaler display |
| digiSettingsPanel.h/cpp | Register editing panel for board and channel settings |
| scope.h/cpp | Oscilloscope for waveform display |
| SingleSpectra.h/cpp | Per-channel energy spectra (1D histograms) |
| SOLARISpanel.h/cpp | SOLARIS-specific detector mapping and configuration |
| CustomThreads.h | ReadDataThread and TimingThread for async acquisition |
| CustomWidgets.h | Custom Qt widgets (RComboBox, etc.) |
| Histogram1D.h / Histogram2D.h | Histogram classes using QCustomPlot |
| qcustomplot.h/cpp | QCustomPlot plotting library |
| macro.h | Global constants and macros |

### Auxiliary Tools (Aux/)

| File | Description |
|------|-------------|
| EventBuilder.cpp | Offline event builder: merges .sol files, builds time-correlated events, outputs ROOT trees |
| SolReader.h | Reader for .sol binary data files |
| test.cpp | Comprehensive register and raw data decode tests |
| debug_raw.cpp | Dumps raw blob word contents and decoded hits for debugging |
| check_gate.cpp | Tests GateOffsetT read/write on the digitizer |

### Other

| File | Description |
|------|-------------|
| ClassInfluxDB.h/cpp | InfluxDB client for scaler rate logging |

## Data Formats

The digitizer supports multiple readout formats, selected via `SetDataFormat()`:

| Format | ID | Description |
|--------|----|-------------|
| ALL | 0x00 | All metadata + 2 analog probes + 4 digital probes |
| OneTrace | 0x01 | 1 analog probe + energy/timestamp/flags |
| NoTrace | 0x02 | Energy/timestamp/flags, no waveforms |
| Minimum | 0x03 | Channel/energy/timestamp only |
| MiniWithFineTime | 0x04 | Channel/energy/timestamp/fine_timestamp |
| Raw | 0x0A | Raw binary blob from digitizer, decoded in software via RawDecoder |

### Raw Mode

Raw mode reads from the `/endpoint/raw` endpoint, which returns many events per TCP transaction (vs. one event per call for decoded endpoints). The `RawDecoder` class unpacks the big-endian 64-bit word stream into individual hits, supporting:

- Normal 2-word events (channel, timestamp, energy, energy_short, fine_timestamp, flags)
- Single-word events (EnDataReduction mode)
- Waveform events (analog/digital probe data)
- Time/counter statistics events (for scaler display without extra TCP overhead)
- Start/Stop Run special events

**Status:** The raw decoding has been tested and verified against the decoded endpoint on VX2730 DPP-PSD (firmware 2025052203). Decoded channel, energy, timestamp, and fine_timestamp values match the decoded endpoint output. The `.sol` file round-trip (write then read back via SolReader) is also verified.

However, the saving/acquisition pipeline for Raw mode is **not yet fully optimized or enabled in the GUI**. The current implementation yields one decoded hit per `ReadData()` call (to preserve the existing `ReadDataThread` loop contract), which limits throughput to roughly the same as the decoded endpoint. To achieve higher throughput, the pipeline needs to be restructured to save entire decoded blobs in batch. Raw mode is not yet selectable from the GUI data format dropdown.

## Build

### Prerequisites

- Ubuntu 22.04+
- Qt6: `sudo apt install qt6-base-dev libqt6charts6-dev`
- libcurl: `sudo apt install libcurl4-openssl-dev`
- CAEN FELib: CAEN_FELib v1.2.2+ (install first)
- CAEN Dig2: CAEN_DIG2 v1.5.3+
- ROOT (for EventBuilder only)

### Compile the DAQ

```bash
qmake6 SOLARIS_DAQ.pro
make
```

### Compile auxiliary tools

```bash
# First compile the main project to generate ClassDigitizer2Gen.o
cd Aux/

# EventBuilder (requires ROOT)
make EventBuilder

# Register and raw decode test
make test
```

### Using CAENDig2.h

The CAENDig2.h header is not installed to the system include path by default. Copy it from the CAEN Dig2 source:

```bash
cp caen_dig2-vXXXX/include/CAENDig2.h /usr/local/include/
```

## Supported Firmware

Tested with:
- V2745-dpp-pha-1G / V2740-dpp-pha-1G
- V2745-dpp-psd-1G / V2740-dpp-psd-1G
- VX2730 DPP-PSD (firmware 2025052203+)

## Additional Features

### Analysis Working Directory

When the analysis path is set, the DAQ will:
- Save the expName.sh
- Save digitizer settings
- Load the Mapping.h from the working directory

### End Run Script

When a run stops, the DAQ executes the bash script at `scripts/endRunScript.sh`.

## Known Issues

- The "Trig." Rate in the Scaler does not include the coincidence condition. This is related to the ChSavedEventCnt from the firmware.
- LVDSTrgMask cannot be accessed.
- CoincidenceLengthT not loaded.
- Sometimes the digitizer halts after the `/cmd/armacquisition` command (CAEN library issue).
- Event/Wave trigger source cannot be set as SWTrigger.
- After updating to CAEN_FELib v1.2.5 and CAEN_DIG2 v1.5.10, firmware versions before 202309XXXX are not supported.

## Wiki

https://fsunuc.physics.fsu.edu/wiki/index.php/FRIB_SOLARIS_Collaboration
