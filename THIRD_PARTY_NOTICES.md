# Third-party software and media

The MIT license at the repository root covers original RaisingStudio code and documentation. It does not replace the terms of dependencies, fonts, plugins, generated media, or reference-game assets.

The Windows Qt preview uses dynamically linked Qt 6.10.1 Core/Gui/Widgets and deployed plugins. Qt DLLs remain separate and replaceable. Its LGPL/GPL license texts are under `licenses/qt`; the packager also copies relevant installed Qt attribution pages. Qt's documented obligations and module licenses must be reviewed for the **actual** files shipped: https://doc.qt.io/qt-6/licensing.html and https://www.qt.io/development/open-source-lgpl-obligations . A link alone is not an offer of corresponding source controlled by the distributor. Public binary publication remains gated on arranging corresponding Qt/dependency source and verifying the complete notice inventory.

Node.js 22.14.0 and its bundled third-party code: `licenses/node/LICENSE-v22.14.0.txt`. Service packages Ajv, jsonc-parser, yauzl and transitive dependencies retain their original package license files; versions are recorded by the package manifest. Newer Node versions require their matching license inventory before packaging.

Windows player: SDL 3.2.24 / SDL_image 3.2.4 (zlib), RmlUi 6.1 (MIT), FreeType 2.13.3 (FreeType license), miniaudio 0.11.23, libwebp 1.6.0 (BSD and patent notice), nlohmann/json (MIT). Their supplied notices are retained under the player's `licenses/` directory. MinGW/GCC runtime exceptions and the exact static-link dependency inventory require review before public binary publication.

NotoSansKR font: SIL Open Font License 1.1, preserved under `fonts/OFL.txt` in the Windows runtime and `licenses/NotoSansKR-OFL.txt` in the Web runtime. Qt/OS fallback fonts are not copied from Windows. React, React DOM and Scheduler license texts accompany the Web runtime.

Public sample provenance and permission review are tracked separately in `docs/public-assets.md`. The public sample allowlist and excluded reference hashes are in `config/repository-content.json`. PM2/Magical Princess reference experiments and user projects are not included. Passing the hash check is not a universal copyright clearance or a history/secret audit.

Optional animation extensions retain their own manifests and licenses and must be installed separately. This preview does not bundle proprietary Spine runtimes or claim their license.
