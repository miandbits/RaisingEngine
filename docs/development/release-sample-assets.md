# Binary preview sample media

The release package uses original geometric PNG demonstration art generated solely by `scripts/release-sample-media.mjs`. The generator and its original drawing definitions are distributed under this repository's MIT license; generated outputs may be used, modified and redistributed under the same license. Retain the MIT copyright and permission notice. These are simple demonstration assets, not illustrations from a commercial game.

Seven image designs replace 18 copies in the selected visual, journey and adventure templates. The replacement policy pins each original file's hash and target path; unknown media stops packaging. The resulting `licenses/sample-media.json` records every image's hash, size, generator and license. The audit regenerates each design and rejects missing, modified or additional media in packaged examples.

Generated image-tool artwork and commercial-game reference images are **not shipped in this binary preview**. Existing repository source art, local games and animation projects are preserved. Generation prompts alone do not establish a redistribution license or third-party rights clearance, so this package does not depend on them. The existing source repository's artwork boundary is a separate matter; this policy does not rewrite history or claim to remove old source releases.

No music, voice or video is bundled in these templates. Noto Sans KR and Noto Sans JP are separately licensed under SIL OFL 1.1; their original notices accompany the fonts. Do not apply the engine's MIT license to those fonts or to third-party libraries. The full dependency notices and corresponding Qt sources remain in `licenses/`.
