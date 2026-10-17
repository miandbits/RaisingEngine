import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {content,doc,temporary,samplePath} from './helpers.mjs';
import {createVisual} from '../../dist/packages/visual/model.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('relationship change text rejects old renderer capabilities, including reusable components',async t=>{
  const root=await temporary(t),runtime=resolve(root,'runtime');await mkdir(runtime);await writeFile(resolve(runtime,'RaisingPlayer.exe'),'test runtime placeholder');
  const capabilities={formatVersion:'1',imageExtensions:['.png'],audioExtensions:['.wav'],visualFeatures:['components-v1','anchors-v1','monthly-schedule-v1']};
  await writeFile(resolve(runtime,'capabilities.json'),JSON.stringify(capabilities));
  const c=await content(p=>{
    const design=createVisual();design.components=[{id:'relation-card',label:'Relation',width:500,height:48,elements:[{id:'trust',kind:'text',x:0,y:0,width:500,height:48,binding:'relationship-value',characterId:'mentor',relationshipId:'trust',valueFormat:'{label}: {value} ({delta})'}]}];
    design.screens[1].elements.push({id:'relation',kind:'component',componentId:'relation-card',x:0,y:0,width:500,height:48});doc(p,'project').visual=design;
  });
  await assert.rejects(exportGame({content:c,projectRoot:samplePath,destination:resolve(root,'export'),target:'windows',runtimes:{windows:runtime,web:runtime}}),/relationship-change-v1/);
});
