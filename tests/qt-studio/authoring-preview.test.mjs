import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,writeFile,access} from 'node:fs/promises';
import path from 'node:path';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {authoringPreview} from '../../apps/qt-studio/authoring-preview.mjs';
import {nativeRequest} from '../../dist/packages/native-bridge/index.js';
import {encodeSave} from '../../dist/packages/runtime-service/save.js';
import {RuleDebugSession} from '../../dist/packages/rule-debug/index.js';

async function fixture() {
  await mkdir('.cache/authoring-preview-tests',{recursive:true});
  const work=await mkdtemp(path.resolve('.cache/authoring-preview-tests/run-'));
  const root=path.join(work,'project');await cp('tests/fixtures/projects/component-context',root,{recursive:true});
  const runtime=path.join(work,'runtime');await mkdir(runtime);
  await writeFile(path.join(runtime,'RaisingPlayer.exe'),'contract fixture');
  await writeFile(path.join(runtime,'capabilities.json'),JSON.stringify({formatVersion:'1',imageExtensions:['.png'],audioExtensions:['.ogg'],fontExtensions:['.ttf'],visualFeatures:['renderer-preview-v1','authoring-preview-v1','component-context-v1','custom-states-v1','presentation-variants-v1','free-screens-v1','relationship-change-v1','roster-activities-v1','character-scoped-rules-v1','data-ui-v1','generic-rules-v1','components-v1','anchors-v1']}));
  const service=new QtStudioService({runtimes:{windows:runtime,web:runtime}});await service.handle({method:'open',params:{root}});
  const {snapshot:content}=await service.editor.snapshot(service.editor.revision);
  return {service,content,root,runtime};
}
test('inspection is isolated and binds only one component; explanations use resolved owners',async()=>{
  const {content}=await fixture();const before=structuredClone(content);
  const group=content.project.stateGroups[0],activity=content.activities.find(a=>a.label.includes('연회'));
  const value=authoringPreview(content,{screenId:'screen-1',elementId:'instance-1__element-1',actorId:'trainee-2',activityId:activity.id,attributes:{skill:15},states:{[group.id]:group.values[2].id}});
  assert.equal(value.state.roster.attributes['trainee-2'].skill,15);assert.equal(value.state.roster.attributes.apprentice.skill,10);
  assert.equal(value.state.customStates.shared[group.id],group.values[2].id);
  assert.equal(value.visual.screens.find(s=>s.id==='screen-1').elements.find(e=>e.id==='instance-2').uiContext.actorId,'trainee-2');
  assert.equal(value.explanation.rules.find(r=>r.elementId==='instance-1__element-5').enabled,false);
  const variant=value.explanation.rules.find(r=>r.elementId==='instance-1__element-1').variants;
  assert.equal(variant.filter(r=>r.selected).length,1);assert.ok(variant.some(r=>r.reason==='활동 불일치'));
  assert.deepEqual(content,before);
  for(const changes of [{actorId:'missing'},{activityId:'missing'},{attributes:{skill:999}},{states:{[group.id]:'missing'}},{flags:{missing:true}},{elementId:'gone'}])
    assert.throws(()=>authoringPreview(content,{screenId:'screen-1',elementId:'instance-1',actorId:'apprentice',...changes}),/제작 미리보기/);
});
test('routing explains priority; preview never edits draft, disk, history or saves',async()=>{
  const {service,content,root,runtime}=await fixture();const before=service.editor.view();
  // Use the fixture's real conditional rule (if any); preserve valid authoring shape.
  const current=authoringPreview(content,{screenId:'screen-1',followRouting:true});
  assert.equal(current.screenId,current.explanation.routing.screenId);assert.equal(current.explanation.forced,false);
  const disk=await readFile(path.join(root,'project.json'));
  const response=await service.handle({method:'preview',params:{revision:before.revision,authoringContext:{screenId:'screen-1',elementId:'instance-1',actorId:'trainee-2',attributes:{skill:25}}}});
  const inspection=JSON.parse(await readFile(response.play.inspection,'utf8'));
  assert.equal(inspection.state.roster.attributes['trainee-2'].skill,25);assert.equal(response.play.gameplayValidated,false);
  for(const key of ['documents','revision','dirty','undo','redo','diskVersion'])assert.deepEqual(service.editor.view()[key],before[key]);
  assert.deepEqual(await readFile(path.join(root,'project.json')),disk);await assert.rejects(access(response.play.save));
  const caps=path.join(runtime,'capabilities.json'),old=JSON.parse(await readFile(caps,'utf8'));old.visualFeatures=['renderer-preview-v1'];await writeFile(caps,JSON.stringify(old));
  await assert.rejects(service.handle({method:'preview',params:{revision:before.revision,authoringContext:{screenId:'screen-1'}}}),/authoring-preview-v1/);
});

test('custom state selects conditional screen and reports shadowed matching routes',async()=>{
  const {content}=await fixture();const c=structuredClone(content),g=c.project.stateGroups[0];
  c.project.visual.screens.push({id:'inspection-other',label:'다른 화면',elements:[]});
  c.project.visual.routing={defaultScreenId:'screen-1',phases:[],conditions:[
    {label:'연회 상태',screenId:'inspection-other',when:{op:'stateIs',stateGroupId:g.id,stateValueId:g.values[2].id}},
    {label:'다른 연회 연결',screenId:'screen-1',when:{op:'stateIs',stateGroupId:g.id,stateValueId:g.values[2].id}}
  ]};
  assert.equal(authoringPreview(c,{screenId:'screen-1',followRouting:true}).screenId,'screen-1');
  const result=authoringPreview(c,{screenId:'screen-1',followRouting:true,states:{[g.id]:g.values[2].id}});
  assert.equal(result.screenId,'inspection-other');assert.equal(result.explanation.routing.candidates[0].selected,true);
  assert.match(result.explanation.routing.candidates[1].reason,/상위 연결/);
  assert.equal(authoringPreview(c,{screenId:'screen-1',states:{[g.id]:g.values[2].id}}).screenId,'screen-1');
});

test('legacy single-character project needs no roster or custom state migration',async()=>{
  const service=new QtStudioService();await service.handle({method:'open',params:{root:'examples/academy-visual'}});
  const {snapshot:content}=await service.editor.snapshot(service.editor.revision);
  const attribute=content.attributes[0],value=attribute.min;
  const result=authoringPreview(content,{screenId:content.project.visual.screens[0].id,attributes:{[attribute.id]:value}});
  assert.equal(result.state.attributes[attribute.id],value);assert.equal(result.state.roster,undefined);
  assert.equal(service.editor.view().dirty,false);
});

test('event inspection follows the chosen actor even during another actors dialogue',async()=>{
  const content=structuredClone((await fixture()).content);
  const event=content.events.find(e=>e.id==='exam-invitation');
  event.requirements.left.target.owner={kind:'actor'};event.requirements.right.value=30;
  const initial=authoringPreview(content,{screenId:'screen-1'}).state;
  const baseline=structuredClone(initial);
  baseline.activeNarrative={kind:'event',id:event.id,nodeId:'intro',actorId:'apprentice',returnPhase:'planning'};baseline.phase='narrative';
  const before=structuredClone(baseline);
  const first=authoringPreview(content,{screenId:'screen-1',actorId:'apprentice',eventId:event.id},baseline);
  const second=authoringPreview(content,{screenId:'screen-1',actorId:'trainee-2',eventId:event.id},baseline);
  assert.equal(first.explanation.event.condition.leftValue,10);assert.equal(first.explanation.event.condition.value,false);
  assert.equal(second.explanation.event.condition.leftValue,40);assert.equal(second.explanation.event.condition.value,true);
  assert.equal(second.explanation.values.find(v=>v.id==='skill').value,40);
  assert.equal(second.state.activeNarrative.actorId,'apprentice');assert.deepEqual(baseline,before);
  const debug=new RuleDebugSession(content,1),prior=debug.view();debug.inspectSnapshot(baseline,{kind:'event',id:event.id,actorId:'trainee-2'});assert.deepEqual(debug.view().state,prior.state);
  assert.throws(()=>authoringPreview(content,{screenId:'screen-1',eventId:'missing'}),/검사 대상/);
});

test('character flags and phase visibility match the rendered inspection without leaking shared records',async()=>{
  const content=structuredClone((await fixture()).content);const flag=content.flags[0];
  content.events[0].requirements={op:'flag',flagId:flag.id,equals:true,owner:{kind:'actor'}};
  content.project.visual.screens.find(s=>s.id==='screen-1').elements.push({id:'phase-choice',kind:'text',text:'choice',x:0,y:0,width:100,height:30,visibleWhen:'choice'});
  const selected=authoringPreview(content,{screenId:'screen-1',actorId:'trainee-2',eventId:content.events[0].id,flagScope:'character',flags:{[flag.id]:true}});
  assert.equal(selected.state.roster.flags['trainee-2'][flag.id],true);
  assert.notEqual(selected.state.flags[flag.id],true);assert.notEqual(selected.state.roster.flags.apprentice?.[flag.id],true);
  assert.equal(selected.explanation.event.condition.leftValue,true);assert.equal(selected.explanation.modified,true);
  assert.ok(selected.explanation.event.condition.subjectLabel.includes(flag.label??flag.name??''));
  assert.equal(selected.explanation.rules.find(r=>r.elementId==='phase-choice').visible,false);
  const state=structuredClone(selected.state);state.phase='narrative';state.activeNarrative={kind:'event',id:'exam-invitation',nodeId:'registration',actorId:'trainee-2',returnPhase:'planning'};
  const choice=authoringPreview(content,{screenId:'screen-1'},state);
  assert.equal(choice.explanation.phase,'choice');assert.equal(choice.explanation.rules.find(r=>r.elementId==='phase-choice').visible,true);
});

test('preview imports replay-validated saves and rejects corrupt states without editing project or source save',async()=>{
  const {service,content,root}=await fixture();
  const origin={seed:42,mode:'full',initialOverrides:{attributes:{},flags:{},relationships:{}}};
  const initial=nativeRequest({operation:'create',content,seed:42,options:origin});assert.equal(initial.ok,true);
  const command={type:'executeActorAction',actorId:'trainee-2',actionId:'action-1',quantity:1};
  const next=nativeRequest({operation:'command',content,state:initial.state,command,expectedRevision:initial.state.stateRevision});assert.equal(next.ok,true,JSON.stringify(next.error));
  const save=await encodeSave(content,{origin,commands:[command],state:next.state});assert.equal(save.ok,true,JSON.stringify(save));
  const savePath=path.join(root,'preview-input.json');await writeFile(savePath,save.text);
  const before=service.editor.view();
  const params={revision:before.revision,authoringContext:{screenId:'screen-1',actorId:'trainee-2',savePath}};
  const reply=await service.handle({method:'preview',params});
  const inspection=JSON.parse(await readFile(reply.play.inspection,'utf8'));
  assert.deepEqual(inspection.state,next.state);assert.equal(inspection.explanation.source,'validated-save');assert.equal(inspection.explanation.modified,false);
  assert.equal(inspection.explanation.values.find(v=>v.id==='skill').value,45);
  assert.equal(await readFile(savePath,'utf8'),save.text);
  const changed=JSON.parse(save.text);changed.state.roster.attributes['trainee-2'].skill+=1;const bad=path.join(root,'invalid-preview.json');await writeFile(bad,JSON.stringify(changed));
  await assert.rejects(service.handle({method:'preview',params:{...params,authoringContext:{...params.authoringContext,savePath:bad}}}),/미리보기 저장 불러오기/);
  for(const key of ['documents','revision','dirty','undo','redo'])assert.deepEqual(service.editor.view()[key],before[key]);
});
