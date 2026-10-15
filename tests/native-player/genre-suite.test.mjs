import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

const click=id=>({click:id}), check=id=>({checkpoint:id}), key=id=>({key:id});
const act=id=>[click(id),click('confirm-purchase')];
function sampleSteps(kind){
 const s=[click('start'),check('started')];
 if(kind==='free')s.push(click('action__actions__work'),click('cancel-purchase'),check('declined'),...act('action__actions__work'),check('pending'),key('F5'),check('saved'),click('progress__actions__cancel'),check('cancelled'),key('F9'),check('restored'),click('progress__actions__advance'),check('worked'));
 if(kind==='relationship')s.push(...act('action__actions__talk'),...act('action__actions__talk'),check('befriended'));
 if(kind==='pet')s.push(click('actor-action__actions__pet__feed'),click('actor-action__actions__pet__feed'),check('fed'));
 if(kind==='adventure')s.push(...act('action__actions__depart'),click('move-expedition-clearing'),check('battle'),key('F5'),check('saved'),click('battle-attack'),check('won'),key('F9'),check('restored'),click('battle-attack'),click('move-expedition-gate'),click('return-expedition'),check('returned'));
 s.push(click('schedule-link'));
 if(kind==='roster'||kind==='pet'){
  const assignments=kind==='roster'?[{select:'roster-hero',value:'train'},{select:'roster-friend',value:'rest'}]:[{select:'roster-pet',value:'train'}];
  s.push(...assignments,click('roster-assign'),click('roster-execute'),check('period-one'),key('F5'),check('month-saved'),click('advance'),check('month-changed'),key('F9'),check('month-restored'),click('advance'),...assignments,click('roster-assign'),click('roster-execute'),click('advance'),click('advance'));
 }else{
  s.push({select:'plan-0',value:'train'},{select:'plan-1',value:'train'},click('advance'),click('advance'),check('period-one'),key('F5'),check('month-saved'),click('advance'),check('month-changed'),key('F9'),check('month-restored'),click('advance'),click('advance'),click('advance'),click('advance'));
 }
 s.push(check('finished'),key('F5'));
 return s;
}

test('six Qt-authored genres play to their goal endings and restore in a fresh player',{timeout:420000},async t=>{
 const proof=JSON.parse(await readFile('.cache/genre-suite-proof.json','utf8'));
 assert.ok(proof.guiTemplates&&proof.saveReopen);
 for(const item of proof.projects){if(process.env.GENRE_ONLY&&process.env.GENRE_ONLY!==item.kind)continue;
  await t.test(item.kind,async()=>{
   const b=await buildSnapshot(await loadProject(item.root));assert.equal(b.ok,true,JSON.stringify(b.diagnostics));
   const root=await mkdtemp(resolve('.cache/genre-player-')),game=resolve(root,'game'),evidence=resolve('docs/evidence/genre-suite',item.kind);await mkdir(evidence,{recursive:true});
   await exportGame({content:b.snapshot,projectRoot:item.root,destination:game,target:'windows',runtimes:{windows:resolve('dist/native-player-direction'),web:resolve('dist/web')}});
   const save=resolve(evidence,'player-save.json');
   async function play(steps,name){
    const script=resolve(evidence,name+'-steps.json'),report=resolve(evidence,name+'-report.json');await writeFile(script,JSON.stringify(steps,null,2));
    const p=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',save,'--automation',script,'--report',report,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:90000});
    const data=JSON.parse(await readFile(report,'utf8'));assert.equal(p.status,0,JSON.stringify(data.error??p.stderr));assert.equal(data.ok,true);return data;
   }
   const steps=sampleSteps(item.kind).map(s=>s.checkpoint?{...s,screenshot:resolve(evidence,s.checkpoint+'.png')}:s);
   const result=await play(steps,'play'),ui=id=>result.steps.find(s=>s.command.checkpoint===id)?.ui;
   assert.deepEqual(ui('month-restored').state,ui('month-saved').state);assert.notDeepEqual(ui('month-changed').state,ui('month-saved').state);
   if(item.kind==='free'){assert.deepEqual(ui('started').state,ui('declined').state);assert.deepEqual(ui('saved').state,ui('restored').state);assert.ok(!ui('cancelled').state.progression?.pending);}
   if(item.kind==='adventure')assert.deepEqual(ui('saved').state,ui('restored').state);
   assert.equal(ui('finished').state.phase,'finished');assert.equal(ui('finished').state.endingId,'success');
   const fresh=await play([key('F9'),{checkpoint:'fresh',screenshot:resolve(evidence,'fresh.png')}],'fresh');
   assert.deepEqual(fresh.steps.find(s=>s.command.checkpoint==='fresh').ui.state,ui('finished').state);
  });
 }
});
