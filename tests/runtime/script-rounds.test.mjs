import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { content, doc, replaceEvents } from './helpers.mjs';
import { createSession, dispatchSession } from '../../dist/packages/runtime-service/session.js';
import { encodeSave, decodeSave } from '../../dist/packages/runtime-service/save.js';
import { createRun, dispatchCommand } from '../../dist/packages/core/run.js';
import { getNarrativeView } from '../../dist/packages/core/index.js';
import { nativeRequest } from '../../dist/packages/native-bridge/index.js';

// Emitted by the actual Qt compiler and compared byte-for-structure in its test.
// All values are synthetic authoring fixtures, not original-game formulas.
const authored = JSON.parse(await readFile(new URL('../fixtures/script-rounds.json', import.meta.url), 'utf8'));
const must = r => { assert.equal(r.ok, true, JSON.stringify(r)); return r; };

for (const scenario of [
  { name: 'win', skill: 10, money: 30, result: 'Won', delta: 57 },
  { name: 'fail second round', skill: 2, money: 30, result: 'Lost', delta: -1 },
  { name: 'withdraw after first reward', skill: 10, money: 30, result: 'Withdrew', delta: -4, withdraw: true },
  { name: 'unqualified admission', skill: 10, money: 5, result: 'Withdrew', delta: 0, blocked: true },
]) test(`Qt-authored rounds: ${scenario.name}, TS/native parity and exact save/replay at every cursor`, async () => {
  const c = await content(p => {
    replaceEvents(p, [structuredClone(authored)]);
    doc(p, 'project').initialState.attributes = { money: scenario.money, skill: scenario.skill };
  });
  let session = must(createSession(c, 913)).session;
  assert.deepEqual(nativeRequest({ operation: 'create', content: c, seed: 913, options: { mode: 'full' } }), createRun(c, 913, { mode: 'full' }));
  const visited = [], commands = [];
  async function send(command) {
    const previous = session;
    const expected = dispatchCommand(c, previous.state, command, previous.state.stateRevision);
    assert.deepEqual(nativeRequest({ operation: 'command', content: c, state: previous.state, command, expectedRevision: previous.state.stateRevision }), expected);
    session = must(dispatchSession(c, previous, command, previous.state.stateRevision)).session;
    assert.deepEqual(session.state, must(expected).state);
    commands.push(command);
    const saved = must(await encodeSave(c, session));
    const restored = must(await decodeSave(c, saved.text)).session;
    assert.deepEqual(restored, session);
    assert.equal(must(await encodeSave(c, restored)).text, saved.text);
    session = restored;
    if (session.state.activeNarrative) visited.push(session.state.activeNarrative.nodeId);
  }
  for (const command of [{ type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'] }, { type: 'executeActivity' }, { type: 'acknowledgeResult' }, { type: 'resolvePeriodEnd' }]) await send(command);
  assert.equal(session.state.activeNarrative.nodeId, 'rounds-1-entry');
  const beforeMoney = session.state.attributes.money;
  if (scenario.blocked) {
    assert.deepEqual(getNarrativeView(c, session.state).choices.map(v => v.enabled), [false, true]);
    const bytes = JSON.stringify(session);
    const rejected = dispatchSession(c, session, { type: 'choose', choiceId: 'enter' }, session.state.stateRevision);
    assert.equal(rejected.ok, false); assert.equal(rejected.error.code, 'CHOICE_DISABLED'); assert.equal(JSON.stringify(session), bytes);
  }
  let seenResult = false, continued = false;
  for (let n = 0; session.state.activeNarrative && n < 40; n++) {
    const view = getNarrativeView(c, session.state), node = view.node;
    if (node.type === 'dialogue' && node.text === scenario.result) seenResult = true;
    if (node.id === 'original') continued = true;
    if (node.type === 'choice') {
      const leave = scenario.blocked || (scenario.withdraw && node.id === 'rounds-1-r2-choice');
      const choiceId = leave ? 'leave' : node.id === 'rounds-1-entry' ? 'enter' : 'attempt';
      await send({ type: 'choose', choiceId });
    } else await send({ type: 'advanceNarrative' });
  }
  assert.equal(session.state.activeNarrative, null);
  assert.equal(seenResult, true); assert.equal(continued, true);
  assert.equal(session.state.attributes.money, beforeMoney + scenario.delta);
  assert.deepEqual(session.state.completedEvents, ['rounds-test']);
  assert.equal(session.state.periodIndex, 1);
  if (!scenario.blocked) assert.ok(visited.includes('rounds-1-r1-result'));
  if (scenario.name === 'win') assert.ok(visited.includes('rounds-1-r2-result'));
  assert.ok(commands.length < 40);
});
