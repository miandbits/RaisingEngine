import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { getNarrativeView, inspectEvents } from '../../dist/packages/core/index.js';
import { runScenario, encodeSave, decodeSave } from '../../dist/packages/runtime-service/index.js';
import { content, start, step, send, boundary, reject, scenarios, doc, event, replaceEvents, numeric, literal, temporary } from './helpers.mjs';

test('both existing two-month scenario contracts reach their exact expected ending and values', async () => {
  const snapshot = await content();
  for (const scenario of scenarios) {
    const result = await runScenario(snapshot, scenario);
    assert.equal(result.ok, true, JSON.stringify(result.error));
    const state = result.session.state;
    const certified = scenario.id === 'certified-route';
    assert.equal(state.endingId, certified ? 'certified' : 'ordinary');
    assert.equal(state.phase, 'finished'); assert.equal(state.periodIndex, 5);
    assert.deepEqual(state.attributes, certified ? { skill: 35, money: 22, stamina: 60, stress: 5 } : { skill: 10, money: 61, stamina: 60, stress: 8 });
    assert.deepEqual(state.flags, { registered: certified, certified });
    assert.equal(state.relationships.mentor.trust, certified ? 1 : 0);
    assert.deepEqual(state.completedEvents, certified ? ['exam-invitation', 'final-exam'] : ['exam-invitation']);
    assert.equal(result.assertions.length, certified ? 14 : 9);
    assert.ok(result.assertions.every(a => a.passed));
    assert.equal(result.steps, certified ? 28 : 25);
    assert.equal(state.stateRevision, result.steps);
  }
});

test('last event applies effects and completes before ending selection; ending completes on its own end node', async () => {
  const snapshot = await content(); const states = [];
  const result = await runScenario(snapshot, scenarios.find(s => s.id === 'certified-route'), {
    checkpoint: async session => { states.push(session.state); return { ok: true, session }; },
  });
  assert.equal(result.ok, true);
  const afterEffect = states.find(s => s.activeNarrative?.id === 'final-exam' && s.activeNarrative.nodeId === 'end');
  assert.equal(afterEffect.flags.certified, true); assert.equal(afterEffect.attributes.skill, 35);
  assert.equal(afterEffect.endingId, null); assert.equal(afterEffect.completedEvents.includes('final-exam'), false);
  const endingStart = states.find(s => s.activeNarrative?.kind === 'ending');
  assert.equal(endingStart.endingId, 'certified'); assert.ok(endingStart.completedEvents.includes('final-exam'));
  assert.equal(endingStart.phase, 'narrative'); assert.equal(endingStart.periodIndex, 5);
  assert.equal(states.at(-1).phase, 'finished'); assert.equal(states.at(-1).activeNarrative, null);
});

test('event windows are inclusive; priority then ASCII ID, once per run and at most one per period', async () => {
  const snapshot = await content(p => replaceEvents(p, [
    event('c-high', undefined, { priority: 9 }), event('a-high', undefined, { priority: 9 }),
    event('b-expiring', undefined, { window: { fromPeriod: 0, throughPeriod: 0 } }),
    event('d-future', undefined, { window: { fromPeriod: 1, throughPeriod: 1 } }),
    event('e-blocked', undefined, { requirements: { op: 'flag', flagId: 'registered', equals: true } }),
  ]));
  let session = boundary(snapshot);
  let result = step(snapshot, session, { type: 'resolvePeriodEnd' }); session = result.session;
  assert.equal(session.state.activeNarrative.id, 'a-high'); assert.deepEqual(session.state.completedEvents, []);
  assert.equal(result.transition.eventDecisions.find(d => d.eventId === 'c-high').reason, 'priority');
  assert.equal(result.transition.eventDecisions.find(d => d.eventId === 'd-future').reason, 'before-window');
  session = send(snapshot, session, { type: 'advanceNarrative' });
  assert.equal(session.state.periodIndex, 1); assert.deepEqual(session.state.completedEvents, ['a-high']);
  reject(snapshot, session, { type: 'resolvePeriodEnd' }, 'WRONG_PHASE');
  session = boundary(snapshot, session);
  const decisions = inspectEvents(snapshot, session.state);
  assert.equal(decisions.find(d => d.eventId === 'a-high').reason, 'completed');
  assert.equal(decisions.find(d => d.eventId === 'b-expiring').reason, 'expired');
  assert.equal(decisions.find(d => d.eventId === 'e-blocked').reason, 'requirements');
  assert.equal(decisions.find(d => d.eventId === 'd-future').reason, 'priority');
  session = send(snapshot, session, { type: 'resolvePeriodEnd' });
  assert.equal(session.state.activeNarrative.id, 'c-high');
  session = send(snapshot, session, { type: 'advanceNarrative' });
  session = boundary(snapshot, session);
  session = send(snapshot, session, { type: 'resolvePeriodEnd' });
  assert.equal(session.state.periodIndex, 3); assert.equal(session.state.phase, 'planning');
  assert.deepEqual(session.state.completedEvents, ['a-high', 'c-high']);
});

test('selected event continues when effects change its initial condition, branch reads updated values', async () => {
  for (const initial of [false, true]) {
    const snapshot = await content(p => replaceEvents(p, [event('branch-event', [
      { id: 'effect', type: 'effect', effects: [{ op: 'setFlag', flagId: 'registered', value: true }], next: 'branch' },
      { id: 'branch', type: 'branch', condition: { op: 'flag', flagId: 'certified', equals: true }, then: 'yes', else: 'no' },
      { id: 'yes', type: 'effect', effects: [numeric('skill', 2)], next: 'end' },
      { id: 'no', type: 'effect', effects: [numeric('skill', 1)], next: 'end' }, { id: 'end', type: 'end' },
    ], { requirements: { op: 'flag', flagId: 'registered', equals: false } })]));
    let session = boundary(snapshot, start(snapshot, { initialOverrides: { attributes: {}, flags: { certified: initial }, relationships: {} } }));
    session = send(snapshot, session, { type: 'resolvePeriodEnd' });
    for (let n = 0; n < 4; n++) session = send(snapshot, session, { type: 'advanceNarrative' });
    assert.equal(session.state.attributes.skill, initial ? 2 : 1);
    assert.equal(session.state.flags.registered, true); assert.deepEqual(session.state.completedEvents, ['branch-event']);
  }
});

test('choice availability, cursor and revision prevent disabled or duplicate effects', async () => {
  const snapshot = await content(p => replaceEvents(p, [event('choices', [
    { id: 'choice', type: 'choice', prompt: 'Choose', choices: [
      { id: 'disabled', text: 'No', requirements: { op: 'flag', flagId: 'registered', equals: true }, effects: [numeric('money', 100)], next: 'end' },
      { id: 'enabled', text: 'Yes', requirements: { op: 'always' }, effects: [numeric('money', -5)], next: 'end' },
    ] }, { id: 'end', type: 'end' },
  ])]));
  let session = send(snapshot, boundary(snapshot), { type: 'resolvePeriodEnd' });
  assert.deepEqual(getNarrativeView(snapshot, session.state).choices.map(c => c.enabled), [false, true]);
  reject(snapshot, session, { type: 'choose', choiceId: 'disabled' }, 'CHOICE_DISABLED');
  reject(snapshot, session, { type: 'choose', choiceId: 'missing' }, 'UNKNOWN_CHOICE');
  reject(snapshot, session, { type: 'advanceNarrative' }, 'CHOICE_REQUIRED');
  const revision = session.state.stateRevision;
  const result = step(snapshot, session, { type: 'choose', choiceId: 'enabled' }); session = result.session;
  assert.equal(result.transition.narrative.changes[0].delta, -5);
  assert.equal(session.state.attributes.money, 25); assert.equal(session.state.activeNarrative.nodeId, 'end');
  reject(snapshot, session, { type: 'choose', choiceId: 'enabled' }, 'REVISION_CONFLICT', revision);
  reject(snapshot, session, { type: 'choose', choiceId: 'enabled' }, 'WRONG_PHASE');
});

test('all-disabled choice is a structured content error and rolls back effect plus cursor', async () => {
  const snapshot = await content(p => replaceEvents(p, [event('dead-choice', [
    { id: 'effect', type: 'effect', effects: [numeric('money', 10)], next: 'choice' },
    { id: 'choice', type: 'choice', prompt: 'Unavailable', choices: [
      { id: 'blocked', text: 'Blocked', requirements: { op: 'flag', flagId: 'registered', equals: true }, effects: [], next: 'end' },
    ] }, { id: 'end', type: 'end' },
  ])]));
  const session = send(snapshot, boundary(snapshot), { type: 'resolvePeriodEnd' });
  const error = reject(snapshot, session, { type: 'advanceNarrative' }, 'NO_AVAILABLE_CHOICE');
  assert.match(error.message, /blocked/); assert.equal(session.state.attributes.money, 30);
  assert.equal(session.state.activeNarrative.nodeId, 'effect');
});

test('narrative effect overflow rejects the complete node transaction', async () => {
  const snapshot = await content(p => replaceEvents(p, [event('overflow', [
    { id: 'effect', type: 'effect', effects: [numeric('money', -5), numeric('skill', { op: 'multiply', left: literal(2147483647), right: literal(2) })], next: 'end' },
    { id: 'end', type: 'end' },
  ])]));
  const session = send(snapshot, boundary(snapshot), { type: 'resolvePeriodEnd' });
  reject(snapshot, session, { type: 'advanceNarrative' }, 'INTEGER_OVERFLOW');
  assert.equal(session.state.attributes.money, 30); assert.equal(session.state.activeNarrative.nodeId, 'effect');
});

test('scene fields retain/clear/replace correctly and sound cues do not replay on load', async t => {
  const root = await temporary(t);
  await mkdir(path.join(root, 'assets'));
  await writeFile(path.join(root, 'assets/image.png'), 'fixture image');
  await writeFile(path.join(root, 'assets/audio.wav'), 'fixture audio');
  const snapshot = await content(p => {
    p.root = root;
    doc(p, 'assets').assets = [{ id: 'image', kind: 'image', path: 'assets/image.png' }, { id: 'audio', kind: 'audio', path: 'assets/audio.wav' }];
    doc(p, 'character', 'mentor').portraits = { neutral: 'image' };
    doc(p, 'activity', 'rest').presentation = { backgroundAssetId: 'image', soundAssetId: 'audio' };
    replaceEvents(p, [event('scenes', [
      { id: 'scene', type: 'scene', scene: { musicAssetId: 'audio', portraits: [{ characterId: 'mentor', portraitId: 'neutral', position: 'left' }], soundAssetId: 'audio' }, next: 'say' },
      { id: 'say', type: 'dialogue', speakerId: null, text: 'Hello', next: 'clear' },
      { id: 'clear', type: 'scene', scene: { musicAssetId: null, backgroundAssetId: null, portraits: [] }, next: 'end' },
      { id: 'end', type: 'end' },
    ])]);
  });
  let session = send(snapshot, start(snapshot), { type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'] });
  let result = step(snapshot, session, { type: 'executeActivity' }); session = result.session;
  assert.deepEqual(result.transition.soundCues, ['audio']);
  session = send(snapshot, session, { type: 'acknowledgeResult' });
  session = send(snapshot, session, { type: 'resolvePeriodEnd' });
  result = step(snapshot, session, { type: 'advanceNarrative' }); session = result.session;
  assert.deepEqual(result.transition.soundCues, ['audio']);
  assert.equal(session.state.presentation.backgroundAssetId, 'image');
  const encoded = await encodeSave(snapshot, session); assert.equal(encoded.ok, true);
  const restored = await decodeSave(snapshot, encoded.text); assert.equal(restored.ok, true);
  assert.deepEqual(restored.session.state.presentation, session.state.presentation);
  assert.equal(getNarrativeView(snapshot, restored.session.state).node.type, 'dialogue');
  result = step(snapshot, restored.session, { type: 'advanceNarrative' });
  assert.deepEqual(result.transition.soundCues, []);
  result = step(snapshot, result.session, { type: 'advanceNarrative' });
  assert.deepEqual(result.session.state.presentation, { backgroundAssetId: null, musicAssetId: null, portraits: [] });
});

test('ending priority ties use ID, ending choice is supported by core, and selection is not reconsidered', async () => {
  const snapshot = await content(p => {
    replaceEvents(p, []);
    const candidate = doc(p, 'ending', 'certified'); candidate.requirements = { op: 'always' };
    const alpha = { ...structuredClone(candidate), id: 'alpha', entryNodeId: 'choose', nodes: [
      { id: 'choose', type: 'choice', prompt: 'End?', choices: [{ id: 'finish', text: 'Finish', requirements: { op: 'always' }, effects: [numeric('skill', -100)], next: 'end' }] },
      { id: 'end', type: 'end' },
    ] };
    alpha.requirements = { op: 'compare', comparator: 'gte', left: { op: 'read', target: { kind: 'attribute', attributeId: 'skill' } }, right: literal(5) };
    p.documents.push({ kind: 'ending', file: 'endings/alpha.json', data: alpha, version: '', locations: new Map() });
  });
  let session = start(snapshot, { initialOverrides: { attributes: { skill: 5 }, flags: {}, relationships: {} } });
  for (let period = 0; period < 6; period++) session = send(snapshot, boundary(snapshot, session), { type: 'resolvePeriodEnd' });
  assert.equal(session.state.endingId, 'alpha');
  assert.equal(session.state.phase, 'narrative');
  session = send(snapshot, session, { type: 'choose', choiceId: 'finish' });
  session = send(snapshot, session, { type: 'advanceNarrative' });
  assert.equal(session.state.attributes.skill, 0); assert.equal(session.state.endingId, 'alpha');
  assert.equal(session.state.phase, 'finished');
  reject(snapshot, session, { type: 'advanceNarrative' }, 'WRONG_PHASE');
});

test('full mode cannot silently skip events with the stage3-only completion command', async () => {
  const snapshot = await content();
  reject(snapshot, boundary(snapshot), { type: 'completePeriodWithoutEvents' }, 'WRONG_PHASE');
  reject(snapshot, boundary(snapshot, start(snapshot, { mode: 'activities-only' })), { type: 'resolvePeriodEnd' }, 'WRONG_PHASE');
});
