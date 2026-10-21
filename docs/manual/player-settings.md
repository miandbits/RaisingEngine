# Player settings

The Windows player supports an authored `playerSettings` widget. In the Qt editor, add **Player settings · sound and reading** to a screen or reusable component. Open that screen with a normal `openModal` button from a title screen or game menu. Use `closeModal` to return to play.

The widget follows the element's text color, border color and accent (`hoverFill`). Its two-column layout is designed for a 1160 × 500 element on a 1280 × 720 canvas. Labels support Korean, English and Japanese through the player locale.

| Setting | Values | Default |
| --- | --- | --- |
| Music volume | 0–100%, 10% steps | 80% |
| Sound effects | 0–100%, 10% steps | 80% |
| Display | Windowed / fullscreen | Windowed |
| Dialogue speed | 20 / 40 / 70 characters per second, instant | 40 |
| Automatic dialogue | On / off | Off |
| Wait after a complete line | 1–6 seconds | 3 seconds |

Changes apply and save immediately. Delay controls are disabled while automatic dialogue is off. The first advance input during a revealing line shows the complete line; a second advances the story. An `advance` button with `autoLabel: true` follows this behavior with **Show full line / Next line** labels. A text element bound to `reading-status` can show manual reading, automatic reading or waiting for a choice.

Automatic reading stops at choices, menus, staged direction and inactive windows. It does not choose answers or advance an ending. Schedule playback and gallery/recollection playback keep their existing, separate controls. Opening a modal also pauses schedule playback.

An authored button can use **Dialogue auto-advance · toggle** (`action: "dialogueAuto"`) directly on a dialogue screen or reusable component. With `autoLabel: true`, its label reports **Auto: On / Auto: Off** in the player language. It toggles the current preference, persists immediately, and does not change game state. It remains usable while a choice waits, but never selects a choice. Use `autoLabel: false` for your own text. The control is disabled during recollection, which has separate playback controls.

## Persistence and compatibility

Preferences are stored beside the active save file in `settings-<gameId>.json`. They are separate from game progress, so loading an earlier save does not undo the player's settings. A custom `--save` directory has its own preferences. Failed writes retain the previous live values and file and show a notice. Invalid settings files open with defaults without overwriting the unreadable file.

The feature is opt-in: projects without a `playerSettings` element or `dialogueAuto` button retain their existing reading and audio behavior. The toggle also works without a settings panel. Editor previews do not change the player's settings or display mode. Settings widgets require `player-settings-v1`; authored dialogue toggles additionally require `dialogue-controls-v1`. A runtime lacking the required capability, including the current web runtime, rejects the export explicitly rather than silently dropping the widget.

## Verification

The native player keeps authored coordinates when a window is resized or switched to fullscreen. Font atlases use the output display density (up to 4×), while line wrapping, button bounds and pointer coordinates remain in the authored canvas. This also applies to projects without a settings widget. Font effects retain the original RmlUi rendering path. Raster backgrounds and character images still require source assets large enough for their displayed size; enlarging an image cannot recover missing detail.

`tests/native-player/display-density.test.mjs` checks fractional scaling, 2×/3× output, letterboxing/pillarboxing, return to 1×, mouse/touch hit testing and save restoration without changing the authored layout.

For an existing release, the export service supports [image-only updates](image-only-updates.md) that preserve its save baseline while explicitly recording replacement image checksums.

`tests/native-player/settings.test.mjs` covers persistence across launches and save restoration, independent audio gains, UTF-8 reveal, first-click reveal, automatic choice/modal stops, malformed preferences, write rollback and unsupported exports. The Qt `saveSlotsAuthoring` test includes creating and retaining the widget. Fullscreen toggling and relaunch persistence additionally require a real display test.
