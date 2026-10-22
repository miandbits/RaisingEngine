# Displaying the participant of an actor-scoped story

One shared event can run for different trainees. The speaking character can be a mentor or narrator, so `speaker` does not identify whose decision is being made.

Select a text element in the Qt editor, open **Find and connect display data**, and choose **Current story participant** (한국어: 현재 사건의 대상 인물). This stores `binding: narrative-actor` and displays the character name identified by the active narrative's `actorId`. It is empty for a global story, an unknown actor, or no active story. It does not change the speaker, selection target, conditions, or game state.

The same binding is available in the text-binding list, batch editing, and component source text elements. The canvas labels it as determined during play. Windows and web exports require the `narrative-actor-binding-v1` runtime capability; older runtimes are rejected instead of silently dropping the label. Existing projects need no migration.

Verification:

- Qt widget test `narrativeActorBinding`: create text, connect through the data picker, save, reopen.
- Native player `tests/native-player/narrative-actor.test.mjs`: two actor-scoped stories, independent speaker, choices, exact save/load, blank outside stories, old runtime rejection.
- Browser renderer `tests/visual/narrative-actor.test.mjs`: changing participant, missing/global stories, literal text escaping, independent speaker.

These automated checks do not establish that a first-time designer can discover the feature without help.
