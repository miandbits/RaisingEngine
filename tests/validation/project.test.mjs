import assert from 'node:assert/strict';
import { cp, mkdir, mkdtemp, readFile, writeFile, rm, rename, symlink } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';
import test from 'node:test';
import { checkProject, loadProject, validateProject } from '../../dist/packages/project-service/index.js';
import { parseDocument } from '../../dist/packages/project-service/json-document.js';

const root = fileURLToPath(new URL('../../', import.meta.url));
const sample = path.join(root, 'examples/academy-demo');
const cache = path.join(root, '.cache/validation-tests');
await mkdir(cache, { recursive: true });
const fixture = async (t) => {
  const directory = await mkdtemp(path.join(cache, 'case-'));
  await cp(sample, directory, { recursive: true });
  t.after(async () => {
    const relative = path.relative(cache, path.resolve(directory));
    assert.ok(relative && !relative.startsWith('..') && !path.isAbsolute(relative));
    await rm(directory, { recursive: true, force: true });
  });
  return directory;
};
const edit = async (directory, file, change) => {
  const full = path.join(directory, file);
  const data = JSON.parse(await readFile(full, 'utf8'));
  change(data);
  await writeFile(full, JSON.stringify(data, null, 2) + '\n');
};
const findIssue = (result, code, file, pointer) => {
  const issue = result.diagnostics.find((d) => d.code === code && (!file || d.file === file) && (!pointer || d.pointer === pointer));
  assert.ok(issue, `${code} ${file ?? ''} ${pointer ?? ''}: ${JSON.stringify(result.diagnostics)}`);
  assert.ok(issue.line >= 1 && issue.column >= 1);
  if (issue.severity === 'error') assert.equal(result.valid, false);
  return issue;
};

test('an invalid manifest reports its cause without inventing missing character support', async (t) => {
  const directory = await fixture(t);
  await edit(directory, 'project.json', (d) => { d.unexpectedWidgetProperty = null; });
  await edit(directory, 'activities/work.json', (d) => {
    d.baseEffects.push({ op: 'add', target: { kind: 'attribute', attributeId: 'stamina', owner: { kind: 'actor' } }, value: { op: 'literal', value: 1 } });
  });
  let result = await checkProject(directory);
  findIssue(result, 'SCHEMA_INVALID', 'project.json', '/unexpectedWidgetProperty');
  findIssue(result, 'SEMANTICS_DEFERRED', 'project.json');
  assert.equal(result.diagnostics.some(d => d.code === 'CHARACTER_STATE_REQUIRED'), false);
  assert.equal(result.diagnostics[0].severity, 'error');
  // Validating siblings still exposes their structural errors.
  await edit(directory, 'activities/work.json', (d) => { d.unexpected = true; });
  result = await checkProject(directory);
  findIssue(result, 'SCHEMA_INVALID', 'activities/work.json', '/unexpected');
  await edit(directory, 'activities/work.json', (d) => { delete d.unexpected; });
  await edit(directory, 'project.json', (d) => { delete d.unexpectedWidgetProperty; });
  result = await checkProject(directory);
  findIssue(result, 'CHARACTER_STATE_REQUIRED', 'activities/work.json');
  assert.equal(result.diagnostics.some(d => d.code === 'SEMANTICS_DEFERRED'), false);
});
// Test the CLI's own streams, without parent-process instrumentation preloaders.
const { NODE_OPTIONS: ignoredNodeOptions, ...cliEnvironment } = process.env;
const runCli = (...args) => spawnSync(process.execPath, [path.join(root, 'dist/apps/tools/cli/main.js'), ...args], { cwd: root, encoding: 'utf8', env: cliEnvironment });

test('existing minimum sample is valid without any modification', async () => {
  const result = await checkProject(sample);
  assert.equal(result.valid, true, JSON.stringify(result.diagnostics));
  assert.deepEqual(result.summary, { parsedDocuments: 15, structuredDocuments: 15, errors: 0, warnings: 0 });
  const loaded = await loadProject(sample);
  assert.equal(loaded.documents[0].version.length, 64);
  const validated = await validateProject(loaded);
  assert.equal(validated.structuredDocuments.length, 15);
});

test('duplicate and escaped duplicate JSON keys report the second key precisely', () => {
  const result = parseDocument('test.json', 'project', '{\r\n  "a/b": 1,\r\n  "a\\u002fb": 2\r\n}');
  assert.equal(result.document, undefined);
  assert.equal(result.diagnostics[0].code, 'JSON_DUPLICATE_KEY');
  assert.equal(result.diagnostics[0].pointer, '/a~1b');
  assert.equal(result.diagnostics[0].line, 3);
  assert.equal(result.diagnostics[0].column, 3);
});

test('malformed JSON, comments and trailing commas are rejected, not repaired', () => {
  for (const source of ['{"a":}', '{/*comment*/"a":1}', '{"a":1,}', '', '{} trailing']) {
    const result = parseDocument('bad.json', 'project', source);
    assert.equal(result.document, undefined, source);
    assert.ok(result.diagnostics.some((d) => d.code === 'JSON_SYNTAX'));
  }
  assert.equal(parseDocument('ok.json', 'project', '{"__proto__":1}').document.data.__proto__, 1);
});

test('loader handles missing files, invalid UTF-8, and non-directory roots as diagnostics', async (t) => {
  const directory = await fixture(t);
  await rm(path.join(directory, 'flags.json'));
  await writeFile(path.join(directory, 'attributes.json'), Buffer.from([0xff, 0xfe, 0xff]));
  const result = await checkProject(directory);
  findIssue(result, 'DOCUMENT_MISSING', 'flags.json');
  findIssue(result, 'JSON_ENCODING', 'attributes.json');
  findIssue(await checkProject(path.join(directory, 'project.json')), 'PROJECT_DIRECTORY');
});

test('a schema-invalid file does not prevent valid siblings from being checked', async (t) => {
  const directory = await fixture(t);
  await edit(directory, 'activities/work.json', (d) => { d.surprise = true; });
  await edit(directory, 'events/final-exam.json', (d) => { d.window.throughPeriod = 9; });
  const result = await checkProject(directory);
  findIssue(result, 'SCHEMA_INVALID', 'activities/work.json', '/surprise');
  findIssue(result, 'EVENT_WINDOW_BOUNDS', 'events/final-exam.json', '/window/throughPeriod');
  assert.equal(result.summary.structuredDocuments, 14);
});

test('unsupported versions are rejected with an explicit format diagnostic', async (t) => {
  const directory = await fixture(t);
  await edit(directory, 'project.json', (d) => { d.formatVersion = '9.0.0'; });
  findIssue(await checkProject(directory), 'FORMAT_VERSION', 'project.json', '/formatVersion');
});

test('duplicate IDs in definitions and separate files include the first declaration', async (t) => {
  const directory = await fixture(t);
  await edit(directory, 'attributes.json', (d) => { d.attributes.push({ ...d.attributes[0] }); });
  await cp(path.join(directory, 'activities/study.json'), path.join(directory, 'activities/study-copy.json'));
  const result = await checkProject(directory);
  const duplicate = findIssue(result, 'ID_DUPLICATE', 'attributes.json', '/attributes/4/id');
  assert.equal(duplicate.relatedLocations[0].pointer, '/attributes/0/id');
  findIssue(result, 'ID_DUPLICATE', 'activities/study.json', '/id');
  findIssue(result, 'REFERENCE_AMBIGUOUS', 'events/final-exam.json');
  findIssue(result, 'FILENAME_ID_MISMATCH', 'activities/study-copy.json');
});

test('relationship, node and choice IDs use their own scopes', async (t) => {
  const directory = await fixture(t);
  await edit(directory, 'characters/mentor.json', (d) => { d.relationships.push({ ...d.relationships[0] }); });
  await edit(directory, 'events/exam-invitation.json', (d) => { d.nodes[1].choices[1].id = 'register'; });
  await edit(directory, 'events/final-exam.json', (d) => { d.nodes.push({ id: 'end', type: 'end' }); });
  const result = await checkProject(directory);
  findIssue(result, 'ID_DUPLICATE', 'characters/mentor.json', '/relationships/1/id');
  findIssue(result, 'ID_DUPLICATE', 'events/exam-invitation.json', '/nodes/1/choices/1/id');
  findIssue(result, 'ID_DUPLICATE', 'events/final-exam.json', '/nodes/3/id');
});

test('same IDs in different content namespaces remain valid', async () => {
  // 'certified' is an ending and flag ID in the original fixture.
  assert.equal((await checkProject(sample)).valid, true);
});

test('attribute, flag, relationship, speaker and event references are checked', async (t) => {
  const directory = await fixture(t);
  await edit(directory, 'activities/study.json', (d) => {
    d.baseEffects[0].target.attributeId = 'unknown-attribute';
    d.requirements = { op: 'eventCompleted', eventId: 'unknown-event' };
  });
  await edit(directory, 'events/exam-invitation.json', (d) => {
    d.nodes[0].speakerId = 'unknown-character';
    d.nodes[1].choices[0].effects[1].flagId = 'unknown-flag';
    d.nodes[1].choices[0].effects[2].target.relationshipId = 'unknown-relation';
  });
  const result = await checkProject(directory);
  for (const [file, pointer] of [
    ['activities/study.json', '/baseEffects/0/target/attributeId'],
    ['activities/study.json', '/requirements/eventId'],
    ['events/exam-invitation.json', '/nodes/0/speakerId'],
    ['events/exam-invitation.json', '/nodes/1/choices/0/effects/1/flagId'],
    ['events/exam-invitation.json', '/nodes/1/choices/0/effects/2/target/relationshipId'],
  ]) findIssue(result, 'REFERENCE_MISSING', file, pointer);
});

test('project roots and default ending requirements are checked', async (t) => {
  const directory = await fixture(t);
  await edit(directory, 'project.json', (d) => { d.protagonistId = 'missing'; });
  await edit(directory, 'endings/ordinary.json', (d) => { d.requirements = { op: 'flag', flagId: 'certified', equals: true }; d.priority = 1; });
  const result = await checkProject(directory);
  findIssue(result, 'REFERENCE_MISSING', 'project.json', '/protagonistId');
  findIssue(result, 'DEFAULT_ENDING_CONDITION', 'endings/ordinary.json');
  findIssue(result, 'DEFAULT_ENDING_PRIORITY', 'endings/ordinary.json');
});

test('declared ranges, relationship bounds and initial overrides are checked', async (t) => {
  const directory = await fixture(t);
  await edit(directory, 'attributes.json', (d) => { d.attributes[0].min = 101; d.attributes[1].initial = 1000; });
  await edit(directory, 'characters/mentor.json', (d) => { d.relationships[0].initial = -1; });
  await edit(directory, 'project.json', (d) => {
    d.initialState.attributes.money = 1000;
    d.initialState.flags.missing = true;
    d.initialState.relationships.mentor = { trust: 101, missing: 1 };
  });
  await edit(directory, 'scenarios/certified-route.json', (d) => { d.initialOverrides.attributes.money = -1; });
  const result = await checkProject(directory);
  findIssue(result, 'RANGE_ORDER', 'attributes.json', '/attributes/0/max');
  findIssue(result, 'INITIAL_OUT_OF_RANGE', 'attributes.json', '/attributes/1/initial');
  findIssue(result, 'INITIAL_OUT_OF_RANGE', 'characters/mentor.json');
  findIssue(result, 'INITIAL_OUT_OF_RANGE', 'project.json', '/initialState/attributes/money');
  findIssue(result, 'INITIAL_OUT_OF_RANGE', 'project.json', '/initialState/relationships/mentor/trust');
  findIssue(result, 'REFERENCE_MISSING', 'project.json', '/initialState/flags/missing');
  findIssue(result, 'REFERENCE_MISSING', 'project.json', '/initialState/relationships/mentor/missing');
  findIssue(result, 'INITIAL_OUT_OF_RANGE', 'scenarios/certified-route.json');
});

test('range bounds are inclusive and values are not silently clamped by validation', async (t) => {
  const directory = await fixture(t);
  await edit(directory, 'project.json', (d) => { d.initialState.attributes = { skill: 100, money: 0 }; });
  const before = await readFile(path.join(directory, 'project.json'), 'utf8');
  assert.equal((await checkProject(directory)).valid, true);
  assert.equal(await readFile(path.join(directory, 'project.json'), 'utf8'), before);
});

test('cost duplicates, forbidden stat costs and reversed clamp ranges are diagnosed', async (t) => {
  const directory = await fixture(t);
  await edit(directory, 'activities/study.json', (d) => {
    d.costs.push({ attributeId: 'money', amount: 1 }, { attributeId: 'skill', amount: 1 });
    d.baseEffects[0].value = { op: 'clamp', value: { op: 'literal', value: 1 }, min: 2, max: 1 };
  });
  const result = await checkProject(directory);
  findIssue(result, 'COST_DUPLICATE', 'activities/study.json', '/costs/2/attributeId');
  findIssue(result, 'COST_KIND', 'activities/study.json', '/costs/3/attributeId');
  findIssue(result, 'RANGE_ORDER', 'activities/study.json', '/baseEffects/0/value/max');
});

test('event windows check order and project bounds, including the last period', async (t) => {
  const directory = await fixture(t);
  await edit(directory, 'events/final-exam.json', (d) => { d.window = { fromPeriod: 6, throughPeriod: 4 }; });
  const result = await checkProject(directory);
  findIssue(result, 'EVENT_WINDOW_ORDER', 'events/final-exam.json', '/window/throughPeriod');
  findIssue(result, 'EVENT_WINDOW_BOUNDS', 'events/final-exam.json', '/window/fromPeriod');
  await edit(directory, 'events/final-exam.json', (d) => { d.window = { fromPeriod: 5, throughPeriod: 5 }; });
  assert.equal((await checkProject(directory)).valid, true);
});

test('missing narrative entry and choice/branch destinations are diagnosed', async (t) => {
  const directory = await fixture(t);
  await edit(directory, 'events/exam-invitation.json', (d) => { d.entryNodeId = 'missing-entry'; d.nodes[1].choices[0].next = 'missing-next'; });
  await edit(directory, 'events/final-exam.json', (d) => {
    d.nodes[1] = { id: 'certify', type: 'branch', condition: { op: 'always' }, then: 'missing-branch', else: 'end' };
  });
  const result = await checkProject(directory);
  findIssue(result, 'REFERENCE_MISSING', 'events/exam-invitation.json', '/entryNodeId');
  findIssue(result, 'REFERENCE_MISSING', 'events/exam-invitation.json', '/nodes/1/choices/0/next');
  findIssue(result, 'REFERENCE_MISSING', 'events/final-exam.json', '/nodes/1/then');
});

test('reachable cycles, self-loops, unreachable cycles and ending cycles are rejected', async (t) => {
  const directory = await fixture(t);
  await edit(directory, 'events/exam-invitation.json', (d) => { d.nodes[1].choices[0].next = 'intro'; });
  await edit(directory, 'events/final-exam.json', (d) => {
    d.nodes.push({ id: 'loop', type: 'effect', effects: [], next: 'loop' });
  });
  await edit(directory, 'endings/ordinary.json', (d) => { d.nodes[0].next = 'intro'; });
  const result = await checkProject(directory);
  findIssue(result, 'NARRATIVE_CYCLE', 'events/exam-invitation.json');
  findIssue(result, 'NARRATIVE_CYCLE', 'events/final-exam.json', '/nodes/3/next');
  findIssue(result, 'NARRATIVE_UNREACHABLE', 'events/final-exam.json', '/nodes/3/id');
  findIssue(result, 'NARRATIVE_NO_END', 'endings/ordinary.json', '/nodes/0/id');
});

test('a branching DAG merging into one terminal node is valid', async (t) => {
  const directory = await fixture(t);
  await edit(directory, 'events/final-exam.json', (d) => {
    d.nodes[0] = { id: 'intro', type: 'branch', condition: { op: 'always' }, then: 'certify', else: 'end' };
  });
  assert.equal((await checkProject(directory)).valid, true);
});

test('unreachable but well-formed nodes produce warnings without failing validation', async (t) => {
  const directory = await fixture(t);
  await edit(directory, 'endings/ordinary.json', (d) => { d.nodes.push({ id: 'unused', type: 'end' }); });
  const result = await checkProject(directory);
  assert.equal(result.valid, true);
  assert.equal(findIssue(result, 'NARRATIVE_UNREACHABLE').severity, 'warning');
  assert.equal(runCli('validate', directory).status, 0);
});

test('scenario activity, choice node, choice ID and ending references are checked', async (t) => {
  const directory = await fixture(t);
  await edit(directory, 'scenarios/certified-route.json', (d) => {
    d.commands[0].activityIds[0] = 'missing-activity';
    d.commands[3].choices[0].choiceId = 'missing-choice';
    d.assertions[0].endingId = 'missing-ending';
  });
  await edit(directory, 'scenarios/ordinary-route.json', (d) => { d.commands[3].choices[0].nodeId = 'intro'; });
  const result = await checkProject(directory);
  findIssue(result, 'REFERENCE_MISSING', 'scenarios/certified-route.json', '/commands/0/activityIds/0');
  findIssue(result, 'REFERENCE_MISSING', 'scenarios/certified-route.json', '/commands/3/choices/0/choiceId');
  findIssue(result, 'REFERENCE_MISSING', 'scenarios/certified-route.json', '/assertions/0/endingId');
  findIssue(result, 'REFERENCE_KIND', 'scenarios/ordinary-route.json', '/commands/3/choices/0/nodeId');
});

test('image/audio kinds, portrait references, repeated positions and missing asset files are checked', async (t) => {
  const directory = await fixture(t);
  await edit(directory, 'assets/manifest.json', (d) => { d.assets = [{ id: 'sound', kind: 'audio', path: 'assets/missing.wav' }]; });
  await edit(directory, 'characters/mentor.json', (d) => { d.portraits.normal = 'sound'; });
  await edit(directory, 'activities/study.json', (d) => {
    d.presentation = { portraits: [
      { characterId: 'mentor', portraitId: 'missing', position: 'left' },
      { characterId: 'mentor', portraitId: 'normal', position: 'left' },
    ] };
  });
  const result = await checkProject(directory);
  findIssue(result, 'REFERENCE_KIND', 'characters/mentor.json', '/portraits/normal');
  findIssue(result, 'ASSET_MISSING', 'assets/manifest.json', '/assets/0/path');
  findIssue(result, 'REFERENCE_MISSING', 'activities/study.json', '/presentation/portraits/0/portraitId');
  findIssue(result, 'PORTRAIT_POSITION_DUPLICATE', 'activities/study.json', '/presentation/portraits/1/position');
});

test('protagonist cannot declare a relationship with itself', async (t) => {
  const directory = await fixture(t);
  await edit(directory, 'characters/apprentice.json', (d) => { d.relationships = [{ id: 'trust', label: '신뢰', min: 0, max: 10, initial: 0 }]; });
  findIssue(await checkProject(directory), 'PROTAGONIST_RELATIONSHIP', 'characters/apprentice.json');
});

test('deep expressions are bounded before execution', async (t) => {
  const directory = await fixture(t);
  await edit(directory, 'activities/study.json', (d) => {
    let condition = { op: 'always' };
    for (let i = 0; i < 33; i++) condition = { op: 'not', condition };
    d.requirements = condition;
  });
  findIssue(await checkProject(directory), 'EXPRESSION_DEPTH', 'activities/study.json');
});

test('folder symlinks escaping the project are refused', async (t) => {
  const directory = await fixture(t);
  const external = await fixture(t);
  await rename(path.join(directory, 'activities'), path.join(directory, 'old-activities'));
  await symlink(path.join(external, 'activities'), path.join(directory, 'activities'), process.platform === 'win32' ? 'junction' : 'dir');
  findIssue(await checkProject(directory), 'PATH_OUTSIDE_PROJECT', 'activities');
});

test('CLI provides parseable JSON, deterministic diagnostics, failure codes and usage errors', async (t) => {
  const directory = await fixture(t);
  await edit(directory, 'events/final-exam.json', (d) => { d.window.throughPeriod = 6; });
  const json = runCli('validate', directory, '--json');
  assert.equal(json.status, 1, json.stderr);
  const report = JSON.parse(json.stdout);
  findIssue(report, 'EVENT_WINDOW_BOUNDS', 'events/final-exam.json', '/window/throughPeriod');
  assert.equal(json.stderr, '');
  assert.equal(runCli('validate', directory, '--json').stdout, json.stdout);
  const text = runCli('validate', directory);
  assert.equal(text.status, 1);
  assert.match(text.stdout, /events\/final-exam\.json:\d+:\d+ error EVENT_WINDOW_BOUNDS/);
  assert.match(text.stdout, /\/window\/throughPeriod/);
  assert.equal(runCli('validate').status, 2);
  assert.equal(runCli('validate', directory, '--wat').status, 2);
  assert.equal(runCli('--help').status, 0);
});
