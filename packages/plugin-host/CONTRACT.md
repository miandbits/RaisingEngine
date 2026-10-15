# Engine plugin contract (plugin api 1)

Status: v1, first-party trusted local modules. This is not a public marketplace and
does not run user scripts inside game rules. Plugins add document types, authoring
panels and presentation runtimes. Game rules, costs and RNG stay in the C++ core.

## 1. Package (`*.raiseplugin`)

A ZIP archive (stored or deflated entries, no absolute paths, no `..`, no symlinks,
max 64 MiB unpacked, max 2000 entries) whose root contains `plugin.json`:

```jsonc
{
  "manifestVersion": 1,
  "id": "cutout-animation",                 // ^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$
  "name": "컷아웃 캐릭터 애니메이션",
  "version": "1.0.0",                        // semver MAJOR.MINOR.PATCH
  "engineApi": { "min": 1, "max": 1 },       // ENGINE_PLUGIN_API must be within range
  "dependencies": [ { "id": "other", "range": "^1.2.0" } ],   // optional
  "documentTypes": [
    { "type": "rig", "label": "캐릭터 리그", "schema": "schema/rig.schema.json" },
    { "type": "controller", "label": "애니메이션 연결", "schema": "schema/controller.schema.json" }
  ],
  "elementTypes": [ { "type": "character", "label": "애니메이션 캐릭터", "refType": "controller" } ],
  "modules": [
    { "role": "authoring", "platform": "windows-qt6.10-mingw64", "path": "authoring/windows/cutout-studio.dll" },
    { "role": "runtime", "platform": "windows-x64", "abi": "raising-extension-1", "path": "runtime/windows/cutout-runtime.dll" },
    { "role": "runtime", "platform": "web-wasm", "abi": "raising-extension-1", "path": "runtime/web/cutout-runtime.mjs", "files": ["runtime/web/cutout-runtime.wasm"] },
    { "role": "service", "platform": "node", "path": "service/index.mjs" }
  ],
  "exportTargets": ["windows", "web"],
  "capabilities": ["cutout-animation-v1"],
  "files": { "<relative path>": "<sha256 hex>" }   // every file except plugin.json
}
```

`role` separates authoring tools (never exported) from runtime modules (copied into
exported games) and the node service module (validation / authoring helpers used by
the shared Studio/MCP service, never exported).

Versions are plain `MAJOR.MINOR.PATCH` (no prerelease/build tags); dependency ranges are
`^x.y.z`, `~x.y.z` or exact `x.y.z`. At most one module per `role`+`platform`; runtime
modules need `abi`. The package file itself must be ≤ 128 MiB. `files` must list every
entry except `plugin.json` (extra, missing or mismatching entries are refused).

## 2. Engine store

* Location: `$RAISING_PLUGIN_STORE` or `<engineRoot>/plugin-store/`.
* Installed files: `<store>/installed/<id>/<version>/…` (immutable once installed).
* `<store>/registry.json`: `{ "version": 1, "installed": [{ "id", "version", "sha256", "source": "local"|"remote", "installedAt" }] }`.
* `sha256` is the hash of the package file bytes. Reinstalling the same id+version with a
  different hash is refused (`PLUGIN_VERSION_CONFLICT`).
* Install is staged into `<store>/.staging-<uuid>` then renamed. Remote download (later)
  must save the archive to a temporary file and call the same `installPackage` path.
* `installPackage` refuses a package whose `engineApi` excludes this engine
  (`PLUGIN_API_INCOMPATIBLE`); an incompatible version can still be present when the store
  is shared with another engine build, and projects then report it (§3).
* `<store>/.lock` (owner pid) serializes installs/uninstalls across processes.

## 3. Project files

| File | Kind | Purpose |
|---|---|---|
| `plugins/lock.json` | `pluginLock` | pins plugin versions per project |
| `plugins/<pluginId>/<type>/<docId>.json` | `extension` | plugin-owned documents |

```jsonc
// plugins/lock.json
{ "formatVersion": "0.1.0", "plugins": [ { "id": "cutout-animation", "version": "1.0.0", "enabled": true } ] }
// plugins/cutout-animation/rig/hero.json
{ "formatVersion": "0.1.0", "plugin": "cutout-animation", "type": "rig", "id": "hero", "data": { … } }
```

The envelope (`extension.schema.json`) is validated by the engine; `data` is validated by
the plugin (its JSON schema, then its service module). The path must equal
`plugins/<plugin>/<type>/<id>.json`.

### Missing / disabled / incompatible plugins never delete data

| Situation | Diagnostic (severity) | Behavior |
|---|---|---|
| document's plugin not in lock | `PLUGIN_NOT_LOCKED` (warning) | kept verbatim, not validated |
| lock entry `enabled:false` | `PLUGIN_DISABLED` (warning) | kept verbatim; elements show placeholder |
| locked version not installed | `PLUGIN_MISSING` (warning) | kept verbatim; elements show placeholder |
| installed but `engineApi` out of range | `PLUGIN_API_INCOMPATIBLE` (error) | kept verbatim |
| dependency missing / out of range | `PLUGIN_DEPENDENCY` (error) | kept verbatim |
| export/preview needs runtime module that is unavailable | `PLUGIN_RUNTIME_MISSING` (error at export) | export refused |

Saving a project re-serializes only documents whose data changed; untouched extension
files keep their bytes.

Lock-entry diagnostics (`PLUGIN_DISABLED`, `PLUGIN_MISSING`, `PLUGIN_API_INCOMPATIBLE`,
`PLUGIN_DEPENDENCY`, `PLUGIN_LOCK_DUPLICATE`, `PLUGIN_SERVICE_ERROR`) are reported once on
`plugins/lock.json` at `/plugins/<i>…`; `PLUGIN_NOT_LOCKED` on each document (`/plugin`).
Screen elements of a disabled/missing/unlocked plugin get the same code as a warning at
`…/extension/plugin`. Other engine codes: `EXTENSION_PATH` (path ≠ envelope; also a loader
warning for unreadable names under `plugins/`), `EXTENSION_TYPE_UNKNOWN`,
`PLUGIN_ELEMENT_TYPE`, `PLUGIN_ELEMENT_REF` (engine checks `refType` only when the plugin
has no service), `UI_EXTENSION_KIND` (`extension` on a non-plugin element), and
`SCHEMA_INVALID` under `/data…` for plugin-schema failures — like engine schema errors these
block saving. Engine semantic checks never walk plugin `data`.

## 4. Edits (shared by Qt, Electron and MCP)

Existing `replace` / `delete` / `batch` / `undo` / `redo` apply to `extension` and
`pluginLock` documents. New primitive:

```jsonc
{ "type": "create", "file": "plugins/cutout-animation/rig/hero.json", "data": { …envelope… } }
```

Allowed only for extension and lock paths; fails if the file already exists. Revisions,
draft/disk versions, conflict detection and history are unchanged.

## 5. Screen element

`visual.screens[].elements[]` gains kind `plugin`:

```jsonc
{ "id": "hero-on-title", "kind": "plugin", "x": 40, "y": 120, "width": 300, "height": 450,
  "extension": { "plugin": "cutout-animation", "type": "character", "ref": "hero-main",
                 "props": { "scale": 1, "flipX": false } } }
```

`extension.props` is plugin-defined (object, ≤ 32 keys, primitive values). Export requires
player capability `plugin-elements-v1` and the plugin's runtime module for the target.

One prop is host-reserved: `posterAssetId` (an image asset id; `PLUGIN_POSTER_ASSET` error when
it names no image asset). Editors draw it, contain-fitted, as a still preview when the plugin's
authoring module cannot paint the element; players ignore it. Plugins must accept and ignore it.

## 6. Exported game

`game.json` gains optional `extensions` (absent when no plugin is used, so existing exports
are byte-for-byte unchanged):

```jsonc
"extensions": { "version": 1, "plugins": [ {
  "id": "cutout-animation", "version": "1.0.0", "abi": "raising-extension-1",
  "entry": "plugins/cutout-animation/cutout-runtime.dll",          // or .mjs for web
  "documents": [ { "type": "rig", "id": "hero", "data": { … } } ]
} ] }
```

Runtime module files are copied to `plugins/<id>/` in the export. Authoring and service
modules are never copied. Content snapshot and `contentFingerprint` do not include
extension documents: changing animation never invalidates saves.

Only plugins referenced by a screen element are exported; `documents` holds all of that
plugin's documents sorted by `type`, `id`. Module files are flattened to their basename and
re-hashed against `manifest.files` while copying.

## 7. Runtime extension ABI (`raising-extension-1`)

Exports (DLL: C symbols; Wasm: ES module default factory, `cwrap` of the same names):

```c
int         raising_extension_api_version(void);            // 1
const char* raising_extension_request(const char* json);    // reply valid until next call
```

Requests (replies are `{ "ok": true, … }` or `{ "ok": false, "error": "…" }`):

| op | request | reply |
|---|---|---|
| `init` | `{documents}` | `{diagnostics}` |
| `mount` | `{instance, element:{type, ref, width, height, props}}` | re-mount keeps state |
| `unmount` | `{instance}` | |
| `observe` | `{before, after, command, error}` where before/after = `{state, narrative}` | `{signals, matched}` |
| `update` | `{dtMs}` | `{notifications}` (presentation only) |
| `frame` | `{instance}` | `{items:[{imageAssetId, matrix:[a,b,c,d,e,f], width, height, alpha, mesh?}]}` |
| `saveAll` / `restoreAll` | — / `{state}` | opaque `state` |
| `describeAll` | — | debug/report data |

`matrix` maps image pixels to element-local pixels: `x' = a·x + c·y + e`, `y' = b·x + d·y + f`.
Hosts draw textured quads in item order (back→front), clipped to nothing (overflow allowed).

**Mesh items** (ABI extension, still `raising-extension-1`): an item may carry
`mesh: {positions: [x, y, …], uvs: [u, v, …], indices: [i, j, k, …]}` — element-local vertex
positions (already placed, like `matrix`'s output), texture coordinates 0..1 of the item's image
(one pair per vertex) and triangle index triples, drawn in order with the item's `alpha`. Such an
item still carries `matrix`/`width`/`height` (the undeformed rectangle) only so older hosts
degrade instead of failing; a host that supports meshes draws the triangles and never the
rectangle, skips a malformed mesh (counts differ, index out of range, more than 2048 vertices or
4096 triangles) and advertises player capability **`plugin-mesh-v1`** in `capabilities.json`
`visualFeatures` (the native player emits RmlUi triangle geometry, the web player draws each
triangle as an affine-mapped image on a per-item `<canvas>`).

A plugin's node service may export `requiredHostFeatures(documents)` → capability ids the game's
documents need beyond `plugin-elements-v1` (cutout returns `["plugin-mesh-v1"]` when a rig uses
a mesh). `exportBundle` collects them as `PreparedExtensions.features` (a failing service is
`PLUGIN_SERVICE_ERROR`) and `exportGame` refuses a target player whose capabilities lack one,
before anything is written.

Hosts must call `observe` exactly once per submitted command (after the reply), in
command order, and never from frame callbacks. `update` uses wall-clock presentation
time and is never consulted by game logic. Hosts ignore notifications for gameplay.

**Event notifications and sounds** (ABI extension, still `raising-extension-1`): `update` replies
`{notifications: {<instance>: [{kind, …}]}}`. An entry with `kind: "event"` may carry a payload
next to its `name` (`int`, `float`, `string`, and `audio` = an audio asset id with `volume` 0..1
and `balance` -1..1, defaults 1 / 0). Hosts keep the newest 32 event entries (plus `instance`,
`plugin`, `clockMs`) for reports (`extensions.events`, `eventCount`, `soundCount`) and MAY play
`audio` as a one-shot effect through their normal sound path while unmuted (native: miniaudio
volume/pan, report `audio.extensionSounds`; web: `HTMLAudioElement.volume`, balance through a
Web Audio stereo panner). A missing or undecodable file is counted, never an error of the game.
Playing sounds is the host's job: plugins only name them, and nothing here reaches game state.

## 8. Save data

`playerPresentation.extensions` (optional) = `{ "<pluginId>": <saveAll state> }`.
Unknown plugin ids are preserved on load+save round trips when possible and otherwise
ignored; a missing plugin never blocks loading the game state. Limits: ≤ 32 plugin ids,
≤ 1 MiB canonical JSON per plugin, ≤ 4 MiB total (`SAVE_PRESENTATION` otherwise).
`runtime-service/save.ts` strips `extensions` before `PlayerPresentation.restore` and
carries it through unchanged; other hosts restoring an envelope must do the same.

## 9. Qt authoring ABI (`raising-studio-plugin-1`)

`native/studio/plugin-api.hpp` (Qt plugin interface IID `com.raising.StudioPlugin/1`).
The DLL receives a `StudioHost*` and must perform every document change through
`StudioHost::request("edit", …)`; widget state is only a cache of the service view.

Optional workspaces (same ABI name and `apiVersion` 1). `StudioHost`/`StudioPlugin` vtables
are frozen, so the extension is a second Qt interface instead of appended virtuals:
`StudioWorkspacePlugin` (IID `com.raising.StudioWorkspacePlugin/1`, listed in the plugin's
`Q_INTERFACES`; the metadata IID stays `com.raising.StudioPlugin/1`). After `activate()` the
host `qobject_cast`s the loader instance; when the cast succeeds it calls
`createWorkspaces(StudioWorkspaceHost*)` once and gets `{id, title, widget, shortcut}` entries.
The host owns the widgets (deleted with the plugin docks before unload), shows a switcher
(menu bar corner `studioWorkspaceSwitcher`, 보기 → 작업 공간 actions
`studioWorkspace-screen` / `studioWorkspace-<plugin>-<id>`, the entry's shortcut toggles) and,
while a workspace is the central area, hides every dock and the screen editor; returning
restores the exact `saveState` taken when the screen editor was left. Menus, undo/redo, save
and play stay available; switching never sends a request. `workspaceChanged(id, shown)`
reports switches; `StudioWorkspaceHost::showWorkspace(id)` (empty = 화면 편집) lets the plugin
switch (e.g. from `openEditor`). The last workspace is remembered per project root and comes
back when that project loads the plugin again. Old DLLs (no interface) keep their docks; a
new DLL on an old host is never asked and must keep working in its dock.

## 10. Node service module and host API (engine TS)

```js
// service/index.mjs (role "service", platform "node"); loaded once per installed id@version
export async function createService({ pluginDir, manifest }) {
  return {
    // documents/elements are only those of this plugin that passed envelope + plugin schema
    validate({ documents: [{ file, type, id, data }], assets: [{ id, kind, path, absolutePath }],
               elements: [{ file, pointer, element }] }) { return [{ file, pointer, code, severity, message, relatedIds }]; },
    call(op, payload) { return json; },  // pure: never writes project files
    requiredHostFeatures(documents) { return ['plugin-mesh-v1']; }   // optional (§7): export-time player needs
  };
}
```

`packages/plugin-host` exports `installPackage`, `inspectPackage`, `uninstallPlugin`,
`listInstalled`, `resolveProjectPlugins`, `pluginStatus`, `callProjectPlugin`,
`loadService`, `exportBundle`. `StudioEditor({ pluginStore })` adds `plugins()` and
`exportExtensions(revision, content, target)`; `exportGame({ …, extensions })` refuses
plugin elements without prepared extensions. Hosts: Qt service methods `pluginStatus`,
`pluginInstall {file}`, `pluginEnable {id, version?, revision}`, `pluginDisable {id, revision}`,
`pluginCall {plugin, op, payload}` and a `plugins` member in every reply; MCP tools
`plugin_status`, `plugin_call`, `get_schema {name:"extension", plugin, type}` and the
`create` edit in `apply_project_changes`.
