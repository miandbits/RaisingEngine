import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/index.js';

const click=id=>({click:id}),key=id=>({key:id});
function route(content,kind,evidence){
 let session=createSession(content,42,{mode:'full'}).session;
 const steps=[click('start')],expected={},milestones=[],commands=[];
 function send(command){const r=dispatchSession(content,session,command,session.state.stateRevision);assert.equal(r.ok,true,JSON.stringify({command,error:r.error,phase:session.state.phase}));session=r.session;commands.push(command);}
 function node(){const c=session.state.activeNarrative;return c?(c.kind==='event'?content.events:content.endings).find(s=>s.id===c.id)?.nodes.find(n=>n.id===c.nodeId):null;}
 function settle(){for(let i=0;i<1000;i++){if(session.state.phase==='period-end')send({type:'resolvePeriodEnd'});else if(session.state.phase==='narrative'&&!['dialogue','choice'].includes(node()?.type))send({type:'advanceNarrative'});else return;}throw Error('settle loop');}
 function checkpoint(id,screenshot=false){expected[id]=structuredClone(session.state);steps.push({checkpoint:id,...(screenshot?{screenshot:resolve(evidence,kind+'-'+id+'.png')}:{})});}
 function action(command,ui){steps.push(...ui);send(command);settle();}
 function restoreTrial(command,ui,id){const saved=structuredClone(session),length=commands.length;steps.push(key('F5'));checkpoint(id+'-saved',true);action(command,ui);checkpoint(id+'-changed');steps.push(key('F9'));session=saved;commands.length=length;checkpoint(id+'-restored');milestones.push(id);}
 function stories(save=false){let count=0;while(session.state.phase==='narrative'){const n=node();assert.ok(n);const choice=n.type==='choice'?n.choices.find(c=>c.id==='free')??n.choices[0]:null;const command=choice?{type:'choose',choiceId:choice.id}:{type:'advanceNarrative'},ui=[click(choice?'choose-'+choice.id:'advance')];if(save&&count===0)restoreTrial(command,ui,'story-'+session.state.periodIndex);action(command,ui);if(++count>80)throw Error('narrative loop');}}
 function visit(locationId,button='visit-'+locationId){action({type:'visit',locationId},[click(button)]);stories();}
 function navigate(screenId,button){action({type:'navigate',screenId},[click(button)]);}
 checkpoint('started');
 if(kind==='success'){
  navigate('screen-2','button-2');checkpoint('living-before');steps.push(click('action__rules-1__action-1'),click('cancel-purchase'));checkpoint('living-cancelled');action({type:'executeAction',actionId:'action-1',quantity:1},[click('action__rules-1__action-1'),click('confirm-purchase')]);checkpoint('meal',true);navigate('home','button-1');
 }
 if(kind!=='ordinary'){
  visit('market');action({type:'buy',shopId:'florist',itemId:'flower',quantity:1},[click('buy-florist-flower'),click('confirm-purchase')]);
  if(kind==='success')action({type:'buy',shopId:'florist',itemId:'training-sword',quantity:1},[click('buy-florist-training-sword'),click('confirm-purchase')]);
  visit('city');action({type:'interact',interactionId:'gift-yuna'},[click('interact-gift-yuna')]);checkpoint('relationship-event',true);stories(true);visit('home');
 }
 function month(plan,index){navigate('schedule','schedule-link');action({type:'setSchedule',activityIds:plan},[...plan.map((value,i)=>({select:'plan-'+i,value})),click('advance')]);for(let i=0;i<3;i++){action({type:'executeActivity'},[click('advance')]);checkpoint('month-'+index+'-period-'+i,i===0);if(i===1&&kind==='success')restoreTrial({type:'acknowledgeResult'},[click('advance')],'month-'+index);action({type:'acknowledgeResult'},[click('advance')]);stories(i===2&&kind==='success');}if(session.state.phase!=='finished'){visit('city','button-new');visit('home');}}
 month(kind==='success'?['study','rest','study']:['rest','rest','rest'],1);
 if(kind==='success'){
  assert.ok(session.state.attributes.skill>=16);
  steps.push(click('direction-greeting'),{timelineAdvanceMs:1000},click('directionPause'),key('F5'),{checkpoint:'direction-saved',screenshot:resolve(evidence,'direction-saved.png')},click('directionPause'),{timelineAdvanceMs:1500},key('F9'),{checkpoint:'direction-restored'},click('directionClose'));checkpoint('after-direction');
  navigate('tactics-camp','tactics-entry');action({type:'tacticsEquip',unitId:'vanguard',equipmentId:'wooden-sword'},[click('party-equip-vanguard-wooden-sword')]);action({type:'acceptQuest',questId:'first-expedition'},[click('accept-quest-first-expedition')]);action({type:'executeAction',actionId:'tactics-depart',quantity:1},[click('party-depart'),click('confirm-purchase')]);action({type:'moveExpedition',nodeId:'clearing'},[click('move-expedition-clearing')]);checkpoint('battle',true);restoreTrial({type:'tacticsFlee'},[click('party-flee')],'retreat');
  let turns=0;
  while(session.state.phase==='battle'){
   const b=session.state.systems.tactics.active,actor=b.units.find(u=>u.id===b.order[b.cursor]),definition=content.project.systems.tactics.units.find(u=>u.id===actor.unitId);
   let chosen;
   for(const skillId of ['spark','strike','venom','mend','guard'].filter(s=>definition.skillIds.includes(s))){for(const target of b.units.filter(u=>u.hp>0)){const c={type:'tacticsAction',unitId:actor.id,skillId,targetId:target.id};if(dispatchSession(content,session,c,session.state.stateRevision).ok){chosen=c;break;}}if(chosen)break;}
   assert.ok(chosen,'battle has a legal action');const ui=[click('party-skill-'+chosen.skillId),click('party-target-'+chosen.targetId)];if(turns===1)restoreTrial(chosen,ui,'combat');action(chosen,ui);if(++turns>80)throw Error('battle loop');
  }
  assert.equal(session.state.systems.tactics.last.outcome,'victory');assert.equal(session.state.flags.graduated,true);checkpoint('victory',true);action({type:'moveExpedition',nodeId:'entrance'},[click('move-expedition-entrance')]);action({type:'returnFromExpedition'},[click('return-expedition')]);action({type:'claimQuest',questId:'first-expedition'},[click('claim-quest-first-expedition')]);stories();checkpoint('quest',true);navigate('home','party-home');
 }
 month(kind==='success'?['study','rest','work']:['rest','rest','rest'],2);
 month(kind==='success'?['rest','study','socialize']:['rest','rest','rest'],3);
 checkpoint('finished',true);assert.equal(session.state.phase,'finished');assert.equal(session.state.endingId,{success:'knight',relationship:'neighbor',ordinary:'wanderer'}[kind]);
 if(kind==='success')steps.push(click('direction-celebration'),{timelineAdvanceMs:2500},{checkpoint:'celebration',screenshot:resolve(evidence,'celebration.png')},click('directionSkip'),click('directionClose'));
 steps.push(key('F5'));return {steps,expected,milestones,commands,final:session.state};
}

test('commercial pilot: three months, living, relationship, party battle, direction and three endings',{timeout:300000},async t=>{
 const proof=JSON.parse(await readFile('.cache/commercial-pilot-proof.json','utf8'));assert.ok(proof.guiEdited&&proof.saveReopen&&proof.undoRedo);
 const built=await buildSnapshot(await loadProject(proof.project));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));assert.equal(built.snapshot.project.durationMonths,3);
 const root=await mkdtemp(resolve('.cache/commercial-player-')),game=resolve(root,'game'),evidence=resolve(process.env.PILOT_EVIDENCE??'docs/evidence/commercial-pilot');await mkdir(evidence,{recursive:true});
 await exportGame({content:built.snapshot,projectRoot:proof.project,destination:game,target:'windows',runtimes:{windows:resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-direction'),web:resolve('dist/web')}});
 for(const kind of ['success','relationship','ordinary']){if(process.env.PILOT_ROUTE&&process.env.PILOT_ROUTE!==kind)continue;await t.test(kind,async()=>{
  const plan=route(built.snapshot,kind,evidence);await writeFile(resolve(evidence,kind+'-commands.json'),JSON.stringify(plan.commands,null,2));const save=resolve(evidence,kind+'-save.json');
  async function play(steps,name){const script=resolve(evidence,name+'-steps.json'),report=resolve(evidence,name+'-report.json');await writeFile(script,JSON.stringify(steps,null,2));const p=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',save,'--automation',script,'--report',report,...(process.env.PILOT_RECORD?['--record-route',resolve(evidence,name+'-recording.json')]:[]),'--hidden'],{encoding:'utf8',windowsHide:true,timeout:120000});const result=JSON.parse(await readFile(report,'utf8'));assert.equal(p.status,0,JSON.stringify(result.error??p.stderr));assert.equal(result.ok,true);return result;}
  const result=await play(plan.steps,kind==='success'?'play':kind);if(process.env.PILOT_RECORD){const recording=JSON.parse(await readFile(resolve(evidence,(kind==='success'?'play':kind)+'-recording.json'),'utf8'));assert.deepEqual(recording.commands,plan.commands);assert.equal(recording.state.endingId,plan.final.endingId);}const ui=id=>result.steps.find(s=>s.command.checkpoint===id)?.ui;
  for(const [id,state] of Object.entries(plan.expected))assert.deepEqual(ui(id)?.state,state,kind+' checkpoint '+id);
  for(const id of plan.milestones)assert.deepEqual(ui(id+'-saved').state,ui(id+'-restored').state);
  if(kind==='success'){assert.deepEqual(ui('living-before').state,ui('living-cancelled').state);assert.deepEqual(ui('direction-saved').direction,ui('direction-restored').direction);}
  const fresh=await play([key('F9'),{checkpoint:'fresh',screenshot:resolve(evidence,kind+'-fresh.png')}],kind+'-fresh');assert.deepEqual(fresh.steps.find(s=>s.command.checkpoint==='fresh').ui.state,plan.final);
  await writeFile(resolve(evidence,kind+'-verification.json'),JSON.stringify({ok:true,game,project:proof.project,ending:plan.final.endingId,periods:plan.final.periodIndex,checkpoints:Object.keys(plan.expected).length,restoreTrials:plan.milestones,actualPlayer:true,freshLoad:true},null,2));
 });}
});
