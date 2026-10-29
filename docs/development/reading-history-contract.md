# Recent reading contract

`visual.readingHistory: {}` opts a project into **Recent reading · this session**. An authored button can use `action: "readingHistory"`; validation rejects that action when the project has not opted in. The feature requires `reading-history-v1` from an exported player. Absence retains the existing behavior and performs no capture.

This is a bounded presentation utility. It is not a campaign transcript, saved bookmark, gallery unlock, replay system, or summary of future goals. Closing the utility returns to the same game state and reading position. It must pause automatic progression and preserve focus just like other player overlays.

## What may enter history

A host submits a receipt only after an authored narrative text or prompt has passed visibility conditions and appears in a real foreground rendered box. Hidden, transparent, clipped-away, inactive, preview, gallery, recollection, replay, and background-overlay content is excluded. A receipt does not prove that a human read its words. For a fully revealed long paragraph, the record may include the paragraph when its block appears; this version does not track each glyph's exposure while scrolling.

Native typewriter text contributes only its explicitly revealed UTF-8 prefix. The unrevealed suffix must never enter the tracker, including through a DOM/RML text dump. Repeated frames and growing prefixes update the same entry. A smaller later prefix cannot erase words already presented. Different nodes, actors, periods, locales, or visible speakers remain distinct.

A chosen response is the literal displayed label captured at selection time. Add it only after the corresponding request succeeds for the same presentation epoch. Disabled, rejected, stale, or canceled selections add nothing. Historical responses are plain text and cannot execute an old choice.

## Lifetime and bounds

One tracker belongs to one live host. A successful new run, project replacement, or save restore starts an empty presentation epoch. This includes slot loads, portable imports, and fallback recovery. Failed or canceled loads that retain the current session also retain its history. If a host reports an error after already accepting a new session, history must follow the actual session boundary and must not expose text from the previous run. Late receipts from older epochs are rejected. Saving, opening a menu, resizing, or changing text size does not clear history or modify canonical state and commands.

The tracker writes no local storage, IndexedDB, files, save-envelope fields, or extension state. A new process starts empty. Old entries retain the literal language and speaker/date snapshots shown at the time; changing locale never retranslates them by looking up source nodes.

Fixed engine limits:

- 200 entries and 256 KiB of retained UTF-8 payload in total.
- 16 KiB per entry, including identity, speaker, date, and locale metadata.
- Identity, speaker, and date are limited to 1,024 UTF-8 bytes each; locale is limited to 128 bytes. Overlong identities are rejected rather than truncated into a collision.
- An oversized text retains a valid UTF-8 prefix with a truncation flag. Oldest entries are evicted until both limits fit; the UI identifies omissions.

`packages/visual/reading-history.ts` defines the Web/shared reference implementation. Its `record` method receives `{epoch, key, kind, text, locale?, speaker?, date?}` and reports whether the retained view changed. Hosts own the visibility and successful-action checks. A key identifies the narrative cursor, actor, and period; it must not include a changing typewriter prefix or ordinary repaint revision. `reset` names the new epoch and reason. `snapshot` returns defensive entry copies without source objects or command history.

## Player and editor expectations

The editor exposes a project opt-in and the ordinary button action without requiring code. The utility is also reachable through the grouped reading Menu and player settings. Its title and empty/reset notice state the session boundary. It may show the current formatted date, phase, and pending-choice status already available to the player, but must not derive hidden objectives or ending requirements from source data.

Use the current reading typography, safe areas, text scale, and at least 44-point/CSS-pixel controls. Close/Escape, focus trapping, keyboard isolation, and reading-anchor return follow the existing overlay contracts. Recent reading is not a place for replay, save editing, search/export, or cloud synchronization in this version.

## Acceptance

The shared tests cover coalescing, stale epochs, literal locale snapshots, Unicode truncation, bounded eviction, defensive copies, strict opt-in schema, action dependency, and capability gating. Platform tests additionally exercise real displayed text, hidden and unrevealed exclusions, accepted/rejected choices, successful and failed restore paths, modal isolation, and phone layouts. Passing automated checks does not establish screen-reader compatibility, reading comprehension, or return-after-days usability.

## Native restore acceptance boundary

The native loader strictly replays a save and stages its deterministic automatic continuation before accepting a replacement session. The staged session uses copied presentation and gallery data, invokes no host callbacks, and performs no gallery filesystem operations. Gallery record selection, duplicate handling, capacity rules and pending-queue draining use the same in-memory merge as the live path, so unlock conditions are checked at the same points. A rejected replay, presentation value, effect calculation, automatic loop or gallery condition leaves the existing session and its reading history intact.

After preflight succeeds, the normal live continuation delivers observers and gallery effects once in their original order. Arbitrary host callback exceptions and failures or concurrent changes in external gallery storage are post-acceptance limits; this is not a promise to roll back external effects. `tests/native-player/restore-transaction.test.mjs` covers both rejected restores and successful observer, gallery, pending-choice and paused-timeline behavior.
