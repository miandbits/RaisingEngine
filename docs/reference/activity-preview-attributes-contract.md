# Activity selection details

`activity.previewAttributes` is optional presentation metadata: 1–8 entries of
`{attributeId, label?, valueFormat?}`. `attributeId` references an existing numeric
attribute. `label` defaults to its definition label (1–80 Unicode code points when
provided). `valueFormat` defaults to `{value}` and is at most 160 code points;
every literal `{value}` is replaced with the current integer. Other text is
literal, escaped display text, never an expression or HTML.

Display uses the current run's attribute value, or the project initial value
before a run. It is labelled **현재**: it does not predict costs or stages at a
future queued period. There is no RNG draw, command, state mutation or new core
rule. Legacy activities without this property retain their existing display.
The staged activity wizard writes ordinary references to its generated `tier`
and `fee` attributes. Designers can change the same list in the activity form.

Export requires `activity-summary-v1` so older players cannot silently omit the
details. This optional metadata changes the content fingerprint; saves from the
previous content snapshot are not silently migrated. No save-format or core
state-version change is needed.

Result rendering hides numeric changes whose `delta` is exactly zero, while
retaining all changes in state/history/save records. If all changes are hidden,
the player displays a no-changes message.

Native schedule menus show the summary underneath the activity name. Column
selectors show it in option labels and below the selected value; selection updates
only that summary text, preserving the dropdown's document and focus. React
schedule selectors use the same current-value rule. Summary bodies wrap and scroll
after 120 pixels; native menus with summaries scroll inside the authored viewport.
Legacy activities have no summary text. Native result rows aggregate nonzero
changes as before and suppress zero net totals; React omits individual zero rows.

Verification (2026-10-13): `tests/visual/activity-summary.test.mjs` passed in
headless Edge, including initial override/current values, literal format escaping,
eight long entries, and zero-only results. `tests/native-player/activity-summary.test.mjs`
passed against `dist/native-player-activity-summary`: actual selector/menu input,
unchanged saved zero-delta history and restore, plus eleven activities in two
columns with eight long entries and a physical wheel scroll to select the last
activity. Evidence: `.cache/native-activity-summary/run-rcmiBt`. Player TypeScript
checks and production web build passed. These use disposable fixtures; production
project data was not changed by these tests.

## Qt authoring and verification

Open an activity in the content editor, then use **일정 선택 정보 · 현재 값**.
Add an attribute reference, an optional display label and an optional format.
Removing every row removes the optional metadata. Saving an unchanged row does
not manufacture a previously absent format property. The staged activity wizard
adds stage and cost references automatically; existing activities can use this
same form without regeneration.

Focused Qt tests `contentActivityPreviewRoundtrip` and
`progressionAuthoringRoundtrip` passed on 2026-10-13. They cover authoring,
round-trip preservation, removal, and the generated wizard metadata. The shared
contract suite `tests/runtime/activity-preview.test.mjs` passed 4 tests covering
limits, reference diagnostics, unchanged simulation behavior, and export
capability enforcement. Renderer verification is recorded separately; these
checks alone are not evidence of an actual native or browser display.
