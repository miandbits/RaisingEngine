import test from 'node:test';
import assert from 'node:assert/strict';
import {GitHubGoalReports, readReportArchive} from '../../apps/qt-studio/goal-github.mjs';
import {GoalCiReports} from '../../apps/qt-studio/goal-ci.mjs';

function archive(files) {
  const local=[], central=[];let offset=0;
  for(const [name,value] of files){
    const filename=Buffer.from(name),bytes=Buffer.from(value);let crc=0xffffffff;
    for(const b of bytes){crc^=b;for(let i=0;i<8;i++)crc=(crc>>>1)^((crc&1)?0xedb88320:0);}crc=(crc^0xffffffff)>>>0;
    const h=Buffer.alloc(30);h.writeUInt32LE(0x04034b50);h.writeUInt16LE(20,4);h.writeUInt32LE(crc,14);h.writeUInt32LE(bytes.length,18);h.writeUInt32LE(bytes.length,22);h.writeUInt16LE(filename.length,26);
    local.push(h,filename,bytes);const c=Buffer.alloc(46);c.writeUInt32LE(0x02014b50);c.writeUInt16LE(20,4);c.writeUInt16LE(20,6);c.writeUInt32LE(crc,16);c.writeUInt32LE(bytes.length,20);c.writeUInt32LE(bytes.length,24);c.writeUInt16LE(filename.length,28);c.writeUInt32LE(offset,42);central.push(c,filename);offset+=h.length+filename.length+bytes.length;
  }
  const directory=Buffer.concat(central),end=Buffer.alloc(22);end.writeUInt32LE(0x06054b50);end.writeUInt16LE(files.length,8);end.writeUInt16LE(files.length,10);end.writeUInt32LE(directory.length,12);end.writeUInt32LE(offset,16);
  return Buffer.concat([...local,directory,end]);
}
const report = JSON.stringify({version:1,kind:'goal-regression',ok:false,rows:[{routeId:'ordinary',status:'failed',failure:{step:2,message:'goal missed'}}],errors:[]});
const run={id:123,run_number:5,run_attempt:1,head_sha:'a'.repeat(40),status:'completed',conclusion:'failure',head_branch:'main'};
function mock({expired=false,missing=false,status='completed'}={}){
  const calls=[];
  const command=async(args,binary)=>{calls.push({args,binary});const endpoint=args.at(-1);
    if(endpoint.includes('/runs?'))return {total_count:21,workflow_runs:[{...run,status}]};
    if(endpoint.includes('/artifacts?'))return {artifacts:missing?[]:[{id:456,name:'goal-regression-reports',expired,size_in_bytes:100}]};
    if(endpoint.endsWith('/zip'))return archive([['goal-ci-fixture/reports/report.json',report],['ci-goal-regression/report.json',report]]);
    throw Error('unexpected request');
  };return {client:new GitHubGoalReports(command),calls};
}
test('GitHub run pagination, bounded reports, provenance and trusted local import',async()=>{
  const {client,calls}=mock();const listing=await client.list('/project',{repository:'owner/repo',branch:'feat/a&b',page:1});
  assert.equal(listing.hasNext,true);assert.match(calls[0].args.at(-1),/branch=feat%2Fa%26b/);
  const bundle=await client.reports('/project',listing.id,123);assert.equal(bundle.reports.length,2);assert.equal(bundle.reports[0].primary,true);assert.equal(bundle.reports[1].primary,false);
  assert.equal(bundle.reports[0].failedRoutes,1);
  const editor={root:'/project',current:r=>assert.equal(r,7)},ci=new GoalCiReports();
  const opened=client.open(editor,ci,7,bundle.id,bundle.reports[0].id);
  assert.throws(()=>ci.openBytes(editor,Buffer.from('null'),7,'invalid'),/지원하지/);
  assert.equal(opened.source.rows[0].status,'failed');assert.equal(opened.origin.run.id,123);assert.equal(opened.origin.repository,'owner/repo');assert.equal(opened.origin.primary,true);assert.notEqual(opened.id,bundle.id);
  assert.throws(()=>client.open({...editor,root:'/other'},ci,7,bundle.id,bundle.reports[0].id),/만료/);
  assert.throws(()=>client.open(editor,ci,7,bundle.id,'../../file'),/없습니다/);
  await client.list('/project',{repository:'owner/repo',page:2});assert.throws(()=>client.open(editor,ci,7,bundle.id,bundle.reports[0].id),/만료/);
  await assert.rejects(client.list('/project',{repository:'--help'}),/owner/);
});
test('expired, absent and incomplete CI runs stay actionable, retries work',async()=>{
  for(const [options,pattern] of [[{expired:true},/만료/],[{missing:true},/없습니다/],[{status:'in_progress'},/완료/]]){
    const {client}=mock(options);const list=await client.list('/p',{repository:'o/r'});await assert.rejects(client.reports('/p',list.id,123),pattern);assert.equal(client.bundle,null);
  }
  const {client}=mock();let calls=0;const original=client.command;client.command=(...args)=>{if(++calls===1)throw Error('network failed');return original(...args);};
  await assert.rejects(client.list('/p',{repository:'o/r'}),/network/);const list=await client.list('/p',{repository:'o/r'});assert.equal(list.runs.length,1);
  await assert.rejects(client.reports('/p','stale',123),/만료/);
});
test('archive rejects traversal, duplicate paths, invalid zip and oversized reports without extracting files',async()=>{
  await assert.rejects(readReportArchive(Buffer.from('invalid')),/손상/);
  await assert.rejects(readReportArchive(archive([['../report.json',report]])),/경로/);
  await assert.rejects(readReportArchive(archive([['a/report.json',report],['a/report.json',report]])),/중복/);
  await assert.rejects(readReportArchive(archive([['report.json','a'.repeat(16*1024*1024+1)]])),/크기/);
  const entries=await readReportArchive(archive([['ignored.txt','no'],['report.json',report]]));assert.equal(entries.length,1);
});
