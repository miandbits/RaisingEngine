import test from 'node:test';
import assert from 'node:assert/strict';
import { cp, mkdir, mkdtemp, readFile, writeFile } from 'node:fs/promises';
import { resolve, join } from 'node:path';
import { spawnSync } from 'node:child_process';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { exportGame } from '../../dist/packages/export-service/export.js';
import { desktopTarget, gameDirectory, playerExecutable } from '../../dist/packages/export-service/targets.js';
import { firstGameRoute } from '../qt-studio/first-game-route.mjs';

test('a choice autosaves after automatic effect nodes; following dialogue preserves that checkpoint', { timeout: 120000 }, async () => {
  await mkdir('.cache/choice-autosave', { recursive: true });
  const out = await mkdtemp(resolve('.cache/choice-autosave/run-')), projectRoot = join(out, 'project');
  await cp(resolve('examples/my-first-raising-game'), projectRoot, { recursive: true });
  const projectFile = join(projectRoot, 'project.json'), project = JSON.parse(await readFile(projectFile, 'utf8'));
  project.visual.saveSystem = { manualSlots: 6, autoSlots: 3 };
  await writeFile(projectFile, JSON.stringify(project));
  const eventFile = join(projectRoot, 'events/garden-invitation.json'), event = JSON.parse(await readFile(eventFile, 'utf8'));
  event.nodes.find(node => node.id === 'invitation').choices.find(choice => choice.id === 'accept').next = 'automatic-effect';
  event.nodes.find(node => node.id === 'accepted').next = 'continued';
  event.nodes.push({ id: 'automatic-effect', type: 'effect', effects: [{ op: 'add', target: { kind: 'attribute', attributeId: 'stamina' }, value: { op: 'literal', value: 1 } }], next: 'accepted' },
    { id: 'continued', type: 'dialogue', speakerId: 'mira', text: 'We can talk about the details tomorrow.', next: 'end' });
  await writeFile(eventFile, JSON.stringify(event));
  const built = await buildSnapshot(await loadProject(projectRoot));
  assert.equal(built.ok, true, JSON.stringify(built.diagnostics));
  const route = firstGameRoute(built.snapshot, true), choiceIndex = route.steps.findIndex(step => step.click === 'choose-accept');
  assert.ok(choiceIndex > 0);
  const target = desktopTarget(), destination = join(out, 'game');
  const runtime = resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME ?? process.env.RAISING_NATIVE_PLAYER_RUNTIME ?? 'dist/native-player-app');
  await exportGame({ projectRoot, content: built.snapshot, target, destination, runtimes: { windows: runtime, macos: runtime, web: runtime } });
  const prefix = route.steps.slice(0, choiceIndex), results = [];
  for (const [name, extra] of [['before', []], ['choice', [{ click: 'choose-accept' }]], ['dialogue', [{ click: 'choose-accept' }, { click: 'advance' }]]]) {
    const dir = join(out, name); await mkdir(dir);
    const steps = join(dir, 'steps.json'), reportFile = join(dir, 'report.json');
    await writeFile(steps, JSON.stringify([...prefix, ...extra, { waitFrames: 4 }, { checkpoint: 'settled' }]));
    const child = spawnSync(playerExecutable(target, destination), ['--bundle', gameDirectory(target, destination), '--save', join(dir, 'save.json'), '--automation', steps, '--report', reportFile, '--hidden'], { encoding: 'utf8', windowsHide: true, timeout: 30000 });
    assert.equal(child.status, 0, child.error?.message ?? child.stderr);
    const report = JSON.parse(await readFile(reportFile, 'utf8')); assert.equal(report.ok, true);
    const records = await Promise.all([1, 2, 3].map(async i => JSON.parse(await readFile(join(dir, 'save.slots', `auto-${i}.json`), 'utf8'))));
    records.sort((a, b) => b.playerPresentation.saveRecord.sequence - a.playerPresentation.saveRecord.sequence);
    results.push({ ui: report.final, record: records[0] });
  }
  const [before, choice, dialogue] = results;
  assert.equal(choice.ui.commands.at(-1).type, 'advanceNarrative', 'Automatic effect settles after the submitted choice');
  assert.equal(choice.ui.state.phase, 'narrative');
  assert.equal(choice.record.playerPresentation.saveRecord.sequence, before.record.playerPresentation.saveRecord.sequence + 1, 'Exactly one checkpoint is added for the choice');
  assert.deepEqual(choice.record.state, choice.ui.state, 'The checkpoint includes the choice and automatic effect');
  assert.equal(dialogue.record.playerPresentation.saveRecord.sequence, choice.record.playerPresentation.saveRecord.sequence, 'Ordinary following dialogue must not rotate the checkpoint');
  assert.deepEqual(dialogue.record.state, choice.ui.state);
  assert.notDeepEqual(dialogue.ui.state, choice.ui.state, 'The following dialogue actually advanced');
  await writeFile(resolve('.cache/choice-autosave/latest.json'), JSON.stringify({ out, target, sequences: results.map(result => result.record.playerPresentation.saveRecord.sequence) }, null, 2));
});
