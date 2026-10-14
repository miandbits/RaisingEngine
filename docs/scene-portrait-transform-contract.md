# Scene portrait transform

Each existing `scene.portraits` entry may declare `scale` (0.1 through 4),
`offsetX` and `offsetY` (each -2 through 2). All are finite JSON numbers.
Missing values mean 1, 0 and 0. Missing fields remain absent in stored content
and presentation state; no migration or new state version is introduced.

Transforms apply to the image rectangle of a matching `portrait-left`,
`portrait-center` or `portrait-right` UI slot. For slot `(x,y,w,h)`, use
`(x+(w-w*scale)/2+w*offsetX, y+h-h*scale+h*offsetY, w*scale, h*scale)`.
The fixed pivot is bottom center. Offsets are fractions of the untransformed
slot dimensions. The slot's existing image fit and screen layout remain in use.
The operation does not transform a speaker portrait or fixed asset image.

Qt's scene portrait table edits scale and offsets as percentages. Unchanged
controls preserve exact original numbers, including greater precision than the
control displays. New default controls do not add optional properties. Cancel
leaves the node unchanged. A later scene without transform properties resets
its portrait to the defaults, because a portraits array replaces the previous
array as before.

The existing TS spread-copy and native JSON-copy scene paths preserve optional
fields. Save replay must retain transformed presentations exactly; legacy saves
and scene data must not gain fields. New content changes its fingerprint as
usual. Export requires `scene-portrait-transform-v1` to reject older renderers
which would silently ignore transforms.

Qt verification on 2026-10-13: `scriptPortraitTransformRoundtrip` and
`scriptPortraitTransformPreservesPrecision` passed. The tests cover 20% scale,
positive/negative offsets, cancel, reopen, absence-preserving defaults and exact
retention of existing high-precision numbers even when adding/removing rows.
Build artifact: `dist/qt-studio-portrait-transform/RaisingStudioQt.exe`.
`tests/runtime/scene-portrait-transform.test.mjs` passed 3 focused tests after
updating both save-schema presentation locations. Coverage includes content and
save bounds/type rejection, legacy field absence, high-precision TS/native
state and transition equality, exact save/replay across nine commands, reset by
a later legacy portrait array, and web/Windows capability enforcement. Renderer
geometry checks are recorded separately.

Renderer verification: `tests/native-player/portrait-transform.test.mjs` and
`tests/visual/schedule-layout.test.mjs` verify the rendered rectangle
`(300,500,80,120)` for a `(100,80,400,600)` slot with scale `.2`, x offset `.1`
and y offset `-.1`. Fixed-asset images remain unchanged; the web test checks
that a later scene without transforms restores the full slot. Native evidence:
`.cache/portrait-transform/run-ZAWnA2`.

Display-only scene preview uses `--preview-screen ID --preview-scene FILE`
and requires `scene-preview-v1`. The JSON is a scene patch, limited to 64 KiB.
The renderer overlays it on initial presentation without changing session
state, executing scripts, advancing time, or saving. `report.preview` includes
`sceneOverride` and `displayScene`. Isolation and input rejection are covered
by `tests/native-player/scene-preview.test.mjs`; actual renderer evidence is
`.cache/scene-preview/run-X8XqLb`.
