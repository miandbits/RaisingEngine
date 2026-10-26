# Activity growth and contextual planning

A result widget can compare the numeric values committed by **one activity**. Select the widget, open **Widget configuration**, then choose **This activity's growth · before / after / change**. The editor canvas shows a representative placeholder; Play displays actual committed values.

Each row keeps the full target identity, including character ownership. Repeated changes to that target become one row: the first recorded value, the last recorded value, and their net difference. Zero-net rows are omitted; a result with none says **No net numeric changes**. Costs and effects recorded in the activity transaction are included. Later story choices and period-end processing are not accumulated here. This is not a phase total, prediction, or a new rule execution.

In **Project settings**, enable **Show activity results after presentation and wait for confirmation** to reveal the result screen after an authored timeline or inline result presentation finishes. Skip also reveals the result. Neither operation acknowledges the activity result; the authored Advance action does that. Continuous schedule playback waits for this review. Saved presentation cursors still restore, including a completed cursor. Leave the option unset to retain the existing playback behavior.

For a home or outing button, open **Widget configuration** and enable **Use this button to open the activity planner**. Choose an ordinary screen's activity board, an optional initial category and an optional card to inspect. This requires explicit screen routing. The action performs validated screen navigation and sets temporary inspection context. It keeps the existing draft; only Apply assigns an activity and normal Commit advances the plan. Component-contained boards and modal boards are not target choices.

Activity boards also offer a compact layout, optional card summaries, selection of the next empty period after Apply, an inline phase forecast, and ordered summary priorities. Enable activity rules for summaries and phase forecasts for the inline forecast. All options are optional and preserve the original board behavior when absent. Summaries and forecasts use the existing authored rules and forecast limits; they do not apply effects.

Status widgets honor the selected attribute list and its order in both native and Web players. The compact style changes presentation only. Keep player text scaling, scrolling and actual exported rendering in your review: these features do not certify a particular screen's readability or accessibility.

Export capability checks distinguish activity growth, result review, contextual planner entry and the new board options, so a runtime lacking a requested feature is rejected instead of silently dropping it. No game-specific assets, targets or progression rules are required.
