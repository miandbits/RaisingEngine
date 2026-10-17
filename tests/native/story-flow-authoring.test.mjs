import test from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import {mkdir,readFile,writeFile,cp,mkdtemp} from 'node:fs/promises';
import {execFileSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {createRun,dispatchCommand} from '../../dist/packages/core/run.js';
import {nativeCreateRun,nativeDispatchCommand,nativeReplay} from '../../dist/packages/native-bridge/index.js';
import {encodeSave,decodeSave} from '../../dist/packages/runtime-service/save.js';

const root=path.resolve('.cache/story-flow-tests');await mkdir(root,{recursive:true});
const exe=path.resolve(process.env.STORY_FLOW_TEST_EXE??'dist/qt-studio-creator-flow/qt-story-flow-test.exe');
const env={...process.env,PATH:`C:\\Qt\\6.10.1\\mingw_64\\bin;C:\\Qt\\Tools\\mingw1310_64\\bin;${process.env.PATH}`};
const base=await loadProject(path.resolve('examples/academy-demo'));
const view={documents:base.documents.map(d=>({file:d.file,kind:d.kind,data:d.data}))};
const settings={id:'story-test-flow',name:'약초밭의 약속',newCharacter:true,characterName:'소하',newRelationship:true,relationshipLabel:'신뢰',relationshipInitial:0,newActivity:true,activityLabel:'약초 손질',costAttributeId:'money',cost:2,gainAttributeId:'skill',gain:1,encounterTitle:'약초밭의 부탁',encounterText:'함께 약초를 살펴볼래?',prompt:'어떻게 답할까?',fromPeriod:1,throughPeriod:6,choices:[
 {id:'yes',label:'도와줄게',reply:'고마워.',delta:3,followupEnabled:true,followupTitle:'다시 만난 소하',followupText:'약속을 지켰구나.',minimumRelationship:3,followupFrom:2},
 {id:'no',label:'오늘은 쉬고 싶어',reply:'다음에 보자.',delta:-1,followupEnabled:false,followupFrom:2,minimumRelationship:0}
]};
async function plan(v,s){const dir=await mkdtemp(path.join(root,'plan-'));const input=path.join(dir,'input.json'),output=path.join(dir,'output.json');await writeFile(input,JSON.stringify({view:v,settings:s}));execFileSync(exe,[input,output],{env,windowsHide:true});return JSON.parse(await readFile(output,'utf8'));}
function updated(v,p){const docs=structuredClone(v.documents);for(const e of p.edits)if(e.type==='replace'){const i=docs.findIndex(d=>d.file===e.file);const kind=i>=0?docs[i].kind:e.file.startsWith('characters/')?'character':e.file.startsWith('activities/')?'activity':'event';const d={file:e.file,kind,data:e.data};if(i<0)docs.push(d);else docs[i]=d;}return {documents:docs};}
async function snapshot(p){const dir=await mkdtemp(path.join(root,'project-'));await cp(path.resolve('examples/academy-demo'),dir,{recursive:true});for(const e of p.edits)if(e.type==='replace'){const f=path.join(dir,e.file);await mkdir(path.dirname(f),{recursive:true});await writeFile(f,JSON.stringify(e.data));}const b=await buildSnapshot(await loadProject(dir));assert.equal(b.ok,true,JSON.stringify(b.diagnostics));return b.snapshot;}

test('story authoring compiles valid ordinary documents, preserves shared data and round-trips edits',async()=>{
 const p=await plan(view,settings);assert.equal(p.error,'');const c=await snapshot(p);
 assert.equal(c.characters.find(x=>x.id==='mentor').name,base.documents.find(x=>x.kind==='character'&&x.data.id==='mentor').data.name);
 assert.equal(c.events.find(x=>x.id==='story-test-flow-meeting').nodes.find(x=>x.type==='choice').choices.length,2);
 const v=updated(view,p),s=structuredClone(p.settings);s.choices[0].delta=4;
 const p2=await plan(v,s);assert.equal(p2.error,'');assert.equal(p2.edits.filter(e=>e.type==='add').length,0,'reopen updates existing documents');await snapshot(p2);
 const external=structuredClone(v);external.documents.find(d=>d.data.id==='story-test-flow-meeting').data.nodes[0].text='An advanced edit';
 assert.match((await plan(external,s)).error,/다른 편집기/,'external narrative changes cannot be lost');
});
test('invalid references, impossible dates, duplicate branch ids and unlinked schedule groups are rejected atomically',async()=>{
 for(const change of [s=>{s.costAttributeId='missing';},s=>{s.fromPeriod=8;},s=>{s.choices[0].minimumRelationship=200;},s=>{s.choices[1].id='yes';}]){const s=structuredClone(settings);change(s);const p=await plan(view,s);assert.ok(p.error);assert.deepEqual(p.edits,[]);}
 const v=structuredClone(view);v.documents.find(d=>d.kind==='project').data.visual={screens:[{id:'schedule',elements:[{id:'schedule',kind:'schedule',scheduleGroups:[{id:'study',label:'수업',activityIds:['study']}]}]}]};
 assert.match((await plan(v,settings)).error,/분류/);
 const s={...settings,scheduleLink:{screenId:'schedule',elementId:'schedule',groupId:'study'}};const p=await plan(v,s);assert.equal(p.error,'');
 assert.ok(p.edits.find(e=>e.file==='project.json').data.visual.screens[0].elements[0].scheduleGroups[0].activityIds.includes('story-test-flow-activity'));
});
test('removed follow-up is disabled without deleting referenced documents',async()=>{
 const first=await plan(view,settings),s=structuredClone(first.settings);s.choices[0].followupEnabled=false;
 const p=await plan(updated(view,first),s);assert.equal(p.error,'');const e=p.edits.find(e=>e.file==='events/story-test-flow-yes-next.json');assert.deepEqual(e.data.requirements,{op:'not',condition:{op:'always'}});assert.ok(!p.edits.some(e=>e.type==='remove'));
});
test('existing person, relationship and activity retain their advanced data when linked',async()=>{
 const person=base.documents.find(d=>d.kind==='character'&&d.data.id==='mentor').data;
 const activity=base.documents.find(d=>d.kind==='activity'&&d.data.id==='study').data;
 const s={...settings,newCharacter:false,characterId:person.id,newRelationship:false,relationshipId:person.relationships[0].id,newActivity:false,activityId:activity.id};
 const p=await plan(view,s);assert.equal(p.error,'');
 assert.ok(!p.edits.some(e=>e.file==='characters/mentor.json'),'existing character remains byte-for-byte untouched');
 const linked=p.edits.find(e=>e.file==='activities/study.json').data;
 assert.deepEqual({...linked,baseEffects:linked.baseEffects.slice(0,-1)},activity,'only the completion marker is added');
 await snapshot(p);
 const p2=await plan(updated(view,p),p.settings);assert.equal(p2.error,'');
 assert.equal(p2.edits.find(e=>e.file==='activities/study.json').data.baseEffects.length,linked.baseEffects.length,'reopening does not duplicate activity effects');
});
test('activity → choice → relationship → follow-up runs in native and TS; unchosen branch stays closed; saves replay',async()=>{
 const p=await plan(view,settings);assert.equal(p.error,'');const c=await snapshot(p);
 for(const choice of ['yes','no']){
  const origin={seed:73,mode:'full',initialOverrides:{attributes:{},flags:{},relationships:{}}},commands=[];
  let state=createRun(c,73,origin).state;assert.deepEqual(nativeCreateRun(c,73,origin).state,state);
  const send=command=>{const r=dispatchCommand(c,state,command,state.stateRevision),n=nativeDispatchCommand(c,state,command,state.stateRevision);assert.equal(r.ok,true,JSON.stringify(r));assert.deepEqual(n,r);state=r.state;commands.push(command);};
  const save=async()=>{const e=await encodeSave(c,{origin,commands,state});assert.equal(e.ok,true,JSON.stringify(e));const d=await decodeSave(c,e.text);assert.equal(d.ok,true,JSON.stringify(d));assert.deepEqual(d.session.state,state);assert.deepEqual(nativeReplay(c,origin,commands),{ok:true,state});state=d.session.state;};
  send({type:'setSchedule',activityIds:['story-test-flow-activity','rest','rest']});send({type:'executeActivity'});send({type:'acknowledgeResult'});send({type:'resolvePeriodEnd'});
  assert.equal(state.activeNarrative.id,'story-test-flow-meeting');await save();
  send({type:'advanceNarrative'});send({type:'choose',choiceId:choice});await save();
  for(let n=0;state.phase==='narrative'&&n<10;++n)send({type:'advanceNarrative'});
  assert.equal(state.relationships['story-test-flow-person']['story-test-flow-bond'],choice==='yes'?3:-1);await save();
  send({type:'executeActivity'});send({type:'acknowledgeResult'});send({type:'resolvePeriodEnd'});
  if(choice==='yes'){assert.equal(state.activeNarrative.id,'story-test-flow-yes-next');await save();for(let n=0;state.phase==='narrative'&&n<10;++n)send({type:'advanceNarrative'});assert.ok(state.completedEvents.includes('story-test-flow-yes-next'));}
  else assert.ok(!state.completedEvents.includes('story-test-flow-yes-next'));
  await save();
 }
});
