# Public repository cleanup

The product boundary is Qt authoring, C++ rules, native/Web presentation, shared services and optional plugins. This cleanup preserves game/save schemas, user projects, current editor windows and the animation/integration worktrees.

## Changes

- Electron shells/editor and their build configuration are under `legacy/`; production Web presentation stays in `apps/player/renderer`. Default editor/player commands target Qt/native builds. Legacy commands require an explicit `legacy:` prefix.
- The 27 experiment launchers are no longer tracked at the root. Historical copies live in `tools/experiments`, while existing local root copies remain untouched.
- Wuxia authoring trials moved from public examples to regression fixtures. Original local project copies remain at their previous paths. Public examples emphasize five selector templates, the genre suite, rule extension example and three-month pilot.
- Documentation is separated into getting started, manual, reference, development and historical archive. Private reference-game study notes and asset preparation scripts remain local and untracked.
- 111 repeated fixture media files (208,984,711 bytes) are represented by a source/hash manifest. `npm ci` reconstructs the project-local files, checks hashes and refuses to overwrite local edits. Self-contained public templates keep their own assets.
- Native graphical dependencies were previously available only in local ignored checkouts. Exact upstream commits now have an explicit bootstrap/verification step. Generated Web save-validator code is built from its schema rather than versioned.

## Acceptance and preservation

Clean-checkout and history results are recorded in [the verification report](repository-cleanup-verification.json). The independent clone passed fresh dependency installation, C++ core/Windows player, Qt and Web builds, seven public/service tests, actual Qt widget creation/edit/export, Windows exact save restoration and Chromium exploration/combat/save replay. A clean checkout may use installed compilers/SDKs, but must not copy the original checkout's `node_modules`, `dist`, dependency source trees or build caches. Qt creation/export runs through actual widgets offscreen; browser/Windows play tests remain separate evidence.

The pre-cleanup Git bundle and preservation inventory are outside the repository in the local review directory. The inventory covers 13,723 persistent user/reference files and records animation/integration worktree heads/status. All original refs remain available; existing saves and user projects are not migrated by repository reorganization.

## History experiment

History filtering is permitted only in an independent bare mirror under the review directory. Remove local-only paths, private tools and known excluded asset blobs, verify all refs and object integrity, then compare the filtered current branch tree with the tested source tree. A filtered mirror is a review artifact, not authorization to force-push shared refs or publish rewritten history.

Known-path/hash checks are not proof of complete secret detection or media rights clearance. Public binary signing, clean-VM testing and full dependency/source/provenance review remain release gates.

The isolated experiment removed 3,731 local/private historical paths and 142 known excluded asset blobs, with zero remaining matches and a successful `git fsck --full`. Its current branch tree stayed identical. Original shared history still contains those historical objects: ordinary cleanup commits do not remove them. Publishing filtered history requires a separately coordinated decision; this cleanup only performs a normal branch push.

For a read-only audit of a repository or mirror, use `python scripts/audit-public-history.py --repository <path> --policy config/repository-content.json --output <outside-repository-report.json>`. Reports list suspect object IDs and paths, never matched credential values. Preserve a verified bundle and all worktree branches before experimenting with history.
