# Native core compiler and sanitizer audit

Verified 2026-10-11/12 on the existing Windows host, without SDK installations.
Every build compiled the same `native/src/runtime.cpp` and included gameplay,
view and protocol sources. Audit binaries were isolated under
`.cache/native-audittoolchain`; no production manifest was published.

| Build | Compiler | Result |
|---|---|---|
| Production | MinGW GNU 14.2.0, C++17 | Native/Node-API parity verified separately |
| MSVC Release | MSVC 19.44.35217.0, Windows SDK 10.0.26100.0 | 33/33 differential tests |
| Clang Release | ClangCL 19.1.5, same Windows SDK | 33/33 differential tests |
| MSVC ASan | MSVC 19.44, `/fsanitize=address` | 33/33; no sanitizer reports |
| Clang UBSan | ClangCL 19.1.5, `-fsanitize=undefined -fno-sanitize-recover=all` | 33/33; no sanitizer reports |

The corpus covers all seven two-/twelve-month routes, every state/transition,
PCG vectors, malformed commands, stale revisions, cost/effect rollback, int32
arithmetic boundaries, Korean/emoji text, travel/shop/gift/immediate events,
content-cache behavior and exact save replay. These checks provide evidence for
the exercised paths; they are not a proof that arbitrary future features are safe.

The later schedule-state regression found that an empty stored schedule could
reach an unchecked C++ array access. The runtime now checks the three string
entries before executing and bounds-checks completed slots during rescheduling.
After this correction, both sanitizer binaries were rebuilt from the updated
source. Each passed 14/14 schedule-state and seven-route differential tests with
no sanitizer diagnostics (`.cache/schedule-asan-test.log` and
`.cache/schedule-ubsan-test.log`). The same focused malformed-schedule cases also
passed in the production subprocess, Node-API addon and WebAssembly core.

## Reproduction

Build the ordinary targets with CMake's `Visual Studio 17 2022` generator and
`-A x64`; use `-T ClangCL` for Clang. Build target `raising-native`, configuration
`Release`. Use separate build folders for each toolchain.

For ASan, use the MSVC generator with:

```text
CMAKE_CXX_FLAGS=/EHsc /fsanitize=address
CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
CMAKE_TRY_COMPILE_CONFIGURATION=Release
CMAKE_EXE_LINKER_FLAGS=/INCREMENTAL:NO
```

Build `RelWithDebInfo`; copy the installed MSVC toolchain's
`bin/Hostx64/x64/clang_rt.asan_dynamic-x86_64.dll` beside the audit executable.
MSVC also reported that incremental linking was disabled by ASan metadata.

For UBSan, use ClangCL with:

```text
CMAKE_CXX_FLAGS=/EHsc -fsanitize=undefined -fno-sanitize-recover=all
CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
CMAKE_TRY_COMPILE_CONFIGURATION=Release
```

Link the installed Clang `x64/lib/clang/19/lib/windows` libraries
`clang_rt.ubsan_standalone-x86_64.lib` and
`clang_rt.ubsan_standalone_cxx-x86_64.lib`. This installation's compiler also
requests the unsuffixed names via default-library metadata, so exact copies were
placed in the isolated audit library folder under those names. The static CRT
matches these installed UBSan libraries. Build `RelWithDebInfo`.

Force the selected audit executable rather than the production addon:

```powershell
$env:RAISING_NATIVE_TRANSPORT = 'process'
$env:RAISING_NATIVE_EXECUTABLE = (Resolve-Path '<audit executable>').Path
$env:ASAN_OPTIONS = 'halt_on_error=1:log_path=.cache/native-audittoolchain/asan-report'
$env:UBSAN_OPTIONS = 'halt_on_error=1:print_stacktrace=1:log_path=.cache/native-audittoolchain/ubsan-report'
node --test tests/journey/native-parity.test.mjs tests/native/view.test.mjs tests/native/gameplay-parity.test.mjs tests/native/audit.test.mjs tests/native/wasm.test.mjs
```

The TypeScript build and compiled Wasm reference must already exist. Sanitizer
log paths are relative to avoid interpreting a Windows drive colon as an option
separator. No sanitizer report files were produced by the successful runs.

## Limits and observed toolchain behavior

Clang's installed combined ASan/UBSan runtime failed before entering the program
with an `interception_win: unhandled instruction` diagnostic on this OS. Separate
MSVC ASan and Clang UBSan successfully ran the complete corpus instead.

MSVC warns about unsigned negation in the PCG rejection threshold. Unsigned
wraparound there is intentional and defined; cross-compiler PCG vectors matched.

WSL inventory returned access denied. No Linux, macOS, mobile or console build
was performed in this audit. Compiling the portable core under three Windows
compilers does not establish complete platform renderer, packaging or SDK support.
