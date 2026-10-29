# Contributing to Raising Engine

Original engine/editor code is MIT licensed. Reference-game assets, fonts, plugins and dependencies retain separate terms. See [release preparation](docs/development/open-source-readiness.md) for remaining publication checks.

## Development entry points

- Current editor: `native/studio` (Qt Widgets) and `apps/qt-studio` (Node service).
- Shared authoring, validation and MCP: `packages/`.
- Production simulation: `native/src`; TypeScript rule code in `packages/` is a compatibility/reference implementation.
- Windows player: `native/player`; web player: `apps/web`.
- Archived Electron shells/editor: `legacy/`, opt-in `legacy:*` commands. The shared React renderer in `apps/player/renderer` remains current Web code.

Follow the [root setup instructions](README.md). Qt desktop development is verified on Windows and macOS; see the current platform-specific evidence in the first-game guide. A successful Linux headless CI run is not proof that the desktop editor works on Linux.

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

## Creator smoke test (P1)

After `npm ci` and `npm run build:headless`, run:

```sh
npm run test:creator-smoke
npm run benchmark:creator
```

The first command checks the starter, recovery with imported files, release archives and pinned dependencies, ending explanations, editing/reference navigation, iOS source packaging, and named-playtest failure/approval semantics. It writes logs and a machine-readable report under `.cache/creator-smoke/`. The route checks take around two minutes on the tested Mac. Local loopback networking is required for preview-server tests.

For the real graphical path, build `npm run build:player` and `npm run build:studio`, then `npm run test:creator-smoke -- --desktop`. The Qt harness links the same editor UI library and operates its actual forms. It creates an activity and branching encounter, edits/undoes/redoes, restores recovery, inspects an ending, corrects an export failure, plays to completion, exports and reopens. The native player test exercises both routes and save restoration. The desktop smoke also runs story-form guards and a disposable diagnostics-to-form repair journey, including invalid input retention. Existing Windows installation verification remains `npm run test:qt-package`.

Build Web with the pinned Emscripten SDK documented in `native/README.md`, install the Playwright browser (`npx playwright install chromium`), and add `--web` to exercise the exported WASM game. To use installed Chrome instead, set `RAISING_BROWSER_CHANNEL=chrome`.

On macOS, package with `npm run package:macos` using the standalone Node distribution documented in the README. Then run:

```sh
npm run test:creator-smoke -- --web --desktop --package dist/macos/RaisingStudio.app
```

The package gate audits bundled binary dependencies and signatures with developer paths removed, launches the actual app and exported player, and reruns the Qt journey against the bundled Node/services/templates/runtime. The test harness itself uses the matching development Qt build. This is not a clean-machine or human usability study.

`benchmark:creator` measures service round trips for the 17-document starter and a synthetic 517-document project, including recovery and compatibility work. `CREATOR_EVENTS=1000` changes the added-story count. Reports include hardware, first/median/p95 timings and explicit investigation thresholds. Synthetic tests do not establish long-session GUI responsiveness or media-heavy project performance.

`.github/workflows/creator-journey.yml` builds and tests Linux services, the macOS package, and the Windows editor/player/package on pushes, PRs and manual runs. Windows uses Qt 6.10.1 with its matching MinGW 13.1 toolchain, runs both native/Web routes and the actual Qt forms, then checks the bundled starter with developer Node/Qt paths removed. It verifies corresponding-source hashes, tests portable exports, and builds/tests the installer including overwrite protection and uninstall preservation. The tested unsigned installer and machine-readable evidence are retained as workflow artifacts for 14 days; no release is published. The release event also runs these checks as a post-publication audit, not an enforced release approval.
