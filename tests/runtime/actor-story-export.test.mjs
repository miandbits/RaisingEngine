import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,mkdir,readFile,writeFile} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('personal flags and post-event continuation require a capable export runtime',async()=>{
  const root=await mkdtemp(resolve('.cache/actor-story-export-')),runtime=join(root,'runtime');
  await mkdir(runtime);
  // Packaging stubs: these files are never executed and do not prove renderer support.
  await writeFile(join(runtime,'index.html'),'fixture');
  await writeFile(join(runtime,'RaisingPlayer.exe'),'fixture');
  const projectRoot=resolve('tests/fixtures/projects/character-state-demo');
  const built=await buildSnapshot(await loadProject(projectRoot));assert.equal(built.ok,true);
  const caps=JSON.parse(await readFile('dist/web/capabilities.json','utf8'));
  const feature='actor-story-links-v1',base=caps.visualFeatures.filter(x=>x!==feature);
  let serial=0;
  async function run(content,features,target){
    await writeFile(join(runtime,'capabilities.json'),JSON.stringify({...caps,visualFeatures:features}));
    return exportGame({content,projectRoot,destination:join(root,'export-'+serial++),target,runtimes:{web:runtime,windows:runtime}});
  }
  await run(built.snapshot,base,'windows');
  for(const variant of ['condition','effect','after-event']){
    const content=structuredClone(built.snapshot),event=content.events.find(e=>e.id==='personal-gift');
    if(variant==='condition')event.requirements={op:'flag',flagId:content.flags[0].id,equals:false,owner:{kind:'actor'}};
    if(variant==='effect')event.nodes.unshift({type:'effect',id:'personal-flag',effects:[{op:'setFlag',flagId:content.flags[0].id,value:true,owner:{kind:'actor'}}],next:event.entryNodeId});
    if(variant==='after-event')event.trigger='after-event';
    for(const target of ['windows','web'])await assert.rejects(run(content,base,target),/actor-story-links-v1/,variant+' '+target);
    for(const target of ['windows','web'])await run(content,[...base,feature],target);
  }
  for(const trigger of ['period-start','after-activity','location-enter']){
    const content=structuredClone(built.snapshot),event=content.events.find(e=>e.id==='personal-gift');event.trigger=trigger;
    if(trigger==='after-activity')event.activityId='study';if(trigger==='location-enter')event.locationId='home';
    const compatible=caps.visualFeatures.filter(f=>f!=='story-appointments-v1');
    for(const target of ['windows','web'])await assert.rejects(run(content,compatible,target),/story-appointments-v1/);
    for(const target of ['windows','web'])await run(content,[...compatible,'story-appointments-v1'],target);
  }
});
