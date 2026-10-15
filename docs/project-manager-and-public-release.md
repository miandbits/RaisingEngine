# Project manager and public release boundary

## Product entry point

Raising Studio is the authoring application; Raising Engine is its runtime and shared data contract. The intended distribution opens a project manager first, then the same editor for any compatible project. Public samples are starting points, not hard-coded games.

The Qt launcher now supports:

- Recent projects with title/path search and persistent, user-local history (up to 40 entries).
- Opening a project folder or `project.json`, including projects outside the engine checkout.
- Creating a project from one of five public templates into a new/empty directory. The default location is Documents/Raising Studio Projects.
- Explicit missing-path and service errors, retry after failed creation, and removing a recent entry without deleting its files.
- `File → Project manager` inside the editor, using the existing unsaved-change guard.
- Existing `--project` launchers continue to open their project directly. Successful direct opens also enter the recent list.

Recent history uses Qt's user-scope INI settings for RaisingEngine/ProjectManager. It is not part of project JSON or Git. `RAISING_PROJECT_HISTORY` supplies an isolated settings file for tests; ordinary Qt tests do not modify real recent history.

Validation: `node scripts/test-qt-studio.mjs projectManagerRoundtrip projectSetupAuthoring` covers actual Qt controls, failed-create retry, non-destructive overwrite rejection, Unicode paths, persisted history/deduplication, filtering, entry removal, missing-path errors, reopening and editor loading. `npm run test:qt-service` covers the shared service. Test history is isolated and temporary projects are disposable.

Run `npm run studio:qt` without `--project` to open the manager. `QT_STUDIO_OUT` selects an alternate build directory for both build and launch.

## What this does not yet establish

The startup workflow is implemented; this is still an authoring alpha. A self-contained distributable must bundle the Node service/dependencies, public templates, runtime/player, fonts and required notices so it needs neither this repository nor a separately installed Node toolchain. Release installers, upgrades, crash recovery, clean-machine checks and a supported-version policy remain separate release work.

Godot-like product maturity is the direction, not a completed acceptance criterion. Priorities after the launcher:

1. Owner-approved engine license, dependency and public-asset provenance notices; history/secret review.
2. Reproducible standalone Windows package and installer tested on a clean machine, with engine upgrades independent of user projects.
3. Template/version migration coverage, recovery from interrupted saves and missing dependencies, actionable import errors.
4. A short first-game guide and stable editor workflows verified by designers without developer help.
5. Performance and usability trials on large commercial-scale content, followed by platform-specific certification work. Console support is unverified.

## History cleanup and shared worktrees

The development checkout shares Git history with animation/integration worktrees. Rewriting it in place would invalidate those branches and their ongoing work. Preserve the working repository, ignored projects, save data and windows; create a verified `git bundle --all` outside it, then filter an independent mirror for publication.

The public-history policy removes `projects/`, `internal/`, generated/local/evidence directories and the private tools in `config/repository-content.json`, across all mirrored refs. Known reference-asset SHA-256 fingerprints must also be removed regardless of historical filenames. Retain engine source history and original public examples.

Verify the cleaned history for prohibited paths and known asset fingerprints, check object integrity, and compare the current integration branch tree byte-for-byte with the tested source tree. A cleaned local mirror is not a rewritten GitHub repository: publishing rewritten refs requires an explicitly coordinated remote update and fresh clones for collaborators. Do not merge an old, unfiltered branch back into the clean history.

This policy is not an assertion that every asset is licensed, a secret scan, or removal from GitHub caches/forks. Those release checks remain necessary.
