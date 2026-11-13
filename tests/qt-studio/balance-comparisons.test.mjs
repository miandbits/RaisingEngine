import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,writeFile,rm} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
const call=(s,method,params={})=>s.handle({method,params:{revision:s.editor.revision,...params}});
const options={runs:2,seed:42,strategy:'random',balance:'sampled-v1',maxCommands:1000,timeoutMs:60000};
async function done(service,job){for(let i=0;i<400&&job.status==='running';i++){await new Promise(r=>setTimeout(r,25));job=await service.analysisJobs.get(service.editor.root,job.id);}assert.equal(job.status,'completed');return job;}
test('no-code draft balancing compares actual immutable jobs and a separate read-only analysis bridge',{timeout:30000},async t=>{
 await mkdir('.cache/balance-series',{recursive:true});const root=await mkdtemp(resolve('.cache/balance-series/service-'));await cp('examples/academy-demo',root,{recursive:true});
 for(const dir of ['activities','events','scenarios']){await rm(join(root,dir),{recursive:true,force:true});await mkdir(join(root,dir));}
 const project=JSON.parse(await readFile(join(root,'project.json')));project.durationMonths=1;await writeFile(join(root,'project.json'),JSON.stringify(project));
 const action={formatVersion:'0.1.0',id:'rest',label:'Paid rest',description:'Deterministic balancing fixture',requirements:{op:'always'},costs:[],baseEffects:[{op:'add',target:{kind:'attribute',attributeId:'money'},value:{op:'literal',value:2}}]};
 await writeFile(join(root,'activities/rest.json'),JSON.stringify(action));const service=new QtStudioService(),reader=new QtStudioService();t.after(async()=>{await service.editor.close();await service.releases.close();await reader.editor.close();await reader.releases.close();});await call(service,'open',{root});
 const first=await done(service,(await call(service,'analysisStart',{options})).analysis.job);const file=join(root,'.studio/analysis',first.id+'.json'),original=await readFile(file);
 const document=service.editor.documents.find(d=>d.kind==='activity'&&d.data.id==='rest');const changed=structuredClone(document.data);changed.baseEffects[0].value.value=5;await call(service,'edit',{edit:{type:'replace',file:document.file,data:changed}});assert.equal(service.editor.view().dirty,true);
 const second=await done(service,(await call(service,'analysisStart',{options})).analysis.job);const view=structuredClone(service.editor.view()),reply=await call(service,'analysisBalanceCompare',{root,jobId:second.id,beforeJobId:first.id});const comparison=reply.analysis.comparison;
 assert.equal(comparison.status,'matched-setup');assert.equal(comparison.contentChanged,true);const money=comparison.attributes.find(a=>a.id==='money');assert.equal(money.points[0].meanDifference,3);assert.ok(money.points.every(p=>p.meanDifference>=3));assert.deepEqual(service.editor.view(),view);assert.deepEqual(await readFile(file),original);assert.equal(JSON.parse(await readFile(join(root,'activities/rest.json'))).baseEffects[0].value.value,2,'Comparison and analysis preserve the unsaved authoring draft.');
 const listing=await call(service,'analysisList',{root});assert.ok(listing.analysis.jobs.length>=2);assert.ok(listing.analysis.jobs.every(j=>j.result.balance?.periodSeries===undefined),'History transport omits large series without changing archives.');const status=await call(service,'analysisStatus',{root,jobId:first.id,includeGraph:false});assert.ok(status.analysis.job.result.balance.periodSeries.points.length>0);assert.deepEqual(await readFile(file),original);
 const readOnly=await call(reader,'analysisBalanceCompare',{root,jobId:second.id,beforeJobId:first.id});assert.deepEqual(readOnly.analysis.comparison,comparison,'The actual Qt pane uses a separate unbound read-only bridge.');
 await assert.rejects(call(reader,'analysisBalanceCompare',{root,jobId:'../../outside',beforeJobId:first.id}));
 await writeFile(join(root,'service-comparison.json'),JSON.stringify(comparison,null,2));
});
