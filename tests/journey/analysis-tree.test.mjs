import test from 'node:test';
import assert from 'node:assert/strict';
import {RouteTreeIndex} from '../../dist/packages/analysis-service/route-tree.js';
const trace=(run,ids,outcome='finished')=>({run,outcome,state:{endingId:'same',periodIndex:2},route:ids.map(nodeId=>({nodeId,label:nodeId,kind:'event',periodIndex:0,passed:null}))});
test('tree retains exact ordered prefixes through reconvergence, repeated nodes and identical endings',()=>{
  const tree=new RouteTreeIndex();[['start','A','X','C'],['start','B','X','D'],['start','A','X','E'],['start','A','X','C'],['start','B','X','B','X','D']].forEach((ids,i)=>tree.add(trace(i,ids)));
  const trunk=tree.page();assert.equal(trunk.count,5);assert.deepEqual(trunk.children[0].prefix,['start']);
  const split=tree.page(['start']);assert.equal(split.totalBranches,2);assert.equal(split.children.reduce((n,c)=>n+c.count,0),5);
  const a=split.children.find(c=>c.label==='A');assert.deepEqual(a.prefix,['start','A','X']);assert.equal(a.count,3);assert.equal(a.parentCount,5);assert.equal(a.observedPercent,60);
  assert.deepEqual([...tree.runs(a.prefix)],[0,2,3]);assert.deepEqual([...tree.runs(['start','B','X'])],[1,4]);assert.deepEqual([...tree.runs(['start','B','X','B'])],[4]);assert.deepEqual([...tree.runs(['start','A','X','D'])],[]);
  const endings=tree.page(a.prefix);assert.equal(endings.children.length,2);assert.equal(endings.children[0].count,2);assert.ok(endings.children.every(c=>c.terminal));assert.deepEqual([...tree.runs(endings.children[0].prefix)],[0,3]);
});
test('tree pagination retains every branch, bounded compression and outcome distinctions',()=>{
  const tree=new RouteTreeIndex();for(let i=0;i<100;i++)tree.add(trace(i,['start',`branch-${i}`]));
  const first=tree.page(['start'],0,8),last=tree.page(['start'],96,8);assert.equal(first.totalBranches,100);assert.equal(first.children.length,8);assert.equal(last.children.length,4);
  const all=[];for(let offset=0;offset<100;offset+=8)all.push(...tree.page(['start'],offset,8).children.flatMap(c=>[...tree.runs(c.prefix)]));assert.equal(new Set(all).size,100);
  assert.throws(()=>tree.page('invalid'),/접두사/);assert.throws(()=>tree.page([],0,101),/범위/);
  const outcomes=new RouteTreeIndex();outcomes.add(trace(0,['start'],'finished'));outcomes.add(trace(1,['start'],'limit'));assert.equal(outcomes.page(['start']).children.length,2);
  const long=new RouteTreeIndex();long.add(trace(0,Array.from({length:500},(_,i)=>`step-${i}`)));assert.equal(long.page().children[0].prefix.length,32);
});
