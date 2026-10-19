# 0.1.0-preview.2 preparation

Prepared from the public `miandbits/RaisingEngine` history, without the private repository's binaries, projects or animation worktree. The installed editor is `RaisingStudioQt.exe`.

## Included

- Windows x64 Qt editor, bundled Node services, C++ runtime, Windows player and Web export runtime.
- Korean, English and Japanese editor language selection, persisted per computer and applied on restart. Session override: `--ui-language`.
- Translated main flow and advanced tools: 2,886 Korean literal editor messages covered, plus 537 service/validation catalog entries. Unknown external errors and designer content retain their original text. Error parameters, IDs and paths are preserved.
- Versioned Start menu and Installed apps registration; installer and portable ZIP with SHA-256 checksums.
- Noto Sans JP and OFL notice, standard Qt button translations, three-language installation/first-game guides.
- Hash-verified corresponding Qt module sources, additional stb/RmlUi container notices, package inventory audit.

## Reproduce

Use README prerequisites and build headless, native player, Web and Qt from this checkout. Use an output directory separate from any running editor. Then:

```powershell
npm run check:editor-i18n
npm run prepare:qt-source
$env:QT_PACKAGE_OUT = 'release/0.1.0-preview.2/RaisingStudio'
$env:QT_INSTALLER_OUT = 'release/0.1.0-preview.2/RaisingEngine-0.1.0-preview.2-windows-x64.exe'
$env:NSIS_COMPILER = 'C:/path/to/makensis.exe'
npm run package:qt-studio
node scripts/audit-preview-package.mjs
npm run installer:qt-studio
npm run package:preview-artifacts
$env:RAISING_TEST_UI_LANGUAGE = 'en'
npm run test:qt-package
$env:RAISING_TEST_WEB_EXPORT = (Get-Content .cache/qt-release/latest.json | ConvertFrom-Json).web
node --test tests/journey/authored-adventure-web.test.mjs
```

Use a new package output directory for a second run. The installer workflow checks existing-folder rejection, GUI project creation/editing, Windows/Web export, Windows exact-state save restoration and uninstall preserving unowned user files. Use `--portable` to test another UI language without reinstalling; it explicitly does not claim installer verification. Offscreen Qt tests operate real widgets; they are not manual user acceptance tests.

The installer receipt records SHA-256 and signing status. `package-manifest.json` records source commit, dirty-tree status, payload hashes, languages, dependency versions and Qt source archives. `scripts/audit-preview-package.mjs` verifies required notices, archive/payload hashes and exclusion of test/shader compiler DLLs. This technical audit is not a universal legal clearance.

## Remaining checks

This is an **unsigned preview**, not a stable release. A clean Windows VM test, signing decision, public sample media permission review and native-speaker editorial review remain. No mobile or console certification is implied. Prepare versioned assets in a draft GitHub Release for review; do not publish the draft or mark `publicReleaseReady` true solely because local tests pass. See [dependency review](preview-license-review.md) and [language scope](../manual/editor-languages.md).

GitHub Releases is the download channel for the desktop application. GitHub Packages is intended for dependency/container distributions and is not required to install this editor. A future winget manifest should reference an already published, immutable installer and its verified hash; no package-manager publication is claimed by this preview.
