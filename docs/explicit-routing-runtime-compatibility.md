# Explicit routing and native bundle validation

The editor schema and semantic validator allow a project with explicit screen routing to use one freely named screen. The Windows bundle validator still required the four legacy screen IDs, so a valid export could fail before opening. Native validation now follows the same distinction:

- With `visual.routing`: one or more screens, a valid default screen, and existing targets for phase and conditional routes.
- Without explicit routing: retain the legacy `title`, `schedule`, `story`, and `ending` requirements.
- Empty/duplicate screens, invalid geometry and malformed routes still fail with a diagnostic.

`tests/native-player/single-screen.test.mjs` validates an authored one-screen project and runs its start, assignment, activity result and save/load through the actual native player. It also checks missing route targets and legacy rejection. `bundle-errors.test.mjs` accepts `RAISING_NATIVE_PLAYER_RUNTIME` to run its existing malformed-bundle cases against a selected review build.

No project migration is required. This corrects validation of the existing explicit-routing format.
