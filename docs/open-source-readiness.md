# Open-source preparation and cleanup

Initial audit: 2026-10-15, based on the tracked working tree after `161b1fc`.

## Current status

This is a development repository being prepared for release, not a reviewed public source distribution. On 2026-10-18 the owner approved MIT for original engine/editor code; see the root LICENSE. Third-party software and media retain their own terms. See [Qt preview release](qt-preview-release.md) and [asset boundary](public-assets.md) for the current package and remaining public-release gates. The figures below describe the historical initial audit.

The initial tracked tree contains 4,748 files and approximately 530 MiB. `projects/` accounts for 3,673 files / 451.5 MiB, `examples/` 38.9 MiB and `assets/` 15.1 MiB. Several project copies contain the same large fonts or assets. Size reduction must preserve playable samples and regression coverage.

The binary inventory found 202 duplicate-content groups totaling 462.4 MiB of repeated working-tree bytes. This is not an estimate of Git clone/download savings: Git can already share identical blobs. Twelve PM2/Magical reference-project folders and three already-tracked evidence files need explicit review. The filename-only check found no credential candidates; file contents and history have not been secret-scanned.

PM2 project provenance identifies extracted source sprites, and Magical Princess reference provenance identifies a locally installed game's assets. Those records establish origin; they do not establish redistribution permission. Reference projects, copied assets in other fixtures, and prior commits must be reviewed before a public release. No files or Git history were removed by this first cleanup.

## Cleanup sequence and acceptance criteria

| Priority | Work | Completion evidence |
|---|---|---|
| P0 | MIT selected by owner; complete dependency and asset review | Root LICENSE is MIT. Dependency source/notice inventory and asset-by-asset distribution decisions remain |
| P0 | Separate reference-game projects and replace copied assets in public fixtures | Curated public sample set with verified provenance; no dependency on local game installations |
| P0 | Review secrets, personal paths and Git history | Dedicated secret scan covering history and working tree; documented handling of findings; no publication based solely on this inventory |
| P1 | Reproducible contributor setup | Fresh checkout on another Windows environment builds editor/player and runs a small documented smoke suite without private fixtures or local-only toolchain paths |
| P1 | Classify samples and tests | Public examples, minimal test fixtures and private experiments separated; existing route/save regressions still pass |
| P1 | Current documentation and feature/platform matrix | Qt-first entry point; historical Electron records clearly identified; each feature's verified targets recorded |
| P2 | Refactor by responsibility | Project service dispatch, editor tools, rendering, runtime and tests split incrementally; no behavior changes hidden in moves/formatting |
| P2 | Common style and automated checks | Agreed C++/TS/JSON formatting, linting and bounded CI checks; format-only commits separate from behavioral changes |
| P3 | Release packaging and contributor workflow | Version/changelog policy, complete dependency notices, reproducible artifacts, issue/PR and security reporting process |

Do not begin with a repository-wide rename or deletion. Many tests and launchers currently reference `projects/` directly, and the Electron path still has callers. First replace those dependencies with minimal fixtures or explicit adapters, then move or retire files.

## Intended repository boundaries

- Engine: `packages/`, `native/src/`, shared schemas and project services.
- Qt editor: `native/studio/`, `apps/qt-studio/`.
- Players and compatibility surfaces: `native/player/`, `apps/web/`, existing Electron paths.
- Public examples: a small reviewed set, with asset provenance and documented play goals.
- Tests: synthetic/minimal fixtures plus selected public end-to-end projects.
- Private reference experiments: retained locally or in a separate non-public workspace after dependency removal; not mixed into public sample releases.
- Generated outputs and local evidence: ignored paths; never required to build from a fresh checkout.

The first sample/fixture migration is now implemented; see [public sample separation](public-sample-separation.md). Original local paths remain intact. The earlier audit numbers above describe the pre-separation tree, not the new public tree. Git history and final licensing review are still separate release work.

## First cleanup completed

- Root README now describes the Qt editor, Windows-focused support and current regression workflow.
- Added a documentation index, contribution guidance and this release checklist.
- Added editor formatting defaults without rewriting existing source files.
- Added ignore rules for local evidence, working data, credentials and signing material. Ignore rules do not remove already tracked files or historical objects.
- Added a reproducible tracked-file inventory command, including reference-project categories and duplicate binary assets.
- Fixed the Qt launcher to honor the same `QT_STUDIO_OUT` used by the builder and report a useful missing-build error.

## Reproduce the inventory

```powershell
npm run audit:repository
node scripts/repository-audit.mjs --out .cache/repository-audit.json
```

The command reads tracked files only and optionally writes a JSON report. It does not modify projects, delete artifacts, scan untracked files, inspect prior commits, test secret validity or grant redistribution rights. Zero filename findings is not a security or licensing clearance.

## Decisions still needed

1. Engine license and contributor policy.
2. Which sample assets may be redistributed and which require replacement.
3. Release history strategy: cleaned existing history or a separate curated public repository. History rewriting requires a backup and coordination with collaborators.
4. First officially supported release target and the supported Qt/toolchain matrix.
