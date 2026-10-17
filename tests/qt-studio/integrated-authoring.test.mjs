import test from 'node:test';
import assert from 'node:assert/strict';
import {authoringWorkspace,workspaceImpact} from '../../dist/packages/authoring-workspace/index.js';

test('named workspace indexes designer states, values, UI elements, conditional screens and presentation without mutating documents',()=>{
  const docs=[{file:'project.json',kind:'project',data:{stateGroups:[{id:'mode',name:'행동 모드',initialValueId:'idle',values:[{id:'idle',name:'평상시'},{id:'training',name:'수련'}]}],gameplay:{actions:[{id:'train',name:'검법 수련',effects:[{op:'setState',stateGroupId:'mode',stateValueId:'training'}]}]},visual:{screens:[{id:'home',label:'우리집',elements:[{id:'start',kind:'button',text:'수련 시작',action:'executeAction',actionId:'train'},{id:'caption',kind:'text',text:'평상시',variants:[{label:'수련 모습',when:{op:'stateIs',stateGroupId:'mode',stateValueId:'training'},values:{text:'수련 중'}}]}]}],routing:{defaultScreenId:'home',phases:[],conditions:[{label:'수련 화면',screenId:'home',when:{op:'stateIs',stateGroupId:'mode',stateValueId:'training'}}]}}}},{file:'activities/study.json',kind:'activity',data:{id:'study',label:'독서',baseEffects:[{op:'setState',stateGroupId:'mode',stateValueId:'training'}]}}];
  const before=structuredClone(docs),index=authoringWorkspace(docs);assert.deepEqual(docs,before);
  for(const key of ['stateGroup:mode','stateValue:mode/training','screenElement:home/start','presentation:home/caption/0','screenRoute:0','screenRouting:routing'])assert.ok(index.nodes.some(n=>n.key===key),key);
  assert.equal(index.nodes.find(n=>n.key==='screenElement:home/start').label,'우리집 · 수련 시작');
  assert.ok(index.edges.some(e=>e.from==='screenElement:home/start'&&e.to==='action:train'&&e.resolved));
  assert.ok(index.edges.some(e=>e.from==='activity:study'&&e.to==='stateValue:mode/training'&&e.relation==='effect'));
  const impact=workspaceImpact(index,'stateGroup:mode');for(const from of ['action:train','screenRoute:0','presentation:home/caption/0'])assert.ok(impact.paths.some(e=>e.from===from));
  assert.ok(index.edges.some(e=>e.from==='screenRouting:routing'&&e.to==='screen:home'));
});

test('invalid draft entries retain exact edit positions for nested workspace nodes',()=>{
  const index=authoringWorkspace([{file:'project.json',kind:'project',data:{stateGroups:[null,{id:'mode',name:'모드',values:[null,{id:'idle',name:'대기'}]}],visual:{screens:[null,{id:'home',elements:[null,{id:'caption',kind:'text',variants:[false,{label:'기본'}]}]}],routing:{conditions:[null,{label:'대기 화면',screenId:'home'}]}}}}]);
  const paths=Object.fromEntries(index.nodes.map(n=>[n.key,n.path]));
  assert.equal(paths['stateValue:mode/idle'],'/stateGroups/1/values/1');
  assert.equal(paths['screenElement:home/caption'],'/visual/screens/1/elements/1');
  assert.equal(paths['presentation:home/caption/1'],'/visual/screens/1/elements/1/variants/1');
  assert.equal(paths['screenRoute:1'],'/visual/routing/conditions/1');
});
