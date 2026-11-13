import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,readdir} from 'node:fs/promises';
import Ajv from 'ajv/dist/2020.js';
import {saveStorageGuidanceText} from '../../dist/packages/player-service/save-storage-guidance.js';
import {requiredPlayerFeatures,missingPlayerFeatures} from '../../dist/packages/export-service/compatibility.js';
import {resolveScreenElements} from '../../dist/packages/visual/components.js';

test('storage guidance follows actual storage and backup capabilities, with localized neutral preview',()=>{
  const browser=saveStorageGuidanceText({location:'browser'},'en');
  assert.match(browser,/this browser/);assert.match(browser,/Download backup/);
  assert.match(saveStorageGuidanceText({location:'browser',available:false},'en'),/storage is unavailable/);
  assert.match(saveStorageGuidanceText({location:'file'},'en'),/downloaded save files/);
  assert.match(saveStorageGuidanceText({location:'browser',available:true,writeFailed:true},'en'),/last record could not be saved/);
  const local=saveStorageGuidanceText({location:'device'},'en');
  assert.match(local,/this device/);assert.doesNotMatch(local,/browser|Download|Export|Files/);
  const files=saveStorageGuidanceText({location:'device',portableBackup:true},'en');
  assert.match(files,/Export current progress/);assert.match(files,/do not sync automatically/);
  assert.equal(saveStorageGuidanceText(undefined,'en'),saveStorageGuidanceText({location:'preview',portableBackup:true},'en'));
  for(const state of [{location:'browser'},{location:'browser',available:false},{location:'browser',available:true,writeFailed:true},{location:'file'},{location:'device'},{location:'device',portableBackup:true},{location:'preview'}]){
    const before=structuredClone(state);
    assert.match(saveStorageGuidanceText(state,'ko-KR'),/[가-힣]/);
    assert.match(saveStorageGuidanceText(state,'ja-JP'),/[ぁ-んァ-ン]/);
    assert.equal(saveStorageGuidanceText(state,'en-US'),saveStorageGuidanceText(state,'en'));
    assert.deepEqual(state,before,'Guidance never mutates storage or save state');
  }
});

test('guidance is valid on direct and nested shared text; only opted-in content requires the new runtime capability',async()=>{
  const ajv=new Ajv({strict:true,allErrors:true});
  for(const file of(await readdir('packages/schema')).filter(f=>f.endsWith('.schema.json')))ajv.addSchema(JSON.parse(await readFile('packages/schema/'+file,'utf8')));
  const text={id:'note',kind:'text',x:0,y:0,width:500,height:80,binding:'save-storage-guidance',text:'Authored fallback stays untouched'};
  const visual={width:1280,height:720,screens:[{id:'title',label:'Title',elements:[text,{id:'outer',kind:'component',componentId:'wrapper',x:0,y:100,width:500,height:80}]}],components:[{id:'wrapper',label:'Wrapper',width:500,height:80,elements:[{id:'inner',kind:'component',componentId:'footer',x:0,y:0,width:500,height:80}]},{id:'footer',label:'Footer',width:500,height:80,elements:[text]}]};
  const validate=ajv.getSchema('https://raising-engine.example/schemas/0.1.0/visual.schema.json');
  assert.equal(validate(visual),true,ajv.errorsText(validate.errors));
  assert.equal(resolveScreenElements(visual,'title').find(e=>e.id==='outer__inner__note').binding,'save-storage-guidance');
  const content={project:{visual},activities:[],events:[],endings:[],characters:[],attributes:[],assets:[]};
  const features=requiredPlayerFeatures(content);assert.ok(features.includes('save-storage-guidance-v1'));
  assert.deepEqual(missingPlayerFeatures(features,features.filter(f=>f!=='save-storage-guidance-v1')),['save-storage-guidance-v1']);
  const variant=structuredClone(visual);variant.screens[0].elements[0].variants=[{label:'Invalid binding override',when:{op:'always'},values:{binding:'save-storage-guidance'}}];assert.equal(validate(variant),false,'Bindings cannot be set through presentation variants');
  const override=structuredClone(visual);override.screens[0].elements[1].overrides=[{elementId:'inner',values:{binding:'save-storage-guidance'}}];assert.equal(validate(override),false,'Bindings cannot be set through component instance overrides');
  delete text.binding;assert.ok(!requiredPlayerFeatures(content).includes('save-storage-guidance-v1'));
});
