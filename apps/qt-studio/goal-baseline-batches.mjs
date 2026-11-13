import {runtimeIdentity} from './goal-baselines.mjs';
import {publishStagedGoalBaseline} from './goal-baseline-publication.mjs';
import {readdir} from 'node:fs/promises';
import path from 'node:path';

export async function startGoalBaselineBatch(service,params){
  const editor=service.editor,root=editor.root,content=await service.debugSnapshot(params.revision),sourceVersion=editor.draftVersion();
  const prepared=await service.goalRouteBatches.prepare(root,params);
  if(!prepared.entries.length)throw Error('Choose registered routes to capture as baselines.');
  if(prepared.entries.some(entry=>entry.error||!entry.route))throw Error('Fix invalid registered routes before creating baselines.');
  const existing=await readdir(path.join(root,'playtests','baselines')).catch(error=>{if(error.code==='ENOENT')return [];throw error;});
  if(existing.filter(name=>name.endsWith('.json')).length+prepared.entries.length>128)throw Error('기준 기록은 프로젝트당 최대 128개입니다.');
  const names=params.names;
  if(!names||typeof names!=='object'||Array.isArray(names)||Object.keys(names).length!==prepared.entries.length||prepared.entries.some(entry=>typeof names[entry.route.id]!=='string'||!names[entry.route.id].trim()||names[entry.route.id].length>120))throw Error('Give every selected baseline a name of 1–120 characters.');
  const runtime=await runtimeIdentity(),expected=new Map(prepared.entries.map(entry=>[entry.route.id,entry.sourceHash]));
  const guard=async()=>{
    const check=()=>{if(editor.root!==root||editor.draftVersion()!==sourceVersion)throw Object.assign(Error('The project changed during baseline capture. Completed records remain; restart to capture the new version.'),{code:'STALE_INPUT'});};
    check();await editor.assertDiskUnchanged();
    const fresh=await service.goalRouteBatches.prepare(root,{ids:[...expected.keys()]});
    if(fresh.entries.length!==expected.size||fresh.entries.some(entry=>expected.get(entry.route?.id)!==entry.sourceHash))throw Object.assign(Error('Registered routes changed during baseline capture.'),{code:'STALE_INPUT'});
    check();
  };
  prepared.baseline={names:Object.fromEntries(Object.entries(names).map(([id,name])=>[id,name.trim()])),runtime};
  prepared.publishBaseline=async(file,metadata,index,checkOwned)=>publishStagedGoalBaseline(root,file,metadata,async()=>{
    if(metadata.fingerprint!==content.contentFingerprint)throw Error('Prepared baseline content does not match the captured version.');
    if(metadata.runtime!==runtime||await runtimeIdentity()!==runtime)throw Object.assign(Error('The runtime changed during baseline capture. Restart with the current runtime.'),{code:'STALE_RUNTIME'});
    await guard();checkOwned();
  });
  return service.goalRouteBatches.start(root,content,sourceVersion,params.revision,{...params,limits:{routeTimeoutMs:300000,batchTimeoutMs:3600000,...params.limits}},guard,prepared);
}
