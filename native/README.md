# Portable C++ raising simulation runtime

The scheduling, numeric expressions, conditions, transactional effects, PCG32 RNG,
events, narrative, endings, travel, shopping, gifts and read-only gameplay projection
are implemented in C++17. The executable does not invoke Node or TypeScript.

The Electron editor remains TypeScript/React. Its production session, analysis and
save replay services select this native core by default. `RAISING_RUNTIME=typescript`
explicitly selects the frozen-compatible reference implementation for differential
tests. Neither native transport falls back to TypeScript rules.

## Build and runtime boundary

Prerequisites for Windows: CMake and a C++17 MinGW compiler on PATH. Run
`node scripts/build-native.mjs`. Other desktop hosts use the platform CMake generator.
The tested Windows compiler was GNU 14.2.0. MSVC can build `native/CMakeLists.txt`
directly; the convenience script currently selects MinGW on Windows.

Outputs are immutable `dist/native/raising-native-<source-hash>.exe` files.
`dist/native/runtime.json` identifies the active build. Build locks and per-hash
source/build directories prevent concurrent builds from overwriting a running file.
Old binaries remain available to already running applications. Packaged applications
copy the selected binary plus a manifest outside Electron's ASAR archive.
The default adapter pins its selected executable for the application lifetime;
rebuilding the editor cannot change an in-progress game's rule implementation.

The static library target is `raising_core`. Native renderers link it and include
`native/include/raising/runtime.hpp`:

```cpp
std::string raising::process(const std::string& request);
```

The standalone executable reads one UTF-8 JSON request per line from stdin and writes
one JSON response per line to stdout. Diagnostic transport errors do not alter state.
The Node adapter prefers a matching Node-API addon, calling the same C++ function
in the existing analysis worker without pipe or worker-message round trips. If the
addon cannot load, it keeps one native child process and a worker for pipe I/O.
Both preserve the synchronous API without launching a process per command.

## Node-API transport

After building the core, run `node scripts/build-native-addon.mjs`. It creates an
immutable `dist/native/raising-addon-<hash>.node` and `addon.json`. The manifest binds
the addon to its matching rules executable, host platform and architecture. Packagers
copy both native transports outside ASAR, with the Node header license.

The addon uses stable C Node-API version 8 only. On Windows it resolves C symbols from
the host executable, avoiding any import of `node.dll` or dependency on the V8/Node
C++ ABI. The exact same GNU 14.2.0 binary was tested in Node 22.14.0 and Electron 44.7.0
(actual Electron main process with Node 24.21.0). Protocol caches are thread-local;
parallel analysis workers cannot overwrite each other's content registrations.

`RAISING_NATIVE_TRANSPORT=process` forces the pipe transport for diagnosis.
`RAISING_NATIVE_TRANSPORT=auto` (default) prefers the addon and falls back to the C++
subprocess on load/compatibility failure. `nativeTransport()` reports the selected
transport and fallback reason. A failure executing an already loaded addon is
propagated, not silently replayed in a different runtime.

`tests/native/addon.test.mjs` verifies both hosts, route parity, rejected inputs,
worker cache isolation and fallback. `tests/native/benchmark-addon.mjs` compares
entire automatic-play results (including recorded traces) with identical seeds and
policies. A local 10-run journey sample measured 1.62s process / 0.79s addon; a 30-run
sample measured 6.89s / 2.48s. These are measurements, not timing guarantees.

## Protocol version 1

All content values are validated `buildSnapshot` results. The desktop project loader
and JSON Schema/semantic authoring validator remain TypeScript services. The native
boundary is not a replacement for validating arbitrary project files.

| operation | Fields | Result |
|---|---|---|
| `info` | none | Runtime, protocol and core versions, implementation |
| `create` | `content`, `seed`, `options:{mode,initialOverrides?}` | `{ok,state}` or `{ok:false,error}` |
| `command` | `content`, `state`, `command`, `expectedRevision` | Existing core dispatch result, including full transition |
| `replay` | `content`, `origin:{seed,mode,initialOverrides?}`, `commands` | Replayed state, or exact failing `/commands/index` |
| `view` | `content`, `state` (nullable) | `{ok,time,narrative,gameplay}` |
| `evaluate` | `content`, `state`, either `expression` or `condition` | `{ok,value}` |
| `random` | `seed` or `rng`, `count?`, `bound?` | `{ok,rng,values}` |
| `registerContent` | `contentId`, `content` | Registers immutable content for transport reuse |
| `releaseContent` | `contentId` | Releases transport cache entry |

After registration, requests may provide `contentId` instead of `content`. This is a
bounded transport cache (32 entries), not hidden run state. Every command still
provides and returns the complete run state. The Node adapter caches at most 16 frozen
snapshots; mutable test inputs continue to send full content. Saved games never refer
to this volatile cache. Replay executes its complete journal within one native call.

The state remains core version `0.2.0`; optional gameplay fields do not change legacy
project states. PCG32 uses uint64 wraparound and fixed stream 54. Numeric rule results
must fit int32, failed commands retain the original state and revision, and generated
save files are accepted only when replay reproduces the saved state exactly.
The inter-language boundary uses JSON's integer representation: JavaScript's
transient IEEE `-0` is serialized as `0`, matching C++ integer zero.

## WebAssembly

Run `node scripts/build-wasm.mjs` with the official Emscripten SDK installed under
`.toolchains/emsdk`, or set `EMSDK` to its installation. The local verification used
Emscripten **6.0.12**, SDK repository commit
`35ff8a6d150541276abbc6bae512ca90bcfbe220`, release
`6e9f6885cb17a3d6c8fd488a5cff730f50aba61d`.
Official SDK source: <https://github.com/emscripten-core/emsdk>.
The SDK is a build-time tool and is not copied into exported games.

The build emits `dist/wasm/raising-wasm.mjs` and its sibling `.wasm` file. C++ exceptions
are explicitly enabled so invalid input produces structured errors in the browser.
The module has no filesystem dependency. Load it with its default factory, then
pass the resulting module to `WasmRuntime` in `packages/native-bridge/wasm.ts`.
The adapter only moves UTF-8 JSON across the C ABI; it does not implement game rules.

```js
const core = new WasmRuntime(await createModule());
const result = core.request({operation: 'create', content, seed: 42, options: {mode:'full'}});
```

## Verification and platform status

The unchanged core also passes the same 33-test differential corpus under MSVC
19.44, ClangCL 19.1.5, MSVC AddressSanitizer and Clang UndefinedBehaviorSanitizer.
See [PORTABILITY-AUDIT.md](PORTABILITY-AUDIT.md) for exact scope and reproduction.

- Windows native core: compiled and exercised by the actual player/session services.
- Web core: compiled to WebAssembly and exercised in real Chromium, including the
  home/city/shop/gift/event route; PCG vectors and all seven existing sample routes
  are compared against both native and reference states/transitions.
- Read projections, revision conflicts, cost/stock failures, overflow rollback,
  immediate event return presentation and save replay are tested.
- Android, iOS, Linux, macOS and consoles: this portable core is a foundation, not a
  claim of tested builds. Platform renderer, input, audio, storage/lifecycle integration
  and device verification remain distinct requirements. Console SDK access is external.

Run `node --test tests/native/view.test.mjs tests/native/gameplay-parity.test.mjs` after
a native/TypeScript build. After a Wasm build, additionally run
`node --test tests/native/wasm.test.mjs tests/native/wasm-browser.test.mjs`.
Browser verification requires installed Chrome. Independent parity checks are in
`tests/journey/native-parity.test.mjs`.

## Dependency

nlohmann/json v3.11.3 is vendored with SHA-256 provenance and MIT license under
`native/vendor`. Builds do not download it. Native distributions include its license.
