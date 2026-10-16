import test from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { cp,writeFile,readFile } from 'node:fs/promises';
import { join,resolve } from 'node:path';
import { temporary,samplePath } from './helpers.mjs';
import { loadProject,buildSnapshot } from '../../dist/packages/project-service/index.js';
import { createSession,saveRun,loadRun } from '../../dist/packages/runtime-service/index.js';

test('migration CLI previews, rejects stale approval and publishes a separately loadable save',async t=>{
  const root=await temporary(t),from=join(root,'old'),to=join(root,'new'),save=join(root,'original.json'),output=join(root,'new-save.json');
  await cp(samplePath,from,{recursive:true});await cp(samplePath,to,{recursive:true});
  const original=await buildSnapshot(await loadProject(from));assert.ok(original.ok);const run=createSession(original.snapshot,42);assert.ok(run.ok);assert.ok((await saveRun(save,original.snapshot,run.session)).ok);
  const loaded=await loadProject(to),attribute=loaded.documents.find(d=>d.kind==='attributes');attribute.data.attributes.find(a=>a.id==='money').initial+=3;await writeFile(join(to,attribute.file),JSON.stringify(attribute.data));
  const before=await readFile(save);
  const args=['scripts/migrate-save.mjs','--from',from,'--to',to,'--save',save];
  const invoke=extra=>spawnSync(process.execPath,[...args,...extra],{cwd:resolve('.'),encoding:'utf8',timeout:30000,windowsHide:true});
  const preview=invoke([]);assert.equal(preview.status,0,preview.stderr);const report=JSON.parse(preview.stdout).report;assert.ok(report.approval);
  const stale=invoke(['--output',output,'--approve','stale']);assert.equal(stale.status,1);assert.equal(JSON.parse(stale.stdout).error.code,'MIGRATION_STALE');
  const applied=invoke(['--output',output,'--approve',report.approval]);assert.equal(applied.status,0,applied.stderr);
  const target=await buildSnapshot(await loadProject(to));assert.ok(target.ok);const restored=await loadRun(output,target.snapshot);assert.ok(restored.ok);assert.equal(restored.session.state.attributes.money,run.session.state.attributes.money+3);
  assert.deepEqual(await readFile(save),before);
  const exists=invoke(['--output',output,'--approve',report.approval]);assert.equal(exists.status,1);assert.equal(JSON.parse(exists.stdout).error.code,'MIGRATION_IO');
});
