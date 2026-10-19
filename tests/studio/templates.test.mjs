import test from 'node:test';
import assert from 'node:assert/strict';
import { access, mkdir, mkdtemp, readFile, writeFile } from 'node:fs/promises';
import { resolve, join } from 'node:path';
import ts from 'typescript';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';

// Transpile only the dependency-free copy service; do not rebuild the running editor.
const source = await readFile(new URL('../../packages/studio-service/templates.ts', import.meta.url), 'utf8');
const compiled = ts.transpileModule(source, { compilerOptions: { module: ts.ModuleKind.ES2022, target: ts.ScriptTarget.ES2022 } }).outputText;
const { studioTemplates, createTemplateProject, createProjectFromSource } = await import(`data:text/javascript;base64,${Buffer.from(compiled).toString('base64')}`);

test('adventure template creates an independent valid project and preserves existing destinations', async () => {
  assert.ok(studioTemplates.includes('academy-adventure'));
  await mkdir('.cache/studio-template-tests', { recursive: true });
  const target = await mkdtemp(resolve('.cache/studio-template-tests/adventure-'));
  const originalProject = await readFile('examples/academy-adventure/project.json');
  await createTemplateProject(resolve('examples'), 'academy-adventure', target);
  assert.deepEqual(await readFile(join(target, 'project.json')), originalProject);
  await assert.rejects(access(join(target, '.studio')), { code: 'ENOENT' });
  const copied = await buildSnapshot(await loadProject(target));
  const original = await buildSnapshot(await loadProject(resolve('examples/academy-adventure')));
  assert.ok(copied.ok, JSON.stringify(copied)); assert.ok(original.ok, JSON.stringify(original));
  assert.equal(copied.snapshot.contentFingerprint, original.snapshot.contentFingerprint);
  assert.ok(copied.snapshot.project.systems.expeditions.length > 0);
  await writeFile(join(target, 'keep.txt'), 'user work');
  await assert.rejects(createTemplateProject(resolve('examples'), 'academy-adventure', target), /빈 폴더/);
  assert.equal(await readFile(join(target, 'keep.txt'), 'utf8'), 'user work');
  assert.deepEqual(await readFile('examples/academy-adventure/project.json'), originalProject);
});

test('content reuse copies authored data and assets but excludes saves, analysis and unrelated files', async () => {
  await mkdir('.cache/studio-template-tests', {recursive:true});
  const sandbox=await mkdtemp(resolve('.cache/studio-template-tests/reuse-'));
  const source=join(sandbox,'source'), target=join(sandbox,'copy');
  await createTemplateProject(resolve('examples'),'academy-adventure',source);
  await mkdir(join(source,'saves'));await writeFile(join(source,'saves/private.json'),'save');
  await mkdir(join(source,'.studio'));await writeFile(join(source,'.studio/analysis.json'),'analysis');
  await writeFile(join(source,'unrelated.txt'),'do not clone');
  const before=await readFile(join(source,'project.json'));
  await createProjectFromSource(source,target);
  const copied=await buildSnapshot(await loadProject(target));const original=await buildSnapshot(await loadProject(source));
  assert.ok(copied.ok,JSON.stringify(copied));assert.equal(copied.snapshot.contentFingerprint,original.snapshot.contentFingerprint);
  for(const entry of ['saves','.studio','unrelated.txt'])await assert.rejects(access(join(target,entry)),{code:'ENOENT'});
  assert.deepEqual(await readFile(join(source,'project.json')),before);
  await assert.rejects(createProjectFromSource(source,source),/원본 프로젝트/);
  await assert.rejects(createProjectFromSource(source,join(source,'nested')),/원본 프로젝트/);
  await assert.rejects(access(join(source,'nested')),{code:'ENOENT'});
  await assert.rejects(createProjectFromSource(source,target),/빈 폴더/);
  assert.deepEqual(await readFile(join(target,'project.json')),before);
});

test('content reuse preserves plugin lock, rig and controller together with referenced artwork', async () => {
  await mkdir('.cache/studio-template-tests', { recursive: true });
  const target = await mkdtemp(resolve('.cache/studio-template-tests/plugin-'));
  const source = resolve('tests/fixtures/projects/last-grandson-polished');
  await createProjectFromSource(source, target);
  for (const file of ['plugins/lock.json', 'plugins/cutout-animation/rig/seol.json', 'plugins/cutout-animation/controller/seol.json', 'assets/art/seol-neutral.png']) {
    assert.deepEqual(await readFile(join(target, file)), await readFile(join(source, file)));
  }
  const copied = await buildSnapshot(await loadProject(target));
  assert.ok(copied.ok, JSON.stringify(copied));
  await assert.rejects(access(join(target, '.studio')), { code: 'ENOENT' });
});

test('template creation refuses a source descendant before making directories', async () => {
  await mkdir('.cache/studio-template-tests', { recursive: true });
  const samples = await mkdtemp(resolve('.cache/studio-template-tests/nested-'));
  const source = join(samples, 'academy-demo');
  await mkdir(source);
  await writeFile(join(source, 'project.json'), '{"sentinel":"original template"}');
  const target = join(source, 'new-game', 'nested');
  await assert.rejects(createTemplateProject(samples, 'academy-demo', target), /원본 프로젝트 안/);
  await assert.rejects(access(join(source, 'new-game')), { code: 'ENOENT' });
  await assert.rejects(createTemplateProject(samples, 'academy-demo', source), /원본 프로젝트 안/);
  assert.equal(await readFile(join(source, 'project.json'), 'utf8'), '{"sentinel":"original template"}');
});
