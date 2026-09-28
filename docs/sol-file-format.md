# The `.sol` file format

The on-disk format the DAQ writes for every normal run. `format_RAW.md` covers a different thing —
the raw endpoint blob and the `.sol_raw` container — so this is the one you need to read ordinary
SOLARIS data.

**The format is defined by two pieces of code that must agree byte for byte:**

| | |
|---|---|
| writer | `Digitizer2Gen::SaveDataToFile()` — `ClassDigitizer2Gen.cpp:1074` |
| reader | `SolReader::ReadNextBlock()` — `Aux/SolReader.h:105` |

If you change one, change the other in the same commit, and bear in mind that **every file already
written becomes unreadable** if the layout shifts. There is no version field to protect you.

---

## File naming and rollover

```
<expName>_<runID>_<digiIdx>_<serialNumber>_<fileIndex>.sol
```

built in `MainWindow::StartACQ` (`mainwindow.cpp:498-511`) and finished by
`Digitizer2Gen::OpenOutFile()`, which appends `_%03d` and the extension. `digiIdx` is
right-justified to 2 digits, `fileIndex` to 3.

- Extension is **`.sol_raw`** when the data format is `Raw`, **`.sol`** otherwise.
- One file per **board**, so a run with 3 boards produces at least 3 files.
- At `MaxOutFileSize` (2 GB) the writer closes the file, increments `fileIndex` and opens the next.
  Those parts are a single continuous stream — the offline `Aux/EventBuilder` groups them by
  `digiIdx` and treats the group as one logical stream.
- `CloseOutFile()` `chmod`s the file to read-only, which is why a finished run cannot be
  accidentally overwritten.

Alongside each run, `StartACQ` also writes `<expName>_<runID>XSetting_<serialNumber>.dat` — the
CAEN register dump for that board, in the settings-file format (see `settings-system.md`).

## Block structure

The file is a bare sequence of blocks. **No file header, no index, no trailer.** Every block starts
with a 2-byte identifier and the identifier alone tells you how to parse the rest:

```
0xAA | dataFormat          e.g. 0xAA00 = ALL, PHA
       + 0x10 if PSD            0xAA13 = Minimum, PSD
```

built at `ClassDigitizer2Gen.cpp:510`:

```cpp
dataStartIndetifier = 0xAA00 + dataFormat;
if( FPGAType == DPPType::PSD ) dataStartIndetifier += 0x0010;
```

So the low nibble is the `DataFormat` enum from `Hit.h`, and bit 4 is the PSD flag. A reader
recovers both from the identifier and needs no external metadata — which is the one genuinely good
property of this format, since a file is self-describing block by block.

| identifier | format | |
|---|---|---|
| `0xAA00` / `0xAA10` | ALL | everything, 2 analog + 4 digital probes |
| `0xAA01` / `0xAA11` | OneTrace | 1 analog probe |
| `0xAA02` / `0xAA12` | NoTrace | metadata only |
| `0xAA03` / `0xAA13` | Minimum | channel, energy, timestamp |
| `0xAA04` / `0xAA14` | MiniWithFineTime | Minimum + fine timestamp |
| `0xAA0A` / `0xAA1A` | Raw | opaque blob, `.sol_raw` — see `format_RAW.md` |

All fields are **little-endian**, written with raw `fwrite` of the in-memory value. There is no
padding and no alignment: fields are packed back to back.

## Field layouts

`energy_short` is present **only for PSD**. Sizes are bytes.

### `Minimum` (0xAA03 / 0xAA13)

| field | size | notes |
|---|---|---|
| identifier | 2 | |
| channel | 1 | |
| energy | 2 | |
| energy_short | 2 | **PSD only** |
| timestamp | **6** | not 8 — see below |

### `MiniWithFineTime` (0xAA04 / 0xAA14)

`Minimum`, then:

| field | size |
|---|---|
| fine_timestamp | 2 |

### `NoTrace` (0xAA02 / 0xAA12)

`MiniWithFineTime`, then:

| field | size |
|---|---|
| flags_high_priority | 1 |
| flags_low_priority | 2 |

### `OneTrace` (0xAA01 / 0xAA11)

`NoTrace`, then:

| field | size | notes |
|---|---|---|
| traceLenght | 8 | number of samples |
| analog_probes_type[0] | 1 | |
| analog_probes[0] | `traceLenght * 4` | `int32_t` per sample |

### `ALL` (0xAA00 / 0xAA10)

Note this is **not** `OneTrace` plus extras — the metadata block is longer and the order differs.

| field | size | notes |
|---|---|---|
| identifier | 2 | |
| channel | 1 | |
| energy | 2 | |
| energy_short | 2 | **PSD only** |
| timestamp | 6 | |
| fine_timestamp | 2 | |
| flags_high_priority | 1 | |
| flags_low_priority | 2 | |
| downSampling | 1 | |
| board_fail | 1 | |
| flush | 1 | |
| trigger_threashold | 2 | |
| event_size | 8 | |
| aggCounter | 4 | |
| traceLenght | 8 | |
| analog_probes_type | 2 | both types, one byte each |
| digital_probes_type | 4 | four types, one byte each |
| analog_probes[0] | `traceLenght * 4` | |
| analog_probes[1] | `traceLenght * 4` | |
| digital_probes[0..3] | `traceLenght` each | 1 byte per sample |

### `Raw` (0xAA0A / 0xAA1A) — `.sol_raw`

| field | size |
|---|---|
| identifier | 2 |
| dataSize | 8 |
| data | `dataSize` |

The blob is the digitizer's own format; `format_RAW.md` documents its contents and `RawDecoder`
unpacks it.

---

## Traps

**The timestamp is 6 bytes, not 8.** It is written as the low 6 bytes of a `uint64_t`
(`fwrite(&hit->timestamp, 6, 1, ...)`), which works only because the machine is little-endian.
Reading 8 would swallow the next field.

A reader must therefore read 6 bytes into a destination whose top two bytes are already zero.
`SolReader` relies on `Hit::Init()` zeroing the field once at construction and on the hardware
timestamp being 48-bit, so bytes 6-7 are never written and stay zero for the life of the object. That
is correct but implicit — if you write your own reader, zero the field explicitly rather than
inheriting the assumption.

**`energy_short` presence depends on firmware, not on the identifier alone.** It is keyed off
`FPGAType == DPPType::PSD` at write time — but the identifier's bit 4 records that, so a reader can
determine it. Do not assume PHA.

**Timestamps are in ns**, already scaled by `tick2ns` before writing
(`ClassDigitizer2Gen.cpp:1015`).

**`fine_timestamp` is scaled by `tick2ns` too**, which makes its unit 1/1024 ns (≈0.977 ps) rather
than exactly ps — the raw field is a 10-bit fraction of one coarse tick. Read as picoseconds it is
high by 1024/1000, i.e. 2.4%. Combining coarse and fine as `ts + fine/1000` therefore overshoots the
tick boundary by ~190 ps at the top of the range, which appears as a sawtooth in timing spectra. The
same convention is in `Aux/EventBuilderRaw.cpp:137`, so files and the offline builder agree with each
other; a correction would have to change both together.

**No file header means no way to know the format without reading a block.** Tools generally read the
first identifier and assume the rest of the file matches — true in practice, since the data format
is fixed for a run, but nothing in the format enforces it.

**`CloseOutFile()` does not null `outFile` after `fclose`**, so two stops in a row double-close.
Pre-existing; relevant if you script start/stop cycles.

## Reading a file

The supported path is `Aux/SolReader.h` — header-only, no ROOT:

```cpp
SolReader reader;
reader.OpenFile("run_0_51554_000.sol");
while( !reader.IsEndOfFile() ){
  if( reader.ReadNextBlock() != 0 ) break;
  // reader.hit is refilled in place each call
  printf("ch %d  E %d  t %lu\n", reader.hit->channel, reader.hit->energy, reader.hit->timestamp);
}
```

`ReadNextBlock(fastRead=true)` seeks over payloads instead of reading them, for quick scanning.
`ScanNumBlock()` builds an index so `ReadBlock(i)` can random-access.

`hit` is one reusable object, not a new allocation per block — copy anything you need to keep.

To turn files into time-correlated events offline, use `Aux/EventBuilder` (`.sol`, needs ROOT) or
`Aux/EventBuilderRaw` (`.sol_raw`). Both merge the per-board streams by timestamp and apply a
coincidence window; see `online-analysis-design.md` for how the online builder differs.
