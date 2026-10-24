# Phone reading layouts

Use **Screens → Readable layout…** to give an existing game a scrolling phone presentation. You choose existing text, portraits, choices and controls in reading order. Their actions, conditions, bindings and game rules stay the same. No code or duplicate story content is required.

1. Enable readable layouts for the project. Choose a background that works with your authored text colors. Start with 18-point text, 44-point controls, 16-point page padding and 12-point spacing.
2. Select a screen and enable its reading order. Add the essential information first: date or context, portrait, speaker, dialogue and choices. **Add remaining elements** is a starting point; remove decoration and reorder the useful content.
3. Put frequent buttons in **Bottom actions**. They remain outside the scrolling page. Use a small number so that enlarged labels leave room for the story. Other buttons can remain in the reading order.
4. Set individual heading text sizes or portrait heights if needed. Text can grow naturally; portrait height does not grow with text size. Changes use normal editor undo and redo.
5. Choose **Save and open preview**. Inspect 320 × 568, 390 × 844 and 844 × 390 at 100%, 125%, 150% and 200% text. Use **Always on** to inspect the same layout on a large screen. Preview does not change game progress or saved player preferences.
6. Repeat for title, planning, story, endings, save/load and every help or records screen. Validation identifies missing references, unsupported elements, duplicate entries and omitted interactive controls.
7. Export with a current runtime. Older runtimes without `readable-layout-v1` are rejected so they cannot silently discard the authored layout.

Players can change text size from the native **Reading options** button or Web **Settings**. Preferences stay on that device, separately from campaign saves. **Auto** activates reading layouts when the viewport's short edge is 600 points or less by default; **Always on** also enables them on larger screens. Screens without a reading order retain their authored canvas.

The first version supports text, images, dialogue, choices, buttons, stats, meters, schedules, results, inputs, save/load records and native player settings. Unsupported specialised widgets need their own reading implementation before they can be selected. A Web export still checks the separate capability of each widget.

## Review on the intended device

Read the longest paragraph and choice, scroll every records list, open settings, save and resume at a pending choice, and rotate while playing. Check both hands and the smallest intended device. The engine uses safe areas, natural text height and controls of at least 44 logical points; these mechanisms do not establish VoiceOver support or complete accessibility conformance.

Reading layouts are part of exported content. Changing them changes the release fingerprint. Keep the previous release for players with old saves and use the explicit save migration workflow when publishing an update; do not replace fingerprints by hand.

See [the reusable contract](../development/readable-layout-contract.md) for schema, runtime and automated regression details, and [exporting games](qt-game-export.md) for iOS source projects and signing boundaries.
