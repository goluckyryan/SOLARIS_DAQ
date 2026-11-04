# SOLARIS DAQ documentation

## Documents

| Document | What it covers |
|---|---|
| [run-lifecycle.md](run-lifecycle.md) | What happens between Start and Stop, in order, and which parts of that order matter |
| [threading-model.md](threading-model.md) | Every thread in the DAQ, the state it owns, and what is safe to call from where |
| [scalar.md](scalar.md) | The scalar panel: how the update works, how the Accepted rate is calculated, its cost and its rate ceiling |
| [settings-system.md](settings-system.md) | How a digitizer parameter is described, addressed, edited, cached and saved |
| [sol-file-format.md](sol-file-format.md) | The `.sol` file format written for every normal run |
| [experiment-workflow.md](experiment-workflow.md) | Experiment setup: the settings file, git, and the analysis folder |
| [online-analysis-design.md](online-analysis-design.md) | Why the online event builder and analyzer are designed the way they are |
| [issues.md](issues.md) | Open issues and behaviour not yet checked on hardware |
| [plan-dummy-generator.md](plan-dummy-generator.md) | Plan (not implemented): dummy digitizers as signal generators |

Related, outside this folder: [`../README.md`](../README.md) (the project), [`../format_RAW.md`](../format_RAW.md)
(the raw endpoint blob and the `.raw` container), [`../analyzers/README.md`](../analyzers/README.md)
(writing an online analysis plugin).

## Principles for these documents

1. **Describe the code as it is now.** A document says what the code does today, as plain
   present-tense fact.
2. **No change history.** No "what was fixed" or "change history" sections, dated entries or
   timelines. No "was / now", "before / after", "used to", "no longer", "since the X fix",
   "old Y for comparison", commit hashes, branch names, or "abandoned on …" decision logs. History
   belongs in git commit messages.
3. **Rationale is welcome, in the present tense.** Explain why the code is the way it is, as a
   reason, not as the story of a fix. Example: "one container per column, because Qt recurses once
   per sibling while painting and 16+ boards overflow the stack."
4. **Measurements, constraints, known gaps and limits are welcome,** as long as they describe the
   current code.
5. **Refer only to what is in the repository.** Nothing points to `tests/` (it is not tracked), to
   other untracked files, to `/tmp` or scratch directories. A measured number may stay, but it is
   stated as a measurement of the system, not credited to a file the reader cannot find. `Aux/` is
   tracked and may be referenced.
6. **References must match the current code.** File names, functions and `file:line` numbers must
   point at what the text describes. When code moves, update the reference.

Exception: `plan-dummy-generator.md` is a plan for code that does not exist yet, so principle 1
does not apply to it. `issues.md` lists only open items; delete an item when it is fixed.
