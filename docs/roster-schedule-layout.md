# Multi-character schedule layout

The schedule element adapts to the number of characters in a project's roster. The character list scrolls inside the authored rectangle; the apply and execute buttons remain below the list. At widths below 600 design pixels, the native player uses one column and stacked buttons. The web player applies the same layout to its available container width.

This fixes overlapping descriptions and controls when a designer adds more trainees. Changing an activity or applying assignments preserves the native list's scroll position. This is transient UI state, not part of the saved simulation. Activity descriptions remain opt-in through `showActivityDescription`.

Give the element enough height for its heading, controls and at least one visible character. A height of 270 design pixels is tested at widths of 360 and 1100 pixels; larger areas are more comfortable for long descriptions. Manually authored character cards elsewhere in a screen do not automatically duplicate when a roster grows. Use a bound collection for dynamic cards.

Verification:

- Six-character native layouts: wheel scrolling, a later character's activity change, reachable fixed controls, shared execution, exact save/load state and navigation outside the element.
- Actual browser layouts at narrow and wide widths, including activity descriptions, draft changes and apply controls.
- Existing description opt-out behavior remains covered.

Tests: `tests/native-player/roster-scroll.test.mjs`, `tests/visual/roster-scroll.test.mjs`, and the adjacent `roster-description.test.mjs` files.
