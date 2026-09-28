# The settings system

How a digitizer parameter is described, addressed, edited, cached and persisted.

`DigiParameters.h` (1,218 lines) and `digiSettingsPanel.cpp` (3,523 lines) are the largest
undocumented area in the project. Nearly all of it is mechanical once you know the four ideas below.

---

## 1. `Reg` — one parameter, described once

Every CAEN parameter is a `Reg` object (`DigiParameters.h:15`) holding its name, its scope, whether
it is readable/writable, what kind of answer it takes, and — for a combo box — the list of legal
values paired with their display strings.

```cpp
Reg(name, readWrite, type, answers, ansType, unit, isCmd)
```

| enum | values | meaning |
|---|---|---|
| `TYPE` | `CH, DIG, LVDS, VGA, GROUP` | what the parameter belongs to |
| `RW` | `ReadOnly, WriteOnly, ReadWrite` | |
| `ANSTYPE` | `INTEGER, FLOAT, COMBOX, STR, BYTE, BINARY, NONE` | drives which widget the panel builds |

They live in namespaces by firmware and scope — `PHA::DIG::`, `PHA::CH::`, `PHA::LVDS::`,
`PSD::CH::` and so on — and are collected into `AllSettings` vectors that the panel and the
save/load code iterate.

For an `ANSTYPE::COMBOX` the `answers` vector is `{value, displayText}` pairs and becomes the combo
items. For `INTEGER`/`FLOAT` the convention is different and easy to miss: **`answers[0]` is the
minimum, `answers[1]` the maximum, and `answers[2]`, if present, the step** — see
`SetupSpinBox` (`digiSettingsPanel.cpp:3038-3056`).

## 2. `GetFullPara()` — the CAEN path is computed, never written out

`Reg::GetFullPara(ch_index, nChannels)` turns the scope and name into the path CAEN_FELib expects:

| `TYPE` | `ch_index = -1` (all) | specific index |
|---|---|---|
| `DIG` | `/par/<name>` | — |
| `CH` | `/ch/0..63/par/<name>` | `/ch/<i>/par/<name>` |
| `LVDS` | `/lvds/0..3/par/<name>` | `/lvds/<i>/par/<name>` |
| `VGA` | `/vga/0..3/par/<name>` | `/vga/<i>/par/<name>` |

`isCmd` swaps `/par/` for `/cmd/`, which is how `/cmd/Reset` and friends are expressed.

The `0..63` form is a CAEN broadcast — one write hits every channel. That is what "apply to all
channels" in the panel actually does, and why it is one network round-trip rather than 64.

## 3. The in-memory cache, and why dummies work

`Digitizer2Gen` keeps a parallel copy of every setting:

```cpp
std::vector<Reg> boardSettings;
std::vector<Reg> chSettings[MaxNumberOfChannel];
std::vector<Reg> LVDSSettings[4];
Reg VGASetting[4];
Reg InputDelay[16];
```

`WriteValue(Reg, value, ch)` writes to hardware **and** updates the cache — `ClassDigitizer2Gen.cpp:216`:

```cpp
if( WriteValue(para.GetFullPara(ch_index, nChannels).c_str(), value) || isDummy ){
  // ... update the cache
```

That `|| isDummy` is the entire reason dummy boards exist: with no hardware the write fails, the
cache accepts the value anyway, and settings can be loaded, edited and saved with no digitizer
present. See the README's *Dummy digitizers* section.

`GetSettingValueFromMemory()` reads the cache without touching hardware, which is what lets the GUI
and the elog template query settings while a run is in progress.

## 4. The settings file (`.dat`)

Written by `SaveSettingsToFile()` (`ClassDigitizer2Gen.cpp:1441`), read by `LoadSettingsFromFile()`
(`:1587`). One line per parameter:

```
<full CAEN path>       !<RW>!<id>!<value>
```

`%-45s` left-pads the path, so the file is column-aligned and reasonably readable.

**The `id` is a positional namespace**, and it — not the path — is what the loader uses to decide
where a value belongs:

| id range | scope | mapping |
|---|---|---|
| `< 7000` | channel | `ch = id/100`, `index = id - ch*100` |
| `7000-7999` | LVDS | `7000 + 4*i + index` |
| `8000-8999` | board | `8000 + i` |
| `9000-9049` | VGA | `9000 + i` |
| `>= 9050` | group / InputDelay | `9050 + idx` |

So **the id encodes the position in the `AllSettings` vector**. Reordering or inserting into those
vectors silently changes what an old file's ids mean. Append, do not insert.

On load, a value is also **replayed to the hardware** when `readWrite == "2"` (i.e. `ReadWrite`) and
the board is connected (`:1663`). A dummy is never connected, so loading into a dummy populates the
cache only — which is exactly what makes offline settings editing work.

Where these files land:

- `Settings/setting_<SN>_<PHA|PSD>.dat` — the panel's Save/Load buttons, under
  `expDataPath + "/Settings/"`
- `<expName>_<runID>XSetting_<SN>.dat` — written automatically next to the raw data at every save
  run, so a run's data and its configuration stay together

## 5. The panel

One `DigiSettingsPanel` for all boards, built lazily on first open (`mainwindow.cpp:1063`) and
destroyed in `CloseDigitizers()`. Three levels of nested `QTabWidget`:

```
tabWidget                       one tab per digitizer, then "Inquiry / Copy"
 └── digiTab[iDigi]             inside a QScrollArea
      ├── bdTab                 Board | Test Pulse | VGA | ITL-A/B | LVDS | Input Delay
      └── chTabWidget[iDigi]    All/Single Ch. | Input | Trapezoid | Probe | Others |
                                Trigger | Status | Trigger Map
```

The per-digitizer loop is `digiSettingsPanel.cpp:84-1027`, all inline in the constructor. The
current board is the member `ID`, driven by the top-level tab index (`:1473`); every widget member
is an array indexed `[MaxNumberOfDigitizer]`, and handlers use the *current* `ID` at runtime.

Channel pages are generated: `SetupSpinBoxTab` / `SetupComboBoxTab` (`:3180`, `:3192`) build **one
sub-tab per parameter**, each a 64-channel grid laid out `ch/4, ch%4*2`.

### Two idioms you must follow

**`enableSignalSlot`** (`digiSettingsPanel.h:88`) is false during construction and set true at
`:1491`. Every handler begins `if( !enableSignalSlot ) return;`, and every programmatic fill is
wrapped in a `false` / `true` sandwich. Without it, populating a combo triggers a write.

**Spin boxes commit on Enter, not on change.** `valueChanged` only paints the box blue;
`returnPressed` does the write (`:3058-3062`). The style sheet is the state: `""` committed,
`blue` edited-not-committed, `red` write failed, `orange` non-default.

## Adding a new parameter

1. Add a `Reg` to the right namespace in `DigiParameters.h` and **append** it to the matching
   `AllSettings` vector — appending, because the settings-file id is its position.
2. Add a widget in `digiSettingsPanel.cpp` with `SetupSpinBox` / `SetupComboBox`, or a whole page
   with `SetupSpinBoxTab` / `SetupComboBoxTab`.
3. Add a refresh line in `UpdatePanelFromMemory()` (per-channel loop from `:2707`).
4. Nothing else — save, load and the cache are all driven off the vectors.

## Traps

- **Ids are positional.** Inserting into an `AllSettings` vector invalidates every existing `.dat`.
- **`EnableControl()`** (`:2426`) re-enables an explicit list of widgets based only on `IsAcqOn()`,
  and clobbers the construction-time dummy greying at `:264`. It also recursively disables
  everything inside `inputTab`/`trapTab`/`probeTab`/`otherTab` via `findChildren` — so anything added
  to those four is dead during a run.
- **`UpdatePanelFromMemory()` early-returns when the panel is not visible** (`:2562`).
- **`chBox->setFixedWidth(900)`** (`:883`) constrains how wide a channel grid can be.
- The panel is rebuilt wholesale when digitizers are reopened, so `IsDummy()` is stable for its
  entire lifetime and construction-time branching on it is safe.

## Related

- `experiment-workflow.md` — where `Settings/` lives and how the analysis folder is laid out
- `sol-file-format.md` — the per-run settings dump written beside the data
