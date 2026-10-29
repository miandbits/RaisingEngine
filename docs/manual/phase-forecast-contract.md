# Read-only phase forecast contract

`forecastPhase` is a C++ core operation shared by the native player, native
service and WASM player. The browser-safe `requestPhaseForecast` helper in
`packages/player-service/phase-forecast.ts` supplies the typed, frozen response.
It does not implement a second version of the rules.

```ts
requestPhaseForecast(content, state, activityIds, runtimeRequest)
// Sends { operation: 'forecastPhase', content, state, activityIds }.
// Returns { ok: true, forecast, baseline } or { ok: false, error }.
```

The draft contains every slot in the current phase, including already completed
slots. Existing command validation rejects changes to completed slots. The
horizon starts at the current period and includes the final period boundary of
that phase. Missing selections return `FORECAST_INCOMPLETE` without pretending
that empty slots mean rest. Roster schedules, active intermissions, timed sessions
and expeditions have explicit unsupported/blocking results.

## Two different projections

`forecast` uses the current run mode and canonical commands. Its status is
`exact` only when all remaining activities and their boundaries can be resolved
without additional player input or randomness. A `partial` result reports only
the deterministic prefix; `lastProjectedPeriodIndex`, row `activityComplete`
and row `boundaryComplete` describe its reach. A `blocked` result cannot start.

Automatic scene, effect, deterministic branch and end nodes before the first
story dialogue or choice may execute in the copied run. This includes upkeep
authored as a story effect before dialogue. Narrative text, event identifiers,
choices and speculative save states are not returned. Effects belonging to a
new phase are not included in the previous phase's projection. An automatic
story prefix is bounded to 256 nodes per boundary.

`baseline` uses the existing `activities-only` command contract. A complete
baseline has status `projected`, **never `exact`**. It assumes current selected
policies and diet, no optional actions, and excludes all narrative and
intermission effects. Consequently it excludes upkeep authored inside stories,
while retaining deterministic upkeep authored as automatic rules. Its values
must be labelled as an activities-only estimate with these assumptions. A partial
baseline must not be displayed as a phase-end total.

Both projections stop before a random draw, including probabilistic actions
called by generic rules. They do not use the hidden seed to reveal future
outcomes. Even 0% or 100% success checks retain this conservative behavior.
A failure inside a command rolls back that copied transaction, just as it does
in a real run.

## Numeric breakdown

Each attribute row contains `before`, `after`, `delta`, `costs`, `effects` and
`automatic`. These use actual clamped changes, with signed arithmetic:

```text
after - before = delta = costs + effects + automatic
```

`costs` contains the selected activities' authored costs, normally negative.
`effects` contains their base effects. `automatic` is the remaining net change
from selected policies, diet, preparation, automatic rules and resolved boundary
or deterministic story-prefix effects. Paid preparation is included here,
including when a later activity cannot be afforded. This category does not
infer concepts such as recovery or upkeep from attribute names.

Each period retains canonical `activityChanges` and separately measured
`boundaryChanges`. Existing paid preparation is not charged again. The last
period's automatic rules execute, including any phase-end rules.

## Isolation and validation

The operation runs commands against private JSON copies and exposes no copied
run state. It does not write live state, command history, RNG, saves or
collections. Its scoped randomness guard resets even on failure; subsequent
real commands continue normally. The transport may cache immutable content, but
never forecasts or mutable run state implicitly.

`tests/native/phase-forecast.test.mjs` checks canonical execution equality,
conservation, clamping, paid preparation, policies/diet, final upkeep, completed
slots, story/intermission exclusions, random-rule rollback, phase bounds and
native/WASM parity. Headless environments without a WASM artifact explicitly
skip the parity case; desktop/web validation sets
`RAISING_REQUIRE_FORECAST_WASM=1` to require it.
