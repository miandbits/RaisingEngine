import test from 'node:test';
import assert from 'node:assert/strict';
import { connect,fixture } from '../mcp/helpers.mjs';
test('MCP searches, runs in background, shares replay and exports with version checks',{timeout:30000},async t=>{
 const root=await fixture();const mcp=await connect(t,root);const project=await mcp.call('read_project');
 const found=await mcp.call('search_content',{expectedVersion:project.version,query:{kind:'attributes',text:'skill'}});assert.equal(found.items[0].id,'skill');
 const graph=await mcp.call('content_relationships',{expectedVersion:project.version,key:found.items[0].key,depth:2});assert.ok(graph.edges.some(e=>e.kind==='writes'));
 const started=await mcp.call('start_analysis',{expectedVersion:project.version,options:{runs:2,workers:4}});assert.equal(started.ok,true,JSON.stringify(started));assert.equal(started.job.options.workers,4);assert.equal(started.job.effectiveWorkers,2);
 let status;for(let i=0;i<100;i++){status=await mcp.call('analysis_status',{id:started.job.id});if(status.job.status!=='running')break;await new Promise(r=>setTimeout(r,50));}
 assert.equal(status.job.status,'completed',JSON.stringify(status));assert.equal(status.job.result.completed,2);
 const replay=await mcp.call('replay_analysis',{id:started.job.id,trace:0});assert.equal(replay.state.phase,'finished');
 const saved=await mcp.call('export_analysis_scenario',{expectedVersion:project.version,id:started.job.id,trace:0,scenarioId:'mcp-auto-route'});assert.equal(saved.ok,true,JSON.stringify(saved));
 const stale=await mcp.call('export_analysis_scenario',{expectedVersion:project.version,id:started.job.id,trace:0,scenarioId:'stale-route'});assert.equal(stale.ok,false);
 assert.equal(mcp.stderr(),'');
});
