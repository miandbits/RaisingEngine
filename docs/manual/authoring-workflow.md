# Project diagnostics and continuing an existing story

## Follow connections between events

Open **Story authoring → Find existing events → Event connections**. The map uses
the current authoring draft, including changes you have not saved yet. Search by
event, character, activity or place; inspect one to four upstream/downstream steps,
or switch to the whole project. The overview and Ctrl+wheel help navigate large maps.

Select a card to see its incoming and outgoing connections and the exact condition
group behind each link. Double-click a connection in the side list to follow it.
**Edit conditions** opens the same visual condition editor used elsewhere;
**Edit script** returns directly to the event or choice in the script workspace.
Condition changes stay in the story draft: save the story and project to apply
them, or cancel the story editor to discard them. The normal batch validation,
undo and project revision checks still apply.

The map connects location interactions/actions that start events, completed-event
requirements, and matching flag/state writes and reads. Choice effects, including
effects in later script nodes, retain their choice origin. AND, OR and NOT are
labelled separately. Identical event titles retain character, timing and predecessor
context. Neither display labels nor graph layout change saved content identifiers.

Dashed links are **possible dependencies**, not guaranteed play paths. Another
condition, event priority, period limit, character context, later overwrite or
script branch can prevent an event from occurring. Numeric expressions, extension
rules, scheduled-event dispatch and arbitrary runtime dependencies are not inferred
as edges in this version. Inspect the complete conditions and run route verification
to prove that an event or ending can actually be reached.

The Qt editor can repair a project's local plugin-store connection and extend an
existing event without editing its JSON or writing node identifiers.

## Repair a missing animation or other plugin

1. Open the project. If a required plugin is missing, disabled, incompatible, or
   its native editor fails to load, use the plugin health button in the status bar.
2. In **Plugins → Manage plugins**, inspect the required version and diagnostics.
   Missing packages and native editor loading failures have separate messages.
3. Select an existing plugin store (a directory containing `registry.json`) and
   choose **Connect to this project**. Alternatively install the required package.
4. Use **Check again** after repairing a package or a native dependency.

The store path is a machine-local setting keyed by the project's canonical path.
It is not written into project content, shared lock files, or game saves. Changing
the store preserves drafts, undo history, document versions and pinned versions.
Reopening the project restores its connection. An unavailable remembered store
produces a warning while still opening the content. New projects use the default
store rather than inheriting the previous project's connection.

Packaged Qt editors keep new plugin installations in your user data folder:
`~/Library/Application Support/RaisingEngine/Raising Studio/plugin-store` on macOS,
or `%LOCALAPPDATA%\RaisingEngine\Raising Studio\plugin-store` on Windows
(with `~/AppData/Local` as the Windows fallback). An explicit store setting or
`RAISING_PLUGIN_STORE` takes precedence; repository/CLI defaults are unchanged.
Older stores inside an application bundle are not moved or deleted automatically.
To migrate one, copy its complete folder to a writable location and connect that
copy in **Manage plugins**. Keep the original until the copied packages validate.

## Continue an existing event

1. Open **Story authoring → Find existing events / continue a story**.
2. Search by event name or dialogue and select the event.
3. Choose **Connect an earlier event** and select its name. The summary shows the
   required predecessor and the current event.
4. Select an existing dialogue, scene or effect, then choose **Insert dialogue
   after the selected dialogue/scene**. Write the new line and select its speaker.
5. Review the graph, apply the changes, then save the project. Export or play the
   result using the normal editor commands.

The predecessor adds an `eventCompleted` requirement while preserving existing
activity, date, choice and other conditions. Adding the same predecessor twice is
idempotent. Direct and mandatory transitive completion cycles are rejected; this
is not a proof that every combination of arbitrary conditions is reachable.

Insertion reconnects the selected node to the new dialogue and the new dialogue
to the original next node. Existing rewards, effects, scenes and node identifiers
are preserved. The advanced identifier controls remain available for repairs.

Changing the selected search result does not discard edits. Applying is a single
shared-service edit transaction. Stale revisions and service failures retain the
draft and display an error. Inputs are disabled during the apply request so a
late keystroke cannot be lost after a successful response.

## Verification

- `node --test tests/qt-studio/service.test.mjs tests/qt-studio/plugin-host.test.mjs`
- `qt-story-flow-dialog-test` and `qt-script-continuation-test` from a Qt build
- `node scripts/check-editor-translations.mjs`

On Windows, Qt test executables need the Qt and MinGW `bin` directories on PATH.
For headless tests also set `QT_QPA_PLATFORM=offscreen` and point
`QT_QPA_PLATFORM_PLUGIN_PATH` to that Qt installation's `plugins/platforms`.
Do not treat a startup dialog or an empty log as a passing test.

The continuation tests cover guard and reward preservation, insertion wiring,
cycle checks, search/draft retention, revision conflicts, failed-apply retry and
canonical per-project settings. The service tests cover store changes, unavailable
paths, failed opens, reopening, and new-project isolation.

## Navigate a long script

The graph reads from top to bottom. Choice and condition branches occupy separate
lanes; a shared continuation appears below the longest incoming branch. Invalid
cycles and disconnected nodes remain visible so they can be repaired. Disconnected
nodes have a dashed border and an explanatory tooltip.

- Search within the current script to select and reveal dialogue or a choice.
- Choice captions show the source card number. Converging options share one
  caption above their destination; its tooltip retains the full option text.
- Use **Focus current node · 100%** to return from the overview to readable text.
- Click or drag in the small overview to move through a long script. The outlined
  rectangle shows the visible area. Ctrl+wheel zooms; ordinary scrolling moves.
- **Show entire flow** fits the complete graph. Large scripts are not silently
  truncated at 500 nodes. Full dialogue remains in the inspector and tooltips.
- Selecting a node keeps its context in view. Editing text preserves zoom and
  scroll position. Expand **Event settings** to edit names, timing and conditions.

The event picker and predecessor picker display character names, one-based schedule
periods, and mandatory earlier events beneath each title. Search accepts this
context as well as titles and dialogue. Character names are inferred from explicit
speaker, portrait and character/actor references; they do not prove who will appear
on every conditional branch. Periods are project schedule indices, not months.

Labels and summaries are derived editor information. Selection still uses the
original document and event identifiers. Full context and the source filename are
available in tooltips, including when two events have identical names and context.
Renaming a label or changing timing updates the summaries without rewriting IDs.

`qt-script-navigation-test` covers a 601-node script, uneven branches and merges,
disconnected cycles, same-title selection, metadata refresh and viewport retention.
It also covers option-text search and combined captions for converging choices.
Richer summaries of arbitrary activity/choice conditions remain future work; these
workflows do not introduce new runtime rules.
## Creating follow-ups from the event connection map

In the story workshop, open **Event connections**, select an event or a choice
card, then choose **Create / connect follow-up**. No identifiers or JSON are
required. Enter a name, opening dialogue, speaker, and eligible period range,
or select a compatible existing event. Period numbers in this form start at 1.

The preview lists the conditions that will be added:

- Completion of the source event.
- For a choice card, a persistent record that this exact choice was selected at
  least once. Another choice granting the same promise does not satisfy it.
- Optionally, a promise flag/value authored on that branch. Its current value
  must still match when the follow-up is evaluated.

Existing target conditions, dialogue, rewards, and source choice effects are
preserved. The new event starts with the entered dialogue and, when present,
the source's opening scene. Further presentation and rewards remain editable
in the story workshop. Shared follow-ups run at eligible period-end checks;
a project allowing only one event per period can defer them behind higher-priority events.

The map updates immediately, but all changes remain part of the workshop
draft. Save applies the new event, choice record, and flag definitions in one
batch under the existing revision guard. Cancel discards them; project undo
reverts the batch. Reconnecting the same source choice and target is idempotent.
Mandatory completion cycles and invalid period ranges are rejected.

### Character-specific choices and promises

Enable character-owned state (`character-v1`) and set the source event's scope to
the acting character. In a choice's effect editor, use **Set flag → Owner → Acting
character** to record a personal promise. The same flag definition can be used by
several characters: each has a separate value. Omitting the owner retains the
existing shared-flag behavior; existing projects are not silently converted.

From that event or choice card, **Create / connect follow-up** automatically adds
completion, exact-choice and selected promise conditions owned by the same acting
character. The promise picker offers that character's writes, not shared writes
or another fixed character's records. The preview explains the scope, and the map
labels personal flag links separately from shared ones.

A newly created personal follow-up uses **After event**: it starts after the
current script finishes, with the same actor and return screen/phase. Its default
period range includes the source period. Chains retain that context through saves
and loading; completed follow-ups run once per character. Higher-priority eligible
follow-ups run first. To defer a meeting, configure its period/other conditions and
an appropriate later trigger; this is not an automatic future-date reservation.
Existing personal interaction targets retain their explicit-action trigger. Only
targets with the same shared/personal scope are offered; mixed-scope links are rejected.

Personal flag values are saved separately and fall back to the flag definition's
initial value when absent, never another character's or the shared current value.
Old saves without personal flags remain valid. Choice records are not retroactively
reconstructed from older saves; the source choice must be played with the connection
present. Windows and Web exports require `actor-story-links-v1` for these new
rules. Current C++/Wasm web builds support the same personal records and follow-up
chains. Older runtimes without this capability reject export with a feature
diagnostic instead of silently sharing records. See [Web actor stories](web-actor-stories.md)
for browser play and save/load verification.
