// An owned process group surrounds the replay thread and any native subprocess.
// The editor can retire all descendants even if a synchronous replay stops yielding.
import {Worker} from 'node:worker_threads';
import {execFile} from 'node:child_process';
import path from 'node:path';
let runner,done=false;
const send=value=>{if(process.connected)process.send(value);};
process.on('message',data=>{if(runner)return;runner=new Worker(new URL('./release-workflow-runner.mjs',import.meta.url),{workerData:data,execArgv:[],resourceLimits:{maxOldGenerationSizeMb:512}});runner.on('message',value=>{if(value.result||value.error)done=true;send(value);});runner.on('error',error=>{done=true;send({error:{message:error.message}});});runner.on('exit',code=>{if(!done)send({error:{message:'Release replay stopped without a result ('+code+').'}});});});
process.on('disconnect',()=>{if(process.platform==='win32')execFile(path.join(process.env.SystemRoot??'C:\\Windows','System32','taskkill.exe'),['/PID',String(process.pid),'/T','/F'],{windowsHide:true,timeout:5000},()=>process.exit(1));else try{process.kill(-process.pid,'SIGKILL');}catch{process.exit(1);}});
