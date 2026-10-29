# Editor acceptance priorities — 2026-10-29

This is an implementation and acceptance map, not a new public release claim. The repository remains an authoring alpha. A green build is not proof that unfamiliar designers can finish commercial production without help.

| Priority | Finding | Current change or acceptance boundary |
| --- | --- | --- |
| P1 | Actual-state selection boxes were computed in raster coordinates while runtime nodes use logical coordinates | Share the raster transform between renderer inspection and the editable canvas; convert drag, resize and group movement back to authored coordinates. Regression cases cover Retina, downsampled and letterboxed images, picking and authored moves. |
| P1 | External version-control configuration required `.exe` even on Mac | Accept macOS app bundles and Unix executables, keep Windows executable validation, and pass the project as a literal argument without shell evaluation. |
| P1 | Task navigation vanished when opening a tool; unrelated inspector fields obscured the selected element | Keep a dockable stage panel and current selection context; hide inapplicable binding/action rows without editing stored values. This does not yet unify every activity, story and timeline editor into one form. |
| P1 | Historical Electron instructions looked current | Mark historical documentation and route Qt users to current workflow guides. Public preview downloads remain separate from the current PR. |
| P1 | A contaminated runtime directory could carry private metadata or signing material into an export | Fail before publication for known authoring directories, editable project-backup ZIPs, environment files and key containers. The filter is not a general secret scanner or DRM. |
| P2 | Schedule, stats and growth widgets cannot be linked as shared-component children | Implemented across schema, component authoring, readable-layout and planner references, runtime input ownership and export capability gates. Shared source uses the ordinary widget settings; nested components have a distinct capability. Regression fixtures cover conversion, propagation, old-runtime rejection and actual player interaction. See [shared sections](../manual/shared-screen-sections.md). |
| P2 | Activity → conditions/effects → presentation → growth still crosses tool boundaries | Activity rules and linked presentation now share a local draft and a single final transaction. The full timeline editor returns to the selected activity; shared-use warnings and per-activity duplication preserve intent. Cancel, stale drafts, Undo/Redo and reopen have Qt coverage. See [activity workflow](../manual/activity-workflow.md). |
| P2 | Screen-flow generator supports a narrower subset than the engine | Keep its explicit rejection of unsupported roster, progression and routing combinations until each is covered by an authoring/export journey. The engine's separate support is not a wizard guarantee. |
| Human acceptance | Unfamiliar designers, physical iOS, VoiceOver, clean-machine release installation | Open. Internal automated and simulator tests do not replace these checks. |

For the next human study, give five unfamiliar designers the same installed editor and public starter. Ask each to change one activity and its result, build a home/planning/result flow, import an image, recover a backup, compare a changed route and export a playable game. Record completion, time, wrong turns, requested help, recovery and export playback. Acceptance: at least four finish without intervention and another person plays each export. Do not substitute agents for human participants.

Distribution protection has a separate [proposal](distribution-protection.md). No encryption, store ownership adapter or signed public release is claimed by this patch.
