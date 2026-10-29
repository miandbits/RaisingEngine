// Keep the owning process alive even if the replay worker crashes or exhausts
// its heap. The editor can still retire the exact process tree on Windows.
import {Worker} from 'node:worker_threads';
import {execFile} from 'node:child_process';
import path from 'node:path';
let runner,done=false;
const cancel=new Int32Array(new SharedArrayBuffer(4));
const send=value=>{if(process.connected)process.send(value);};
const failed=error=>{if(done)return;done=true;send({fatal:String(error?.message??error).slice(0,4096)});};
process.on('message',message=>{
  if(message.cancel){Atomics.store(cancel,0,1);return;}
  if(message.baselinePublication){runner?.postMessage(message.baselinePublication);return;}
  if(runner)return;
  try{
    runner=new Worker(new URL('./goal-route-batch-runner.mjs',import.meta.url),{workerData:{...message,cancelBuffer:cancel.buffer},execArgv:[],resourceLimits:{maxOldGenerationSizeMb:message.baseline?1024:512}});
    runner.on('message',value=>{if(done)return;if(value.done)done=true;send(value);});
    runner.on('error',failed);
    runner.on('exit',code=>{if(!done)failed(`Replay worker exited without a result (${code}).`);});
  }catch(error){failed(error);}
});
process.on('uncaughtException',failed);
process.on('unhandledRejection',failed);
process.on('disconnect',()=>{
  // Also retire descendants if the editor exits before its normal close path.
  if(process.platform==='win32')execFile(path.join(process.env.SystemRoot??'C:\\Windows','System32','taskkill.exe'),['/PID',String(process.pid),'/T','/F'],{windowsHide:true,timeout:5000},()=>process.exit(1));
  else try{process.kill(-process.pid,'SIGKILL');}catch{process.exit(1);}
});
