# Third-party software and media

The MIT license at the repository root covers original RaisingStudio code and documentation. It does not replace the terms of dependencies, fonts, plugins, generated media, or reference-game assets.

The Windows Qt preview uses dynamically linked Qt 6.10.1 Core/Gui/Widgets/Network/Svg and deployed plugins. Qt DLLs remain separate and replaceable. Its LGPL/GPL license texts are under `licenses/qt`; the packager also copies installed Qt attribution pages and hash-verified corresponding sources for Qt Base, SVG, Image Formats and Translations under `licenses/qt/sources/`. See [the version-specific review](docs/development/preview-license-review.md) (installed as `licenses/README.md`). Qt's documented obligations apply to the actual files shipped: https://www.qt.io/development/open-source-lgpl-obligations . Replacement and debugging of modified LGPL libraries are not prohibited.

Node.js 22.14.0 and its bundled third-party code: `licenses/node/LICENSE-v22.14.0.txt`. Service packages Ajv, jsonc-parser, yauzl and transitive dependencies retain their original package license files; versions are recorded by the package manifest. Newer Node versions require their matching license inventory before packaging.

Windows player: SDL 3.2.24 / SDL_image 3.2.4 (zlib), stb_image (MIT option), RmlUi 6.1 and its bundled containers (MIT notices), FreeType 2.13.3 (FreeType license), miniaudio 0.11.23 (MIT option), libwebp 1.6.0 (BSD and patent notice), nlohmann/json (MIT). Their supplied notices are retained under the player's `licenses/` directory. MinGW/GCC runtime license and exception texts are retained under `licenses/mingw`; the review records enabled and disabled static dependencies.

NotoSansKR font: SIL Open Font License 1.1, preserved under `fonts/OFL.txt` in the Windows runtime and `licenses/NotoSansKR-OFL.txt` in the Web runtime. Qt/OS fallback fonts are not copied from Windows. React, React DOM and Scheduler license texts accompany the Web runtime.

Editor Noto Sans JP: unmodified Google Fonts distribution, SIL OFL 1.1; copyright, reserved-name notice and provenance are in `native/studio/fonts/` (installed as `fonts/`). The editor loads this font for Japanese glyph coverage; game language fonts remain a designer choice.

Public sample provenance and permission review are tracked separately in `docs/development/public-assets.md`. The public sample allowlist and excluded reference hashes are in `config/repository-content.json`. PM2/Magical Princess reference experiments and user projects are not included. Passing the hash check is not a universal copyright clearance or a history/secret audit.

Optional animation extensions retain their own manifests and licenses and must be installed separately. This preview does not bundle proprietary Spine runtimes or claim their license.
