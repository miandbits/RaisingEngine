import test from 'node:test';
import assert from 'node:assert/strict';
import {writeFile} from 'node:fs/promises';
import {baseProject,addRules} from './helpers.mjs';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {validateProject} from '../../dist/packages/validation/index.js';
import {compileRules} from '../../dist/packages/rule-extensions/compiler.js';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/index.js';
import {encodeSave,decodeSave} from '../../dist/packages/runtime-service/save.js';
import {projectDataUi} from '../../dist/packages/visual/data-ui-projection.js';
import {nativeRequest} from '../../dist/packages/native-bridge/index.js';
import {StudioEditor} from '../../dist/packages/studio-service/editor.js';

const action=actionId=>({type:'executeAction',actionId,quantity:1});
const make=async()=>{const root=await baseProject();await addRules(root);const loaded=await loadProject(root);const built=await buildSnapshot(loaded);assert.equal(built.ok,true,JSON.stringify(built.diagnostics));return {root,loaded,content:built.snapshot};};

test('portable expansion, deterministic progression and atomic failure',async()=>{
  const {content,loaded}=await make();const before=JSON.stringify(loaded);
  const compiled=compileRules(loaded);assert.equal(compiled.diagnostics.length,0);assert.equal(JSON.stringify(loaded),before);
  assert.equal(content.project.ruleExtensions,undefined);assert.equal(content.ruleExtensionLock.compiler,'raising-rules-1');
  assert.equal(content.project.gameplay.actions.length,4);
  let run=createSession(content,23,{mode:'full'}).session;
  const initial=structuredClone(run);
  const early=dispatchSession(content,run,action('rx-flow-1-complete'),run.state.stateRevision);assert.equal(early.ok,false);assert.deepEqual(run,initial);
  const commands=['prepare','practice','complete','restart'].map(n=>action('rx-flow-1-'+n));
  const replay=()=>{let s=createSession(content,23,{mode:'full'}).session;for(const c of commands){const r=dispatchSession(content,s,c,s.state.stateRevision);assert.equal(r.ok,true,JSON.stringify(r.error));s=r.session;}return s;};
  run=replay();assert.equal(run.state.attributes.skill,12);assert.equal(run.state.attributes.money,25);assert.equal(run.state.attributes.stress,2);assert.deepEqual(run,replay());
  const prepared=dispatchSession(content,run,action('rx-flow-1-prepare'),run.state.stateRevision);assert.equal(prepared.ok,true,JSON.stringify(prepared.error));run=prepared.session;
  for(let i=0;i<20;i++){const before=structuredClone(run);const r=dispatchSession(content,run,action('rx-flow-1-practice'),run.state.stateRevision);if(!r.ok){assert.ok(before.state.attributes.money<3,'only the insufficient balance stops repeated training');assert.deepEqual(run,before);return;}run=r.session;}
  assert.fail('unaffordable action must be rejected');
});

test('editor saves authored calls, undo/redo and stale writers retain conflict protection',async()=>{
  const {root}=await make();const a=new StudioEditor(),b=new StudioEditor();await a.open(root);await b.open(root);
  const raw=structuredClone(a.documents.find(d=>d.kind==='project').data);raw.ruleExtensions.progressions[0].args.gain=13;
  a.edit({type:'replace',file:'project.json',data:raw},a.revision);
  a.edit({type:'undo'},a.revision);assert.equal(a.documents.find(d=>d.kind==='project').data.ruleExtensions.progressions[0].args.gain,12);
  a.edit({type:'redo'},a.revision);await a.save(a.revision);await assert.rejects(()=>b.save(b.revision));
  const reread=await loadProject(root),doc=reread.documents.find(d=>d.kind==='project');assert.equal(doc.data.ruleExtensions.progressions[0].args.gain,13);assert.equal(doc.data.gameplay.actions.length,0);
  const built=await buildSnapshot(reread);assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
});

test('content changes pin save identity; old projects keep their snapshot identity',async()=>{
  const {root,content}=await make();const loaded=await loadProject(root);const p=loaded.documents.find(d=>d.kind==='project').data;
  p.ruleExtensions.packages[0].version='1.0.1';p.ruleExtensions.progressions[0].version='1.0.1';
  for(const t of p.ruleExtensions.packages[0].progressions[0].transitions){for(const e of t.effects??[])if(e.op==='ruleEffect')e.version='1.0.1';if(t.requirements)t.requirements.version='1.0.1';}
  const newer=await buildSnapshot(loaded);assert.equal(newer.ok,true,JSON.stringify(newer.diagnostics));assert.notEqual(newer.snapshot.contentFingerprint,content.contentFingerprint);
  const oldSave=await encodeSave(content,createSession(content,4,{mode:'full'}).session);assert.equal(oldSave.ok,true);const rejected=await decodeSave(newer.snapshot,oldSave.text);assert.equal(rejected.ok,false,'changed pack must not silently resume an old save');
  const legacy=await loadProject('examples/academy-demo');const compiled=compileRules(legacy);assert.equal(compiled.documents,legacy.documents);assert.equal(compiled.lock,undefined);
  const one=await buildSnapshot(legacy),two=await buildSnapshot(legacy);assert.deepEqual(one,two);
});

for(const [label,edit,code] of [
  ['missing package',p=>p.ruleExtensions.packages=[],'RULE_PACKAGE_MISSING'],
  ['version mismatch',p=>p.ruleExtensions.progressions[0].version='9.0.0','RULE_VERSION'],
  ['missing argument',p=>{delete p.ruleExtensions.packages[0].progressions[0].parameters.gain.default;delete p.ruleExtensions.progressions[0].args.gain;},'RULE_ARGUMENT'],
  ['argument range',p=>p.ruleExtensions.progressions[0].args.gain=999,'RULE_ARGUMENT'],
  ['unknown argument',p=>p.ruleExtensions.progressions[0].args.typo=1,'RULE_ARGUMENT'],
  ['state reference',p=>p.ruleExtensions.packages[0].progressions[0].transitions[0].to='missing','RULE_STATE'],
  ['duplicate instance',p=>p.ruleExtensions.progressions.push(structuredClone(p.ruleExtensions.progressions[0])),'RULE_DUPLICATE'],
  ['recursive effect',p=>p.ruleExtensions.packages[0].effects[0].body=[{op:'ruleEffect',packageId:'dojo',version:'1.0.0',ruleId:'practice',args:{}}],'RULE_CYCLE'],
  ['generated collision',p=>p.gameplay.actions.push({id:'rx-flow-1-prepare',name:'conflict',costs:[],effects:[]}),'RULE_COLLISION'],
  ['unused recursion',p=>{p.ruleExtensions.progressions=[];p.ruleExtensions.packages[0].effects[0].body=[{op:'ruleEffect',packageId:'dojo',version:'1.0.0',ruleId:'practice',args:{}}];},'RULE_CYCLE'],
  ['undeclared placeholder',p=>p.ruleExtensions.packages[0].effects[0].body[0].value.value={$param:'typo'},'RULE_PARAMETER'],
])test(`reject ${label} with source location`,async()=>{
  const root=await baseProject();const p=await addRules(root);edit(p);await writeFile(root+'/project.json',JSON.stringify(p,null,2));
  const result=await validateProject(await loadProject(root));assert.equal(result.valid,false);const d=result.diagnostics.find(d=>d.code===code);assert.ok(d,JSON.stringify(result.diagnostics));assert.equal(d.file,'project.json');assert.ok(d.line>1);assert.ok(d.pointer.startsWith('/ruleExtensions'),d.pointer);
});

test('expanded effect source map preserves the following authored effect index',async()=>{
  const root=await baseProject();const p=await addRules(root);p.gameplay.actions=[{id:'mapped',name:'mapped',costs:[],effects:[{op:'ruleEffect',packageId:'dojo',version:'1.0.0',ruleId:'practice',args:{}},{op:'add',target:{kind:'attribute',attributeId:'missing'},value:{op:'literal',value:1}}]}];
  await writeFile(root+'/project.json',JSON.stringify(p,null,2));const result=await validateProject(await loadProject(root));
  assert.equal(result.valid,false);assert.ok(result.diagnostics.some(d=>d.code==='REFERENCE_MISSING'&&d.pointer==='/gameplay/actions/0/effects/1/target/attributeId'),JSON.stringify(result.diagnostics));
});

test('two characters keep independent progression and save/replay state',async()=>{
  const root=await baseProject();const p=await addRules(root);p.ruleExtensions.progressions[0].scope='character';
  const actionIds=['prepare','practice','complete','restart'].map(id=>'rx-flow-1-'+id);
  p.roster={stateModel:'character-v1',localAttributeIds:['skill','stamina','stress'],members:['apprentice','mentor'].map(characterId=>({characterId,activityId:'rest',initialAttributes:{skill:0,stamina:50,stress:0},actionIds}))};
  p.visual.screens.find(s=>s.id==='schedule').elements.push({id:'actor-rules',kind:'actions',x:40,y:120,width:800,height:320});
  await writeFile(root+'/project.json',JSON.stringify(p,null,2));const built=await buildSnapshot(await loadProject(root));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));const content=built.snapshot;
  let session=createSession(content,18,{mode:'full'}).session;
  const send=(actorId,id)=>{const r=dispatchSession(content,session,{type:'executeActorAction',actorId,actionId:'rx-flow-1-'+id,quantity:1},session.state.stateRevision);assert.equal(r.ok,true,JSON.stringify(r.error));session=r.session;};
  send('apprentice','prepare');send('apprentice','practice');send('apprentice','complete');
  assert.equal(session.state.customStates.characters.apprentice['rx-flow-1'],'mastered');assert.equal(session.state.customStates.characters.mentor['rx-flow-1'],'ready');
  const beforeProjection=structuredClone(session.state),ui=projectDataUi(content,session.state,null,nativeRequest);
  assert.deepEqual(session.state,beforeProjection,'actor availability projection never mutates state');
  assert.equal(ui.actorActions[0].actions.find(a=>a.id==='rx-flow-1-prepare').enabled,false);
  assert.equal(ui.actorActions[1].actions.find(a=>a.id==='rx-flow-1-prepare').enabled,true,'each actor receives independent canonical availability');
  const saved=await encodeSave(content,session);assert.equal(saved.ok,true);const restored=await decodeSave(content,saved.text);assert.equal(restored.ok,true);assert.deepEqual(restored.session,session);
  session=restored.session;send('mentor','prepare');assert.equal(session.state.customStates.characters.apprentice['rx-flow-1'],'mastered');assert.equal(session.state.customStates.characters.mentor['rx-flow-1'],'training');
});

test('public package JSON Schema accepts the developer sample and rejects unknown fields',async()=>{
  const {default:Ajv}=await import('ajv/dist/2020.js');const {json}=await import('./helpers.mjs');
  const validate=new Ajv({strict:false,allErrors:true}).compile(await json('packages/rule-extensions/package.schema.json'));const pack=await json('examples/rule-extension-lab/dojo.raiserules');assert.equal(validate(pack),true,JSON.stringify(validate.errors));pack.runScript='unsafe';assert.equal(validate(pack),false);
});

test('GUI-exported public package compiles and restores an independently instantiated progression',async()=>{
  const {json}=await import('./helpers.mjs');const {default:Ajv}=await import('ajv/dist/2020.js');
  const pack=await json('examples/rule-extension-lab/visual-authoring.raiserules');
  const schema=new Ajv({strict:false,allErrors:true}).compile(await json('packages/rule-extensions/package.schema.json'));
  assert.equal(schema(pack),true,JSON.stringify(schema.errors));
  const root=await baseProject();const p=await json(root+'/project.json');
  p.ruleExtensions={apiVersion:1,packages:[pack],progressions:[{id:'visual-test',name:'시각적 수련',packageId:pack.id,version:pack.version,ruleId:pack.progressions[0].id,scope:'shared',args:{}}]};
  await writeFile(root+'/project.json',JSON.stringify(p));const built=await buildSnapshot(await loadProject(root));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  const content=built.snapshot;let session=createSession(content,42,{mode:'full'}).session;
  for(const transition of pack.progressions[0].transitions.slice(0,3)) {
    const result=dispatchSession(content,session,action('rx-visual-test-'+transition.id),session.state.stateRevision);assert.equal(result.ok,true,JSON.stringify(result.error));session=result.session;
    const saved=await encodeSave(content,session);assert.equal(saved.ok,true);const loaded=await decodeSave(content,saved.text);assert.equal(loaded.ok,true);assert.deepEqual(loaded.session,session);session=loaded.session;
  }
  assert.equal(session.state.attributes.skill,12);assert.equal(session.state.attributes.money,25);assert.equal(session.state.attributes.stress,2);
  assert.equal(session.state.customStates.shared['rx-visual-test'],pack.progressions[0].states[2].id);
});

test('committed portable sample reaches its recorded ending with automatic save checkpoints',async()=>{
  const {json}=await import('./helpers.mjs');const {runGoalRoute}=await import('../../dist/packages/analysis-service/goal-routes.js');
  const built=await buildSnapshot(await loadProject('examples/rule-extension-lab/game'));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  const route=await json('examples/rule-extension-lab/game/playtests/routes/dojo-breakthrough.json');const report=await runGoalRoute(built.snapshot,route);assert.equal(report.ok,true,JSON.stringify(report));assert.ok(report.saves>=12);
});

test('compiled references receive normal semantic validation mapped to author source',async()=>{
  const root=await baseProject();const p=await addRules(root);p.ruleExtensions.progressions[0].args.stat='missing';await writeFile(root+'/project.json',JSON.stringify(p,null,2));
  const result=await validateProject(await loadProject(root));assert.equal(result.valid,false);assert.ok(result.diagnostics.some(d=>d.pointer.startsWith('/ruleExtensions/progressions/0')&&d.relatedLocations?.length),JSON.stringify(result.diagnostics));
});

test('rule source maps preserve exact ordinary fields and shifted effect locations',async()=>{
  const root=await baseProject();const p=await addRules(root);
  p.gameplay.actions=[{id:'broken',name:'참조 오류',costs:[{attributeId:'absent',amount:1}],effects:[
    {op:'ruleEffect',packageId:'dojo',version:'1.0.0',ruleId:'practice',args:{}},
    {op:'add',target:{kind:'attribute',attributeId:'missing'},value:{op:'literal',value:1}}
  ]}];
  await writeFile(root+'/project.json',JSON.stringify(p,null,2));
  const loaded=await loadProject(root),compiled=compileRules(loaded);assert.equal(compiled.diagnostics.length,0);
  const diagnostic=pointer=>({code:'REFERENCE_MISSING',severity:'error',file:'project.json',pointer,line:1,column:1,message:'missing'});
  assert.equal(compiled.remap(diagnostic('/gameplay/actions/0/costs/0/attributeId')).pointer,'/gameplay/actions/0/costs/0/attributeId');
  assert.equal(compiled.remap(diagnostic('/gameplay/actions/0/effects/2/target/attributeId')).pointer,'/gameplay/actions/0/effects/1/target/attributeId');
  assert.equal(compiled.remap(diagnostic('/gameplay/actions/0/effects/0/target/attributeId')).pointer,'/gameplay/actions/0/effects/0');
  assert.equal(compiled.remap(diagnostic('/gameplay/actions/1/costs/0/attributeId')).pointer,'/ruleExtensions/progressions/0');
  const checked=await validateProject(loaded);
  assert.ok(checked.diagnostics.some(d=>d.pointer==='/gameplay/actions/0/effects/1/target/attributeId'),JSON.stringify(checked.diagnostics));
});
