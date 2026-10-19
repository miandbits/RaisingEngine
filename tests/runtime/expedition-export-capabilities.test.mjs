import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,mkdir,readFile,writeFile} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

const detail='expedition-conditions-v1',party='party-tactics-v1';
async function fixture(){
 const root=await mkdtemp(resolve('.cache/expedition-export-')),runtime=join(root,'runtime');await mkdir(runtime);
 // Packaging stubs only: never execute these files.
 await writeFile(join(runtime,'index.html'),'fixture');await writeFile(join(runtime,'RaisingPlayer.exe'),'fixture');
 const built=await buildSnapshot(await loadProject(resolve('examples/academy-adventure')));assert.equal(built.ok,true);
 const capabilities=JSON.parse(await readFile('dist/web/capabilities.json','utf8'));
 const base=capabilities.visualFeatures.filter(x=>x!==detail&&x!==party);
 let serial=0;async function run(content,features,target='web'){
  await writeFile(join(runtime,'capabilities.json'),JSON.stringify({...capabilities,visualFeatures:[...base,...features]}));
  return exportGame({content,projectRoot:resolve('examples/academy-adventure'),destination:join(root,'export-'+serial++),target,runtimes:{web:runtime,windows:runtime}});
 }
 return {content:structuredClone(built.snapshot),run};
}

test('conditional expedition needs its own feature, not party tactics; legacy runtime still rejects it',async()=>{
 const {content,run}=await fixture();content.project.systems.expeditions[0].nodes[1].requirements={op:'flag',flagId:content.flags[0].id,equals:true};
 await assert.rejects(run(content,[]),/expedition-conditions-v1/);
 await run(content,[detail]);
 // Older Windows bundles advertised the encompassing party feature only.
 await run(content,[party],'windows');
});

test('ordinary expedition remains exportable without either added feature',async()=>{
 const {content,run}=await fixture();await run(content,[]);
});

test('specific expedition support never grants party tactics or encounter support',async()=>{
 const {content,run}=await fixture();
 for(const kind of ['tactics','node-encounter','quest-encounter']){
  const variant=structuredClone(content);
  if(kind==='tactics')variant.project.systems.tactics={};
  if(kind==='node-encounter')variant.project.systems.expeditions[0].nodes[1].encounterId='party-encounter';
  if(kind==='quest-encounter')variant.project.systems.quests.push({id:'party-quest',encounterId:'party-encounter'});
  await assert.rejects(run(variant,[detail]),/party-tactics-v1/,kind);
  await run(variant,[party],'windows');
 }
});
