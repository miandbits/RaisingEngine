import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {spawnSync} from 'node:child_process';
import path from 'node:path';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {gameDirectory,playerExecutable,desktopTarget} from '../../dist/packages/export-service/targets.js';
import {firstGameRoute} from '../qt-studio/first-game-route.mjs';

test('starter desktop app: both routes, midgame save/load, and fresh-process ending restoration',{timeout:180000},async()=>{
  await mkdir('.cache/first-game-native',{recursive:true});
  const out=await mkdtemp(path.resolve('.cache/first-game-native/run-'));
  const service=new QtStudioService();await service.handle({method:'create',params:{root:path.join(out,'project'),template:'my-first-raising-game'}});
  const built=await service.editor.snapshot(service.editor.revision);assert.equal(built.ok,true);
  const target=desktopTarget(),destination=path.join(out,'published');
  await service.handle({method:'export',params:{revision:service.editor.revision,target,destination}});
  const executable=playerExecutable(target,destination),directory=gameDirectory(target,destination);
  const results=[];
  for(const accept of [true,false]) {
    const route=firstGameRoute(built.snapshot,accept),steps=path.join(out,route.id+'.json');
    const report=path.join(out,route.id+'-report.json'),save=path.join(out,route.id+'-save.json');
    await writeFile(steps,JSON.stringify([...route.steps,{screenshot:path.join(out,route.id+'.png')}]));
    const run=spawnSync(executable,['--bundle',directory,'--automation',steps,'--report',report,'--save',save,'--hidden'],{encoding:'utf8',timeout:90000});
    assert.equal(run.status,0,run.error?.message??run.stderr);
    const proof=JSON.parse(await readFile(report,'utf8'));assert.equal(proof.ok,true,JSON.stringify(proof));
    for(const [label,state] of Object.entries(route.expected))assert.deepEqual(proof.steps.find(s=>s.command.checkpoint===label)?.ui.state,state,route.id+'/'+label);
    const restoredReport=path.join(out,route.id+'-restored.json');
    await writeFile(steps,JSON.stringify([{key:'F9'},{checkpoint:'restored'}]));
    const restored=spawnSync(executable,['--bundle',directory,'--automation',steps,'--report',restoredReport,'--save',save,'--hidden'],{encoding:'utf8',timeout:30000});
    assert.equal(restored.status,0,restored.stderr);
    const fresh=JSON.parse(await readFile(restoredReport,'utf8'));assert.deepEqual(fresh.steps.find(s=>s.command.checkpoint==='restored').ui.state,route.expected.ending);
    results.push({route:route.id,ending:route.ending,checkpoints:Object.keys(route.expected).length});
  }
  await writeFile(path.resolve('.cache/first-game-native/latest.json'),JSON.stringify({out,destination,results},null,2));
});
