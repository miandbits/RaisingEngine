# Public sample and asset boundary

The release packager copies five explicit project-manager templates: `academy-demo`, `academy-year`, `academy-visual`, `academy-journey`, `academy-adventure`, plus the portable `rule-extension-lab` example. It requires the templates to appear in `config/repository-content.json`. It does not copy the whole checkout or `projects/`, `internal/`, test evidence, private tooling, editor history, save slots, or Git history.

| Material | Distribution treatment |
|---|---|
| Original engine/editor source and documentation | MIT, owner approved 2026-10-18 |
| Public sample project JSON and original generator code | Repository source examples; included in the selected preview templates |
| Original procedural/vector sample images | Keep with their sample and provenance; inventory hashes of the actual package |
| AI-generated journey art | `assets/journey/provenance.json` and `background-provenance.json` record generation, not an independent legal clearance. Public media licensing/provenance review remains a release gate |
| NotoSansKR | Retain OFL notice with the font; reserved names and original attribution are unchanged |
| PM2/Magical Princess extracted/reference assets | Excluded; known SHA-256 hashes checked again on the assembled package |
| User games and saved data | Never copied into the distribution or removed during uninstall |
| Optional animation plugins | Separate installation and license; no proprietary Spine runtime bundled |

The package manifest records every included payload file's bytes and SHA-256. `npm run check:public-content` checks the tracked public boundary; the packager additionally checks actual packaged bytes against the known reference-asset hashes. Neither proves rights for an unknown image, scans secrets throughout old commits, or cleans GitHub history/caches. Do not publish the development repository history as a reviewed release solely because these checks pass.

Before a public binary release, finish media permission/attribution decisions, corresponding Qt/dependency source distribution, a dedicated history/secret audit, code signing and clean-machine installation testing. The generated installer is explicitly an **unsigned local preview** until these checks have evidence.
