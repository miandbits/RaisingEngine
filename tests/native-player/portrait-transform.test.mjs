import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('native portrait geometry uses bottom-center scale and original-slot offsets only for scene slots',async t=>{
  await mkdir('.cache/portrait-transform',{recursive:true});const root=await mkdtemp(resolve('.cache/portrait-transform/run-'));
  const projectRoot=resolve(root,'project');await cp(resolve('examples/academy-journey'),projectRoot,{recursive:true});
  const project=JSON.parse(await readFile(resolve(projectRoot,'project.json'),'utf8'));
  project.visual.defaultScene.portraits[0]={...project.visual.defaultScene.portraits[0],scale:0.2,offsetX:0.1,offsetY:-0.1};
  const elements=[{id:'portrait',kind:'image',imageSource:'portrait-center',fit:'contain',x:100,y:80,width:400,height:600},{id:'fixed',kind:'image',imageSource:'asset',imageAssetId:'visual-courtyard',x:600,y:80,width:400,height:600}];
  project.visual.screens.find(s=>s.id==='title').elements=[...elements,{id:'start',kind:'button',action:'start',x:10,y:10,width:100,height:40,text:'Start'}];
  await writeFile(resolve(projectRoot,'project.json'),JSON.stringify(project));
  const built=await buildSnapshot(await loadProject(projectRoot));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  const destination=resolve(root,'game');await exportGame({content:built.snapshot,projectRoot,destination,target:'windows',runtimes:{windows:resolve(process.env.RAISING_PORTRAIT_RUNTIME??'dist/native-player-portrait-transform'),web:resolve('dist/web')}});
  const file=resolve(root,'steps.json'),reportPath=resolve(root,'report.json');await writeFile(file,JSON.stringify([{checkpoint:'geometry',screenshot:resolve(root,'geometry.png')}]));
  const run=spawnSync(resolve(destination,'RaisingPlayer.exe'),['--bundle',destination,'--hidden','--automation',file,'--report',reportPath],{encoding:'utf8',timeout:60000,windowsHide:true});assert.equal(run.status,0,run.error?.message??run.stderr);
  const report=JSON.parse(await readFile(reportPath,'utf8'));assert.equal(report.ok,true);
  const nodes=report.steps.find(s=>s.command.checkpoint==='geometry').ui.nodes;
  const portrait=nodes.find(n=>n.id==='portrait'),fixed=nodes.find(n=>n.id==='fixed');
  const rect=n=>({x:n.x,y:n.y,width:n.width,height:n.height});
  assert.deepEqual(rect(portrait),{x:300,y:500,width:80,height:120});
  assert.deepEqual(rect(fixed),{x:600,y:80,width:400,height:600});
  t.diagnostic(`Evidence: ${root}`);
});
