# Contributing to Raising Engine

This repository is being prepared for an open-source release. The engine's license has not been selected yet. Do not assume that the repository, reference-game assets, fonts, or dependencies share one license. See [release preparation](docs/open-source-readiness.md) before redistributing a snapshot or accepting external contributions.

## Development entry points

- Current editor: `native/studio` (Qt Widgets) and `apps/qt-studio` (Node service).
- Shared authoring, validation and MCP: `packages/`.
- Deterministic simulation: `native/src` and the TypeScript implementation in `packages/`.
- Windows player: `native/player`; web player: `apps/web`.
- Electron editor/player remain for compatibility and existing tests. Do not remove them as unused without checking callers and packaging.

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
- Do not add extracted commercial game assets to public samples or tests. Existing reference projects are preserved in the ignored local workspace; see [sample separation](docs/public-sample-separation.md).
- For proposed public assets, record source, author, version/hash, license, redistribution terms and required notices. Generated assets also need provenance and a deliberate distribution decision.
- Prefer small fixtures with synthetic data. Do not duplicate a full project to test one rule. Existing large fixtures will be migrated incrementally with regression coverage.
- Run `npm run audit:repository` to inspect tracked-file footprint and known review categories. This inventory does not scan Git history, prove asset rights or replace a secret scanner.
- Run `npm run check:public-content` and `npm run test:public-content` after changing samples or fixtures. A local project that still exists on your PC is not a valid dependency for public CI.

No contributor license agreement, DCO requirement, or project-wide formatting migration is introduced by this document. Those are owner decisions for the release preparation phase.
