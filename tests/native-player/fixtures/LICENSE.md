# Audio test fixtures

`tone.wav`, `tone.mp3`, `tone.ogg` and `tone.flac` contain only an original two-second 440 Hz mathematical sine wave. No recording, sample library, voice, or third-party composition is used. The signal and generator are provided under the repository's MIT license.

Regenerate with `node tests/native-player/fixtures/generate-tone.mjs` and FFmpeg on PATH. Compressed bytes can vary with the encoder version; hashes in `audio-manifest.json` identify the reviewed files. FFmpeg is a generation tool and is not bundled in the editor or release package.
