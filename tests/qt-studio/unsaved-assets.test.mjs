// Files imported for a draft that is never saved (PNG, plugin pictures, sprites, fonts) are removed
// again when the editor closes the project, reopens it or opens another one, and on the next open
// after a crash; saved, hand-replaced and other live editors' files stay.
import test from 'node:test';
import assert from 'node:assert/strict';
import { cp, mkdir, mkdtemp, readFile, writeFile } from 'node:fs/promises';
import { existsSync } from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { QtStudioService } from '../../apps/qt-studio/service.mjs';
import { encodeRgbaPng } from '../../dist/packages/studio-service/png.js';

const JOURNAL = '.studio/unsaved-assets.json';
const sha = bytes => createHash('sha256').update(bytes).digest('hex');
async function fixture() {
  await mkdir('.cache/unsaved-assets-tests', { recursive: true });
  const root = await mkdtemp(path.resolve('.cache/unsaved-assets-tests/project-'));
  await cp('examples/academy-adventure', root, { recursive: true });
  const service = new QtStudioService();
  await service.handle({ method: 'open', params: { root } });
  return { root, service };
}
let colour = 0;
async function picture(root) {
  const bytes = Buffer.from(encodeRgbaPng({ width: 2, height: 1, pixels: Buffer.from([colour++, 20, 30, 255, 40, 50, 60, 255]) }));
  const file = path.join(root, `source-${colour}.png`);
  await writeFile(file, bytes);
  return { file, bytes, asset: `assets/image-${sha(bytes).slice(0, 24)}.png` };
}
const importImage = async (service, file) => (await service.handle({ method: 'importImage', params: { file, revision: service.editor.revision } })).importedAssetId;
const journal = async root => existsSync(path.join(root, JOURNAL)) ? JSON.parse(await readFile(path.join(root, JOURNAL), 'utf8')).entries : [];

test('discarded import: reopen, close and another project remove the unsaved files', async () => {
  const { root, service } = await fixture();
  const a = await picture(root);
  await importImage(service, a.file);
  assert.ok(existsSync(path.join(root, a.asset)), 'published right away (a save stays atomic)');
  assert.deepEqual((await journal(root)).map(e => [e.path, e.owners]), [[a.asset, [process.pid]]]);
  // Reopening the same project discards the draft.
  await service.handle({ method: 'open', params: { root } });
  assert.ok(!existsSync(path.join(root, a.asset)), 'reopen: gone');
  assert.deepEqual(await journal(root), []);
  // Undo leaves it unreferenced too; closing removes it.
  const b = await picture(root);
  await importImage(service, b.file);
  await service.handle({ method: 'edit', params: { revision: service.editor.revision, edit: { type: 'undo' } } });
  assert.deepEqual(await service.editor.close(), [b.asset]);
  assert.ok(!existsSync(path.join(root, b.asset)));
  // Opening another project releases the first one.
  const c = await picture(root);
  await service.handle({ method: 'open', params: { root } });
  await importImage(service, c.file);
  const other = await fixture();
  await other.service.editor.close();
  await service.handle({ method: 'open', params: { root: other.root } });
  assert.ok(!existsSync(path.join(root, c.asset)), 'switching projects discards the first draft');
});

test('saved imports stay; hand-replaced files and live editors are left alone; crash leftovers go on the next open', async () => {
  const { root, service } = await fixture();
  const kept = await picture(root);
  await importImage(service, kept.file);
  await service.handle({ method: 'save', params: { revision: service.editor.revision } });
  assert.deepEqual(await service.editor.close(), [], 'the saved manifest lists it');
  assert.ok(existsSync(path.join(root, kept.asset)));
  assert.deepEqual(await journal(root), []);

  // Records of other processes: a dead one (crash) is released, a live one (the test runner's
  // parent) keeps its file, and a file whose bytes changed since is never deleted.
  const dead = await picture(root), live = await picture(root), edited = await picture(root);
  for (const p of [dead, live, edited]) await writeFile(path.join(root, p.asset), p.bytes);
  await writeFile(path.join(root, edited.asset), Buffer.concat([edited.bytes, Buffer.from('changed by hand')]));
  await mkdir(path.join(root, '.studio'), { recursive: true });
  await writeFile(path.join(root, JOURNAL), JSON.stringify({ version: 1, entries: [
    { path: dead.asset, sha256: sha(dead.bytes), owners: [2 ** 30] },
    { path: live.asset, sha256: sha(live.bytes), owners: [process.ppid] },
    { path: edited.asset, sha256: sha(edited.bytes), owners: [2 ** 30] },
  ] }));
  const next = new QtStudioService();
  await next.handle({ method: 'open', params: { root } });
  assert.ok(!existsSync(path.join(root, dead.asset)), 'crash leftover removed on open');
  assert.ok(existsSync(path.join(root, live.asset)), 'another live editor still owns it');
  assert.ok(existsSync(path.join(root, edited.asset)), 'changed bytes are not ours to delete');
  assert.deepEqual((await journal(root)).map(e => e.path), [live.asset]);
  // The same picture imported here while the other editor owns it: both own it now.
  await importImage(next, live.file);
  assert.deepEqual((await journal(root))[0].owners.sort(), [process.ppid, process.pid].sort());
  await next.editor.close();
  assert.ok(existsSync(path.join(root, live.asset)), 'still owned by the live editor');
});

test('the Qt service process cleans up when the editor quits without saving (stdin closed)', async () => {
  const { spawn } = await import('node:child_process');
  const { root, service } = await fixture();
  await service.editor.close();
  const p = await picture(root);
  const child = spawn(process.execPath, ['apps/qt-studio/service.mjs'], { stdio: ['pipe', 'pipe', 'inherit'] });
  const replies = [];
  let buffer = '', waiting = null;
  child.stdout.on('data', chunk => { buffer += chunk; for (let i; (i = buffer.indexOf('\n')) >= 0;) { replies.push(JSON.parse(buffer.slice(0, i))); buffer = buffer.slice(i + 1); waiting?.(); } });
  const call = (id, method, params) => new Promise(resolve => {
    waiting = () => { const reply = replies.find(r => r.id === id); if (reply) resolve(reply); };
    child.stdin.write(JSON.stringify({ id, method, params }) + '\n');
  });
  const opened = await call(1, 'open', { root });
  const imported = await call(2, 'importImage', { file: p.file, revision: opened.view.revision });
  assert.equal(imported.error, null);
  assert.ok(existsSync(path.join(root, p.asset)));
  child.stdin.end();
  await new Promise(resolve => child.on('exit', resolve));
  assert.ok(!existsSync(path.join(root, p.asset)), 'quit without save removes the picture');
  assert.deepEqual(await journal(root), []);
});
