import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,writeFile,access} from 'node:fs/promises';
import path from 'node:path';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {authoringPreview} from '../../apps/qt-studio/authoring-preview.mjs';

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
