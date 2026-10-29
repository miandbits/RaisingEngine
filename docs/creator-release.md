# Package and restore a release

Open **File → Project Settings → Release**. These settings belong to the asset manifest's separate `release` object. They do not add gameplay assets or change the gameplay fingerprint used by saves. Applying project and release settings is one undoable edit; Save persists it.

## Declare notices

Choose **Add notice…** for each license or notice your game must distribute. Use **View notice** to review it and **Remove selected** to remove a declaration. The editor does not infer licenses, download legal text, or decide whether your distribution complies with terms.

Imports accept nonempty UTF-8 text, at most 1 MiB per file, 64 files and 16 MiB total. Original bytes, BOM and line endings are preserved. Imported files receive immutable content-based names under `assets/release/`. Each declaration records the expected SHA-256 and the original filename as a readable label. Labels do not infer a license type. A missing, changed, unsafe, linked or colliding file blocks export; import the intended replacement instead of editing an imported file in place.

Every target includes the exact declared bytes under `licenses/project/<notice-id>.txt` plus a hash index. For macOS this directory is inside `RaisingPlayer.app/Contents/Resources`; for iOS it is inside the generated game's resources. Versioned project backups and draft recovery also retain these source files. Adjacent files that you have not declared are not automatically included.

## Brand the application

Set **Application name** and choose **Choose icon PNG…**. The common icon source must be an opaque 1024 × 1024 PNG, at most 20 MiB. Import occurs when settings are applied. **Use default icon** clears the custom icon.

The exporter creates actual platform resources:

- Windows: PE icon groups at 16–256 px and ProductName/FileDescription version strings. Resources are read back after stamping. Signed input executables are rejected; sign the final branded export afterward.
- macOS: a multi-resolution ICNS file and Info.plist display-name/icon entries, before the editor applies its development signature.
- Web: PNG favicon, 192/512 px manifest icons, page title and linked Web app manifest. The running browser player retains the application title. HTTP hosting remains required; no offline service worker is added.
- iOS: an AppIcon asset catalog with iPhone, iPad and App Store sizes, a display-name plist, and generated Xcode resource settings. The built-app metadata verifier checks the compiled catalog and primary icon declaration.

Executable and bundle filenames remain `RaisingPlayer`. The native window and browser tab use the application name; authored game-title bindings keep the game title. Changing an application name does not rename save locations or change game identity. Platform signing, provisioning, notarization, store review and physical-device acceptance are separate steps. A development or simulator build is not a store submission.

## Verify and restore a project backup

Export with a release version to create `Game.zip`, `Project-backup.zip` and `RELEASE.json`. Before publishing the export, the editor extracts the project backup into owned temporary storage, verifies every declared source hash, reopens its documents, and checks the source gameplay fingerprint for projects without plugins. The release receipt records that verification and the archive hash.

For restoration, open **Project Settings → Release → Verify and restore project backup…**:

1. Choose `Project-backup.zip` and select **Verify backup**.
2. Read the verification result, then select **Restore to new folder…**.
3. Choose a new folder outside the open project. Existing destinations are never replaced.
4. Open the restored project when ready.

Restoration rechecks the ZIP hash so replacing the selected archive after verification cannot silently restore different bytes. It rejects path traversal, symlinks, case-folded filename collisions, undeclared entries, bad CRCs, source hash mismatches and oversized archives. Current limits are 50,000 files, 256 MiB per file and 1 GiB total.

Verification checks consistency, not the archive author's identity. It never executes plugins from an untrusted archive. Plugin projects receive byte/schema verification with an explicit pending plugin-validation result; review and trust their preserved `release-dependencies/plugin-store` before selecting that store and validating the restored project. Source verification does not certify that a game is fun, accessible or legally ready to distribute.

These controls work in any project. No sample-game path, font name or license is special-cased.
