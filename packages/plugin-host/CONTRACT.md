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
| `frame` | `{instance}` | `{items:[{imageAssetId, matrix:[a,b,c,d,e,f], width, height, alpha}]}` |
| `saveAll` / `restoreAll` | — / `{state}` | opaque `state` |
| `describeAll` | — | debug/report data |

`matrix` maps image pixels to element-local pixels: `x' = a·x + c·y + e`, `y' = b·x + d·y + f`.
Hosts draw textured quads in item order (back→front), clipped to nothing (overflow allowed).

Hosts must call `observe` exactly once per submitted command (after the reply), in
command order, and never from frame callbacks. `update` uses wall-clock presentation
time and is never consulted by game logic. Hosts ignore notifications for gameplay.

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

## 10. Node service module and host API (engine TS)

```js
// service/index.mjs (role "service", platform "node"); loaded once per installed id@version
export async function createService({ pluginDir, manifest }) {
  return {
    // documents/elements are only those of this plugin that passed envelope + plugin schema
    validate({ documents: [{ file, type, id, data }], assets: [{ id, kind, path, absolutePath }],
               elements: [{ file, pointer, element }] }) { return [{ file, pointer, code, severity, message, relatedIds }]; },
    call(op, payload) { return json; }   // pure: never writes project files
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
