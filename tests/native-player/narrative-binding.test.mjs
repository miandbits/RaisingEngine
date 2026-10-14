import test from 'node:test';
import assert from 'node:assert/strict';
import { copyFile, mkdir, mkdtemp, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { spawnSync } from 'node:child_process';

const bundle = process.env.RAISING_NARRATIVE_BUNDLE;
const saved = process.env.RAISING_NARRATIVE_SAVE;
test('authored choice prompt renders in dialogue binding after exact save restoration', {
  skip: !bundle || !saved, timeout: 90000,
}, async () => {
  const packageData = JSON.parse(await readFile(resolve(bundle, 'game.json'), 'utf8'));
  const content = packageData.content ?? packageData;
  const original = JSON.parse(await readFile(saved, 'utf8'));
  const active = original.state.activeNarrative;
  const story = (active.kind === 'event' ? content.events : content.endings).find(x => x.id === active.id);
  const choice = story.nodes.find(x => x.id === active.nodeId);
  assert.equal(choice.type, 'choice');
  assert.ok(choice.prompt.length > 0);
  await mkdir(resolve('.cache/native-narrative-tests'), { recursive: true });
  const root = await mkdtemp(resolve('.cache/native-narrative-tests/choice-'));
  const save = resolve(root, 'save.json'), reportPath = resolve(root, 'report.json');
  await copyFile(saved, save);
  await writeFile(resolve(root, 'steps.json'), JSON.stringify([
    { key: 'F9' }, { waitFrames: 6 }, { checkpoint: 'choice', screenshot: resolve(root, 'choice.png') },
  ]));
  const runtime = resolve(process.env.RAISING_NARRATIVE_RUNTIME ?? 'dist/native-player-app', 'RaisingPlayer.exe');
  const result = spawnSync(runtime, ['--bundle', resolve(bundle), '--save', save,
    '--hidden', '--automation', resolve(root, 'steps.json'), '--report', reportPath],
  { encoding: 'utf8', timeout: 60000, windowsHide: true });
  assert.equal(result.status, 0, result.error?.message ?? result.stderr);
  const report = JSON.parse(await readFile(reportPath, 'utf8'));
  assert.equal(report.ok, true);
  assert.deepEqual(report.final.state, original.state, 'loading/presenting a prompt must not advance simulation');
  assert.ok(Object.values(report.final.texts).some(text => text.includes(choice.prompt)),
    `The dialogue/story binding must show choice.prompt: ${JSON.stringify(report.final.texts)}`);
  assert.ok(choice.choices.every(c => report.final.elements.some(e => e.label === c.text)));
  assert.deepEqual(JSON.parse(await readFile(save, 'utf8')), original, 'inspection must not rewrite save');
  console.log(`Narrative evidence: ${root}`);
});
