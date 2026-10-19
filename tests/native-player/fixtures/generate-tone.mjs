// Original sample-free test signal, licensed under the repository MIT license.
// Run with FFmpeg on PATH; FFmpeg is a build tool, not distributed here.
import {writeFileSync} from 'node:fs';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';

const rate = 22050;
const samples = rate * 2;
const wav = Buffer.alloc(44 + samples * 2);
wav.write('RIFF', 0); wav.writeUInt32LE(wav.length - 8, 4);
wav.write('WAVEfmt ', 8); wav.writeUInt32LE(16, 16);
wav.writeUInt16LE(1, 20); wav.writeUInt16LE(1, 22);
wav.writeUInt32LE(rate, 24); wav.writeUInt32LE(rate * 2, 28);
wav.writeUInt16LE(2, 32); wav.writeUInt16LE(16, 34);
wav.write('data', 36); wav.writeUInt32LE(samples * 2, 40);
for (let i = 0; i < samples; i++) {
  wav.writeInt16LE(Math.round(4096 * Math.sin(2 * Math.PI * 440 * i / rate)), 44 + i * 2);
}
const path = ext => fileURLToPath(new URL(`tone.${ext}`, import.meta.url));
writeFileSync(path('wav'), wav);
for (const [ext, codec] of [['mp3', 'libmp3lame'], ['ogg', 'libvorbis'], ['flac', 'flac']]) {
  const result = spawnSync('ffmpeg', ['-v', 'error', '-y', '-i', path('wav'), '-map_metadata', '-1', '-c:a', codec, path(ext)], {stdio: 'inherit', windowsHide: true});
  if (result.error || result.status !== 0) throw result.error ?? new Error(`FFmpeg failed for ${ext}`);
}
