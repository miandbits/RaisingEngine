import { writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { StudioEditor } from '../dist/packages/studio-service/editor.js';
import { runAnalysis,replayTrace } from '../dist/packages/analysis-service/runner.js';
const editor=new StudioEditor();await editor.open(resolve('examples/academy-year'),true);const snapshot=await editor.snapshot(editor.revision);if(!snapshot.ok)throw new Error('Invalid sample');
const results=[];
for(const ending of snapshot.snapshot.endings){const start=Date.now();const result=await runAnalysis(snapshot.snapshot,{runs:3,seed:42,strategy:'random',maxCommands:2000,timeoutMs:60000,target:{kind:'ending',id:ending.id},beamWidth:24,maxStates:20000});if(result.targetStatus==='found')replayTrace(snapshot.snapshot,result.traces[0]);results.push({id:ending.id,status:result.targetStatus,states:result.exploredStates,elapsedMs:Date.now()-start,stopReason:result.stopReason});}
await writeFile('.cache/analysis-targets.json',JSON.stringify(results,null,2));console.log(JSON.stringify(results));
