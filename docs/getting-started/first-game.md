# My first raising game

Choose **My first raising game · recommended** in the project picker, choose a new or empty folder, and click **Create project**. In an open editor, **File → New project** offers the same starter. Source builds include it; previously published Windows preview installers do not receive it automatically.

The starter is a complete three-month game: nine activity periods, four activities, a relationship choice, a conditional follow-up, two endings, and editable title/schedule/story/ending screens. Its text and geometric layout are original engine sample content under MIT. It requires no plugins or imported artwork.

## Follow the checklist

The **My first raising game** dock opens automatically. Reopen it from the authoring tools menu if closed. The starter is playable immediately; customization steps are optional. Choose **Where will people play?** early. The checklist shows missing runtimes and unsupported features for that target as you edit.

1. **Play your starter.** Use **Play from start**. Try Garden walk, Train, and Rest in the first month. Confirm the schedule, start each activity, and continue past its result. Accept Mira's invitation to see the follow-up next month. Save and Load buttons work during the season.
2. **Name your game.** The checklist opens Project settings. Change its title and description. Each new project gets its own game ID so its saves stay separate from other starter projects.
3. **Shape growth and activities.** Open an activity with the checklist, then edit its label, costs, rewards, or requirements using the existing forms. Rest offers a free recovery option. Attributes are editable in the content tools.
4. **Tell your story.** The checklist opens Connections, where characters, events, choices, conditions and effects can be edited. Follow the garden invitation and its accepted/declined consequences.
5. **Choose your endings.** The Together ending needs at least 30 Skill and 10 affection with Mira. Own path is the fallback. Change the text and requirements in the ending editor; keep a fallback available.
6. **Make the screens yours.** Open the title canvas. Edit text, colors, layout and reusable Save/Load controls, or import your own images. Changes use the normal undo history.
7. **Validate and finish a playthrough.** Resolve errors in Issues, then play the current version through to an ending. Changing content makes this check stale until another playthrough finishes.
8. **Export your game.** Choose a target and a new output folder. The editor exports the current draft, including unsaved changes, without overwriting an existing output. Set a release version such as `0.1.0`, leave **Create a game ZIP and an editable project backup** enabled, then use **Play exported game**. Share `Game.zip`; keep `Project-backup.zip` and `RELEASE.json` for future updates.

The checklist records actual content changes, player progress and successful exports. Clicking a button alone does not complete a step. Progress is local in `.studio/first-game.json`; it is not included in the game. Save the project to apply your authored changes to its files. Accepted unsaved edits also get a recovery copy; text still being typed in an unapplied form is not covered. An old player session cannot complete the validation step for a newer draft.

A Together route is Garden walk / Train / Rest, then Train / Train / Rest, then Work / Rest / Rest, accepting the invitation. Declining the invitation and resting leads to Own path. Both routes have replay scenarios in the sample, including save restoration coverage.

## Play the exported game

- **macOS:** open `RaisingPlayer.app`. The game and fonts are inside the app; user saves live separately in the user's Library. The build matches the Mac's architecture. Local exports use an ad-hoc signature; public distribution still needs Developer ID signing and notarization.
- **Windows:** run `RaisingPlayer.exe` with the other exported files beside it. A Windows runtime must be installed to export this target from a Mac.
- **Web:** click **Play exported game** to open a local browser preview; the editor starts its own local server, with no terminal needed. Keep the editor open while previewing. Publish the contents of `Game.zip` to static HTTP/HTTPS hosting. Do not open `index.html` using `file://`. Build/install the Web runtime first; export does not upload the game.
- **iOS:** on a Mac with Xcode and CMake, export an **iOS · Xcode project**. Open `build-iphonesimulator/RaisingGame.xcodeproj`, select the `raising-player` scheme and an iPhone/iPad simulator, then Run. If generation failed or you moved the folder, double-click **Generate Xcode project.command**. Sources and game data remain available even when project generation fails. Physical devices require your Apple development team and provisioning; store releases also need icons, signing and device testing.

Keep projects outside the engine installation. Keep backups of released content and saves: changing the game may require the editor's explicit save migration tools. macOS and iOS currently export plugin-free games; Windows plugin binaries cannot run on Apple platforms.

## Recover edits and explain a blocked ending

Use **Recover unsaved work…** in the checklist or authoring tools menu after an interrupted session. Select a recovery copy and a **new folder outside the original project**, then choose **Restore as a new project** and **Open recovered project**. Recovery verifies every captured file and includes unsaved imported artwork. It preserves the original project and other sessions' recovery copies. Saving or explicitly discarding a session removes that session's recovery. Recovery is local, so keep external backups too.

Use **Why didn't an event or ending happen?**, choose **Ending**, select Together, and inspect it. The condition list shows names and current values—for example, “Skill is 0; requires at least 30.” Load a player save to inspect that point in a run. The explanation distinguishes unmet requirements, a higher-priority matching ending, and the fallback. It predicts selection *if the season ended at those values*; it does not prove that an ending is reachable.

**Save and rerun named playtests** opens the existing route tools. Keep successful runs under meaningful names and compare them after edits; the comparison identifies the first changed step. A failed recorded route and a bounded search that found no route are different outcomes. Neither proves that every route fails.

## Keep a release you can maintain

`Game.zip` preserves the exact exported player and content bytes. `Project-backup.zip` contains the editable draft, referenced assets, named test routes, approved baselines and any pinned installed plugins. Extract it to a new folder to resume editing. If plugins are present, select `release-dependencies/plugin-store` in Plugin Manager. `RELEASE.json` records the release version, content fingerprint, source/runtime hashes and dependency versions. These are local files; the editor does not upload them.

For iOS, the game ZIP contains sources, presets and the regeneration command. Generated Xcode projects use local paths and must be regenerated after extraction. The archive is not an App Store binary. Archive writing currently supports up to 65,535 files, 256 MiB per file and 4 GiB total; exceeding a limit leaves the export unpublished and the form available for correction.

Keep the exact old release before changing rules or story content. A version number alone does not make player saves compatible. Use the existing save migration and replay tools before promising that an update accepts old saves.
