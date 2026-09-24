---
name: component-checklist
description: Evaluate an F´ component against the Component Checklist and generate a comprehensive markdown report
---

You are evaluating an F´ component against the project Component Checklist.

## Source of Truth

The canonical checklist lives at **`docs/Component-Checklist.md`** (markdown). It is
the authoritative list of items — **always read it fresh** at the start of an
evaluation rather than relying on the copy reproduced in this skill. The checklist
is versioned (see the `Checklist Version` field in its header); report that version
in your output so a reader knows which revision was applied.

The checklist is organized as one markdown table per section. Each row is:

```
| <#> | <item text> | <Status> | <Notes> |
```

The `Status` column takes one of: **YES**, **NO**, **PARTIAL**, **N/A**, **PENDING**.

As of checklist version 1.2.0 there are **73 items** across 5 numbered sections:

| Section           | Items |
| ----------------- | ----- |
| 1. Requirements   | 8     |
| 2. Design         | 17    |
| 3. Implementation | 38    |
| 4. Unit Testing   | 5     |
| 5. Close-Out      | 5     |

Do not hard-code these counts — recompute them from the file so the skill stays
correct as the checklist evolves. The section headings are numbered (`## 1.
Requirements`) and each table's first column is the item number (`| 1.1 | ... |`), so
the item text is the *second* cell. A quick way to enumerate items and confirm counts:

```bash
python3 - <<'EOF'
import re
sec=None; items={}; order=[]
for l in open('docs/Component-Checklist.md'):
    if l.startswith('## '):
        s = l[3:].strip()
        sec = s if re.match(r'^\d+\.', s) else None
        if sec:
            items[sec] = []
            order.append(sec)
        continue
    if sec and l.startswith('|') and not l.startswith('|---') and not l.strip().startswith('| #'):
        cells = l.split('|')
        num, text = cells[1].strip(), cells[2].strip()
        if num:
            items[sec].append((num, text))
for s in order:
    print(f"== {s}: {len(items[s])} ==")
    for num, text in items[s]:
        print(f"  {num} {text}")
print("TOTAL", sum(len(v) for v in items.values()))
EOF
```

## Input

The user provides a component name or path, for example:
- `I2cTester`
- `CuriosityReference/I2cTester`
- `lib/fprime-samd/fprime-samd/Drv/RtcDriver`

## Status Semantics (how to mark each item)

Be objective and evidence-based. Use exactly these statuses:

- **YES** — Requirement verifiably met. **Leave the Notes column empty.** The only
  exception is an item whose own text asks for a value (e.g. "document amount" for
  coverage, "note platform" for the hardware-run item) — record just that value.
- **NO** — Requirement verifiably not met. Notes MUST cite the gap and a
  `file:line` where relevant.
- **PARTIAL** — Partly met. Notes MUST say what is done and what is missing.
- **N/A** — Item does not apply to this component (e.g. ISR items for a component
  with no ISR, command items for a component with no commands). Notes MUST justify
  why it does not apply.
- **PENDING** — Cannot be determined yet (e.g. requires hardware run not performed,
  or a CI result not available). Notes MUST say what evidence is outstanding.

Every non-YES status requires a Notes entry. Never leave an item blank.

## Validation Method

Each checklist item must be **validated against evidence**, not guessed. The table
below maps item categories to the concrete checks that justify a status. Read the
actual item text from `docs/Component-Checklist.md` and apply the matching check.

### Requirements items (section 1)

- Read `<ComponentPath>/docs/sdd.md`. These items ask whether *requirements exist* in the
  SDD for behaviour, inputs, outputs, memory, hardware I/O and performance — and whether
  each states a verification method. A requirement table with no verification column is
  **PARTIAL**, not YES.
- A component whose SDD has no requirements section at all is **NO** on every item in
  this section; say so once and cite the file.

### Design items (section 2)

- Read `<ComponentPath>/docs/sdd.md` and `<ComponentPath>/<Component>.fpp`.
- SDD-content items (functionality, block diagram, port table, input-port behavior,
  state machines/use-cases, commands/tlm/events/params): confirm the SDD section
  exists AND is consistent with the FPP (cross-check port names, command opcodes,
  event/tlm/param names). A section that contradicts the FPP is **NO** or
  **PARTIAL**, not YES.
- "FPP↔SDD match": diff the FPP-declared commands/telemetry/events/params against
  the SDD tables. Note any declared-but-undocumented or documented-but-absent items.
- "each port has a known use-case": every port in the FPP should be referenced by
  the SDD or used in the implementation/topology.
- "port directions and types as expected", "component type (passive/queued/active)":
  read the FPP header and port declarations.
- "data product containers assigned a default priority": N/A if the component
  declares no data products.
- "FPP compiles and generates autocode": confirm by building (see below).

### Implementation items (section 3)

- Read `<Component>.cpp` and `<Component>.hpp` in full.
- **Build & warnings**: build the deployment and the native-UT target; capture
  warnings for this component's translation units.
- **Repo/PR items**: `git status`, `git log`, and `gh pr list --head <branch>`.
- **Static/pattern checks** — run these and inspect results (do not assume):

```bash
CP=<ComponentPath>
# printf / TODO / TBD / FIXME
grep -nE '\b(printf|TODO|TBD|FIXME)\b' "$CP"/*.cpp "$CP"/*.hpp
# switch/case: every switch should have default + break (manual read of each)
grep -nE '\b(switch|case|default|break)\b' "$CP"/*.cpp
# loops and bounds — read each for termination + bracket use
grep -nE '\b(for|while|do)\b' "$CP"/*.cpp
# primitive types instead of F´ platform types (I8/U8/I32/U32/F32...)
grep -nE '\b(int|char|short|long|float|double|unsigned|signed)\b' "$CP"/*.cpp "$CP"/*.hpp | grep -vE '//'
# FW_ASSERT usage (pointer/state validation)
grep -n 'FW_ASSERT' "$CP"/*.cpp
# division (check for divide-by-zero / small-number guards)
grep -nE '/[^/*]' "$CP"/*.cpp
# floating-point equality
grep -nE '==|!=' "$CP"/*.cpp | grep -iE 'float|f32|f64|double'
# magic numbers vs enums/constexpr/#define
grep -nE 'constexpr|#define|enum' "$CP"/*.hpp "$CP"/*.cpp
```

  For each item, decide the status from what the grep + your reading actually show.
  Many of these are "verify no violations exist" items: YES means you looked and
  found none, and you should say so in Notes (e.g. "no printf; all loops bounded").
- **Command acknowledgement**: confirm every `*_cmdHandler` ends every path with a
  `cmdResponse_out(...)` (OK on success, EXECUTION_ERROR on failure). A handler with
  a path that returns without responding is **NO**.
- **Validation-failure returns**: confirm each guard (`if (!ok) { ...; return; }`)
  actually returns before falling through.
- **ISR items**: if the component has no interrupt service routine, mark the ISR
  block **N/A** with that justification. If it does, evaluate each ISR item against
  the ISR code.
- **RateGroup / critical-region items**: N/A if the component has no scheduled
  (`Svc.Sched` / rate-group) input and does no file access.
- **Static analysis**: run `fprime-util check` / any configured linter if available;
  otherwise **PENDING** noting it was not run.

### Unit Testing items (section 4)

- Look for `<ComponentPath>/test/` (typically `test/ut/`).
- Confirm test sources are registered via `register_fprime_ut(...)` in
  `CMakeLists.txt`.
- Run the tests and coverage:

```bash
source venv/bin/activate
fprime-util generate --ut
cd <ComponentPath> && fprime-util check --coverage
```

  Record the pass/fail result and the coverage percentage in Notes (the checklist
  item explicitly asks to document the amount). Look for a committed
  `coverage/summary.txt` if a fresh run is not possible, and say the number is from
  the committed artifact.
- **Boundary conditions**: inspect the tests for boundary/edge cases (address
  range endpoints, short reads, busy rejection, error status, disconnected ports).
- **Memory leak check**: F´ UTs run under the framework's leak detection; a clean
  `fprime-util check` run satisfies this. If not run, **PENDING**.

### Close-Out items (section 5)

- **No TBD/TODO/printf**: reuse the grep above; must be clean.
- **Instantiated/connected/compiled in topology**: grep `instances.fpp` /
  `topology.fpp` for the component and confirm the deployment builds with it.
- **Runs on hardware**: look for evidence in the SDD (a "Tested Configurations" /
  hardware-checkout section naming a board). If present, YES and name the platform
  in Notes; if not, **PENDING**.
- **SDD reflects final state**: judge whether the SDD matches the current FPP/impl.
- **CI passes**: check `.github/workflows/` (or `.gitlab-ci.yml`) and, if possible,
  `gh pr checks <pr>` / `gh run list`. If CI state cannot be observed, **PENDING**.

## Execution Steps

1. **Read the checklist**: read `docs/Component-Checklist.md`; enumerate items and
   note the checklist version.
2. **Locate the component**:
   ```bash
   find . -type d -name "<ComponentName>" | grep -v build
   ```
3. **Gather files**: SDD, FPP, `.cpp`, `.hpp`, `CMakeLists.txt`, `test/`, `coverage/`.
4. **Repo state**: `git status`, `git log --oneline -- <ComponentPath>`,
   `gh pr list --head <branch>`.
5. **Read docs**: SDD then FPP; cross-check them.
6. **Read implementation**: `.cpp` and `.hpp` in full; run the pattern checks.
7. **Build**: build the deployment and native-UT; capture warnings.
8. **Test**: run `fprime-util check --coverage` (or read committed coverage).
9. **Close-out**: topology instantiation, CI config, hardware-test evidence.
10. **Generate the report** (below).

## Output: the report

Write the report to **`<ComponentPath>/docs/checklist-report.md`** and also give the
user a concise summary in chat.

The report MUST:

1. **Header** — component name/path, evaluation date, branch + commit, checklist
   version, and a summary table of counts (YES / NO / PARTIAL / N/A / PENDING) per
   section and overall.
2. **One filled-in table per section** — reproduce the checklist's own table
   structure (`| # | Item | Status | Notes |`) with your Status for every item and
   evidence-backed Notes for every **non-YES** item (YES rows leave Notes empty per
   the status rules above). This makes the report a drop-in completed checklist. Use
   the exact item text from `docs/Component-Checklist.md`.
3. **Findings** — a bulleted list of every NO / PARTIAL / PENDING item with a
   `file:line` reference and what is required to close it.
4. **Risk assessment** — Low / Medium / High with justification tied to the findings.
5. **Recommendations** — prioritized (High/Med/Low), actionable, each closing a
   specific finding.

Use ✅ YES · ❌ NO · 🟡 PARTIAL · ⚪ N/A · ⏳ PENDING as inline markers, and make
file references clickable (`[I2cTester.cpp:142](../I2cTester.cpp#L142)`).

## Guidelines

1. **Evidence over assumption** — every status is backed by something you read or ran.
2. **Cite locations** — `file:line` for NO/PARTIAL findings.
3. **Recompute item lists** from the checklist file; don't trust stale counts.
4. **Justify N/A and PENDING** — never use them to dodge an item.
5. **Note recent commits** that already addressed prior gaps.
6. **Context matters** — hardware drivers may be hardware-validated rather than
   unit-tested; passive components have no ISR/rate-group items; etc.

## Success Criteria

- Every item in `docs/Component-Checklist.md` has an evidence-backed status.
- All NO/PARTIAL/PENDING items have `file:line` and a closure path.
- The report is a valid, drop-in completed copy of the checklist plus analysis.
- Counts in the summary table match the number of items actually evaluated.

Now evaluate the component provided by the user.
