# Current reminders contract

`visual.resumeContext` selects authored current-state text for a presentation-only utility:

```json
{"screenId":"commitments","elementIds":["identity","promise-pending","promise-kept"],"showAfterLoad":true}
```

The source screen must exist. Select 1–64 distinct resolved text-leaf IDs, in the order they should appear. Component instance IDs use the existing resolved `instance__child` form. Collection-generated rows are not source leaves. `showAfterLoad` is optional and, when present, must be `true`; omit it for manual access only. The authored button action is `resumeContext`. Both configuration and action require `resume-context-v1` from an exported player; the action also requires the project configuration.

The player labels the utility **Current reminders** (`현재 상황`, `現在のメモ`). Its compact player description is “Notes for your current progress.” (`현재 진행에 맞춘 안내입니다.`, `現在の進行に合わせたメモです。`). The notes are authored by the maker for the current state; they are not a reading transcript. Makers remain responsible for the meaning of their authored text. The engine does not infer goals, reconstruct decisions, read hidden story nodes, or claim that a player previously read a reminder.

## Eligible source text

The allowed bindings are absent/`none`, `calendar`, `calendar-year`, `calendar-month`, `calendar-day`, `calendar-weekday`, `location`, `user-field`, `attribute-value`, `relationship-value`, and `item-count`. Any `dataBinding`, nonempty `selectionBindings`, or other binding is rejected. Empty `selectionBindings` is harmless and allowed. A `relationship-value` reminder must not use `{delta}` in `valueFormat`: relationship change is temporary presentation data, not saved current state. Its default format and `{value}` / `{label}` remain supported. A component's explicit inherited context can resolve supported current-state attribute/item/character references using the ordinary resolver; transient selected-row context is not introduced.

Use the localized source design and normal component/group expansion. Preserve inherited hidden flags, opacity, phase visibility and conditions, and the normal presentation-variant rules. The reminder phase is the canonical state phase, with an active narrative normalized to `choice` or `dialogue`. Phase visibility additionally treats `ready` as `planning` and `activity-result`/`period-end` as `result`; transient playback labels such as `auto-ready` are not used. Source canvas clipping is ignored, just as in reading layouts, because these selected leaves are explicitly republished in a different reading surface. Do not reveal a hidden or zero-opacity leaf. Ignore missing, invalid and empty leaves defensively at runtime; semantic validation prevents shipping invalid selected references. Display plain text rather than parsed markup. Mirror the existing current value for each selected leaf. Calendar reminder labels and the separate current-date/phase header use localized calendar labels, including legacy calendars; existing base-canvas calendar rendering remains unchanged. Authored calendar labels take precedence.

`packages/visual/resume-context.ts` defines eligibility, selected ordering and bounded projection. It accepts existing host condition evaluation and text resolution callbacks; it never evaluates scripts or changes the source design. The snapshot is `{entries:[{elementId,text,truncated}],utf8Bytes,omittedEntries,truncated}`. The total UTF-8 budget is 64 KiB, including each retained element ID and text. Preserve complete Unicode code points. A partial entry sets its own and the snapshot's truncation flag. Otherwise visible, nonempty entries that cannot fit increment `omittedEntries`; hidden, unknown, invalid and empty leaves do not. Show an explicit shortening/omission notice when the bound is reached.

## Presentation and restore boundary

Manual access is available from the grouped reading menu, player settings, and an authored action, while a live ordinary game exists. The feature does not depend on Recent reading. No capture or configuration storage is added to the run, save envelope, device preferences or game fingerprint beyond the normal authored visual configuration.

When `showAfterLoad` is enabled, open once after a successfully accepted restored generation, including supported manual/automatic slots and portable import paths. A same-generation repaint, save, locale update, canceled load, rejected load, new game or initial preview does not trigger it. Do not claim a failed restore succeeded merely because a host returned an error view. Previews, gallery and replay surfaces have no automatic opening. Failed or canceled loads that retain the current run also retain its current presentation.

The overlay pauses underlying schedule/timeline clocks and prevents background gameplay or save/load shortcuts. Close and Escape issue no gameplay command and preserve the prior reading anchor and focus. Use the existing safe-area reading typography, 200% text setting, scroll behavior and at least 44-point/CSS-pixel controls. The selected text is never recorded into Recent reading; a successful restore still begins that separate session history empty.

Stable UI IDs are `engine-resume-context`, `engine-resume-context-panel`, `engine-resume-context-close`, `engine-resume-context-current`, and `engine-resume-context-entry-N` with zero-based visible entry indices. The empty/truncated notices use `engine-resume-context-empty` and `engine-resume-context-truncated`.

## Acceptance boundary

Shared tests cover schema limits, safe bindings, repair diagnostics, exact ordering, inherited conditions, components, variants and UTF-8 bounds. Platform tests must verify current-state selection, accepted versus canceled/rejected restore behavior, one-time opening, state/command/save equality, no history capture, overlay pause, keyboard/touch isolation and scroll/focus return at small portrait and landscape sizes. Automated checks do not establish novice comprehension or the return-after-days human usability gate.
