# Contributing to Raising Engine

Original engine/editor code is MIT licensed. Reference-game assets, fonts, plugins and dependencies retain separate terms. See [release preparation](docs/development/open-source-readiness.md) for remaining publication checks.

## Development entry points

- Current editor: `native/studio` (Qt Widgets) and `apps/qt-studio` (Node service).
- Shared authoring, validation and MCP: `packages/`.
- Production simulation: `native/src`; TypeScript rule code in `packages/` is a compatibility/reference implementation.
- Windows player: `native/player`; web player: `apps/web`.
- Archived Electron shells/editor: `legacy/`, opt-in `legacy:*` commands. The shared React renderer in `apps/player/renderer` remains current Web code.

Follow the [root setup instructions](README.md). Qt desktop development is currently verified on Windows. A successful Linux headless CI run is not proof that the desktop editor works on Linux.

## Changes and verification

1. Keep changes focused. Separate formatting, file moves and behavior changes.
2. Read the relevant contract and current implementation notes in the [documentation index](docs/README.md).
3. Keep GUI and MCP writes behind the shared project service. Preserve revision checks, validation, undo/redo and atomic writes.
4. For simulation changes, verify seed/command determinism, TypeScript/C++ parity where applicable, and save/resume. Preserve old projects or provide an explicit migration.
5. For authoring changes, verify the real editor and resulting player behavior. Editing JSON with a script is not evidence of GUI authoring.
6. Run the affected tests. Start with `npm run build:headless` and `npm run test:regression` for the route regression workflow. Qt tests need their documented fixtures and environment variables; not every desktop test is a fixture-free smoke test.
7. Summarize the problem, behavior change, verification and remaining limits in the PR. State the actual platform and sample tested.

Use a separate `QT_STUDIO_OUT`, `NATIVE_PLAYER_BUILD` and `NATIVE_PLAYER_OUT` when another editor/player is running. Do not overwrite or delete a user's project, saves, local evidence or build output.

## Repository hygiene

- Do not commit credentials, local `.env` files, signing keys, build output, recordings or `.studio` working data.
- Do not add extracted commercial game assets to public samples or tests. Existing reference projects are preserved in the ignored local workspace; see [sample separation](docs/development/public-sample-separation.md).
- For proposed public assets, record source, author, version/hash, license, redistribution terms and required notices. Generated assets also need provenance and a deliberate distribution decision.
- Prefer small fixtures with synthetic data. Do not duplicate a full project to test one rule. Identical fixture media are reconstructed by `npm run prepare:fixtures`; keep source hashes and never overwrite local edits.
- Run `npm run audit:repository` to inspect tracked-file footprint and known review categories. This inventory does not scan Git history, prove asset rights or replace a secret scanner.
- Run `npm run check:repository`, `npm run check:public-content` and `npm run test:public-content` after structural/sample changes. A local project that still exists on your PC is not a valid dependency for public CI. Test a fresh clone when changing build prerequisites or bootstrap scripts.

No contributor license agreement, DCO requirement, or project-wide formatting migration is introduced by this document. Those are owner decisions for the release preparation phase.
