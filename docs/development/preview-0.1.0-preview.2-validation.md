# Preview 0.1.0-preview.2 validation

This update completes the built-in advanced editor message catalogs and prepares easier Windows distribution. It does not translate authored game content or arbitrary third-party diagnostics.

## Editor checks

- `npm run check:editor-i18n`: 2,886 / 2,886 Korean literal `tr()` messages covered; 2,895 editor catalog rows and 537 diagnostic rows. Duplicate and placeholder checks pass.
- Qt build with Qt 6.10.1 / MinGW 13.1 succeeds.
- `qt-editor-language-test ko|en|ja`: actual Qt project manager and editor, language persistence/restart, all translated CJK glyphs, service error localization, opaque Korean paths/percent tokens, document preservation, and nine advanced dialogs pass.
- Dialogs inspected: reusable components, data UI, battle HUD, expeditions, time sessions, rules, rule extensions, goal routes and localization. Captures use Qt's offscreen platform, not manual desktop acceptance. The plain-label width check reports zero clipped labels in these dialogs at their tested initial sizes. Other sizes, DPI settings and plugin UI still require review.
- Japanese component properties previously compressed editable values beside long labels. Labels now sit above expanding fields, with vertical scrolling and complete combo text in tooltips.
- `designerQuickRules`, `reusableComponentsAndResponsivePlay`, `localizationAuthoringAndPlay`: 5 passed including setup/cleanup, 0 failed, 0 skipped. These use a disposable project and include actual player checks.
- `npm run check:repository` passes. Unknown external errors retain the source text; translated response fields also retain the original diagnostic for inspection.

## Distribution

Installer and portable ZIP are generated from an explicit manifest with pinned source archives and SHA-256 payload hashes. The per-user installer registers versioned Start menu shortcuts and a Windows Installed apps entry. It still rejects existing target folders and preserves unknown files on uninstall. Publication gates remain in `config/preview-release.json`.

The package was built from clean commit `fd7e595606158e68256b51a602e3765fd7e1261c`. Audit: 1,517 payload files, 27 required notices, four pinned corresponding Qt source archives; no legal-clearance claim.

- English installer workflow passes with system Node/Qt paths removed: install registration and shortcut, create/edit/save/reopen in Qt, Windows/Web export, exact native save restoration, existing-folder rejection and uninstall preserving unknown files, project and save.
- Japanese portable workflow passes from the **extracted ZIP**, including manifest hashes, Qt creation/edit/export and exact native save restoration.
- `tests/journey/authored-adventure-web.test.mjs` passes against the installed editor's actual web export: 1 test, 0 failures.
- [GitHub Actions run 39307396608](https://github.com/miandbits/RaisingEngine/actions/runs/39307396608) passes on the package source commit, including translation coverage, headless/runtime build, route regression, rule extensions, save durability and public-content checks.

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| Windows x64 installer | 121788672 | `943fc221ea83214b3f68227541c640e3017457fa0b9cb5e222da2e40a633a572` |
| Windows x64 portable ZIP | 167046998 | `c0c8199e3896f3d7dbf9ff4a248c077f1e2e05ca258bb9a3b19941cc64de5d86` |

Reproduce via [preview preparation](preview-0.1.0.md). Local screenshots and detailed logs are generated under `.cache/preview-i18n/evidence`; generated package/install receipts are not source assets and are not committed.
