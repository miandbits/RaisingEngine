# Image-only updates without resetting saves

The export service can publish higher-resolution backgrounds and illustrations while retaining a previously published game's exact save/replay identity. This is an explicit export API option, not an automatic compatibility override in the editor.

Pass the previous `game.json` as `presentationBase` to `exportGame`, alongside a newly validated `content` snapshot and freshly prepared plugins. The exporter permits changes only to existing image file bytes. Both snapshots must have valid fingerprints and complete asset checksums. Rules, text, UI layouts, asset definitions, audio, fonts and plugin documents/runtimes must match. Any other change requires normal content migration or a new game version.

The exported `content` remains the complete, unchanged baseline snapshot. Thus commands, event nodes, animation clocks, gallery captures and saves remain subject to the player's normal strict replay checks. This does not accept arbitrary old fingerprints and does not rewrite player saves.

`game.json` and `export.json` additionally record `presentationRevision`, containing the new source fingerprint and each changed image's baseline/current SHA-256. For these images the effective packaged checksum is `presentationRevision.images[id].sha256`; for unchanged project assets it is `content.assetHashes[id]`. Export still checks every copied source file against the new validated snapshot. The baseline content's checksums describe the original content identity, not revised pixel files. Plugin-generated assets retain their existing plugin checksums.

Use the same asset IDs, paths, aspect ratios and composition. An image-only update does not adjust sprite rectangles, layouts or animation documents. Keep original art and the previous export for rollback. AI enlargement can improve edge clarity but is not a substitute for an artist creating new source detail.

Validation: `tests/journey/presentation-revision.test.mjs` covers Windows/web packaging, unchanged baseline identity, explicit pixel checksums, repeated art revisions, rejected content/plugin/non-image changes and source files modified after validation. Actual player testing should also load an old manual save and automatic saves, resave, and check presentation clocks and gameplay state.
