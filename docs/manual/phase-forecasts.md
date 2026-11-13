# Let players review a phase forecast

Select a **Schedule** widget in the screen editor, open its widget settings, and enable **Show a phase forecast**. Save the project. The option is off by default and works with the existing schedule layouts, including a compact phone schedule. It adds a **Phase forecast** button that opens a separate, scrollable review, so a small authored planner does not need to fit the full forecast.

The review uses the current schedule draft. Opening it, scrolling it, or returning to the plan does not confirm the schedule, run an activity, spend resources, advance time, or change a save. Edit the draft and reopen the review to recalculate. A calendar or menu draft must have a chosen activity in every remaining period. Played periods remain fixed.

The review distinguishes determined results from estimates:

- **End of this phase** is shown only when the engine can determine the complete remaining phase. Values include the deterministic activity and automatic changes that the engine actually reaches.
- **Known changes** show a determined prefix when a random outcome, story, between-activity choice, or another unresolved step prevents a complete result. The review identifies how far it reached and any blocking period/activity. These values are not phase-end values.
- **Activity-only estimate**, when available, is a separate estimate. It assumes no story effects, between-activity choices, or optional actions, and keeps current policies unchanged. Its assumptions stay visible. Story-authored upkeep is excluded from this estimate. Actual results can differ.

Each changed attribute shows its starting value, resulting value, and net change. **Costs** are signed changes from activity costs; **Activity effects** are the applied base effects; **Automatic rules** contain the remaining determined changes, such as policy, diet, recovery, and recurring rules. These three categories add to the net change. Attribute limits can reduce an applied gain or loss.

If the plan is incomplete, unaffordable, or fails a requirement, the review explains the affected period. Random events and undisplayed story text are not revealed. Multiple-character roster forecasts are not supported yet and show that limitation explicitly.

The native desktop/iOS player and the Web player use the same forecast service. The review supports English, Korean, and Japanese, phone text reflow, touch scrolling, and keyboard navigation. Tab into the reading pane and use arrows, Page Up/Down, Home, or End to read a long review; Escape or **Back to plan** closes it and returns focus to the launcher.

For engine integration details, see [the forecast contract](phase-forecast-contract.md). Forecasts describe a particular draft and its stated assumptions; use route replay and balance analysis as well when evaluating a whole game's difficulty and endings.
