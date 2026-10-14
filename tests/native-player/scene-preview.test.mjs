import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,writeFile,access} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('scene preview renders draft only, without narrative commands, save, or initial state mutation',async t=>{
  await mkdir('.cache/scene-preview',{recursive:true});const root=await mkdtemp(resolve('.cache/scene-preview/run-')),projectRoot=resolve(root,'project');await cp(resolve('examples/academy-journey'),projectRoot,{recursive:true});
  const project=JSON.parse(await readFile(resolve(projectRoot,'project.json'),'utf8'));
  project.visual.screens.find(s=>s.id==='story').elements=[{id:'portrait',kind:'image',imageSource:'portrait-center',visibleWhen:'dialogue',x:100,y:80,width:400,height:600},{id:'background',kind:'image',imageSource:'background',x:700,y:80,width:200,height:300},{id:'save-block',kind:'button',action:'save',text:'Save',x:10,y:10,width:120,height:40},{id:'advance',kind:'button',action:'advance',text:'Continue',x:150,y:10,width:140,height:40}];
  project.visual.screens.find(s=>s.id==='story').elements.push({id:'dialogue',kind:'dialogue',binding:'story',x:10,y:600,width:400,height:100,visibleWhen:'dialogue'},{id:'choices',kind:'choices',x:500,y:600,width:400,height:100,visibleWhen:'choice'});
  await writeFile(resolve(projectRoot,'project.json'),JSON.stringify(project));const built=await buildSnapshot(await loadProject(projectRoot));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  const destination=resolve(root,'game');await exportGame({content:built.snapshot,projectRoot,destination,target:'windows',runtimes:{windows:resolve('dist/native-player-scene-preview'),web:resolve('dist/web')}});
  const scene={backgroundAssetId:null,portraits:[{characterId:'apprentice',portraitId:'visual-neutral',position:'center',scale:.2,offsetX:.1,offsetY:-.1}]};const sceneFile=resolve(root,'scene.json'),steps=resolve(root,'steps.json'),save=resolve(root,'must-not-write.json');await writeFile(sceneFile,JSON.stringify(scene));
  await writeFile(steps,JSON.stringify([{checkpoint:'before'},{clickWindow:'save-block'},{clickWindow:'advance'},{key:'F5'},{checkpoint:'after',screenshot:resolve(root,'scene.png')}]));
  const exe=resolve(destination,'RaisingPlayer.exe');
  function run(name,extra=[]){const report=resolve(root,`${name}.json`);const result=spawnSync(exe,['--bundle',destination,'--hidden','--preview-screen','story','--automation',steps,'--report',report,'--save',save,...extra],{encoding:'utf8',timeout:60000,windowsHide:true});assert.equal(result.status,0,result.stderr);return report;}
  const ordinary=JSON.parse(await readFile(run('ordinary'),'utf8')),preview=JSON.parse(await readFile(run('preview',['--preview-scene',sceneFile]),'utf8'));
  assert.equal(preview.ok,true);assert.deepEqual(preview.final.state,ordinary.final.state);assert.equal(preview.final.state.phase,'planning');assert.equal(preview.final.state.stateRevision,0);assert.equal(preview.final.state.activeNarrative,null);
  assert.deepEqual(preview.final.state,preview.steps.find(s=>s.command.checkpoint==='before').ui.state);assert.equal(preview.final.preview.sceneOverride,true);assert.deepEqual(preview.final.preview.displayScene.portraits,scene.portraits);assert.equal(preview.final.preview.displayScene.backgroundAssetId,null);
  const portrait=preview.final.nodes.find(n=>n.id==='portrait');assert.deepEqual([portrait.x,portrait.y,portrait.width,portrait.height],[300,500,80,120]);assert.equal(ordinary.final.nodes.find(n=>n.id==='portrait').visible,false);await assert.rejects(access(save));assert.equal(preview.final.audio.musicPlaying,false);
  for(const bad of [{effects:[]},{portraits:[{...scene.portraits[0],scale:0}]},{backgroundAssetId:'missing'}]){await writeFile(sceneFile,JSON.stringify(bad));const result=spawnSync(exe,['--bundle',destination,'--hidden','--preview-screen','story','--preview-scene',sceneFile,'--frames','1'],{encoding:'utf8',timeout:10000,windowsHide:true});assert.notEqual(result.status,0);}
  await writeFile(sceneFile,JSON.stringify(scene));const invalid=spawnSync(exe,['--bundle',destination,'--hidden','--preview-scene',sceneFile,'--frames','1'],{encoding:'utf8',timeout:10000,windowsHide:true});assert.notEqual(invalid.status,0);assert.match(invalid.stderr,/requires --preview-screen/);
  t.diagnostic(`Evidence: ${root}`);
});
