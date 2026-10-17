import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {content,doc,temporary,samplePath} from './helpers.mjs';
import {createVisual} from '../../dist/packages/visual/model.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('old renderers reject routing and typed data bindings inside reusable components',async t=>{
  const root=await temporary(t),runtime=resolve(root,'runtime');await mkdir(runtime);await writeFile(resolve(runtime,'RaisingPlayer.exe'),'test placeholder');
  await writeFile(resolve(runtime,'capabilities.json'),JSON.stringify({formatVersion:'1',imageExtensions:['.png'],audioExtensions:['.wav'],visualFeatures:['components-v1','anchors-v1','monthly-schedule-v1']}));
  for(const mode of ['routing','binding']){
    const c=await content(p=>{const v=createVisual();if(mode==='routing')v.routing={defaultScreenId:'title',phases:[]};
      else v.components=[{id:'card',label:'card',width:500,height:48,elements:[{id:'trust',kind:'text',x:0,y:0,width:500,height:48,binding:'data-value',dataBinding:{kind:'relationship',characterId:'mentor',relationshipId:'trust'}}]}];
      doc(p,'project').visual=v;
    });
    await assert.rejects(exportGame({content:c,projectRoot:samplePath,destination:resolve(root,mode),target:'windows',runtimes:{windows:runtime,web:runtime}}),/free-screens-v1/);
  }
});

test('old renderers reject conditional presentation even inside unused reusable parts',async t=>{
  const root=await temporary(t),runtime=resolve(root,'runtime');await mkdir(runtime);await writeFile(resolve(runtime,'RaisingPlayer.exe'),'test placeholder');
  await writeFile(resolve(runtime,'capabilities.json'),JSON.stringify({formatVersion:'1',imageExtensions:['.png'],audioExtensions:['.wav'],visualFeatures:['components-v1','anchors-v1','monthly-schedule-v1','free-screens-v1']}));
  const c=await content(p=>{const v=createVisual();v.components=[{id:'card',label:'card',width:500,height:48,elements:[{id:'caption',kind:'text',x:0,y:0,width:500,height:48,text:'default',variants:[{label:'special',when:{op:'always'},values:{text:'changed'}}]}]}];doc(p,'project').visual=v;});
  await assert.rejects(exportGame({content:c,projectRoot:samplePath,destination:resolve(root,'output'),target:'windows',runtimes:{windows:runtime,web:runtime}}),/presentation-variants-v1/);
});

test('old runtimes reject designer states before publishing a bundle',async t=>{
  const root=await temporary(t),runtime=resolve(root,'runtime');await mkdir(runtime);
  await writeFile(resolve(runtime,'RaisingPlayer.exe'),'test placeholder');await writeFile(resolve(runtime,'index.html'),'test placeholder');
  await writeFile(resolve(runtime,'capabilities.json'),JSON.stringify({formatVersion:'1',imageExtensions:['.png'],audioExtensions:['.wav'],visualFeatures:['free-screens-v1','presentation-variants-v1']}));
  const c=await content(p=>doc(p,'project').stateGroups=[{id:'mode',name:'모드',scope:'shared',initialValueId:'idle',values:[{id:'idle',name:'기본'}]}]);
  for(const target of ['windows','web'])await assert.rejects(exportGame({content:c,projectRoot:samplePath,destination:resolve(root,target),target,runtimes:{windows:runtime,web:runtime}}),/custom-states-v1/);
});
