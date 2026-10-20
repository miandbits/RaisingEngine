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

## Runtime compatibility

Card layouts require the Windows runtime capability `narrative-cards-v1`.
Export rejects older runtimes and the current web runtime when that feature is
used. Choose the default list to retain the existing cross-platform choice
presentation. This capability is a source-build feature; it does not change
already published preview binaries.

## Verification

- Qt `calendarWidgetAuthoring`: create a choices widget, select cards, persist
  the project and undo the change.
- `tests/native-player/narrative-cards.test.mjs`: available/locked branches,
  card styling at two resolutions, restore an unchosen question after applying
  a branch effect, choose another branch, and reject unsupported exports.
