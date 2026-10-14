import assert from 'node:assert/strict';
import { readFile, readdir } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import test from 'node:test';
import Ajv2020 from 'ajv/dist/2020.js';

// Development-only contract tests. This is not the product loader or simulator.
const root = fileURLToPath(new URL('../../', import.meta.url));
const schemaDir = path.join(root, 'packages/schema');
const sampleDir = path.join(root, 'examples/academy-demo');
const namespace = 'https://raising-engine.example/schemas/0.1.0/';
const readJson = async (file) => JSON.parse(await readFile(file, 'utf8'));
const ajv = new Ajv2020({ strict: true, allErrors: true, validateSchema: true });
const schemaFiles = (await readdir(schemaDir)).filter((name) => name.endsWith('.schema.json')).sort();
for (const name of schemaFiles) ajv.addSchema(await readJson(path.join(schemaDir, name)));
const validators = new Map(schemaFiles.map((name) => [name, ajv.getSchema(namespace + name)]));

const mapping = new Map([
  ['project.json', 'project.schema.json'],
  ['attributes.json', 'attributes.schema.json'],
  ['flags.json', 'flags.schema.json'],
  ['assets/manifest.json', 'assets.schema.json'],
]);
for (const [folder, schema] of [
  ['characters', 'character'], ['activities', 'activity'], ['events', 'event'],
  ['endings', 'ending'], ['scenarios', 'scenario'],
]) {
  for (const name of (await readdir(path.join(sampleDir, folder))).filter((name) => name.endsWith('.json')).sort()) {
    mapping.set(`${folder}/${name}`, `${schema}.schema.json`);
  }
}
const documents = new Map(await Promise.all([...mapping.keys()].map(async (name) => [name, await readJson(path.join(sampleDir, name))])));

test('all fifteen schemas compile strictly using local references', () => {
  assert.equal(validators.size, 15);
  assert.ok(validators.has('extension.schema.json'));
  assert.ok(validators.has('plugin-lock.schema.json'));
  assert.ok(validators.has('gameplay.schema.json'));
  assert.ok(validators.has('systems.schema.json'));
  for (const [name, validate] of validators) assert.equal(typeof validate, 'function', name);
});

for (const [file, schema] of mapping) {
  test(`sample structure: ${file}`, () => {
    const data = documents.get(file);
    const before = JSON.stringify(data);
    const validate = validators.get(schema);
    assert.ok(validate(data), `${file}: ${ajv.errorsText(validate.errors)}`);
    assert.equal(JSON.stringify(data), before, 'validation must not mutate content');
    const localSchema = path.resolve(sampleDir, path.dirname(file), data.$schema);
    assert.equal(localSchema, path.join(schemaDir, schema), 'editor schema hint must resolve locally');
  });
}

const mutateAndReject = (name, file, mutate) => test(name, () => {
  const data = structuredClone(documents.get(file));
  mutate(data);
  const validate = validators.get(mapping.get(file));
  assert.equal(validate(data), false, `unexpected acceptance: ${JSON.stringify(data)}`);
});

mutateAndReject('reject unsupported format versions', 'project.json', (d) => { d.formatVersion = '0.2.0'; });
mutateAndReject('reject unknown project fields', 'project.json', (d) => { d.duratonMonths = 2; });
mutateAndReject('reject calendars above twelve periods', 'project.json', (d) => { d.periodsPerMonth = 13; });
mutateAndReject('reject zero-period calendars', 'project.json', (d) => { d.periodsPerMonth = 0; });
test('accept configurable four-week continuous calendars',()=>{
  const data=structuredClone(documents.get('project.json'));data.periodsPerMonth=4;data.schedulePlayback='continuous';
  assert.equal(validators.get('project.schema.json')(data),true);
});
mutateAndReject('reject zero-month games', 'project.json', (d) => { d.durationMonths = 0; });
mutateAndReject('reject omitted required state maps', 'project.json', (d) => { delete d.initialState.flags; });
mutateAndReject('reject fractional attributes', 'attributes.json', (d) => { d.attributes[0].initial = 0.5; });
mutateAndReject('reject numeric strings without coercion', 'attributes.json', (d) => { d.attributes[0].initial = '10'; });
mutateAndReject('reject attribute definition typos', 'attributes.json', (d) => { d.attributes[0].inital = 2; });
mutateAndReject('reject values outside signed 32-bit range', 'attributes.json', (d) => { d.attributes[0].max = 2147483648; });
mutateAndReject('reject non-boolean flags', 'flags.json', (d) => { d.flags[0].initial = 1; });
mutateAndReject('reject malformed stable IDs', 'characters/mentor.json', (d) => { d.id = 'Mentor Name'; });
mutateAndReject('reject unknown relationship fields', 'characters/mentor.json', (d) => { d.relationships[0].secret = 1; });
mutateAndReject('reject negative activity costs', 'activities/study.json', (d) => { d.costs[0].amount = -5; });
mutateAndReject('reject incomplete success check groups', 'activities/study.json', (d) => { d.successCheck = { chance: { op: 'literal', value: 5000 } }; });
mutateAndReject('reject orphan success effects', 'activities/study.json', (d) => { d.successEffects = []; });
mutateAndReject('reject unknown expression fields', 'activities/study.json', (d) => { d.baseEffects[0].value.code = 'eval'; });
mutateAndReject('reject empty logical groups', 'activities/study.json', (d) => { d.requirements = { op: 'all', conditions: [] }; });
mutateAndReject('reject arbitrary code conditions', 'activities/study.json', (d) => { d.requirements = { op: 'javascript', source: 'true' }; });
mutateAndReject('reject unknown effect operators', 'activities/study.json', (d) => { d.baseEffects[0].op = 'teleport'; });
mutateAndReject('reject invalid comparison operators', 'events/exam-invitation.json', (d) => { d.requirements.comparator = '>='; });
mutateAndReject('reject negative event periods', 'events/exam-invitation.json', (d) => { d.window.fromPeriod = -1; });
mutateAndReject('reject out-of-range priorities', 'events/exam-invitation.json', (d) => { d.priority = 101; });
mutateAndReject('reject unknown dialogue fields', 'events/exam-invitation.json', (d) => { d.nodes[0].script = 'anything'; });
mutateAndReject('reject missing node destinations', 'events/exam-invitation.json', (d) => { delete d.nodes[0].next; });
mutateAndReject('reject empty choice lists', 'events/exam-invitation.json', (d) => { d.nodes[1].choices = []; });
mutateAndReject('reject links on terminal nodes', 'endings/ordinary.json', (d) => { d.nodes[1].next = 'intro'; });
mutateAndReject('reject path traversal assets', 'assets/manifest.json', (d) => { d.assets = [{ id: 'bad', kind: 'image', path: 'assets/../secret.png' }]; });
mutateAndReject('reject remote asset URLs', 'assets/manifest.json', (d) => { d.assets = [{ id: 'bad', kind: 'image', path: 'https://example.com/a.png' }]; });
mutateAndReject('reject unknown asset fields', 'assets/manifest.json', (d) => { d.assets = [{ id: 'bad', kind: 'image', path: 'assets/a.png', script: true }]; });
mutateAndReject('reject empty schedules', 'scenarios/certified-route.json', (d) => { d.commands[0].activityIds=[]; });
mutateAndReject('reject schedules above twelve slots', 'scenarios/certified-route.json', (d) => { d.commands[0].activityIds=Array(13).fill('study'); });
mutateAndReject('reject unbounded scenario requests', 'scenarios/certified-route.json', (d) => { d.stepLimit = 0; });
mutateAndReject('reject undocumented scenario commands', 'scenarios/certified-route.json', (d) => { d.commands[0].type = 'runScript'; });

test('success-check, recursive expressions, scene and branch vocabulary are accepted', () => {
  const d = structuredClone(documents.get('activities/study.json'));
  d.successCheck = { chance: { op: 'clamp', value: { op: 'multiply', left: { op: 'read', target: { kind: 'attribute', attributeId: 'skill' } }, right: { op: 'literal', value: 100 } }, min: 0, max: 10000 } };
  d.successEffects = [];
  d.failureEffects = [];
  d.presentation = { backgroundAssetId: null, musicAssetId: null, portraits: [] };
  const activity = validators.get('activity.schema.json');
  assert.ok(activity(d), ajv.errorsText(activity.errors));
  const event = structuredClone(documents.get('events/final-exam.json'));
  event.entryNodeId = 'scene';
  event.nodes.push({ id: 'scene', type: 'scene', scene: { portraits: [] }, next: 'branch' });
  event.nodes.push({ id: 'branch', type: 'branch', condition: { op: 'not', condition: { op: 'any', conditions: [{ op: 'flag', flagId: 'certified', equals: true }] } }, then: 'intro', else: 'end' });
  const validate = validators.get('event.schema.json');
  assert.ok(validate(event), ajv.errorsText(validate.errors));
});

test('schema deliberately leaves cross-field semantics for milestone two', () => {
  const d = structuredClone(documents.get('attributes.json'));
  d.attributes[0].min = 100;
  d.attributes[0].max = 0;
  assert.equal(validators.get('attributes.schema.json')(d), true);
  const event = structuredClone(documents.get('events/final-exam.json'));
  event.entryNodeId = 'missing-node';
  assert.equal(validators.get('event.schema.json')(event), true);
});

const byFolder = (folder) => [...documents].filter(([name]) => name.startsWith(`${folder}/`)).map(([, data]) => data);
const index = (items, label) => {
  const result = new Map(items.map((item) => [item.id, item]));
  assert.equal(result.size, items.length, `duplicate IDs: ${label}`);
  return result;
};

test('minimal sample references, ranges and narrative graphs are internally consistent', () => {
  const project = documents.get('project.json');
  const attributes = index(documents.get('attributes.json').attributes, 'attributes');
  const flags = index(documents.get('flags.json').flags, 'flags');
  const characters = index(byFolder('characters'), 'characters');
  const activities = index(byFolder('activities'), 'activities');
  const events = index(byFolder('events'), 'events');
  const endings = index(byFolder('endings'), 'endings');
  assert.equal(activities.size, 3);
  assert.equal(events.size, 2);
  assert.equal(endings.size, 2);
  assert.equal(project.durationMonths, 2);
  assert.ok(characters.has(project.protagonistId));
  assert.deepEqual(endings.get(project.defaultEndingId).requirements, { op: 'always' });
  assert.equal(endings.get(project.defaultEndingId).priority, 0);
  for (const item of [...attributes.values(), ...[...characters.values()].flatMap((c) => c.relationships)]) {
    assert.ok(item.min <= item.initial && item.initial <= item.max, item.id);
  }
  for (const character of characters.values()) index(character.relationships, character.id);
  for (const activity of activities.values()) {
    assert.equal(new Set(activity.costs.map((c) => c.attributeId)).size, activity.costs.length);
    for (const cost of activity.costs) assert.ok(['resource', 'condition'].includes(attributes.get(cost.attributeId)?.kind));
  }
  const walk = (value) => {
    if (!value || typeof value !== 'object') return;
    if ('attributeId' in value) assert.ok(attributes.has(value.attributeId), value.attributeId);
    if ('flagId' in value) assert.ok(flags.has(value.flagId), value.flagId);
    if ('characterId' in value) assert.ok(characters.has(value.characterId), value.characterId);
    if ('relationshipId' in value) assert.ok(characters.get(value.characterId).relationships.some((r) => r.id === value.relationshipId));
    if (value.speakerId !== undefined && value.speakerId !== null) assert.ok(characters.has(value.speakerId), value.speakerId);
    if ('eventId' in value) assert.ok(events.has(value.eventId), value.eventId);
    if ('endingId' in value) assert.ok(endings.has(value.endingId), value.endingId);
    for (const child of Object.values(value)) walk(child);
  };
  for (const [file, document] of documents) {
    walk(document);
    if (document.id) assert.equal(path.basename(file), `${document.id}.json`);
  }
  for (const event of events.values()) assert.ok(event.window.fromPeriod <= event.window.throughPeriod && event.window.throughPeriod < project.durationMonths * 3);
  for (const narrative of [...events.values(), ...endings.values()]) {
    const nodes = index(narrative.nodes, narrative.id);
    const visited = new Set();
    const visit = (id, active = new Set()) => {
      assert.ok(nodes.has(id), `${narrative.id}: missing ${id}`);
      assert.ok(!active.has(id), `${narrative.id}: cycle at ${id}`);
      if (visited.has(id)) return;
      active.add(id);
      const node = nodes.get(id);
      if (node.type === 'choice') index(node.choices, node.id);
      const next = node.type === 'choice' ? node.choices.map((choice) => choice.next)
        : node.type === 'branch' ? [node.then, node.else]
          : node.type === 'end' ? [] : [node.next];
      for (const target of next) visit(target, active);
      active.delete(id);
      visited.add(id);
    };
    visit(narrative.entryNodeId);
    assert.equal(visited.size, nodes.size, `${narrative.id}: unreachable nodes`);
  }
  for (const scenario of byFolder('scenarios')) {
    assert.equal(scenario.commands.filter((c) => c.type === 'planMonth').length, 2);
    assert.equal(scenario.commands.filter((c) => c.type === 'runPeriod').length, 6);
    for (const command of scenario.commands) {
      for (const id of command.activityIds ?? []) assert.ok(activities.has(id), id);
      for (const selection of command.choices ?? []) {
        const node = events.get(selection.eventId).nodes.find((n) => n.id === selection.nodeId);
        assert.equal(node?.type, 'choice');
        assert.ok(node.choices.some((choice) => choice.id === selection.choiceId));
      }
    }
  }
});
