# Long campaign readiness

The October 2026 campaign work adds reusable engine capabilities. A complete automated playthrough establishes technical completion, not commercial quality or enjoyment. Keep creative projects and their media outside public engine commits.

## Implemented contracts

- **Authored calendars:** configure units per year, days per unit, and calendar labels. A year can contain 13 phases of four activity periods without changing engine code. Existing projects retain their twelve-month defaults. Schema validation, Qt controls, native and Web displays, save descriptions, and export capabilities use the same contract.
- **Web records:** configurable manual slots and rotating automatic checkpoints use IndexedDB, with load/overwrite confirmation, stale-write protection and portable backup import/export. Saves remain scoped to the game's deployment and ID. Storage failures are reported rather than silently pretending to persist. A new deployment or browser does not automatically share saves.
- **Native recovery:** a choice produces an automatic record even when it immediately traverses automatic story nodes. The Load browser remains available when only automatic records exist. Ordinary long-save restoration uses one core replay request while retaining command replay, fingerprint checks and exact final-state validation. Gallery and inspection paths retain their existing specialized behavior.
- **Rendering:** native asset paths retain their absolute location; wrapped font lines are scaled once at each display density. Web controllers register immutable content once with the bounded WASM cache and reuse same-state projections, avoiding full-story transfers for each visibility condition.

Existing event queues, conditions, flags, relationships, authored screens and multi-part endings can express long campaigns without game-specific engine branches. The project must still author meaningful consequences and a recoverable fallback.

## Repeatable evidence

`node scripts/smoke-creator.mjs --web --desktop` includes calendar parity, actual browser records, immutable-content cache correctness, native choice checkpoints, automatic-only loading, rendering, first-game playthroughs and Qt authoring. GitHub's Creator journey builds and runs these on Windows and macOS; the Windows lane also tests portable and installed editor exports and uninstall preservation.

For each substantial game, retain the exact content fingerprint and legal command witnesses for early/middle/late saves, choices, ending selection and epilogues. Record coverage and resource trajectories across contrasting policies. Coverage of every individual branch does not establish coverage of every possible history. A bounded search that finds no witness does not prove unreachability. Performance measurements should identify the device, content size and tested path.

## Remaining reusable work

| Priority | Need | Generic acceptance target |
| --- | --- | --- |
| P0 for a phone release | Readable text and usable controls on small screens | User text scale, content reflow, safe areas, minimum physical target sizes, keyboard/controller focus and screen-reader semantics, tested on actual devices. Scaling a fixed desktop canvas is insufficient. |
| P1 | Resume a long story with context | A localized reading history and author-controlled recap/journal, with save persistence and spoiler rules. Existing authored recap screens are a useful interim option. |
| P1 | Understand a long-term schedule | An optional deterministic forecast of costs, recovery and shared upkeep, with uncertainty clearly identified; reusable schedule presets with revalidation. |
| P1 | Make campaign audits available to designers | Named policy suites, legal reachability witnesses, branch/ending coverage, resource saturation and first-divergence reports in the existing analysis workflow. Project-specific authoring scripts are not a completed no-code audit UI. |
| Release gate | Maintain public versions | Signed packages, verified dependency/license notices, clean machines and physical devices, save migration acceptance, backup recovery and a support policy. |
| Production gate | Establish commercial quality | Independent full-length playtests, editorial and accessibility review, coherent finished art, final audio and localization QA. These need accountable human acceptance in addition to automated checks. |

Record each unresolved need with its proposed contract, supported targets, fallback, evidence and owner. Do not label a game or engine commercial-ready merely because its route and package tests pass.
