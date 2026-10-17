import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdtemp} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/index.js';
import {resolveScreenElements} from '../../dist/packages/visual/components.js';
import {widgetCommand} from '../../dist/packages/visual/commands.js';
import {dataBindingText} from '../../dist/packages/visual/data-binding.js';
import {resolvePresentation,presentationActivity} from '../../dist/packages/visual/variants.js';
import {nativeRequest} from '../../dist/packages/native-bridge/index.js';

const authoring=process.env.RAISING_COMPONENT_CONTEXT;
test('Qt-authored context cards: independent actors/actions/activity assignment, conditions, saves and ending', {skip:!authoring,timeout:180000},async()=>{
 const info=JSON.parse(await readFile(authoring,'utf8')),service=new QtStudioService();await service.handle({method:'open',params:{root:info.root}});
 const built=await service.editor.snapshot(service.editor.revision);assert.equal(built.ok,true,JSON.stringify(built.diagnostics));const c=built.snapshot;
 const out=await mkdtemp(resolve('.cache/component-context/play-')),bundle=resolve(out,'windows'),save=resolve(out,'save.json');
 await exportGame({content:c,projectRoot:info.root,destination:bundle,target:'windows',runtimes:{windows:resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-component-context'),web:resolve('dist/web')}});
 const elements=resolveScreenElements(c.project.visual,info.screen),cards=elements.filter(e=>e.id.startsWith('instance-'));
 const actions=cards.filter(e=>e.action==='executeActorAction'),assign=cards.filter(e=>e.action==='assignActivity');assert.equal(actions.length,2);assert.equal(assign.length,2);
 const button=(type,when)=>elements.find(e=>e.action===type&&(!when||e.visibleWhen===when)).id;
 let session=createSession(c,42,{mode:'full'}).session;const expected={};let steps=[];
 const send=command=>{const r=dispatchSession(c,session,command,session.state.stateRevision);assert.equal(r.ok,true,JSON.stringify(r.error));session=r.session;};
 const node=()=>{const n=session.state.activeNarrative;return n?(n.kind==='event'?c.events:c.endings).find(e=>e.id===n.id).nodes.find(x=>x.id===n.nodeId):null;};
 const settle=()=>{for(let i=0;i<100;++i){if(session.state.phase==='period-end')send({type:'resolvePeriodEnd'});else if(session.state.phase==='narrative'&&!['dialogue','choice'].includes(node()?.type))send({type:'advanceNarrative'});else return;}throw Error('Unbounded settle');};
 const checkpoint=id=>{steps.push({checkpoint:id});expected[id]=structuredClone(session.state);};
 async function run(name){const script=resolve(out,name+'-steps.json'),report=resolve(out,name+'-report.json');await writeFile(script,JSON.stringify(steps));const child=spawnSync(resolve(bundle,'RaisingPlayer.exe'),['--bundle',bundle,'--seed','42','--save',save,'--automation',script,'--report',report],{encoding:'utf8',windowsHide:true,timeout:90000});const actual=JSON.parse(await readFile(report,'utf8'));assert.equal(child.status,0,JSON.stringify(actual.error??child.stderr));assert.equal(actual.ok,true);
   for(const step of actual.steps){if(!step.command.checkpoint)continue;const id=step.command.checkpoint,state=expected[id],ui=step.ui;assert.deepEqual(ui.state,state,id);assert.equal(ui.screenId,info.screen);
     for(const card of cards){const projected=resolvePresentation(card,presentationActivity(state,card),condition=>nativeRequest({operation:'evaluate',content:c,state,condition}).value===true);
       if(card.kind==='text'&&card.binding==='data-value')assert.equal(ui.texts[card.id],dataBindingText(card,{stats:c.attributes.map(a=>({...a,value:state.attributes[a.id]})),dataState:state}),id+'/'+card.id);
       else if(card.kind==='text'&&card.variants)assert.equal(ui.texts[card.id],projected.text,id+'/'+card.id);
       else if(card.binding==='context-name'){const field=card.contextField,rows=field==='actorId'?c.characters:field==='actionId'?c.project.gameplay.actions:c.activities;const row=rows.find(x=>x.id===card.uiContext[field]);assert.equal(ui.texts[card.id],row.name??row.label,id+'/'+card.id);}
     }
   }steps=[];return actual;
 }
 steps.push({click:button('start','title')});checkpoint('initial');steps.push({screenshot:resolve(out,'initial.png')});
 steps.push({click:actions[1].id});send(widgetCommand(actions[1],session.state));checkpoint('second-action');assert.equal(session.state.roster.attributes[info.actors[0]].skill,10);assert.equal(session.state.roster.attributes[info.actors[1]].skill,47);
 steps.push({click:button('save')});const middle=structuredClone(session);steps.push({click:actions[1].id});send(widgetCommand(actions[1],session.state));checkpoint('second-again');steps.push({click:button('load')});session=middle;checkpoint('middle-restored');
 const first=await run('actions');const initial=first.steps.find(x=>x.command.checkpoint==='initial').ui;assert.equal(initial.elements.find(x=>x.id===actions[0].id).enabled,false);assert.equal(initial.elements.find(x=>x.id===actions[1].id).enabled,true);
 steps.push({click:button('load')});checkpoint('fresh-middle');await run('fresh-middle');
 steps.push({click:button('load')});checkpoint('resume');let executedFirst=false;
 for(let guard=0;session.state.phase!=='finished'&&guard<120;++guard){
   if(['planning','ready'].includes(session.state.phase)){
     if(!executedFirst&&session.state.periodIndex>0){steps.push({click:actions[0].id});send(widgetCommand(actions[0],session.state));executedFirst=true;checkpoint('first-action');}
     for(const card of assign){steps.push({click:card.id});send(widgetCommand(card,session.state));}checkpoint('assigned-'+session.state.periodIndex);
     steps.push({click:'roster-execute'});send({type:'executeActivity'});checkpoint('result-'+session.state.periodIndex);steps.push({screenshot:resolve(out,'result-'+session.state.periodIndex+'.png')});
   }else if(session.state.phase==='activity-result'){
     if(session.state.periodIndex===0){steps.push({click:button('save')});const saved=structuredClone(session);steps.push({click:button('advance')});send({type:'acknowledgeResult'});settle();steps.push({click:button('load')});session=saved;checkpoint('result-restored');}
     steps.push({click:button('advance')});send({type:'acknowledgeResult'});
   }else if(session.state.phase==='narrative'){if(node().type==='choice'){const id=node().choices[0].id;steps.push({click:'choose-'+id});send({type:'choose',choiceId:id});}else{steps.push({click:button('advance')});send({type:'advanceNarrative'});}}
   else throw Error(session.state.phase);settle();
 }
 assert.equal(session.state.phase,'finished');assert.equal(executedFirst,true);checkpoint('ending');steps.push({click:button('save')},{click:button('start','finished')},{click:button('load')});checkpoint('ending-restored');await run('campaign');
 steps.push({click:button('load')});checkpoint('fresh-ending');await run('fresh-ending');
 await writeFile(resolve('.cache/component-context/play-evidence.json'),JSON.stringify({out,bundle,checkpoints:Object.keys(expected).length,ending:session.state.endingId,actors:info.actors,actions:info.actions},null,2));console.log('Actual native context play evidence: '+out);
});
