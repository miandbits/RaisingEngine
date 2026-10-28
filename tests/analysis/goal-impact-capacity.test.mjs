import test from 'node:test';
import assert from 'node:assert/strict';
import {randomUUID,createHash} from 'node:crypto';
import {mkdir,mkdtemp,readFile,readdir,open} from 'node:fs/promises';
import path from 'node:path';
import {archiveGoalImpacts,readGoalImpact,resolveGoalImpact,MAX_GOAL_IMPACT_BYTES} from '../../apps/qt-studio/goal-impact.mjs';

const digest=value=>createHash('sha256').update(JSON.stringify(value)).digest('hex');
async function fixture(){await mkdir('.cache/goal-impact-capacity',{recursive:true});return mkdtemp(path.resolve('.cache/goal-impact-capacity/run-'));}
test('long campaign differences over 8 MiB preserve exact observations and partial approval inputs',{timeout:15000},async()=>{
  const root=await fixture(),context=digest('registered route and baseline');
  const observations=Array.from({length:4200},(_,step)=>({step,before:{changes:[{id:'money',before:10,after:6},{note:'A'.repeat(512)}]},after:{changes:[{id:'money',before:10,after:5},{note:'B'.repeat(512)}]}}));
  const item={kind:'state',path:'/lastActivityResult/changes',label:'Activity result',firstStep:0,observations};
  const cost={kind:'cost-rule',path:'/activities/study/costs/0/amount',label:'Study cost',firstStep:1,observations:[{step:1,before:4,after:5}]};
  const changes=[item,cost].map(value=>({...value,context,id:digest({context,...value}),approvable:value===cost,approved:false,expectation:value.observations}));
  const report={version:1,id:randomUUID(),createdAt:new Date().toISOString(),rows:[{routeId:'long-campaign',status:'changed',changeItems:changes,approvedChangeCount:0,pendingChangeCount:2}],counts:{changed:1},stale:false,complete:true,ok:false};
  assert.ok(Buffer.byteLength(JSON.stringify(report))>8*1024*1024);
  const stored=await archiveGoalImpacts(root,report),bytes=await readFile(stored.file),loaded=await readGoalImpact(root,report.id);
  assert.equal(stored.archived,true);assert.equal(loaded.archived,true);assert.ok(bytes.length<MAX_GOAL_IMPACT_BYTES);assert.equal(bytes.includes(Buffer.from('\n  ')),false);
  assert.deepEqual(loaded,{...report,archived:true});
  const approvalInput=i=>({id:i.id,context:i.context,kind:i.kind,path:i.path,approved:i.approved,observations:i.observations,expectation:i.expectation});
  assert.equal(digest(loaded.rows[0].changeItems.map(approvalInput)),digest(changes.map(approvalInput)));
  assert.equal(loaded.rows[0].changeItems[0].observations.at(-1).step,4199);
});

test('impact read/write budgets reject oversize data and erroneous reports cannot become approval inputs',{timeout:15000},async()=>{
  const root=await fixture(),id=randomUUID(),report={version:1,id,rows:[],stale:false,complete:true,ok:true};
  await assert.rejects(archiveGoalImpacts(root,{...report,padding:'x'.repeat(MAX_GOAL_IMPACT_BYTES)}),/64 MiB/);
  const dir=path.join(root,'.studio');assert.deepEqual(await readdir(dir),[]);
  const file=await open(path.join(dir,'goal-impact-'+id+'.json'),'wx');await file.truncate(MAX_GOAL_IMPACT_BYTES+1);await file.close();
  await assert.rejects(readGoalImpact(root,id),/64 MiB/);
  const failed={...report,id:randomUUID(),error:'Archiving failed',archiveError:'disk unavailable'};
  await archiveGoalImpacts(root,failed);
  await assert.rejects(resolveGoalImpact(root,{},failed.id,0),/보고서를 저장하지 못했습니다/);
});
