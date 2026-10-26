# Activity boards

Select a schedule widget on an ordinary screen, then open **Widget configuration**. Choose **Activity board · cards and period slots** as its display layout. This adds activity cards and period slots while retaining the existing schedule rules and normal phase progression.

Use **Add category** to give a group a unique lowercase ID, a display name, an optional image, and one or more activities. Groups belong to this widget. An activity may appear in several groups; the runtime's **All** filter also includes activities you have not grouped. Even a group named `all` remains a separate authored group.

Use **Choose activity icons…** for each activity's image. **Activity / Product card frame** uses the existing image frame, and **Control color style** selects an existing shared style's colors. The widget's appearance can still use the shared Surface settings. Images do not supply padding or change the authored rectangle, and a control color style does not change fonts, images or dimensions. Check image-backed contrast yourself.

Enable **Show a phase forecast** when the player should be able to inspect the existing read-only forecast. The first switch to an activity board enables cost/effect and description details when no preference was saved. Explicitly disabled options stay disabled, and you can change either switch. Browsing a card or cancelling a choice does not change the game. **Apply** changes the current draft slot; the normal plan confirmation is what commits the schedule and advances play.

Saves retain only the assignments you have explicitly applied to draft slots. A card you are inspecting and the current category filter remain temporary and are not restored. Older saves without a saved draft still load normally, using the existing schedule state. This persistence behavior is specific to activity boards; other schedule layouts keep their prior draft behavior.

The editor's canvas shows a representative arrangement. Use Play and **Layout diagnostics** in the renderer inspector to inspect the actual selected screen at desktop and phone sizes, including enlarged text. Confirm that all cards, filters and actions can be reached. Rendered diagnostics do not certify every game state, contrast over images, or human accessibility acceptance.

Widget settings scroll independently of their fixed OK/Cancel footer. Cancel discards the whole settings draft. Apply, undo/redo, save and reopen preserve group images, activity icons, frame, control colors and forecast choices together.

The layout supports a single-character schedule with 1–12 periods. Roster projects must choose another layout. Schedule widgets are currently authored directly on screens; putting this board inside a reusable component is not supported. Reuse its shared image assets and styles in other screen widgets or other projects instead. No specific game's data or artwork is required.

Exports require the independent `activity-board-v1` runtime capability. The older `board` layout keeps its existing compatibility requirements. Adding or changing the activity board, groups or activity connections is a normal game update; it is not classified as a paint-only update compatible with an old gameplay snapshot.
