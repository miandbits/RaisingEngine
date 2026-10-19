# Archived Electron applications

The supported editor is `native/studio` (Qt), started by `npm run studio`. This directory preserves the earlier Electron editor, player main/preload processes and their configuration for compatibility work.

The production Web player still uses `apps/player/renderer`. Those React modules are shared presentation, not discarded Electron code. Rules remain in C++.

Use explicit commands such as `npm run legacy:build:studio` and `npm run legacy:build:player`. Dependencies remain pinned in the root lockfile. If you skipped the Electron binary download, obtain it before Electron UI tests.

Legacy outputs retain `dist/studio` / `dist/player` for existing regression tests. They are not current release installers. Game/save formats and animation contracts are unchanged; do not implement new engine behavior only here.