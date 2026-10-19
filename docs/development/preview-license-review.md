# Preview dependency review

Scope: the Windows x64 Qt Widgets preview, bundled Node service, SDL Windows player and React/Wasm web export. Original engine/editor source and documentation are MIT. Third-party components keep their own licenses. Review the actual `package-manifest.json`, not a generic dependency list.

| Component | License / distribution action |
|---|---|
| Qt 6.10.1 Core, Gui, Widgets, Network, SVG, Image Formats and translations | LGPL-3.0 option for these modules; retain license and attribution pages, keep DLLs replaceable. Unmodified official corresponding source archives are included in `licenses/qt/sources/` and SHA-256 pinned in `corresponding-source.json`. |
| MinGW 13.1 runtime | GCC GPL plus Runtime Library Exception; MinGW-w64 / winpthreads notices retained in `licenses/mingw`. Qt DLLs use the official MinGW kit; native player uses an eligible GCC compilation with static runtime flags. |
| Node 22.14.0 | MIT and embedded dependency terms, complete supplied Node LICENSE retained. |
| Ajv, jsonc-parser, yauzl and installed transitive service dependencies | Installed versions and license declarations in the manifest; package license files retained in `engine/node_modules`. |
| React, React DOM, Scheduler | MIT; license texts retained in the web runtime and every exported web game. |
| SDL 3.2.24, SDL_image 3.2.4 | zlib; pinned source revisions in the repository. SDL_image uses its bundled stb image decoder (MIT notice included) and libwebp. AVIF, TIFF and JXL are disabled. |
| RmlUi 6.1 and bundled containers | MIT plus container notices, both included. No Lua bindings. |
| FreeType 2.13.3 | FreeType license option and attribution retained. Optional zlib, bzip2, PNG, HarfBuzz and Brotli are disabled. |
| libwebp 1.6.0 | BSD notice and patent grant retained. |
| miniaudio 0.11.23 | MIT/public-domain alternatives in the supplied license; use the MIT option. |
| nlohmann/json | MIT notice included in Windows and Web exports. |
| NotoSansKR and Noto Sans JP | SIL OFL 1.1. Font files and original notices retained; no Windows system fonts are redistributed. |
| Public sample images and audio | Separate provenance review. Engine MIT does not certify ownership of generated media or grant third-party game rights. See `public-assets.md`. |

Qt Widgets uses raster painting. Optional `D3Dcompiler_47.dll`, `dxcompiler.dll` and `dxil.dll` are excluded from this preview. QtTest and the offscreen plugin are temporary test tools, not installer payloads. Optional animation plugins and proprietary Spine runtimes are not bundled.

## Replacing Qt and obtaining source

The Qt DLLs and plugins are separate files. Users may replace them with ABI-compatible builds of Qt 6.10.1 using the same x64 MinGW toolchain. This application imposes no signature or hash enforcement on replacement libraries at runtime and does not prohibit reverse engineering for debugging such modifications. The packager's hashes verify release provenance, not permission to modify installed files.

Extract the matching archives in `licenses/qt/sources/`. They include upstream sources, third-party code, license texts and build instructions. Build Qt with the documented Windows CMake/MinGW procedure, then build this repository using `QT_ROOT` and `QT_COMPILER` pointing to the replacement kit. The engine's `native/studio/CMakeLists.txt` and `scripts/build-qt-studio.mjs` are public MIT sources. Qt has no local patches in this distribution. Keep compatible libraries and platform plugins together.

The source checksums were compared against Qt's official `.sha256` files. The archived Qt module sources and installed SDK attribution pages are the version-specific review inputs; the current online Qt documentation may describe a newer patch release.

Primary references: [Qt LGPL obligations](https://www.qt.io/development/open-source-lgpl-obligations), [Qt licenses and third-party code](https://doc.qt.io/qt-6.10/licenses-used-in-qt.html), [GCC Runtime Library Exception](https://www.gnu.org/licenses/gcc-exception-3.1.en.html), [GCC exception FAQ](https://www.gnu.org/licenses/gcc-exception-3.1-faq.en.html).

## Remaining publication checks

The prepared installer is unsigned. A fresh Windows VM, a signing decision, sample media permissions and native-speaker editorial review remain release checks. Catalog coverage and automated Qt glyph/layout checks do not replace that review. Preparing a tested installer does not claim platform certification or a complete legal clearance. Do not remove these gates merely because the package builds. Preserve source archives and notices with any binary redistribution.
