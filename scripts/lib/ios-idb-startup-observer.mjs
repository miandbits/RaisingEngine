import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {lstat,open,writeFile} from 'node:fs/promises';
import path from 'node:path';
import {performance} from 'node:perf_hooks';

// Each tool observes only the retained companion PID. No client connection or
// HID primitive is issued. Sampling may perturb scheduling; readiness is not a fix.
export function startupObservationCommands(pid,attemptWallTime,executable,out) {
  assert.ok(Number.isSafeInteger(pid)&&pid>0);
  assert.ok(Number.isFinite(attemptWallTime)&&path.isAbsolute(executable)&&path.isAbsolute(out));
  const stackFile=path.join(out,'idb-startup-sample.stack.txt');
  // A small wall-clock margin covers formatting truncation; PID scope is exact.
  const start=new Date(attemptWallTime-1000).toISOString().replace('T',' ').replace(/\.\d{3}Z$/,'+0000');
  return [
    {id:'sample',command:'/usr/bin/sample',args:[String(pid),'1','1','-file',stackFile],timeout:5000,outputFile:stackFile},
    {id:'ps',command:'/bin/ps',args:['-ww','-p',String(pid),'-o','pid=,ppid=,lstart=,state=,etime=,comm='],timeout:3000},
    {id:'open-files',command:'/usr/sbin/lsof',args:['-nP','-a','-p',String(pid)],timeout:3000},
    {id:'os-log',command:'/usr/bin/log',args:['show','--style','ndjson','--info','--debug','--start',start,
      '--process',String(pid),'--predicate',`processImagePath == ${JSON.stringify(executable)}`],timeout:5000}
  ];
}

export async function captureStartupCommand(spec,{out,deadline,signal,isAlive}) {
  const startedAt=performance.now();
  const result={id:spec.id,command:spec.command,args:spec.args,startedAt,exitCode:null,signal:null,
    timedOut:false,aborted:false,spawned:false,stdoutBytes:0,stderrBytes:0};
  if(signal.aborted||!isAlive()||startedAt>=deadline) {
    return {...result,skipped:'Companion observation no longer active',completedAt:performance.now()};
  }
  const stdout=await open(path.join(out,`idb-startup-${spec.id}.stdout.log`),'wx',0o600);
  let stderr;
  try {
    stderr=await open(path.join(out,`idb-startup-${spec.id}.stderr.log`),'wx',0o600);
    if(signal.aborted||!isAlive()||performance.now()>=deadline) {
      result.skipped='Companion observation ended before tool spawn';
    } else {
      await new Promise(resolve=>{
        let child,timer,settled=false;
        const finish=()=>{if(settled)return;settled=true;clearTimeout(timer);signal.removeEventListener('abort',abort);resolve();};
        const stop=(reason)=>{
          result[reason]=true;
          // Only this observer's own short-lived tool is killed. No target PID signal.
          if(child&&child.exitCode===null&&child.signalCode===null)result.killRequested=child.kill('SIGKILL');
        };
        const abort=()=>stop('aborted');
        try {
          child=spawn(spec.command,spec.args,{stdio:['ignore',stdout.fd,stderr.fd],
            env:{PATH:'/usr/bin:/bin:/usr/sbin:/sbin',LC_ALL:'C',TZ:'UTC'}});
          result.observerPID=child.pid??null;
          child.once('spawn',()=>{result.spawned=true;result.spawnedAt=performance.now();});
          child.once('error',error=>{result.error=String(error);finish();});
          child.once('close',(code,termination)=>{result.exitCode=code;result.signal=termination;finish();});
          signal.addEventListener('abort',abort,{once:true});
          timer=setTimeout(()=>stop('timedOut'),Math.max(0,Math.min(spec.timeout,deadline-performance.now())));
          if(signal.aborted||!isAlive())abort();
        } catch(error) {result.error=String(error);finish();}
      });
    }
    result.stdoutBytes=Number((await stdout.stat()).size);
    result.stderrBytes=Number((await stderr.stat()).size);
    if(spec.outputFile) {
      result.outputFile=spec.outputFile;
      const info=await lstat(spec.outputFile).catch(error=>{if(error.code!=="ENOENT")throw error;});
      result.outputBytes=info?.isFile()?Number(info.size):null;
      result.outputRegularFile=info?.isFile()??false;
    }
  } finally {await stdout.close();if(stderr)await stderr.close();}
  return {...result,completedAt:performance.now()};
}

export async function observeIDBStartup({pid,device,out,startedAt,startedWallMs,deadline,signal,executable,isAlive}) {
  assert.ok(process.platform==='darwin'&&process.env.GITHUB_ACTIONS==='true'&&
    process.env.RUNNER_ENVIRONMENT==='github-hosted'&&process.env.RUNNER_OS==='macOS'&&
    process.env.GITHUB_REF==='refs/heads/codex/ios-idb-startup-observation',
  'Startup observations are restricted to the isolated hosted Mac');
  assert.match(device,/^[0-9a-f]{8}-(?:[0-9a-f]{4}-){3}[0-9a-f]{12}$/i);
  assert.ok(Number.isFinite(startedAt)&&Number.isFinite(startedWallMs)&&Number.isFinite(deadline)&&deadline-startedAt<=30001&&
    deadline>startedAt&&path.isAbsolute(out)&&path.isAbsolute(executable)&&typeof isAlive==='function');
  const observationStartedAt=performance.now();
  const beforeAlive=isAlive();
  const commands=startupObservationCommands(pid,startedWallMs,executable,out);
  const results=await Promise.allSettled(commands.map(spec=>captureStartupCommand(spec,{out,deadline,signal,isAlive})));
  const receipt={version:1,purpose:'startup-observation-only',pid,parentPID:process.pid,device,executable,
    startedAt,startedWallMs,deadline,observationStartedAt,completedAt:performance.now(),beforeAlive,afterAlive:isAlive(),
    noInputPosted:true,noRPC:true,guestRegistration:'unobserved',samplingMayPerturbScheduling:true,
    commands:results.map((item,index)=>item.status==='fulfilled'?item.value:
      {id:commands[index].id,error:String(item.reason),observationIncomplete:true})};
  await writeFile(path.join(out,'idb-startup-observations.json'),JSON.stringify(receipt,null,2)+'\n',{flag:'wx',mode:0o600});
  return receipt;
}
