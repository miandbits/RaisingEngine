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

Reproduce via [preview preparation](preview-0.1.0.md). Local screenshots and detailed logs are generated under `.cache/preview-i18n/evidence`; generated package/install receipts are not source assets and are not committed.
