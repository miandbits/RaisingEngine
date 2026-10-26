# Phone reading layouts

Use **Screens → Readable layout…** to give an existing game a scrolling phone presentation. You choose existing text, portraits, choices and controls in reading order. Their actions, conditions, bindings and game rules stay the same. No code or duplicate story content is required.

1. Enable readable layouts for the project. Choose a background that works with your authored text colors. Start with 18-point text, 44-point controls, 16-point page padding and 12-point spacing.
2. Select a screen and enable its reading order. Add the essential information first: date or context, portrait, speaker, dialogue and choices. **Add remaining elements** is a starting point; remove decoration and reorder the useful content.
3. Put frequent buttons in **Bottom actions**. They remain outside the scrolling page. Enable **Group bottom actions into primary action and tools** to keep one **Primary action** directly available and move the other buttons into the tools menu. Choose **None** to put all bottom buttons in that menu. Without grouping, the existing wrapping button row remains. Other buttons can remain in the reading order.
4. Set individual heading text sizes or portrait heights if needed. Text can grow naturally; portrait height does not grow with text size. For a schedule item, enable **Use compact planning for this schedule** to show a weekly summary with an activity picker and details. Compact planning currently supports single-character schedules; roster projects retain the standard schedule. Optional **Show planning guidance on first use** explains selecting a period, inspecting an activity and applying a draft, then names the available progression control. Players can skip it or reopen **How planning works**. Changes use normal editor undo and redo.
5. Choose **Save and open preview**. Inspect 320 × 568, 390 × 844 and 844 × 390 at 100%, 125%, 150% and 200% text. Use **Always on** to inspect the same layout on a large screen. Preview does not change game progress or saved player preferences.
6. Repeat for title, planning, story, endings, save/load and every help or records screen. Validation identifies missing references, unsupported elements, duplicate entries and omitted interactive controls.
7. Export with a current runtime. Older runtimes without the required capabilities are rejected so they cannot silently discard the authored layout. Base layouts require `readable-layout-v1`; grouped actions additionally require `readable-actions-v1`, compact schedules require `compact-planning-v1`, and first-use guidance requires `planning-guidance-v1`.

Turning grouping off clears both grouping and the primary-action selection. Removing the chosen primary button from the bottom list clears its selection and displays a notice; it does not delete the screen's original button. If an imported layout references a missing primary button, select a current bottom button or **None** before saving. If compact planning is invalid because the item changed kind or a roster was added, **Restore standard schedule presentation** removes compact planning, guidance and plan reuse without deleting the item. If an imported guidance option no longer has a valid compact schedule, **Remove planning guidance** clears only the guide.

**Cancel** discards dialog changes. Saving applies the dialog as one edit that **Undo** and **Redo** can restore, including removed settings. Disabling readable layouts for the whole project removes every screen's reading order; Undo restores them. Existing layouts without these new options retain their previous presentation.

For an ordinary compact schedule, optional **Offer reuse of last confirmed plan** lets players review their most recent confirmed assignments, including a replan in the current phase. **Apply to draft** copies only remaining editable periods; completed periods stay unchanged. Opening or canceling the review does not change the draft. Applying it does not advance time, spend resources, or guarantee that future activities will be available. Normal confirmation and execution rules still apply. With no confirmed plan, the control explains why it is unavailable. Preview does not invent a plan or gameplay history.

Removing compact presentation also removes plan reuse. If an imported item no longer meets its requirements, **Remove plan reuse** clears only that option; **Restore standard schedule presentation** clears compact planning, guidance and reuse together. Both repairs use normal undo and redo. Projects without this option keep their existing controls.

Planning guidance is remembered separately for each game on that device. It never changes the player’s plan or advances time. Removing compact presentation also removes its guidance option. Older games without the guidance option behave as before.

In reading layouts, saved records show their in-game date, place and phase first. The chosen checkpoint is repeated beside confirmation controls, separately from current progress. Empty slots use less space; existing records and save compatibility rules are unchanged.

Players can change text size from the native **Reading options** button or Web **Settings**. Preferences stay on that device, separately from campaign saves. **Auto** activates reading layouts when the viewport's short edge is 600 points or less by default; **Always on** also enables them on larger screens. Screens without a reading order retain their authored canvas.

The player automatically remembers a reading position for the same unchanged content within the current player session. Returning from menus, settings or reference screens, and changing text size or rotating the viewport, restores a nearby paragraph anchor. New dialogue or phase content starts at the beginning. This presentation state is separate from campaign saves; loading a save or restarting may begin at the top. There is no authoring switch to enable it.

The first version supports text, images, dialogue, choices, buttons, stats, meters, schedules, results, inputs, save/load records and native player settings. Unsupported specialised widgets need their own reading implementation before they can be selected. A Web export still checks the separate capability of each widget.

## Recent reading

In **File → Project settings → Reading**, enable **Recent reading** to let players reread dialogue already displayed and responses they actually selected. Players can open it from the reading menu, or you can select **Open recent reading** in an ordinary button’s action list. This also works with authored canvas screens; a reading layout is not required.

The history exists only in memory during the current player session. It is not a transcript of hidden branches, does not restore earlier text from saves, and clears on a new game or accepted save restoration. Authoring previews do not record it. Reading or closing the history never advances the game.

If validation reports `UI_READING_HISTORY_REQUIRED`, select that problem to open the Reading tab and enable the feature. To disable it later, first change any connected button actions, including those in reusable components. Applying settings is undoable; Cancel leaves the project unchanged. Export requires the `reading-history-v1` player capability.

## Review on the intended device

Read the longest paragraph and choice, scroll every records list, open settings, save and resume at a pending choice, and rotate while playing. Check both hands and the smallest intended device. The engine uses safe areas, natural text height and controls of at least 44 logical points; these mechanisms do not establish VoiceOver support or complete accessibility conformance.

Reading layouts are part of exported content. Changing them changes the release fingerprint. Keep the previous release for players with old saves and use the explicit save migration workflow when publishing an update; do not replace fingerprints by hand.

See [the reusable contract](../development/readable-layout-contract.md) for schema, runtime and automated regression details, and [exporting games](qt-game-export.md) for iOS source projects and signing boundaries.

## Long pages on desktop

In **Screens → Readable layout…**, select a screen, enable its reading order, and set **Reading activation → Always**. Use this for long credits, help, or journal pages that should scroll at their natural text height on desktop as well as phones. Include the full reading order and a bottom button to return to the previous screen. Elements omitted from that order remain omitted in reading mode.

**Automatic** preserves the viewport threshold and the player's existing reading preference. **Always** applies only to the selected screen; it does not change the saved player preference or force other screens into reading mode. Text scaling, conditions, action grouping, and button actions keep their existing behavior. The project must have readable layout enabled. An exported player must support `readable-screen-activation-v1`; older players are rejected until rebuilt.

Inspect the selected page at desktop and phone sizes with 100% and 200% text, scroll to the last line, then use its return action. This setting uses the existing reading layout; it does not promise identical font metrics across renderers or replace human accessibility checks.
