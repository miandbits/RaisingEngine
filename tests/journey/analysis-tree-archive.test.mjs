import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,mkdir,writeFile} from 'node:fs/promises';
import path from 'node:path';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
const root=process.env.RAISING_TREE_ARCHIVE_PROJECT;
test('existing 5000-run archive builds a true prefix tree and returns only the matching real runs',{skip:!root,timeout:180000},async()=>{
  const service=new QtStudioService();const call=async(method,extra={})=>{const reply=await service.handle({method,params:{root,jobId,...extra}});assert.equal(reply.error,null);return reply.analysis;};
  const jobs=(await service.handle({method:'analysisList',params:{root}})).analysis.jobs;
  const job=jobs.find(j=>j.status==='completed'&&j.result.completed===5000);assert.ok(job);const jobId=job.id;const start=performance.now();
  let page=(await call('analysisTree',{prefix:[]})).tree;assert.equal(page.count,5000);assert.equal(page.runCount,5000);
  while(page.children.length===1&&!page.children[0].terminal)page=(await call('analysisTree',{prefix:page.children[0].prefix})).tree;
  const branches=page.children.slice(0,8);assert.ok(branches.length>1);const expected=branches.map(()=>[]);
  for(let offset=0;offset<5000;offset+=16){const records=await Promise.all(Array.from({length:Math.min(16,5000-offset)},(_,i)=>readFile(path.join(root,'.studio','analysis',`${jobId}.runs`,`${offset+i}.json`),'utf8').then(JSON.parse)));
    for(const trace of records){const ids=trace.route.map(p=>p.nodeId);ids.push(JSON.stringify(['route-end',trace.outcome,trace.state.endingId]));branches.forEach((b,i)=>{if(b.prefix.every((id,step)=>ids[step]===id))expected[i].push(trace.run);});}}
  for(let i=0;i<branches.length;i++){const b=branches[i];assert.equal(b.count,expected[i].length);const filtered=(await call('analysisRuns',{routePrefix:b.prefix,limit:100})).runs;assert.equal(filtered.total,b.count);assert.deepEqual(filtered.rows.map(r=>r.run),expected[i].slice(0,100));}
  await mkdir('.cache/qt-tree-evidence',{recursive:true});await writeFile('.cache/qt-tree-evidence/archive-audit.json',JSON.stringify({ok:true,jobId,runs:5000,branchesVerified:branches.length,counts:branches.map(b=>b.count),elapsedMs:performance.now()-start,newSimulations:0},null,2));
});
