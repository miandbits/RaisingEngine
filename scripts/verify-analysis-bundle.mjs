import assert from 'node:assert/strict';
import { mkdir,mkdtemp,cp } from 'node:fs/promises';
import { resolve,join } from 'node:path';
import { Client } from '@modelcontextprotocol/client';
import { StdioClientTransport } from '@modelcontextprotocol/client/stdio';
const bundle=resolve('release/analysis-preview/Raising Studio-win32-x64/resources/mcp');
await mkdir('.cache/analysis-bundle',{recursive:true});const root=await mkdtemp(resolve('.cache/analysis-bundle/project-'));await cp('examples/academy-demo',root,{recursive:true});
const env={...process.env};delete env.NODE_OPTIONS;delete env.ELECTRON_RUN_AS_NODE;
const transport=new StdioClientTransport({command:join(bundle,'node.exe'),args:[join(bundle,'main/index.js'),'--project',root],cwd:root,env,stderr:'pipe'});let stderr='';transport.stderr?.on('data',b=>stderr+=b);
const client=new Client({name:'bundle-analysis-test',version:'1.0.0'});
try{
 await client.connect(transport);const call=async(name,args={})=>{const r=await client.callTool({name,arguments:args});return r.structuredContent??JSON.parse(r.content[0].text);};
 assert.equal((await client.listTools()).tools.length,14);const project=await call('read_project');assert.equal(project.ok,true);
 const schema=await call('get_schema',{name:'common'});assert.ok(schema.schema.$defs.header.properties.tags);
 const started=await call('start_analysis',{expectedVersion:project.version,options:{runs:3}});assert.equal(started.ok,true,JSON.stringify(started));
 let status;for(let i=0;i<100;i++){status=await call('analysis_status',{id:started.job.id});if(status.job?.status!=='running')break;await new Promise(r=>setTimeout(r,50));}
 assert.equal(status.job.status,'completed',JSON.stringify(status));assert.equal(status.job.result.completed,3);
 const replay=await call('replay_analysis',{id:started.job.id,trace:0});assert.equal(replay.state.phase,'finished');assert.equal(stderr,'');console.log(JSON.stringify({ok:true,tools:14,runs:3,replay:replay.state.phase,root}));
}finally{await client.close();}
