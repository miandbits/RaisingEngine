import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {desktopTarget,gameDirectory,playerExecutable} from '../../dist/packages/export-service/targets.js';

test('native wall-clock holds retain state and wait through the terminal command',{timeout:60000},async()=>{
 await mkdir('.cache/native-automation-wait',{recursive:true});
 const out=await mkdtemp(resolve('.cache/native-automation-wait/run-'));
 const root=resolve('examples/my-first-raising-game'),built=await buildSnapshot(await loadProject(root));
 assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
 const destination=join(out,'game'),target=desktopTarget(),runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??'dist/native-player-app');
 await exportGame({projectRoot:root,content:built.snapshot,target,destination,runtimes:{windows:runtime,macos:runtime}});
 async function run(name,steps){
  const script=join(out,name+'.json'),report=join(out,name+'-report.json');await writeFile(script,JSON.stringify(steps));
  const result=spawnSync(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--hidden','--automation',script,'--report',report,'--save',join(out,'save.json')],{encoding:'utf8',timeout:15000,windowsHide:true});
  return {result,report:JSON.parse(await readFile(report,'utf8'))};
 }
 const {result,report}=await run('holds',[{waitMs:1200,checkpoint:'first'},{checkpoint:'after'},{waitMs:800,checkpoint:'terminal'}]);
 assert.equal(result.status,0,result.error?.message??result.stderr);assert.equal(report.ok,true);
 assert.ok(report.steps[1].elapsedMs-report.steps[0].elapsedMs>=1195,'next command waits for elapsed time');
 assert.ok(report.elapsedMs-report.steps[2].elapsedMs>=795,'last wait cannot bypass terminal completion');
 for(const step of report.steps){assert.deepEqual(step.ui.state,report.final.state);assert.deepEqual(step.ui.commands,report.final.commands);}
 for(const delay of [-1,3600001]){
  const rejected=await run('invalid-'+delay,[{waitMs:delay}]);assert.notEqual(rejected.result.status,0);assert.equal(rejected.report.ok,false);assert.match(rejected.report.error,/waitMs must be between/);
 }
 console.log('Native wall-clock evidence: '+out);
});
