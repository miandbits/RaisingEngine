# Isolated macOS Qt accessibility repair

Qt 6.12.0's Cocoa accessibility layer can delete interfaces still owned by a
table or tree view. Native queries over a selected row after model changes can
then fail or crash. The observed editor paths were command search and filtered
hierarchy navigation. This is separate from game/runtime behavior.

The official Qt 6.12 backport is
[b74d6cb](https://github.com/qt/qtbase/commit/b74d6cb3c5e8418602494ae549b60839d88ef858),
fixing QTBUG-149612. It changes two Cocoa accessibility files and adds the native
`tableViewSelectionAfterModelChange` regression. Accessibility stays enabled.

## Build and package an explicit repair

Ordinary packaging is unchanged. The optional repair is currently restricted to
an exact Qt 6.12.0 release arm64 kit and a macOS 14.4 deployment target. Its private
ABI is checked against the kit's QtCore, QtGui and QtWidgets binary hashes and
UUIDs. Do not transplant it into a different Qt kit, architecture or old app.

From the repository, with the existing Qt editor and game runtimes built:

```sh
QT_ROOT=/opt/homebrew/opt/qt node scripts/build-qt-cocoa-repair.mjs \
  --out .cache/qt-cocoa-repair-new

QT_ROOT=/opt/homebrew/opt/qt \
MACOS_QT_COCOA_REPAIR=.cache/qt-cocoa-repair-new/repair-manifest.json \
MACOS_STUDIO_OUT=dist/macos/RaisingStudio-repaired.app \
node scripts/package-macos-studio.mjs
```

Use a supported Node 22 and a standalone `MACOS_NODE_ROOT` as for ordinary Mac
packaging. Both output directories must be new. The repair builder downloads
checksum-pinned official Qt/Khronos source archives, applies only the official
backport, and builds the Cocoa plugin and Qt's native test. It does not build or
install a whole Qt kit. It opens disposable native test windows for the selected
row, table, tree, hierarchy, text-field and checkbox tests. A desktop session is
required. Keep the desktop quiet during these checks: the hierarchy test queries
the system-wide focused accessibility element, so interaction with another app
can invalidate its focus assertion. Failure or a different loaded plugin prevents
acceptance.

Packaging validates the entire repair before creating the app: manifest shape,
relative regular files, binary/archive/log hashes, exact Qt version, plugin
metadata/architecture/deployment target, framework identities, and the original
source/patch/license bytes inside the source kit. It replaces only the copied
Cocoa plugin before normal dependency relocation and signing. Installed Qt and
existing apps are never modified. The package manifest records the repair and
source identities. Missing, stale or incompatible inputs fail explicitly.

The resulting app includes `Contents/Resources/licenses/qt-cocoa-repair/` with
the corresponding-source archive, repair notice and native regression summary. This
adds roughly 56 MB. The archive contains the complete original Qt source, exact
patch, compile-only Khronos sources, LGPL3/GPL3 notices, CMake driver and rebuild
recipe. No third-party source archive or binary is committed to this repository.

## Rebuild from the embedded source kit

Extract `source-kit.tar.gz`. With the same identified Qt kit and Xcode/CMake/Ninja:

```sh
python3 rebuild.py --work-dir /absolute/path/to/new-output \
  --qt-prefix /opt/homebrew/opt/qt
```

The output directory must not exist. The script verifies the kit and all source
checksums, applies the official patch and rebuilds the plugin/test without
installing or launching them. The original source archives retain their full
copyright/license material; the patch and build instructions describe the local
modification. The plugin remains a separately replaceable dynamic library.
Replacing a library in a copied app requires relocating its Qt dependencies and
re-signing that app; do not change a running or preserved app.

## Acceptance and limits

Qt's native regression fails with the original plugin at selected-child lookup
after a model change and passes with the backport. Offscreen widget tests alone
do not verify this boundary. In a separately packaged editor, repeat real macOS
accessibility queries while filtering and selecting command/hierarchy rows and
closing dialogs. Verify keyboard/selection behavior remains available. Then run
`scripts/test-macos-package.mjs` and strict code-signature verification for the
app and its ordinary copy/archive extraction.

This is a local build of an official backport, not a claim that the installed
Qt release has changed. Use an official fixed release when one is available and
validated; do not assume an arbitrary downgrade is equivalent. This option does
not claim notarization, Developer ID distribution, App Store acceptance, or
complete VoiceOver acceptance across all editor interactions. Native/Web/iOS
game runtime sources are unaffected.
