# CAEN x2730 Raw Data Format (DPP-PSD / DPP-PHA)

Raw data from the `/endpoint/raw` endpoint, decoded by `RawDecoder.h`.

**1 word = 64 bits = 8 bytes.** All data is **big-endian** byte order.

Reference: CAEN x2730 DPP-PSD CUP documentation (firmware 2025052203).

---

## Blob Structure

Each `ReadData()` call returns a blob containing one or more aggregates. The first 4 bits of each aggregate (`[63:60]`) identify its type:

```
┌─────────────────────────────┐
│  Aggregate 0 (Start Run)    │  [63:60] = 0x3, see "Start Run Event" below
├─────────────────────────────┤
│  Aggregate 1 (Data)         │  [63:60] = 0x2, see "Aggregate Header" below
├─────────────────────────────┤
│  Aggregate 2 (Data)         │  [63:60] = 0x2, contains N events
├─────────────────────────────┤
│  ...                        │
├─────────────────────────────┤
│  Aggregate N (Stop Run)     │  [63:60] = 0x3, see "Stop Run Event" below
└─────────────────────────────┘

  [63:60] = 0x2 → Individual Trigger Mode (data events)
  [63:60] = 0x3 → Special Event (Start Run / Stop Run)
```

---

## Start Run Event (format = 0x3, 4 words)

Bits [63:60] = 0x3 identifies a Special Event. Bits [59:56] = subtype.

```
Word 0:
 63  60 59  56 55                               32 31                          0
┌──────┬──────┬─────────────────────────────────────┬──────────────────────────┐
│ 0x3  │ 0x0  │             reserved                │    n_words = 0x4         │
│format│start │                                     │                          │
└──────┴──────┴─────────────────────────────────────┴──────────────────────────┘

Example: 0x3000000300000004

┌────────────────────────────────────────────────────────────────────┐
│ Word 0: format=0x3, subtype=0x0 (start run), n_words=4           │
├────────────────────────────────────────────────────────────────────┤
│ Word 1: acquisition width, n_traces, decimation factor            │
├────────────────────────────────────────────────────────────────────┤
│ Word 2: channel mask [31:0]                                       │
├────────────────────────────────────────────────────────────────────┤
│ Word 3: channel mask [63:32]                                      │
└────────────────────────────────────────────────────────────────────┘
```

## Stop Run Event (format = 0x3, 3 words)

```
Word 0:
 63  60 59  56 55                               32 31                          0
┌──────┬──────┬─────────────────────────────────────┬──────────────────────────┐
│ 0x3  │ 0x2  │             reserved                │    n_words = 0x3         │
│format│ stop │                                     │                          │
└──────┴──────┴─────────────────────────────────────┴──────────────────────────┘

┌────────────────────────────────────────────────────────────────────┐
│ Word 0: format=0x3, subtype=0x2 (stop run), n_words=3            │
├────────────────────────────────────────────────────────────────────┤
│ Word 1: run-end timestamp (48 bits, 1 LSB = 8 ns)                │
├────────────────────────────────────────────────────────────────────┤
│ Word 2: total dead time (48 bits, 1 LSB = 8 ns)                  │
└────────────────────────────────────────────────────────────────────┘
```

---

## Aggregate Header (format = 0x2, Individual Trigger Mode)

```
 63  60 59 58 57 56 55                          32 31                           0
┌──────┬──┬─────┬──┬──────────────────────────────┬──────────────────────────────┐
│ 0x2  │fl│ rsv │bf│       aggregate counter      │        n. aggregate words    │
│4 bits│1 │  2  │1 │          24 bits              │           32 bits            │
└──────┴──┴─────┴──┴──────────────────────────────┴──────────────────────────────┘
  fl = flush,  bf = board_fail
```

Followed by `n_words - 1` words of interleaved events (all channels, sorted by timestamp).

---

## Data Event -- No Waveform (2 words)

```
Word 0:
 63 62       56 55 54 53    48 47                                               0
┌──┬──────────┬─────┬────────┬──────────────────────────────────────────────────┐
│ 0│  channel  │s.e. │reserved│                   timestamp                      │
│1b│  7 bits   │2 b  │ 6 bits │                   48 bits                        │
└──┴──────────┴─────┴────────┴──────────────────────────────────────────────────┘
  bit 63 = 0 (not single-word)
  s.e. bit 55 = 1 means time/counter event (see below)
  timestamp: 1 LSB = 8 ns

Word 1:
 63 62 61          50 49       42 41          26 25       16 15                 0
┌──┬──┬─────────────┬───────────┬──────────────┬───────────┬────────────────────┐
│la│W │flags_low_pri │flags_high │ energy_short │ fine_ts   │      energy        │
│1b│1b│   12 bits    │  8 bits   │  16 bits     │ 10 bits   │     16 bits        │
└──┴──┴─────────────┴───────────┴──────────────┴───────────┴────────────────────┘
  la = last header word (1 = no extra words follow)
  W  = waveform flag (0 = no waveform)
  energy_short: PSD only (reserved/zero for PHA)
  fine_ts: 1 LSB = 7.8125 ps
```

---

## Data Event -- With Waveform (W = 1)

```
┌────────────────────────────────┐
│ Word 0  (same as above)        │  bit 63 = 0
├────────────────────────────────┤
│ Word 1  (same, but W=1)       │  bit 63 = 0 (extra words follow)
├────────────────────────────────┤
│ Extra words (optional)         │  bit 63 = 0
├────────────────────────────────┤
│ Waveform Extra Word            │  bit 63 = 1 (last header word)
├────────────────────────────────┤
│ Waveform Header Word           │  contains wf_n_words
├────────────────────────────────┤
│ Waveform Sample Word 0         │  2 samples per word
│ Waveform Sample Word 1         │
│ ...                            │
│ Waveform Sample Word N-1       │
└────────────────────────────────┘
```

### Waveform Extra Word (last header word)

```
 63 62  60 59 56 55    50 49 48 47                32 31  28 27  24 23  20 19  16 15  10 9    4 3   0
┌──┬─────┬─────┬────────┬─────┬────────────────────┬──────┬──────┬──────┬──────┬──────┬──────┬─────┐
│ 1│     │ 0x0 │reserved│t.res│  trigger_threshold  │ dp3  │ dp2  │ dp1  │ dp0  │ ap1  │ ap0  │ rsv │
│1b│     │ 4b  │  6b    │ 2b  │      16 bits        │  4b  │  4b  │  4b  │  4b  │  6b  │  6b  │ 4b  │
└──┴─────┴─────┴────────┴─────┴────────────────────┴──────┴──────┴──────┴──────┴──────┴──────┴─────┘
  t.res = time resolution (0=none, 1=x2, 2=x4, 3=x8 downsampling)
  dpN   = digital probe N type (4 bits)
  apN   = analog probe N info: type[2:0], is_signed[3], mult_factor[5:4]
```

### Waveform Header Word

```
 63                                             32 31                           0
┌────────────────────────────────────────────────┬──────────────────────────────┐
│                  reserved                       │       wf_n_words             │
│                  32 bits                        │        32 bits               │
└────────────────────────────────────────────────┴──────────────────────────────┘
  n_samples = wf_n_words * 2
```

### Waveform Sample Packing

Each 64-bit word = 2 samples (lower half = even index, upper half = odd index):

```
 One 32-bit sample:
 31 30 29           16 15 14 13                  0
┌──┬──┬───────────────┬──┬──┬─────────────────────┐
│d3│d2│ analog_probe_1 │d1│d0│   analog_probe_0    │
│1b│1b│   14-bit signed│1b│1b│   14-bit signed     │
└──┴──┴───────────────┴──┴──┴─────────────────────┘
  d0-d3 = digital probes (1 bit each)
  analog probes: sign-extend from bit 13

 64-bit word layout:
 63                              32 31                              0
┌─────────────────────────────────┬─────────────────────────────────┐
│          sample 1 (odd)         │        sample 0 (even)          │
│           32 bits               │           32 bits               │
└─────────────────────────────────┴─────────────────────────────────┘
```

---

## Single-Word Event (EnDataReduction mode)

```
 63 62       56 55       48 47                               16 15              0
┌──┬──────────┬───────────┬──────────────────────────────────┬──────────────────┐
│ 1│  channel  │flags_high │         reduced timestamp        │     energy       │
│1b│  7 bits   │  8 bits   │         32 bits (LSBs only)      │    16 bits       │
└──┴──────────┴───────────┴──────────────────────────────────┴──────────────────┘
  bit 63 = 1 (single-word marker)
  No energy_short, no fine_timestamp, no waveform.
```

---

## Time/Counter Event (EnStatEvents = true)

Identified by bit 55 of Word 0 being set. Not a physics hit -- contains per-channel statistics.

```
┌────────────────────────────────┐
│ Word 0  (ch, timestamp)        │  bit 55 = 1, timestamp = real_time
├────────────────────────────────┤
│ Word 1  (meaningless, skip)    │
├────────────────────────────────┤
│ Extra Word 0: dead_time        │  [47:0] = dead_time (48 bits, 1 LSB = 8 ns)
├────────────────────────────────┤
│ Extra Word 1: counters         │  [47:24] = trigger_count (24 bits)
│                                │  [23:0]  = saved_event_count (24 bits)
└────────────────────────────────┘
```

---

## Output Content Configuration

### Firmware-side parameters (control what the digitizer writes into the blob)

| Parameter | Scope | Effect |
|-----------|-------|--------|
| `EnDataReduction` | board | Switches all events to single-word format — eliminates waveform, fine timestamp, and `energy_short` |
| `EnStatEvents` | board | Embeds time/counter stat events in the blob; **forced `true`** in raw mode (required for scaler readout) |
| `ChRecordLengthT` | channel | Waveform window length (ns); determines number of waveform sample words per event |
| `ChPreTriggerT` | channel | Pre-trigger offset (ns) within the waveform window |
| `WaveAnalogProbe0` / `WaveAnalogProbe1` | channel | Selects which analog signal is stored in each analog probe slot (e.g. ADC input, energy filter, CFD) |
| `WaveDigitalProbe0`–`3` | channel | Selects which digital signal is stored in each digital probe slot (e.g. Trigger, Long Gate, Pile-up) |

To acquire **no waveforms**, enable `EnDataReduction` — events become single-word and the `W` flag is never set.

To acquire **waveforms**, leave `EnDataReduction` disabled and set `ChRecordLengthT` > 0. The probe type fields in the Waveform Extra Word reflect the `WaveAnalogProbe` / `WaveDigitalProbe` settings at acquisition time.

### Decoder-side (`RawDecoder` / `EventBuilderRaw`)

`RawDecoder::LoadBlob()` takes a `decodeWaveform` flag:
- `false` (default in online DAQ): waveform words are skipped; only energy, timestamp, and flags are extracted.
- `true`: waveform samples are fully parsed into `analog_probes_0/1` and `digital_probes_0–3` vectors.

Waveform data is always written verbatim to the `.sol_raw` file regardless of this flag — decoding is a read-time choice.

`EventBuilderRaw` (offline) does not decode waveforms; it extracts energy, timestamp, and flags only.

---

## .sol_raw File Format

```
┌──────────────┬──────────────┬──────────────────────────┐
│  identifier  │  blob_size   │       raw blob data      │
│   2 bytes    │   8 bytes    │      blob_size bytes     │
├──────────────┼──────────────┼──────────────────────────┤
│  identifier  │  blob_size   │       raw blob data      │
├──────────────┼──────────────┼──────────────────────────┤
│  ...         │  ...         │       ...                │
└──────────────┴──────────────┴──────────────────────────┘
  identifier: 0xAA0A (PHA) or 0xAA1A (PSD)
  blob_size:  uint64_t, number of bytes in raw blob
  blob data:  big-endian 64-bit words (aggregates)
```

Use `EventBuilderRaw` to decode `.sol_raw` files into ROOT trees.

---

## Flags Reference

### High Priority (8 bits)

| Bit | Name | Description |
|-----|------|-------------|
| 0 | Pile-Up | Two events overlapping before peaking time |
| 2 | Event Saturation | Input dynamics saturated |
| 3 | Post Saturation | Event during ADCVetoWidth window |
| 4 | Charge Overflow | Integrated charge overflow (PSD) |
| 5 | SCA Selected | Event in SCA window |
| 6 | Fine TS Valid | Fine timestamp properly calculated |

### Low Priority (12 bits)

| Bit | Name | Description |
|-----|------|-------------|
| 0 | Ext. Inhibit | Waveform during external inhibit |
| 1 | Under-sat. | Under-saturation waveform |
| 2 | Over-sat. | Over-saturation waveform |
| 3 | Ext. Trigger | TRG-IN connector |
| 4 | Global Trigger | Global trigger condition |
| 5 | SW Trigger | Software trigger |
| 6 | Self Trigger | Channel self trigger |
| 7 | LVDS Trigger | LVDS connector |
| 8 | 64-ch Trigger | Other channel(s) combination |
| 9 | ITLA Trigger | ITLA logic |
| 10 | ITLB Trigger | ITLB logic |

---

## Time Constants

```
  Timestamp:      1 LSB = 8 ns
  Fine timestamp: 1 LSB = 7.8125 ps
  Dead time:      1 LSB = 8 ns

  VX2730 (500 Msps):  tick2ns = 2,  ReadValue multiplies by 4
  VX2740 (125 Msps):  tick2ns = 8,  no extra scaling
```
