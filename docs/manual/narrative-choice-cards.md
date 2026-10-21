# Narrative choice cards

The Qt editor can display a story's choices as numbered cards. The same story
nodes, availability conditions, effects and save files drive both the original
list and the cards; selecting a display style does not change the rules.

1. Add or select a **Choices** widget on your story screen.
2. Open **Widget settings** and select **Numbered choice cards** under display
   mode. **Default choice list** keeps the existing presentation.
3. Set the widget's row height, gap, text size, text colour, background, border
   and hover styling. In card mode the background and border apply to each
   choice, rather than an extra box around the list.
4. Place a separate **Dialogue** widget above the cards for the question. Allow
   enough room for the longest translated question and all choices.
5. Play both available and unavailable branches. Check keyboard focus, the
   smallest intended resolution, and saving before and after a choice.

In project data, `choiceLayout: "cards"` selects the new layout. Omission or
`"list"` retains the previous layout. The property is available on screen
elements and reusable component children. Card numbers reflect the current
display order; choice IDs and rule execution do not change. Locked choices
remain visible and disabled.

The native renderer uses a minimum row height, so multiline choices can grow.
The widget must have enough vertical space for the resulting rows and gaps.
This is not an automatic guarantee that every layout or translation fits.

## Explain choices and show their results

In **Widget settings**, enable **Show effects and consequences below choices**
(`showChoiceEffects`). Literal numeric effects appear below the choice label.
In the story editor's choice table, **Consequence hint** (`consequenceText`)
provides a short, player-facing explanation of promises or later story changes.
This text describes the authored rules; it does not create an effect. Internal
flag IDs are not exposed to players. Leave the option off to preserve existing UI.

Place a text element with **Choice result · actual changes** (`choice-result`)
on the dialogue screen. After a choice, it shows actual before/after values and
the consequence hint. For example, a proposed +10 at 99/100 produces 99 → 100
(+1). It stays with that narrative until another choice replaces it or the
narrative ends. Loading reconstructs feedback from validated command history;
effects are not applied a second time and the save format is unchanged.

Allow space for the extra text and test the smallest target resolution.
The current preview lists literal numeric effects, not predictions of later
conditional nodes or arbitrary expressions. Describe those using the hint.
Feedback is currently for live narrative choices, not gallery recollection.

The Windows renderer also keeps unchanged image elements visible across
dialogue and speaker changes. Entrance motion runs when that element's rendered
content changes or enters the screen, rather than for every scene-state update.

## Runtime compatibility

Card layouts require the Windows runtime capability `narrative-cards-v1`.
Export rejects older runtimes and the current web runtime when that feature is
used. Choose the default list to retain the existing cross-platform choice
presentation. This capability is a source-build feature; it does not change
already published preview binaries.

Effect previews and result bindings additionally require `choice-feedback-v1`.
This is currently supported in the native Windows runtime; unsupported runtime
exports are rejected rather than silently omitting the feedback.

## Verification

- Qt `calendarWidgetAuthoring`: create a choices widget, select cards, persist
  the project and undo the change.
- `tests/native-player/narrative-cards.test.mjs`: available/locked branches,
  card styling at two resolutions, restore an unchosen question after applying
  a branch effect, choose another branch, and reject unsupported exports.
- `tests/native-player/choice-feedback.test.mjs`: unchanged background opacity,
  real background transitions, clamped results, no-change branches, restoration
  of choices/results, and capability gating.
- Qt `scriptAuthoringCreateConnectRoundTrip`: consequence text persists through
  story editing and duplication.
