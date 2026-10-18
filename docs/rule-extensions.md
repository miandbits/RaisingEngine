# Portable rule extensions — API 1

Status: API 1 implemented and verified on Windows/Web. This is a separate contract from presentation
plugins (`.raiseplugin`). A `.raiserules` file is a UTF-8 JSON rule package, not a DLL
or a script. It defines reusable, parameterized conditions, effects and finite-state
progressions. Package authors work against this contract; installing a new package
does not require modifying/rebuilding the engine or either player.

## Execution boundary

The shared project validation service compiles rules before structural and semantic
validation. Windows C++, Web Wasm, analysis workers and replay consume the **same
compiled content snapshot**. Expansion invokes no JavaScript, network, filesystem,
wall clock or extra random generator. Costs, transactions, random outcomes, actor
scope, event interruptions and saves remain owned by the deterministic core.

API 1 composes the existing numeric expressions, conditions, effects, actions and
state groups. It does not promise arbitrary new CPU instructions, physics engines,
custom C++ gameplay modules or an unconstrained scripting VM. New primitives would
require a future version of the host contract. Progressions may have cycles because
each transition is an explicit player action; recursive rule definitions are errors.

## Package and project storage

Package fields: `apiVersion: 1`, `id`, exact `version` (`x.y.z`), `name`, and optional
`conditions`, `effects`, `progressions` arrays. Definitions have `id`, `name` and
`parameters`. Conditions have one `body` object; effects have a `body` array.

Parameter entries are keyed by ID and contain `name`, `type`, optional `default`,
and integer `min`/`max`. Types: integer, boolean, text, attribute, character, item,
event, flag, stateGroup, stateValue. A `{ "$param": "parameter-id" }` value is
replaced structurally, never interpolated into source code. Unknown/missing arguments
and wrong types/ranges are errors. Body references are checked after substitution.

Projects store a pinned copy under `project.json.ruleExtensions`:
`{ apiVersion: 1, packages: [...], progressions: [...] }`. There is no dependency on
an installed global store. Importing into one project cannot change another project.

At any condition/effect slot use:
`{ op: "ruleCondition" | "ruleEffect", packageId, version, ruleId, args: {...} }`.
An effect call expands in place to its ordered effects. Rules may call other rules;
package versions must match exactly. Expansion is bounded to prevent recursive or
explosive content from hanging validation. Missing definitions fail closed.

## Progression definitions

A progression adds `initial`, `states: [{id,name}]` and `transitions` to its definition.
Each transition has `id`, `name`, nonempty `from` state IDs, `to`, optional requirements,
costs, effects and eventId. An instance has `id`, `name`, packageId, version, ruleId,
args and scope (`shared` or `character`). The compiler creates state group `rx-ID`
and actions `rx-ID-TRANSITION`. ID conflicts are errors, never silent overwrites.

Transitions require their source state and any author condition, apply costs/effects
and update the state within the existing action transaction. They can start a normal
event. Existing time-session configuration can assign time/AP to the generated action
IDs; existing screens can bind buttons/action lists to them. State is ordinary saved
shared/character state, not an external mutable plugin variable.

## Compatibility, saves and diagnostics

Projects without rule extensions retain the old snapshot path. Authored documents
are preserved: compiled actions and expanded rules are not written over source.
Snapshots include `ruleExtensionLock` (compiler identity and pinned source library)
in the content fingerprint. Changing package code/version invalidates old content
identity and uses the existing explicit save-migration path, never silently resuming
with different rules. Exported games contain compiled rules and need no rule-pack
interpreter. Disabling/removing a required pack blocks validation/export.

Errors have source document/pointer/line/column. Generated progression diagnostics
map back to the instance and retain the generated location as related evidence.
Compiler-specific errors use `RULE_*`; compiled runtime vocabulary still receives
normal schema/reference/range checks.

## Editor workflow

1. Open **디자인 → 규칙 확장 패키지 · 진행 방식…**. Select a `.raiserules`
   file and press **패키지 가져오기**. The project pins a copy; importing the same
   ID cannot silently replace an existing package.
2. In the ordinary condition/effect tree editor, choose **확장 조건** or **확장 효과**.
   Choose the named rule and fill the typed parameter controls. Attribute/item/event
   references are selected from project data. Save and reopen preserves the binding.
3. In the extension workroom, choose a progression definition, add an instance, set
   its name, shared/character scope and parameters. The transition preview uses state
   names. **현재 화면에 배치** creates a standard action-list element in the open screen;
   subsequent position/size/context edits use the normal canvas.
4. Generated progression states appear by name in the standard condition/effect
   state selectors. They can drive existing conditional presentation and events.
5. Save and validate through the ordinary project service. Errors block running and
   export; undo/redo and revision/disk conflict detection still apply. The source
   calls remain in the project; only the runtime snapshot expands them.

Package authors define templates as JSON using the [package schema](../packages/rule-extensions/package.schema.json)
and the [complete example package](../examples/rule-extension-lab/dojo.raiserules).
Consumers use the forms above. This first release does not include a visual package
definition authoring tool, a marketplace or automatic package/save upgrades. Calls
are an **authored** AST extension, compiled before the existing runtime JSON Schemas;
those runtime schemas do not themselves describe `ruleCondition`/`ruleEffect` nodes.

## Verification (2026-10-17)

- TypeScript compilation and the separate Qt editor build succeeded.
- 95 tests passed, none skipped: rule contracts/integration plus existing project
  validation, saves, custom states, component context, conditional routing, editor
  transactions, time progression and rule-flow regressions.
- Real Qt widgets under `QT_QPA_PLATFORM=offscreen`: 20 checks. Package import,
  duplicate import, typed arguments, progression creation/list placement, condition
  and effect forms, generated state references, reopening and explicit version
  reselection were exercised.
- Shared StudioEditor validation/save/reopen preserved authored calls. A second
  writer was rejected rather than overwriting a changed project.
- An unchanged Windows player binary ran with `--hidden`; the exported Web player
  ran in headless Chrome with the C++ Wasm backend. Prepare → practice → breakthrough,
  cancel, mid-run save/restore, template story choices, ending and fresh-process/page
  ending restore matched the reference session. 10 Windows state checkpoints matched;
  the browser reported no page errors.
- The recorded route passed 12 save/restore checkpoints. Lowering the package's
  configured growth caused the old route to fail at the breakthrough action, as expected.
- GitHub Actions now runs the portable rule contract tests and the committed sample
  route after the headless build. Local results are recorded here; configuration is
  not itself a claim that the remote CI run has completed.
- Character-scoped progressions were separately tested in native sessions: completing
  one character's progression did not advance the other, including across save/load.
- A pre-existing debugger test still expected all events to need period-end. Its
  expectation was corrected to the existing interaction-event contract (requirements
  inspection during planning/ready); production debugger behavior was not changed.

The base scene/action scaffolding is a test fixture. Qt forms authored the extension
bindings; the harness connected those returned values to the scaffold and saved them
through StudioEditor. This is an automated widget/integration test, not a new human
usability study or a claim that every sample object was created by mouse.

No shared desktop window, global keyboard/mouse or visible browser was used. Builds
used two workers at BelowNormal priority and a separate output directory. Mobile and
console execution are not part of this verification.

Commands:

```powershell
$env:QT_STUDIO_OUT='dist/qt-studio-rule-extensions'
$env:BUILD_JOBS='2'
npm run build:qt-studio
npx tsc -p tsconfig.json
npm run test:rule-contracts
# Requires existing Windows player + Web build; always offscreen/hidden/headless.
npm run test:rule-extensions
```

Build: `dist/qt-studio-rule-extensions/RaisingStudioQt.exe`.
Sample: `examples/rule-extension-lab/game`.
Portable result: [verification record](rule-extensions-verification.json).
Local screenshots and full reports: `.cache/rule-extension-verification/latest.json`.
