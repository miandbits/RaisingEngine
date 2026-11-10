# Identify and launch the editor

The current source checkout, an installed development package and the published Windows preview are separate artifacts. A feature added to source is not automatically present in an installer downloaded earlier. Keep the source revision and package identity with bug reports and game-release records.

## Launch the intended copy

| Copy | Launch |
|---|---|
| Source checkout | Build following the root README, then run `npm run studio` from that checkout. Keep the same `QT_STUDIO_OUT` if you selected a custom output |
| Windows installed preview | Start **Raising Studio** from its installed Start-menu entry |
| Windows portable package | Extract the whole archive, then run `RaisingStudio/RaisingStudioQt.exe` |
| macOS development package | Open the packaged `.app`; it includes its own Node, services and player |

The source command uses `scripts/start-qt-studio.mjs` and the Qt executable in `dist/qt-studio` by default. It never starts the archived Electron editor and does not build first. A missing executable reports the required build command. A running older binary can still coexist with a newer source checkout; source identity alone cannot prove that binary was rebuilt.

## Read the identity

Choose **Help → About Raising Studio…** (**도움말 → Raising Studio 정보…**) or the About button in the project launcher. The current editor's identity is also separate from the selected game's release identity in **File → Release center…**.

Without opening a game, run:

```sh
node scripts/editor-identity.mjs
```

To inspect the immutable metadata embedded in another package, pass its service engine directory:

```sh
node scripts/editor-identity.mjs --root /path/to/package/engine
# macOS package engine: /path/to/RaisingStudio.app/Contents/Resources/engine
```

| Field | Meaning |
|---|---|
| `engineVersion` | The service package version from `package.json`. This is not the project's game version or the C++ core protocol version |
| `distribution` | Source checkout or packaged app, its channel, and its release version when one was explicitly assigned |
| `source.commit`, `source.tree`, `source.dirty` | Source checkout provenance, or the source captured when packaging. Dirty means local changes were present; the commit alone cannot reproduce those changes |
| `source.binaryVerified` | Remains false: this receipt does not establish that separately compiled editor/player executables match the source commit |
| `nativeCore.sourceRevision` | The native core build-input digest from its runtime manifest. It is not a Git commit |
| `host`, `buildHost` | Current service host; packaged receipts additionally retain their packaging host |
| `publicPreview` | The separately scoped Windows x64 `0.1.0-preview.3` unsigned-preview policy. Its presence does not approve a newly assembled package |
| `warnings` | Missing or invalid metadata and the limits of the provenance record |

Source archives without Git and older packages without an identity receipt report an unknown revision. The reader never borrows the Git revision of a parent folder. Moving a packaged editor preserves its stored source identity. Exported release records can retain this same identity without including personal installation paths.

Use the [current support guide](../development/platform-support.md) for targets and limitations. Runtime capability declarations, source identities and passing CI are useful evidence; they do not establish physical-device, accessibility, signing, store or asset-rights acceptance.

`config/preview-release.json` retains the historical Windows unsigned-preview decision. Newly generated `release-artifacts.json` files record it under `historicalPublicationPolicy` and start with `publicationStatus: not-evaluated`, `publicReleaseReady: false`. Packaging alone cannot reuse a past approval for new bytes. This does not change published artifacts or add a new publishing action to the editor.
