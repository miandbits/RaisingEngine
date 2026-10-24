import test from 'node:test';
import assert from 'node:assert/strict';
import { cp, mkdir, mkdtemp, readFile, writeFile, access, realpath } from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import { QtStudioService } from '../../apps/qt-studio/service.mjs';

async function fixture(features = ['renderer-preview-v1']) {
  await mkdir('.cache/renderer-preview-tests', { recursive: true });
  const work = await mkdtemp(path.resolve('.cache/renderer-preview-tests/run-'));
  const root = path.join(work, 'project'), runtime = path.join(work, 'runtime');
  await cp('examples/academy-visual', root, { recursive: true });
  await mkdir(runtime);
  // Export contract fixture only: native execution is verified by the Qt/player integration tests.
  await writeFile(path.join(runtime, 'RaisingPlayer.exe'), 'export-contract-fixture');
  await writeFile(path.join(runtime, 'capabilities.json'), JSON.stringify({ formatVersion: '1',
    imageExtensions: ['.png', '.jpg', '.jpeg', '.webp'], audioExtensions: ['.wav', '.ogg', '.mp3'],
    fontExtensions: ['.ttf', '.otf'], visualFeatures: features }));
  const service = new QtStudioService({ nativeTarget:'windows', runtimes: { windows: runtime, web: runtime } });
  await service.handle({ method: 'open', params: { root } });
  const doc = service.editor.documents.find(d => d.kind === 'project');
  const replaceText = text => {
    const data = structuredClone(service.editor.documents.find(d => d.kind === 'project').data);
    data.visual.screens.find(s => s.id === 'title').elements.find(e => e.id === 'start').text = text;
    service.editor.edit({ type: 'replace', file: doc.file, data }, service.editor.revision);
  };
  return { service, root, doc, replaceText };
}

test('renderer preview exports the unsaved draft without touching disk, revision or undo/redo', async () => {
  const { service, root, replaceText } = await fixture();
  const editor = service.editor;
  const originalFiles = new Map(await Promise.all(editor.documents.map(async d => [d.file, await readFile(path.join(root, d.file))])));
  replaceText('Unsaved preview text');
  replaceText('Redo must survive preview');
  editor.edit({ type: 'undo' }, editor.revision);
  const before = editor.view();
  assert.equal(before.dirty, true); assert.equal(before.undo, true); assert.equal(before.redo, true);
  const result = await service.handle({ method: 'preview', params: { revision: before.revision } });
  const after = result.view;
  for (const field of ['revision', 'dirty', 'undo', 'redo', 'draftVersion', 'diskVersion'])
    assert.deepEqual(after[field], before[field], field);
  assert.deepEqual(after.documents, before.documents);
  for (const [file, bytes] of originalFiles) assert.deepEqual(await readFile(path.join(root, file)), bytes, file);
  assert.equal(result.play.preview, true); assert.equal(result.play.revision, before.revision);
  assert.ok(result.play.directory.startsWith((await realpath(path.join(process.env.RAISING_STUDIO_CACHE||path.join(os.tmpdir(),'RaisingStudio'),'qt-render-preview')))+path.sep));
  assert.equal(path.dirname(result.play.save), path.dirname(result.play.directory));
  assert.notEqual(path.dirname(result.play.save), root);
  await assert.rejects(access(result.play.save), { code: 'ENOENT' });
  const bundle = JSON.parse(await readFile(path.join(result.play.directory, 'game.json'), 'utf8'));
  assert.equal(bundle.content.project.visual.screens.find(s => s.id === 'title').elements.find(e => e.id === 'start').text, 'Unsaved preview text');
  assert.equal(result.play.fingerprint, bundle.content.contentFingerprint);
  for (const asset of Object.values(bundle.assets)) await access(path.join(result.play.directory, asset));
  editor.edit({ type: 'redo' }, editor.revision);
  assert.equal(editor.documents.find(d => d.kind === 'project').data.visual.screens.find(s => s.id === 'title').elements.find(e => e.id === 'start').text, 'Redo must survive preview');
});

test('renderer preview rejects stale revisions and external disk changes before export', async () => {
  const { service, root, doc, replaceText } = await fixture();
  const stale = service.editor.revision; replaceText('Draft retained');
  await assert.rejects(service.handle({ method: 'preview', params: { revision: stale } }), /갱신/);
  const external = structuredClone(doc.data); external.title += ' external';
  await writeFile(path.join(root, doc.file), JSON.stringify(external, null, 2));
  const before = service.editor.view();
  await assert.rejects(service.handle({ method: 'preview', params: { revision: before.revision } }));
  assert.deepEqual(service.editor.view().documents, before.documents);
  assert.equal(service.editor.revision, before.revision);
  assert.deepEqual(JSON.parse(await readFile(path.join(root, doc.file), 'utf8')), external);
});

test('renderer preview rechecks disk and revision after export before returning a launch descriptor', async t => {
  for (const change of ['disk', 'revision']) await t.test(change, async () => {
    const { service, root, doc, replaceText } = await fixture();
    replaceText('Snapshot');
    const revision = service.editor.revision;
    const check = service.editor.assertDiskUnchanged.bind(service.editor);
    let checks = 0;
    service.editor.assertDiskUnchanged = async () => {
      if (++checks === 2) {
        if (change === 'disk') {
          const data = structuredClone(doc.data); data.title += ' concurrent edit';
          await writeFile(path.join(root, doc.file), JSON.stringify(data));
        } else replaceText('Concurrent editor change');
      }
      await check();
    };
    await assert.rejects(service.handle({ method: 'preview', params: { revision } }));
    assert.equal(checks, 2);
    assert.equal(service.editor.view().dirty, true);
  });
});

test('renderer preview rejects invalid content and old runtimes that ignore preview arguments', async () => {
  const old = await fixture([]);
  await assert.rejects(old.service.handle({ method: 'preview', params: { revision: old.service.editor.revision } }), /렌더러 미리보기/);
  const { service, doc } = await fixture();
  const data = structuredClone(doc.data);
  data.visual.screens.find(s => s.id === 'title').elements.find(e => e.id === 'start').action = { type: 'navigate', screenId: 'missing-screen' };
  service.editor.edit({ type: 'replace', file: doc.file, data }, service.editor.revision);
  const revision = service.editor.revision;
  await assert.rejects(service.handle({ method: 'preview', params: { revision } }), /문제 패널/);
  assert.equal(service.editor.revision, revision);
  assert.equal(service.editor.view().dirty, true);
});
