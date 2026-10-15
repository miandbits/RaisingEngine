# Test fixtures

`projects/` holds engine-owned regression inputs, not the user's working games. Tests should copy a fixture to a temporary `.cache` directory before changing it. Do not assume a sibling checkout, an installed commercial game, an existing save file or a personal plugin store is available.

The `action-day`, `class-queue` and `free-expedition` fixtures retain engine-authored rule IDs and numeric vectors from earlier integration tests, with neutral labels, reflowed test screens and no media, portraits, rigs or plugin installation. They exercise probabilistic actions, class/day progression, conditional UI and free exploration. They are not commercial-game replicas.

Animation envelope/export checks create an isolated project and plugin store from `tests/plugins/host-helpers.mjs`. They no longer load a local character-reference project or a previously recorded personal save. The real plugin package must be built as required by the existing plugin test suite.

Large visual fixtures are still present to preserve existing coverage. Deduplicating their generated art is a later cleanup; replacing it must not silently weaken image/rendering assertions.
