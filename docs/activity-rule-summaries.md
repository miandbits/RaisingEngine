# Activity costs and base effects in schedules

Select a schedule element, open **Widget configuration**, and enable **Show costs and base effects in schedule columns / roster**. This optional setting defaults to off, preserving existing layouts. It works in the column layout and in multi-character assignments; calendar/menu/board layouts retain their existing presentation.

The display reads activity data, so changing a cost or base attribute effect updates the text without maintaining a second numeric description. Multi-character schedules name the acting character for personal attributes and mark shared attributes separately. Explicitly targeted characters keep their own names.

This is a rule summary, not an outcome forecast. It does not execute expressions, consume random numbers, test future events, or mutate the run. Calculated effects are marked as calculated at execution. Non-attribute effects and success/failure effects are indicated without exposing internal flag identifiers. Attribute limits, execution conditions, events and other rules can change the final result. No aggregate budget or success probability is promised by this display.

Windows and web exports require `activity-rules-summary-v1`; older runtimes are rejected with the missing capability rather than silently dropping the display. Save serialization is unchanged. As with other content edits, the content fingerprint changes and existing saves may require an explicit reviewed migration.

Verification covers personal/shared and explicit owners, draft selection, updated data, opt-out, no state mutation, exact Windows save restoration, old-runtime rejection and actual browser rendering. The editor option was also applied and saved in a separate private authoring trial through the Windows UI.
