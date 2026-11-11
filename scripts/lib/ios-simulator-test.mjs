import {spawn} from 'node:child_process';
import {open} from 'node:fs/promises';

export function selectIOSSimulator(runtimes,{family='iphone',minimumMajor=16,runtimeId,deviceType}={}) {
  const prefix='com.apple.CoreSimulator.SimDeviceType.'+(family==='ipad'?'iPad':'iPhone')+'-';
  const preferred=prefix+(family==='ipad'?'Pro-11-inch-M5-12GB':'17e');
  const available=runtimes.filter(runtime=>runtime.isAvailable && runtime.identifier.includes('.iOS-') &&
    Number(runtime.version.split('.')[0])>=minimumMajor && (!runtimeId || runtime.identifier===runtimeId));
  available.sort((left,right)=>right.version.localeCompare(left.version,'en',{numeric:true}) ||
    left.identifier.localeCompare(right.identifier));
  for(const runtime of available) {
    const types=(runtime.supportedDeviceTypes??[]).map(type=>type.identifier).filter(id=>id.startsWith(prefix) &&
      (deviceType?id===deviceType:!id.endsWith('.iPhone-Duo'))).sort();
    const type=types.includes(preferred)?preferred:types[0];
    if(type)return {runtime:runtime.identifier,type};
  }
  const target=[runtimeId,deviceType].filter(Boolean).join(' / ') || family;
  throw Error(`No available iOS ${minimumMajor}+ Simulator supports ${target}`);
}

export async function runBoundedAppleTool(command,args,{cwd,logFile,timeout=600000,env}) {
  const fd=await open(logFile,'w');
  let timer,forceTimer;
  try {
    const child=spawn(command,args,{cwd,env,stdio:['ignore',fd.fd,fd.fd]});
    let expired=false;
    timer=setTimeout(()=>{
      expired=true;
      child.kill('SIGTERM');
      forceTimer=setTimeout(()=>child.kill('SIGKILL'),10000);
    },timeout);
    const {status,signal}=await new Promise((resolve,reject)=>{
      child.once('error',reject);
      child.once('close',(status,signal)=>resolve({status,signal}));
    });
    if(status!==0||expired)throw Error(`${command} ${args[0]}: ${expired?'tool timeout':`exit ${status}, signal ${signal??'none'}`}; inspect ${logFile}`);
  } finally {
    clearTimeout(timer);
    clearTimeout(forceTimer);
    await fd.close();
  }
}

export async function cleanUpIOSSimulator(device,run) {
  const errors={};
  for(const action of ['shutdown','delete']) {
    try {await run(action,device);}
    catch(error) {errors[action]=String(error);}
  }
  return errors;
}
