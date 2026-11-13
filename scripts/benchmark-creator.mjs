import {mkdir,mkdtemp,writeFile,cp,readFile} from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import assert from 'node:assert/strict';
import {QtStudioService} from '../apps/qt-studio/service.mjs';

const count=Number(process.env.CREATOR_EVENTS??500);assert.ok(Number.isInteger(count)&&count>=10&&count<=2000);
await mkdir('.cache/creator-benchmark',{recursive:true});const out=await mkdtemp(path.resolve('.cache/creator-benchmark/run-'));
const report={version:1,at:new Date().toISOString(),node:process.version,platform:process.platform,arch:process.arch,cpu:os.cpus()[0]?.model,note:'Service round trips including recovery and compatibility; synthetic larger project, not a GUI frame-rate or peak-memory claim.',projects:[]};
for(const large of [false,true]) {
  const root=path.join(out,large?'large':'starter');await cp('examples/my-first-raising-game',root,{recursive:true});
  if(large){const event=JSON.parse(await readFile(path.join(root,'events/garden-invitation.json')));for(let i=0;i<count;i++){const clone=structuredClone(event);clone.id='benchmark-'+String(i).padStart(4,'0');clone.label='Garden story '+i;clone.requirements={op:'compare',comparator:'gte',left:{op:'read',target:{kind:'attribute',attributeId:'skill'}},right:{op:'literal',value:100}};await writeFile(path.join(root,'events',clone.id+'.json'),JSON.stringify(clone));}}
  const service=new QtStudioService(),call=(method,params={})=>service.handle({method,params:{revision:service.editor.revision,...params}}),measurements={};
  const measure=async(name,fn,runs=5)=>{const times=[];for(let i=0;i<runs;i++){const start=performance.now();await fn(i);times.push(performance.now()-start);}const sorted=[...times].sort((a,b)=>a-b);measurements[name]={firstMs:times[0],medianMs:sorted[Math.floor(sorted.length/2)],p95Ms:sorted.at(-1),runs};};
  await measure('open',()=>call('open',{root}),3);const title=()=>structuredClone(service.editor.documents.find(d=>d.kind==='project').data);
  await measure('editWithRecovery',i=>call('edit',{edit:{type:'replace',file:'project.json',data:{...title(),title:'Benchmark '+i}}}));
  await measure('view',()=>call('view'));await measure('validate',async()=>{const reply=await call('validate');assert.equal(reply.view.diagnostics.filter(d=>d.severity==='error').length,0);},3);
  await measure('undoRedo',async()=>{await call('edit',{edit:{type:'undo'}});await call('edit',{edit:{type:'redo'}});});
  await measure('save',()=>call('save'),1);await measure('reopen',()=>call('open',{root}),1);assert.equal(title().title,'Benchmark 4');
  report.projects.push({name:large?count+' added stories':'starter',documents:service.editor.documents.length,measurements});await service.editor.close();
}
// Explicit investigation thresholds, not a guarantee on all hardware.
report.budgetsMs={editWithRecovery:500,view:250,undoRedo:1000,open:5000,validate:5000,save:5000,reopen:5000};
report.violations=report.projects.flatMap(p=>Object.entries(p.measurements).filter(([k,v])=>v.p95Ms>report.budgetsMs[k]).map(([operation,value])=>({project:p.name,operation,p95Ms:value.p95Ms,budgetMs:report.budgetsMs[operation]})));
await writeFile(path.join(out,'report.json'),JSON.stringify(report,null,2));await writeFile('.cache/creator-benchmark/latest.json',JSON.stringify({out,...report},null,2));console.log(JSON.stringify({out,projects:report.projects,violations:report.violations},null,2));if(report.violations.length)process.exitCode=1;
