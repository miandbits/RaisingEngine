import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('GUI-authored nested UI: sorted selection, scroll, modal confirmation, tooltip, save/load and three viewports',{timeout:180000},async()=>{
 const proof=JSON.parse(await readFile('.cache/ui-composition-proof.json','utf8'));
 assert.ok(proof.guiEdited&&proof.saveReopen&&proof.undoRedo);
 const built=await buildSnapshot(await loadProject(proof.project));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
 const root=await mkdtemp(resolve('.cache/ui-composition-play-')),game=resolve(root,'game'),evidence=resolve('docs/evidence/ui-composition');await mkdir(evidence,{recursive:true});
 const runtimes={windows:resolve('dist/native-player-ui5'),web:resolve('dist/web')};
 await assert.rejects(exportGame({content:built.snapshot,projectRoot:proof.project,destination:resolve(root,'old'),target:'windows',runtimes:{...runtimes,windows:resolve('dist/native-player-workspace')}}),/ui-composition-v1/);
 await exportGame({content:built.snapshot,projectRoot:proof.project,destination:game,target:'windows',runtimes});
 async function play(steps,name,save){
  const script=resolve(evidence,name+'-steps.json'),report=resolve(evidence,name+'-report.json');await writeFile(script,JSON.stringify(steps,null,2));
  const child=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',save,'--automation',script,'--report',report,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:60000});assert.equal(child.status,0,child.stderr.slice(-1800));
  const result=JSON.parse(await readFile(report,'utf8'));assert.equal(result.ok,true,JSON.stringify(result));return result;
 }
 const row=id=>`collection-1__${id}__component-1__button`,check=(r,id)=>r.steps.find(s=>s.command.checkpoint===id).ui;
 for(const [width,height] of [[1280,720],[1920,1080],[800,1200]]){
  const name=`play-${width}x${height}`,save=resolve(evidence,name+'-save.json'),scale=width/1280,logicalHeight=Math.max(720,height/scale),groupY=180+(logicalHeight-720)/2;
  const steps=[{resize:{width,height}},{click:'start'},{checkpoint:'initial'},{click:'button-6'},{checkpoint:'list',screenshot:resolve(evidence,name+'-list.png')},
   {hover:row('row-2')},{checkpoint:'tooltip',screenshot:resolve(evidence,name+'-tooltip.png')},
   {click:row('row-2')},{checkpoint:'modal',screenshot:resolve(evidence,name+'-modal.png')},
   {wheelWindow:{x:600*scale,y:(groupY+90)*scale,deltaY:-3}},{checkpoint:'scrolled'},
   {key:'Tab'},{checkpoint:'tab'},{click:'modal-1__button-1'},{checkpoint:'confirm',screenshot:resolve(evidence,name+'-confirm.png')},
   {click:'cancel-purchase'},{checkpoint:'cancelled'},
   {click:'modal-1__button-1'},{click:'confirm-purchase'},{checkpoint:'prayer'},
   {key:'Escape'},{checkpoint:'closed'},
   {wheelWindow:{x:600*scale,y:200*scale,deltaY:-5}},{checkpoint:'list-scrolled'},
   {click:row('row-1')},{checkpoint:'rest-modal'},{click:'modal-1__button-1'},{click:'confirm-purchase'},{checkpoint:'rest'},
   {key:'F5'},{checkpoint:'saved'},{key:'Escape'},{key:'F9'},{checkpoint:'loaded',screenshot:resolve(evidence,name+'-loaded.png')}];
  const r=await play(steps,name,save),initial=check(r,'initial'),list=check(r,'list'),modal=check(r,'modal');
  const titles=list.nodes.filter(n=>n.id.endsWith('__title'));assert.ok(titles.find(n=>n.id.includes('row-2')).y<titles.find(n=>n.id.includes('row-1')).y);
  assert.ok(!list.elements.some(e=>e.id.includes('row-3')));
  assert.equal(check(r,'tooltip').tooltipVisible,true);
  assert.deepEqual(check(r,'tooltip').state,list.state);assert.deepEqual(modal.state,list.state);
  assert.equal(modal.uiSelections.service.rowId,'row-2');assert.equal(modal.texts['modal-1__text-1'],'Service: Prayer service');
  assert.ok(modal.elements.filter(e=>!e.id.startsWith('modal-1__')).every(e=>!e.enabled));
  const group=modal.nodes.find(n=>n.id==='modal-1__group-1');assert.equal(group.outsideViewport,false);assert.equal(group.y,groupY);
  assert.ok(check(r,'scrolled').nodes.find(n=>n.id===group.id).scrollTop>0);assert.ok(check(r,'tab').focusId.startsWith('modal-1__'));
  const confirm=check(r,'confirm');assert.ok(confirm.elements.find(e=>e.id==='confirm-purchase').enabled);assert.ok(!confirm.elements.find(e=>e.id==='modal-1__button-1').enabled);
  assert.deepEqual(check(r,'cancelled').state,list.state);
  assert.equal(check(r,'prayer').state.attributes.reputation,initial.state.attributes.reputation+2);
  assert.equal(check(r,'closed').uiModal,'');assert.ok(check(r,'list-scrolled').nodes.find(n=>n.id==='collection-1').scrollTop>0);
  assert.equal(check(r,'rest-modal').texts['modal-1__text-1'],'Service: Rest service');
  assert.equal(check(r,'rest').state.attributes.stamina,initial.state.attributes.stamina+20);
  assert.deepEqual(check(r,'loaded').state,check(r,'saved').state);assert.equal(check(r,'loaded').uiModal,'');assert.deepEqual(check(r,'loaded').uiSelections,{});
  const fresh=await play([{key:'F9'},{checkpoint:'fresh'}],name+'-fresh',save);assert.deepEqual(check(fresh,'fresh').state,check(r,'saved').state);
 }
 await writeFile(resolve(evidence,'verification.json'),JSON.stringify({ok:true,project:proof.project,game,viewports:[[1280,720],[1920,1080],[800,1200]],guiAuthoring:true,actualPlayer:true,saveFreshLoad:true},null,2));
});
