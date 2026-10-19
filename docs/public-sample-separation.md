# Public samples and local experiments

## Directory contract

| Location | Distribution | Purpose |
|---|---|---|
| `examples/` | Tracked public sample candidates | Five academy examples, six genre templates, three-month pilot |
| `tests/fixtures/projects/` | Tracked test data | Isolated regression projects, including asset-free rule fixtures |
| `projects/` | Ignored local workspace | Existing user games, saves, experiments and reference-game projects |
| `internal/`, `.cache/`, `docs/evidence/` | Ignored local work | Private experiments, generated checks, recordings and screenshots |

`config/repository-content.json` records the migration map, public sample roots, local-only tools and hashes of known excluded reference assets. This classifies distribution boundaries, not copyright ownership or an engine-wide license.

## Preservation

All existing project files stay at their original `projects/...` paths. They were removed from the Git index with `git rm --cached`, not deleted from disk. Existing `.studio` state, untracked saves and running user windows are untouched. Reference-only launchers and scripts also remain on the original PC at the same paths but are no longer distributed.

The local preservation inventory is `.cache/project-preservation-2026-10-15.json`. It records SHA-256 and size for 3,708 files removed from tracking, including the original project data, tools and tracked evidence. Do not publish that local inventory as a replacement for the public manifest.

Public pilot and genre copies are independent from the original local projects. Existing users can keep working in their original folders. Launchers for public demos and fixture-based tests now use the new paths.

## Tests

- General rule checks no longer load `projects/magical-*`. `action-day`, `class-queue` and `free-expedition` contain neutral test screens and authored rule vectors, with empty asset manifests and no original dialogue/media/rigs.
- Presentation save/export checks create a fresh synthetic cutout project and plugin store. They do not consume a personal save or `.cache/character-verification/latest.json`.
- Reference-only authoring/verification tools remain local. They are not prerequisites for public CI.
- Registered pilot routes and immutable baselines move together to `examples/starlight-three-months`. Their gameplay content and acceptance criteria are preserved.

```powershell
npm run build:headless
npm run check:public-content
npm run test:public-content
npm run test:regression
```

`check:public-content` checks the current Git index/worktree for local-only tracked paths and known reference-asset hashes, including renamed copies. A negative regression test proves that a renamed excluded byte sequence is rejected. Sample/fixture validation and the affected rule tests run in CI.

Additional native/Wasm and plugin tests retain their normal build prerequisites. The narrow public-content suite does not claim to cover every GUI or animation integration scenario.

## Important history boundary

This change removes reference assets from the **new tracked tree and new source archives**. Earlier commits still contain previously tracked assets. Neither `.gitignore` nor `git rm --cached` removes Git history. Do not make the existing full history public on the assumption that this commit scrubbed it.

A future public release must either use a reviewed, fresh-history source snapshot or perform a separately coordinated history cleanup. No history rewrite, force push, repository visibility change or deletion of original assets is performed here.

Known-hash checking cannot prove that every unknown asset is redistributable. The owner selected MIT for original engine/editor code on 2026-10-18. Generated-asset decisions, font/dependency redistribution review and a full secret/history review remain on the [release checklist](open-source-readiness.md).

## Verification (2026-10-15)

- Preserved files: all 3,708 recorded local files retain their original SHA-256. Pilot JSON and immutable baseline files are byte-identical at the new public location.
- Tracked working-tree footprint: approximately 530.0 MiB → 296.2 MiB. This is not a Git-history/clone-size reduction claim.
- Existing default suite: 343 passed, 0 failed. Fixed a duplicated fixture-path prefix found by the worker-pool regression.
- Public-content suite: 9 passed, including all public sample/test-project loads and rejection of renamed excluded asset bytes.
- Route regression suite: 7 passed. All four canonical pilot routes and their save replays pass from the new public path.
- Isolated source export: extracted only the staged public tree into a separate folder, initialized its own Git index, installed dependencies, built the headless runtime, and passed public-content plus route-regression checks with no `projects/` directory or pre-existing project/save cache.
- Qt build passed in `dist/qt-studio-public-samples`. The real `goalCiReview` UI scenario passed graph navigation, partial approval, baseline restore and save/replay revalidation using a copy of the public pilot.
- Fixed the old unanchored `release/` ignore rule, which also hid new tests under `tests/release/`. Restricted the virtual browser test's dependency discovery and disabled file watching so it does not crawl preserved local projects or isolated source copies.

Local logs and the preservation inventory are under `.cache/`; GUI captures are under `docs/evidence/goal-ci/`. These generated records remain excluded from source distribution.
