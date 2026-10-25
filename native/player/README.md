# Native graphical player

`node scripts/build-native-player.mjs` builds `dist/native-player-app/RaisingPlayer.exe` with static SDL3, RmlUi, FreeType, SDL_image and miniaudio. The executable links the same `raising_core` C++ library used by the native protocol service and WebAssembly build. It does not launch Node, Electron, a browser, or a TypeScript rules interpreter.

The export service copies that **artifact directory**, `game.json` and validated asset bytes into a new game directory. Keep the bundled font and license files. Run `RaisingPlayer.exe` from anywhere; it reads `game.json` beside the executable. `--bundle DIRECTORY` selects another bundle.

Damaged bundles fail with an explicit error before rendering: the native boundary checks canvas and element geometry, required screens, duplicate IDs, asset paths and missing files. This supplements the shared export-time JSON Schema validation. It does not replace the editor's semantic validator.

The renderer reads the authored visual screens and elements, including custom screen navigation, locations, shops, inventory, interactions, relationships, scheduling, dialogue choices and endings. Individual text elements can bind a resource/stat, character relationship or item count with `{label}` / `{value}` formatting. Commands and availability checks use `raising::process`; the renderer never changes attributes or inventory directly. Korean text uses bundled Noto Sans KR. PNG/JPEG/WebP images support contain/cover fitting; element motion supports opacity, horizontal slide and size zoom. WAV, MP3, FLAC and Vorbis playback use miniaudio. Audio initialization is deferred for silent games. The generated `capabilities.json` is the export format contract; unsupported formats must be rejected before export. Animated GIF/WebP playback is not implemented.

Mouse and keyboard are supported. Tab changes focus, Enter activates controls, F5 saves and F9 loads. Gamepad directional buttons change focus and the south button activates the focused control; physical controller coverage is still required. The window resizes with a letterboxed logical canvas. Saving uses the existing versioned command journal; loading replays it through C++ and rejects fingerprint, version and state mismatches.

On iOS, focused authored text fields use UIKit for selection, composing keyboards, clipboard editing and keyboard dismissal/refocus. They use authored fonts, labels and styles; a field covered by its own keyboard is temporarily lifted above it. Other platforms retain SDL/RmlUi input. See the [system UI regressions](../../docs/development/ios-system-ui-regressions.md), including the separately recorded Korean mid-syllable simulator limitation.

On macOS, the player registers SDL's existing `ApplePersistenceIgnoreState` process default before SDL activation, so Cocoa's crash-restoration prompt cannot block startup. This does not write a persistent system preference; game recovery continues through the validated save system.

Default saves are under SDL's per-user `RaisingEngine/<gameId>` preferences directory, in `slot-1.json`. `--save FILE` overrides this for portable playtests. A previous slot is kept as `.bak`. No network service or account is required.

## Reproducible dependencies

Source checkouts are in `native/player/vendor`, using these upstream versions:

`npm run prepare:native` downloads and verifies the exact revisions from `config/native-dependencies.json`; `build:native-player` runs this automatically. Existing modified or mismatched directories are preserved and rejected, never reset. These ignored dependency directories are not part of a clean Git checkout.

- [SDL](https://github.com/libsdl-org/SDL): release-3.2.24, `a8589a84226a6202831a3d49ff4edda4acab9acd`
- [SDL_image](https://github.com/libsdl-org/SDL_image): release-3.2.4, `11154afb7855293159588b245b446a4ef09e574f`
- [RmlUi](https://github.com/mikke89/RmlUi): 6.1, `58c751531f4c1a1eb2dc56f409b8e8a323a0c9ad`
- [FreeType](https://gitlab.freedesktop.org/freetype/freetype): VER-2-13-3, `42608f77f20749dd6ddc9e0536788eaad70ea4b5`
- [miniaudio](https://github.com/mackron/miniaudio): 0.11.23 (`miniaudio.h`, `extras/stb_vorbis.c`, `LICENSE`)
- [libwebp](https://chromium.googlesource.com/webm/libwebp/+/refs/tags/v1.6.0): 1.6.0, `4fa21912338357f89e4fd51cf2368325b59e9bd9`, statically linked with its license and patent grant.
- [Noto Sans KR](https://github.com/google/fonts/tree/main/ofl/notosanskr): bundled variable font with SIL Open Font License.

Requires CMake 3.20+ and a C++17 compiler. The build script discovers the installed Windows MinGW compiler, or uses CMake's default compiler on other hosts. Windows and macOS have dedicated creator and package CI. iOS has source exports, simulator checks and unsigned device compilation; physical-device acceptance and signed distribution remain separate. Android, Linux and console certification require their own platform checks.

## Renderer test hooks

`--hidden --automation steps.json --report report.json --save test-slot.json` runs a bounded pointer-input test. The steps file is an array of objects:

```json
[
  {"click":"start"},
  {"expectScreen":"home"},
  {"click":"visit-market"},
  {"click":"buy-florist-flower"},
  {"screenshot":"D:/test/market.png"}
]
```

`click` targets the real RmlUi element and feeds mouse down/up at its laid-out center. `clickWindow` exercises SDL window-to-logical coordinate conversion. `clickPoint` `{id?, x, y}` clicks a logical point (element-local content px with `id`) through the same SDL mouse path, e.g. on a plugin element's hit area. `select` plus `value` changes a real select control and dispatches its change event. `key` supports Tab, ShiftTab, Enter, Escape, F5 and F9; `waitFrames` waits for motion and `resize` changes the actual window dimensions. `waitMs` holds for 0–3,600,000 milliseconds of monotonic wall time while the event/render loop continues; use it for endurance holds, including a final hold before exit. Each recorded step and the final report include `elapsedMs` from automation startup. Frame counts are not a wall-time estimate. The report records visible control geometry, rendered bound text, audio status and the actual C++ state; it does not inject gameplay state. `--frames N` bounds a run without automation. Native playtest artifacts live under `.cache`, outside authored source projects.

After building, run `node --test tests/native-player/*.test.mjs tests/journey/native-ui.test.mjs tests/journey/native-dropdown.test.mjs`. Coverage includes the authored journey, save interchange and tamper rejection, keyboard navigation, visible dropdown selection, letterboxed pointer input, WebP pixel/fit assertions, corrupt bundles, and actual initialization/decoding/mute for all four advertised audio formats. The small tone fixtures are synthesized test signals, encoded with FFmpeg; they contain no third-party media. Physical gamepad and touch hardware still need platform QA.
