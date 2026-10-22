# Fixing validation issues at their source

In the Qt editor, select **Validate**, then click an issue in the **Problems** panel (or select it and press Enter). The location editor shows the diagnostic code and reason, document, exact field path, and the surrounding data. For screen elements, the corresponding screen and element are also selected behind the dialog.

1. Inspect the selected field. Nearby fields can be selected in the tree; for example, an event period error may require changing its start rather than its end.
2. Edit text, numbers, booleans, or null values. Supported references offer names and IDs from the current project: characters, activities, events, endings, attributes, resources, assets, locations, actions, screens, components, and script nodes.
3. Select **Apply and revalidate**. This applies one undoable editor transaction and runs the shared project validator immediately. The result includes remaining errors and warnings.
4. Close the dialog and select the next issue from the refreshed list. Save the project when ready. Applying a repair does not silently save the project to disk.

**Remove field and revalidate** removes only the selected field or array member. Use it for an unexpected property; required-field errors remain visible if a required value is removed. Containers are expanded to edit their individual fields. Missing scalar fields can be added at the reported location; this is not a replacement for the normal content-creation tools.

The **Revalidate** button remains available in the Problems panel. Validation first commits any focused inspector field and waits for its pending edit. If a diagnostic belongs to an older revision or another project, the editor refreshes the list and asks you to select again rather than trusting an obsolete array index. A repair dialog also refuses to overwrite a newer project revision.

Malformed files that could not be loaded, and plugin-owned documents, are not rewritten by this editor. Their original diagnostic and location remain visible with instructions to repair the file or use plugin diagnostics.

No project schema, save format, or runtime behavior changed. The normal content-fingerprint compatibility rules still apply when you intentionally change game content.

## Verification

Create a disposable public test project:

```powershell
node tests/qt-studio/validation-location-fixture.mjs .cache/validation-location-fixture
$env:RAISING_ENGINE_ROOT = (Get-Location).Path
$env:RAISING_VALIDATION_PLAN = (Resolve-Path .cache/validation-location-fixture-plan.json).Path
$env:RAISING_QT_TEST = '1'
$env:QT_QPA_PLATFORM = 'offscreen'
# Add your Qt runtime/compiler bin directories to PATH if required.
& dist/qt-studio/qt-validation-location-test.exe
```

The fixture intentionally introduces four defects. Real Qt widgets repair the screen size, event window, script link, and activity cost reference; tests also cover cancel, invalid numeric input, undo/redo, stale results, focused inspector edits, project save/reopen, missing scalar fields, and escaped field names. With no fixture plan, the independent pointer and dialog tests still run.

The private game verification uses a separate project copy and isolated save profiles. Its assets, story, and evidence are not distributed with the public engine.
