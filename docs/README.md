# Documentation map

Start with the [root README](../README.md). Older documents retain the implementation and test history; a historical “not implemented” list may have been superseded by a later document.

## Current product and authoring

| Topic | Document |
|---|---|
| Seolmae six-month demo design | [Concept and first-month production specification](game-design/seolmae/README.md) |
| First game without JSON | [Forms, stories, relationships and reference repair](first-game-without-json.md) |
| First-time authoring improvements | [Changes and verification](first-time-authoring-forms.md) |
| Current scope and phase history | [Universal raising engine plan](universal-raising-engine-plan.md) |
| Project manager and public release plan | [Startup workflow and history boundary](project-manager-and-public-release.md) |
| First-time designer usability pilot | [Last Grandson authoring trial](designer-usability-pilot.md) |
| Qt editor build and architecture | [Qt editor](../native/studio/README.md) |
| Shared data contracts | [Data format](data-format/contract.md), [interfaces](interfaces.md) |
| General rules and visual logic | [Rules](generic-rules.md), [logic/debugging](visual-logic-and-debugging.md) |
| Portable condition/effect/progression extensions | [Rule API 1, editor workflow and verification](rule-extensions.md) |
| Visual rule-package authoring | [Package workshop](rule-package-workshop.md), [verification](rule-package-workshop-verification.json) |
| Engine completion sequence | [Ordered implementation and evidence gates](engine-completion-sequence.md) |
| Integrated authoring and references | [Authoring workspace](authoring-workspace.md) |
| Characters and time | [Character-scoped rules](character-scoped-rules.md), [time sessions](time-progression-sessions.md), [unified calendar and parallel sessions](unified-time.md) |
| Reusable UI and data binding | [UI composition](ui-composition.md), [data binding](data-ui-binding.md) |
| Adventure, battle and direction | [Party tactics](party-tactics.md), [direction sequences](direction-sequences.md) |
| Shared human/LLM editing | [MCP](mcp.md) |
| Genre samples and production pilot | [Genre assurance](genre-authoring-assurance.md), [three-month pilot](commercial-pilot.md) |

## Regression workflow

1. [Record and replay routes](goal-route-regression.md).
2. [Find target or replacement routes](goal-route-search.md).
3. [Compare graphs and recorded execution](goal-route-comparison.md), [historical results](goal-route-history.md).
4. [Compare when content is saved](goal-save-impact.md).
5. [Approve expected changes and restore baselines](goal-baseline-approvals.md).
6. [CLI and GitHub Actions](goal-regression-ci.md).
7. [Import a CI report](goal-ci-editor.md) or [browse GitHub runs directly in the editor](github-actions-editor-review.md).

## Development and release

- [Contributing](../CONTRIBUTING.md).
- [Open-source preparation and cleanup order](open-source-readiness.md).
- [Public samples and local experiment separation](public-sample-separation.md).
- [Native runtime](../native/README.md), [Windows player](../native/player/README.md).
- [Qt export](qt-game-export.md), [mobile deployment](mobile-deployment.md).

## Historical/reference material

`implementation-plan.md`, `architecture.md`, `studio.md`, `player.md`, `distribution.md` and the early handoff documents describe earlier milestones, including Electron workflows. Use the current topic documents above to determine later additions and limits. The reference-game audits describe experiments; they are not a promise of complete game parity or permission to redistribute the referenced assets.

Platform support is feature-specific. Early web/Android verification must not be applied automatically to later UI, party combat or direction features.
