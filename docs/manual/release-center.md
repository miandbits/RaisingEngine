# Release center

Open **File → Release center…** to keep a release's save examples, archive checks, recovery results and distribution notes together. This works with any game project. It checks a selected exported release, even while you continue editing the next version.

## Register an exact release

1. Use **Export a new release…**, enter a release version and keep game/project archives enabled.
2. Return to Release center. The export folder is filled in; enter a recognizable name and select **Register release**. You can also browse to an earlier export containing `RELEASE.json`, `Game.zip` and `Project-backup.zip`.
3. Review the selected release's version, target and content identity. Its recorded engine source can differ from the currently running editor. An older release without provenance stays **unknown**.
4. In **Backup & recovery**, run **Check release files**. This compares the declared exported files and game ZIP members with the release receipt. Changed files require a new export and registration; an old pass is not a new check.

If you move an unchanged export, browse to its new folder and register it again. An identical receipt reconnects the existing release, retains its previous location and manual notes, and requires fresh automatic checks. The selected release folder is always shown separately from the registration input.

The editor's About window shows its service/package version, source information, launch entry and support scope. Source information alone does not prove that separately built native binaries match that source. See [editor identity](editor-identity.md) and [platform support](../development/platform-support.md).

## Keep named save examples

In **Save compatibility**, enter a name such as “opening choice,” “year four” or “ending,” select the original save, and add it. An optional previous project folder supplies the original content for a save made before the selected release. Choose **Migrated save** only for a separately converted file; this is a designer label, not an inferred or approved migration history.

Run the selected example or all examples. Checks replay the saved command history against the **exported content**, validate state and presentation, and preserve the original file. The result distinguishes a direct pass, an invalid save, changed inputs, and a save that requires migration. A passed example is not a claim that every historical save or every player route is compatible.

For a save requiring migration:

1. Register its original project folder with the example.
2. Restore and verify the selected release's project backup in **Backup & recovery**.
3. Select the example and open **Review save migration…**. The target is the restored release project, not your potentially newer authoring draft.
4. Review the differences and any explicit presentation reset in the existing migration dialog, then publish to a new save file.
5. Add that file as a separate migrated example and check it. Retain the original example and its result.

Conversion does not bypass ordinary loading, replay checks, incompatible game identities or explicit review. Complete an interactive **load → progress → save → quit → relaunch → load** playtest on the intended player as well; record that evidence in Distribution.

A verified presentation-only export can deliberately retain an earlier runtime fingerprint while its editable backup has a new source fingerprint. Direct save checks still use the exported runtime content. The center does not offer migration into that differing editable backup as though it were the runtime baseline; retain the exact baseline project for any separately reviewed conversion.

## Verify recovery and record distribution

**Verify backup** extracts to temporary storage, checks declared hashes and reopens the project. **Restore and verify in a new folder** additionally retains a named restored project. Existing folders and the current project are never replaced. Plugin-containing backups keep plugin execution/trust review separate from byte and schema verification.

The **Distribution** tab records your target hardware, interactive save roundtrip, accessibility, rights/notices, branding and delivery checks. These are manual records, with an explanation required for a non-pending result. Selecting a result does not make the engine perform signing, certify asset ownership, test a physical device or obtain store approval. Keep unfinished checks pending.

Long checks run in an owned background process. You can continue authoring, close and reopen the center, or cancel the check. Closing the editor cancels owned work; interrupted results require another check. A recovery folder published just before cancellation is preserved for inspection.

## Preserve private evidence

Records live in the open project's private `.studio/release-workflow` directory. Save examples reference their original files and retain their hashes; moving, deleting or changing those files requires attention. Keep the local records, referenced saves, earlier projects and exact exports together in your private backup. The read-only **Details** tab exposes the selected record for inspection.

These private records and player saves do not go into `Game.zip` or the editable project archive. Share only the intended game payload. Archive integrity establishes byte consistency, not the author's identity or commercial readiness.
