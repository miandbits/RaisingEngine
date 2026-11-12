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

export async function runBoundedAppleTool(command,args,{cwd,logFile,timeout=600000,env,abortSignal}) {
  const fd=await open(logFile,'w');
  let timer,forceTimer,cancel;
  try {
    if(abortSignal?.aborted)throw Error(`${command} ${args[0]}: tool canceled before launch; inspect ${logFile}`);
    const child=spawn(command,args,{cwd,env,stdio:['ignore',fd.fd,fd.fd]});
    let expired=false,aborted=false;
    const terminate=()=>{
      child.kill('SIGTERM');
      forceTimer??=setTimeout(()=>child.kill('SIGKILL'),10000);
    };
    cancel=()=>{aborted=true;terminate();};
    timer=setTimeout(()=>{expired=true;terminate();},timeout);
    if(abortSignal?.aborted)cancel();
    else abortSignal?.addEventListener('abort',cancel,{once:true});
    const {status,signal}=await new Promise((resolve,reject)=>{
      child.once('error',reject);
      child.once('close',(status,signal)=>resolve({status,signal}));
    });
    if(status!==0||expired||aborted)throw Error(`${command} ${args[0]}: ${aborted?'tool canceled':expired?'tool timeout':`exit ${status}, signal ${signal??'none'}`}; inspect ${logFile}`);
  } finally {
    if(cancel)abortSignal?.removeEventListener('abort',cancel);
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

// The receipt is trusted only as output from the verified installed player, on the
// exclusive stdout file for this launch. Canonicalize before any host filesystem use.
export function ownedIOSSimulatorDataPath(inventory,device) {
  const matches=Object.values(inventory.devices??{}).flat().filter(item=>item.udid===device);
  if(matches.length!==1 || typeof matches[0].dataPath!=='string' || !matches[0].dataPath.startsWith('/'))
    throw Error('Owned simulator inventory must supply one absolute dataPath');
  return matches[0].dataPath;
}

export async function validateIOSSandboxReceipt(receipt,{nonce,pid,contentFingerprint,gameId,deviceDataPath}) {
  const {realpath,stat}=await import('node:fs/promises');
  const path=await import('node:path');
  if(!receipt || receipt.event!=='sandbox-receipt' || receipt.nonce!==nonce || receipt.pid!==pid ||
      receipt.contentFingerprint!==contentFingerprint)throw Error('Sandbox receipt identity mismatch');
  if(typeof receipt.home!=='string'||typeof receipt.saveFile!=='string'||
      !path.isAbsolute(receipt.home)||!path.isAbsolute(receipt.saveFile)||
      !/^[a-zA-Z0-9_-]+$/.test(gameId))throw Error('Invalid sandbox receipt paths');
  const expected=path.join(receipt.home,'Library','Application Support','RaisingEngine',gameId,'slot-1.json');
  if(receipt.home!==path.resolve(receipt.home)||receipt.saveFile!==expected)throw Error('Unexpected sandbox save path');
  if(typeof deviceDataPath!=='string'||!path.isAbsolute(deviceDataPath))throw Error('Missing owned simulator data path');
  const lexicalRelative=path.relative(path.resolve(deviceDataPath),path.resolve(receipt.home));
  const appContainerPattern=/^Containers\/Data\/Application\/[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
  if(!appContainerPattern.test(lexicalRelative.split(path.sep).join('/')))throw Error('App home escapes owned simulator or is not an app container');
  const deviceData=await realpath(deviceDataPath);
  const home=await realpath(receipt.home);
  const homeRelative=path.relative(deviceData,home);
  if(homeRelative!==lexicalRelative)throw Error('App home contains a symlink or escapes owned simulator data');
  const directory=await realpath(path.dirname(receipt.saveFile));
  // A symlink inside HOME could otherwise redirect staging to another owned directory.
  const canonicalExpected=path.join(home,'Library','Application Support','RaisingEngine',gameId);
  if(directory!==canonicalExpected || !(await stat(directory)).isDirectory())
    throw Error('Sandbox save directory is not canonical');
  return {home,directory,saveFile:path.join(directory,'slot-1.json')};
}

export async function waitForIOSSandboxReceipt(stdout,{timeout=30000,interval=100,read,alive}={}) {
  const {readFile}=await import('node:fs/promises');
  const {setTimeout:delay}=await import('node:timers/promises');
  const deadline=Date.now()+timeout;
  do {
    const lines=(await (read??readFile)(stdout,'utf8')).split('\n');
    const receipts=[];
    for(const line of lines.slice(0,-1)) {
      try {const value=JSON.parse(line);if(value?.event==='sandbox-receipt')receipts.push(value);} catch {}
    }
    if(receipts.length>1)throw Error('Multiple sandbox receipts');
    if(receipts.length===1)return receipts[0];
    if(alive)alive();
    if(Date.now()>=deadline)break;
    await delay(Math.min(interval,Math.max(0,deadline-Date.now())));
  } while(true);
  throw Error('Timed out waiting for player sandbox receipt');
}

export async function readIOSPortableStaging(directory) {
  const {realpath,readdir,lstat}=await import('node:fs/promises');
  const path=await import('node:path');
  if(await realpath(directory)!==directory)throw Error('Sandbox directory changed');
  const staging=path.join(directory,'portable-staging');
  if(!(await lstat(staging)).isDirectory() || await realpath(staging)!==staging)
    throw Error('Portable staging is not an owned directory');
  return readdir(staging);
}
