# Experiment setup: settings file, git, and the analysis folder

Three mechanisms that interlock and that nobody would guess from the code. Getting any of them
wrong costs a run.

---

## 1. `programSettings.txt` is a positional file

Not JSON, not INI, not `QSettings` — **fifteen bare lines, identified by position**, terminated by a
line starting with `//----`. Read by `MainWindow::LoadProgramSettings()` (`mainwindow.cpp:1723`)
through a `switch` on the line index, written back in the same order by `SaveProgramSettings()`
(`mainwindow.cpp:1884`).

| line | field | notes |
|---|---|---|
| 0 | masterExpDataPath | root of all raw data |
| 1 | isSaveSubFolder | the literal string `SubFolder` or anything else |
| 2 | expName | also the git branch name — see below |
| 3 | IPListStr | comma-separated digitizer IPs |
| 4 | analysisPath | the analysis working folder |
| 5 | DatabaseIP | InfluxDB |
| 6 | DatabaseName | |
| 7 | DatabaseToken | |
| 8 | ElogIP | |
| 9 | ElogUser | |
| 10 | ElogPWD | |
| 11 | ElogPort | |
| 12 | ElogUseSSL | the literal string `SSL` |
| 13 | ElogName | |
| 14 | ElogNameSameAsExp | the literal string `SameAsExp` |

Read from `QDir::current()`, written to `programPath` — normally the same place, but not
necessarily if the DAQ is launched from elsewhere.

### The rules that keep it working

**Order is frozen. New fields go on the end, never in the middle.** The comments at
`mainwindow.cpp:1776` and `:1778` say so explicitly, and `:1789` back-fills
(`if(count <= 12) ElogUseSSL = (ElogPort == "443")`) so that a shorter, older file still loads. Insert
a line and every field after it silently shifts to the wrong variable — the file will load without
complaint and your elog password becomes your database token.

**Blank lines are meaningful.** An unset field is an empty line, not an absent one. The checked-in
file has blanks at lines 5-11. Do not tidy them away.

**Editing by hand is possible but unforgiving.** There is no validation and no error on a short or
mis-ordered file.

To add a setting you must touch four places: a `case` in `LoadProgramSettings`, a `file.write` in
`SaveProgramSettings` in the matching position, a widget in `ProgramSettingsPanel()`
(`mainwindow.cpp:1447`), and a read-back where the dialog is accepted (`:1652`). `lAnalysisPath` is
the clearest example to copy.

---

## 2. An experiment *is* a git branch

The DAQ manages the analysis folder as a git repository, with **one branch per experiment named
after `expName`**. This is entirely automatic and driven from `MainWindow` via `QProcess`.

### When you create a new experiment

`mainwindow.cpp:2337-2393`, in order:

```
git init -b <expName>          # if the folder is not yet a repo
git checkout -b <expName>      # if it is
                               # ...then, after writing .gitignore:
git add -A
git commit --allow-empty -m "initial commit."
git remote                     # if a remote exists:
git push --set-upstream <remote> <expName>
```

`--allow-empty` matters: a brand-new experiment folder may have nothing in it, and the commit still
has to exist so the branch does.

A `.gitignore` is created if absent (`:2354`) containing `data_raw`, `root_data`, `*.root`, `*.d`,
`*.so` — i.e. **data and build products are deliberately not versioned**, only analysis code and
configuration.

### When you open the new-experiment dialog

`mainwindow.cpp:2036-2073` inspects the repository first:

```
git fetch                                           # so remote branches are visible
git branch -a                                       # existing experiments
git status --porcelain --untracked-files=no         # must be empty
```

The branch list populates the dialog, the one marked `*` is shown as the current experiment, and
**the working tree must be clean** or the new experiment is refused. Untracked files are ignored for
that check, which is what lets uncommitted raw data sit in the folder.

### Consequences worth knowing

- Switching experiments switches a git branch, so **uncommitted analysis code follows you** or
  blocks the switch.
- The experiment name is constrained by what git accepts as a branch name.
- If `useGit` is off, none of this runs and the folder is just a folder.
- Nothing commits automatically *during* a run — only at experiment creation. Analysis code changes
  made while running are yours to commit.

---

## 3. The analysis folder

Set as `analysisPath` (line 4 of the settings file). The DAQ expects and maintains this layout:

```
<analysisPath>/
├── working/
│   ├── Mapping.h          <- parsed by the DAQ, defines the detector panel
│   └── Settings/          <- per-board register dumps
├── data_raw   -> <masterExpDataPath>/<expName>      (symlink, git-ignored)
├── root_data  -> ...                                (symlink, git-ignored)
├── .gitignore                                       (created if absent)
└── (your analysis code)                             (versioned)
```

**`working/Mapping.h`** is read by `CheckSOLARISpanelOK()` (`mainwindow.cpp:1083-1187`) and parsed
into `mapping[digi][channel] -> detector ID`, plus `detType`, `detGroupID` and `detMaxID`. Those are
handed to `SOLARISpanel` (`:1199`), which is what draws the detector view. It is a C++ header
because the offline analysis includes the same file — one definition of the channel map, shared
between DAQ and analysis.

**The symlinks** are created by `CreateDataSymbolicLink()` (`mainwindow.cpp:2489-2511`) so that
analysis code can refer to `data_raw/` regardless of where the raw data actually lives. They are
git-ignored, so they do not follow the repository to another machine — expect to recreate them.

**`Settings/`** receives the per-board register dumps. During a run, `StartACQ` also writes a
settings snapshot next to the data itself (`<expName>_<runID>XSetting_<SN>.dat`), so a run's raw
files and the configuration that produced them stay together.

**`expName.sh`** is written by `WriteExpNameSh()` — a shell fragment the analysis scripts source to
learn the current experiment name.

---

## Putting it together: starting a new experiment

1. Set `analysisPath` and `masterExpDataPath` in the settings panel.
2. New Experiment → the dialog runs `git fetch` / `branch -a` / `status --porcelain` and refuses if
   the tree is dirty.
3. Enter the name. That name becomes: the git branch, the raw-data subfolder under
   `masterExpDataPath`, `expName` in the settings file, and — unless `ElogNameSameAsExp` is off —
   the elog name.
4. The DAQ creates the folder, writes `expName.sh` and `.gitignore`, makes the symlinks, and does
   the git init/checkout + commit + push.
5. **Create the elog manually.** The DAQ logs a reminder (`mainwindow.cpp:2397`) because it cannot
   create one itself — there is a `//TODO` there about editing `config.cfg` directly.
6. Put `Mapping.h` in `working/` before opening the SOLARIS panel.

## Related

- `sol-file-format.md` — what lands in `data_raw/`
- `settings-system.md` — the `.dat` register dumps in `Settings/`
- the README's *Additional Features* section — elog template and `endRunScript.sh`
