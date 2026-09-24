# Project skills

Four skills live under `.claude/skills/`. All four are general F Prime practice — nothing
in them is specific to this board or this deployment — so they are equally useful when
working in `lib/fprime-samd` or in your own downstream project.

| Skill | Use it when |
| --- | --- |
| `fprime-cpp-design` | Writing or reviewing any F Prime C/C++: components, drivers, OSAL, test infrastructure. The authoritative list of idioms to follow and language features to avoid. |
| `fprime-unit-testing` | Creating or extending a component unit test. Covers `fprime-util impl --ut`, the `Tester` / `TestMain` / `GTestBase` pattern, rules-based testing with STest, and `register_fprime_ut`. |
| `fprime-design-review` | Reviewing a change for design fit — does the implementation match its FPP and topology, does the design match the stated intent, and where does a human design owner need to decide. |
| `component-checklist` | Evaluating a component against `docs/Component-Checklist.md` and producing a filled-in report at `<Component>/docs/checklist-report.md`. |

## Notes for this repository

- `component-checklist` reads `docs/Component-Checklist.md` fresh on every run, and
  reports the `Checklist Version` from that file's header. If you edit the checklist,
  you do not need to edit the skill — but do update the section/item-count table in the
  skill's *Source of Truth* section, which is a convenience summary only.
- The checklist's *Unit Testing* section will come back **NO** for all four template
  components in `CuriosityReference/`: none of them has tests yet. That is a known gap,
  not a bug in the skill.
- The checklist's *Implementation* section includes ISR and critical-region items. They
  genuinely apply here — `GpioIn.transitionIn` runs in EIC interrupt context and the
  drivers' `dmaReplyIn` handlers run in DMAC interrupt context. Do not mark them N/A
  without reading `docs/architecture.md` § 1.6.
- `fprime-cpp-design`'s rules on dynamic allocation, exceptions and RTTI are hard
  constraints on this target, not preferences. See `CLAUDE.md` § Important constraints.

Nothing here is required to build or contribute; the skills are a convenience for anyone
using Claude Code in this repository.
