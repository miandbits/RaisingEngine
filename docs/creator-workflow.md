# Working from a game state in the editor

The canvas now starts with **Game state** enabled for a project that has no saved canvas preference. It uses the existing isolated native renderer with the initial game state, or a replay-validated save chosen with **Choose state**. It does not advance the game or overwrite a player's save. The state label identifies an initial state, a verified save, or virtual changes. The current authored screen stays selected: a preview of it does not prove that every playthrough can reach it.

The opening document prefers the authored title route, then a title screen, the routing default, a home screen, or the first available screen. This avoids opening an incidental save dialog just because it is first in the project file. It does not infer a story route.

**All elements** explicitly returns to the editable design view, including elements whose display conditions are false. **Preview conditions** also uses this design mode for its temporary condition values. Choosing Game state again restores the current runtime snapshot or refreshes it. An explicit canvas-mode preference is retained per project; old projects that were deliberately left in design mode keep that choice. Failed or stale snapshots are never presented as current runtime output. Automatic canvas snapshots use the passive request lane, so a pending refresh does not disable editing or discard a dialog Save. Their replies are tied to the captured project, revision and preview settings; they cannot replace the current editor documents.

## Find a task

The first toolbar action, **Tasks**, opens a movable, dockable panel with **Create content**, **Design screens**, **Check play** and **Prepare release** stages. It is also in the View menu. The panel stays open while a tool is used, retains its selected stage when closed and reopened in the same editor session, and shows the current project, screen and element by name. A stage is navigation, not a passed checklist. Commands use the same live actions, availability and input guards as menus. **Find any tool** opens command search; Cmd+K on macOS and Ctrl+K on Windows remain available. A project must be loaded and its pending operation finished before project-specific tasks become available.

The inspector hides image, value-binding and button-action rows that do not apply to the selected element. Switching selections does not change or delete their stored settings. Runtime-preview selection and movement use the same logical-to-raster transform as the player image, including Retina and letterboxed snapshots; keyboard movement remains in authored pixels.

**Project version control** accepts Windows `.exe` files, macOS `.app` bundles and Unix executable files. Application bundles open the project through macOS Launch Services; executable files receive one literal project-path argument. The panel still only reads Git status, diffs and history. Commit, merge and push remain external-tool operations.

## Reading order before detailed styling

In **Screen → Readable layout**, screen selection, reading order, bottom actions and preview size remain in the basic view. **Appearance and detailed settings** reveals the global style, dimensions and selected-item options. Collapsing the section does not reset values or remove authored fields. Invalid background colors and invalid selected-item settings reveal the relevant advanced controls for repair. The fixed Save/Cancel footer and scrolling settings body retain their short-window behavior.

Screen and element names use authored labels, names or text where available. Known bindings and unnamed widgets use friendly descriptions such as Calendar or Activity plan. Duplicate names show the stable ID as secondary text; IDs always remain in selection data and tooltips. Unnamed screens or unknown element kinds retain their IDs. The primary action picker displays its authored button name rather than exposing an ID as the normal label.

These changes organize existing authoring functions. They do not introduce a new game-state editor, simulate an ending, change the player planner, or certify human usability. Test the actual exported game and conduct unfamiliar-designer playtests separately.

## Preserve saves for a presentation update

Open **Export → Preserve saves for a presentation update…** after saving the current project. Select `game.json` from the previous published export and, optionally, an original player save from that version. The review uses the platform currently selected in Export. **Check presentation changes and saves** accepts only supported paint/layout changes, unbound interface text and image pixels; changes to rules, story, actions, bindings, fonts or audio require the separate compatibility/migration workflow.

A successful result shows the changed locations and images. Technical details are available under **Check details**. Success does not opt in automatically: select **Use this check to preserve existing saves**, then **Apply**. Export then keeps the baseline's canonical save content and adds a separate presentation overlay. The previous export and sample save remain unchanged. The service rechecks the project, baseline and sample before publishing, so a changed file cannot reuse a stale approval.

Changing the platform or current project clears the selection. Editing paths inside the review requires another check. Cancelling a new review retains the previously accepted selection; **Clear selection** in Export removes it explicitly. A presentation check takes precedence over the separate general compatibility review, and the export request never sends both choices. Failed checks, invalid samples and unsaved drafts cannot enable the opt-in.
