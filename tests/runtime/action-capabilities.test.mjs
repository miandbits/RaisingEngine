import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile,access,readdir} from 'node:fs/promises';
import path from 'node:path';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('action export rejects old renderers before publication and retries after capability repair',async()=>{
  const projectRoot=path.resolve('projects/magical-childhood-reference');
  const built=await buildSnapshot(await loadProject(projectRoot));
  assert.equal(built.ok,true,JSON.stringify(built));
  await mkdir('.cache/action-capability-tests',{recursive:true});
  const work=await mkdtemp(path.resolve('.cache/action-capability-tests/run-'));
  const runtime=path.join(work,'runtime');await mkdir(runtime);
  // Packaging fixtures, never executable game binaries.
  await writeFile(path.join(runtime,'RaisingPlayer.exe'),'packaging fixture');
  await writeFile(path.join(runtime,'index.html'),'packaging fixture');
  const capabilities=JSON.parse(await readFile('dist/web/capabilities.json','utf8'));
  for(const target of ['windows','web']){
    for(const missing of ['probabilistic-actions-v1','optional-confirmation-v1']){
      assert.ok(capabilities.visualFeatures.includes(missing));
      const destination=path.join(work,`${target}-${missing}`);
      const request=()=>exportGame({content:built.snapshot,projectRoot,destination,target,runtimes:{windows:runtime,web:runtime}});
      await writeFile(path.join(runtime,'capabilities.json'),JSON.stringify({...capabilities,visualFeatures:capabilities.visualFeatures.filter(f=>f!==missing)}));
      await assert.rejects(request(),error=>error.message.includes(missing));
      await assert.rejects(access(destination),{code:'ENOENT'});
      assert.deepEqual((await readdir(work)).filter(name=>name.startsWith('.raising-export-')),[]);
      await writeFile(path.join(runtime,'capabilities.json'),JSON.stringify(capabilities));
      await request();
      const bundle=JSON.parse(await readFile(path.join(destination,'game.json'),'utf8'));
      assert.deepEqual(bundle.content,built.snapshot);
    }
  }
});
