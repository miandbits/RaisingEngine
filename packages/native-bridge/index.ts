import { spawn, type ChildProcessWithoutNullStreams } from 'node:child_process';
import { Worker } from 'node:worker_threads';
import { createRequire } from 'node:module';
import { existsSync, readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { createInterface } from 'node:readline';
import { resolve, dirname, basename } from 'node:path';
import type { Command, ContentSnapshot, CreateResult, DeepReadonly, DispatchResult, RunState } from '../core/model.js';
import type { createRun } from '../core/run.js';
import { CoreFault, freeze } from '../core/util.js';
import { resolveUserValues } from '../core/user-fields.js';

let pinnedExecutable:string|undefined;

export function nativeExecutable(): string {
  if(process.env.RAISING_NATIVE_EXECUTABLE)return process.env.RAISING_NATIVE_EXECUTABLE;
  // A running application keeps one rules binary even if another editor build
  // publishes a newer manifest. Restarting the application adopts the new build.
  if(pinnedExecutable)return pinnedExecutable;
  const name=process.platform === 'win32' ? 'raising-native.exe' : 'raising-native';
  const resources=(process as NodeJS.Process & {resourcesPath?:string}).resourcesPath;
  const published=(folder:string):string=>{
    const manifest=JSON.parse(readFileSync(resolve(folder,'runtime.json'),'utf8')) as {executable?:string};
    if(manifest.executable&&!/^raising-native(?:-[a-f0-9]{16})?(?:\.exe)?$/.test(manifest.executable))throw new Error('Invalid native executable manifest.');
    return resolve(folder,manifest.executable??name);
  };
  if(resources&&existsSync(resolve(resources,'native/runtime.json')))return pinnedExecutable=published(resolve(resources,'native'));
  const adjacent=fileURLToPath(new URL('../native/',import.meta.url));
  if(existsSync(resolve(adjacent,'runtime.json')))return pinnedExecutable=published(adjacent);
  const local=resolve(process.cwd(),'dist/native');
  return pinnedExecutable=existsSync(resolve(local,'runtime.json'))?published(local):resolve(local,name);
}

type WorkerConnection={worker?:Worker;control?:Int32Array;bytes?:Uint8Array;addon?:(request:string)=>string;transport:'addon'|'process';fallbackReason?:string;contents:Map<object,string>;nextContentId:number};
const connections=new Map<string,WorkerConnection>();
const RESPONSE_BYTES=64*1024*1024;
const immutableJsonObjects=new WeakSet<object>();
function immutableJson(value:unknown,active=new Set<object>()):boolean {
  if(value===null||typeof value==='string'||typeof value==='boolean'||typeof value==='number')return true;
  if(typeof value!=='object'||active.has(value))return false;
  if(immutableJsonObjects.has(value))return true;
  if(!Object.isFrozen(value))return false;
  const prototype=Object.getPrototypeOf(value);
  if(prototype!==Object.prototype&&prototype!==Array.prototype&&prototype!==null)return false;
  active.add(value);
  for(const descriptor of Object.values(Object.getOwnPropertyDescriptors(value))){
    if(!('value' in descriptor)||!immutableJson(descriptor.value,active)){active.delete(value);return false;}
  }
  active.delete(value);immutableJsonObjects.add(value);return true;
}
function connection(executable:string):WorkerConnection {
  const old=connections.get(executable);if(old)return old;
  let fallbackReason:string|undefined;
  if((process.env.RAISING_NATIVE_TRANSPORT??'auto')!=='process'){
    try{
      const folder=dirname(executable);
      const manifest=JSON.parse(readFileSync(resolve(folder,'addon.json'),'utf8')) as {filename:string;executable:string;platform:string;arch:string;napi:number};
      if(manifest.executable!==basename(executable)||manifest.platform!==process.platform||manifest.arch!==process.arch||manifest.napi>Number(process.versions.napi))throw new Error('Native addon does not match the selected runtime or host.');
      if(!/^raising-addon-[a-f0-9]{16}\.node$/.test(manifest.filename))throw new Error('Invalid native addon manifest.');
      const filename=process.env.RAISING_NATIVE_ADDON??resolve(folder,manifest.filename);
      const addon=createRequire(import.meta.url)(filename) as {request:(value:string)=>string};
      const info=JSON.parse(addon.request('{"operation":"info"}')) as {ok:boolean;implementation:string;protocolVersion:number};
      if(!info.ok||info.implementation!=='C++17'||info.protocolVersion!==1)throw new Error('Native addon protocol mismatch.');
      const value:WorkerConnection={addon:addon.request,transport:'addon',contents:new Map(),nextContentId:0};
      connections.set(executable,value);return value;
    }catch(error){fallbackReason=error instanceof Error?error.message:String(error);}
  }
  const shared=new SharedArrayBuffer(RESPONSE_BYTES+8);
  // The worker owns persistent process I/O while the legacy synchronous API waits.
  // Only transport lives in JavaScript: all state transitions run in the C++ child.
  const source=`
    const {parentPort,workerData}=require('node:worker_threads');
    const {spawn}=require('node:child_process');const {createInterface}=require('node:readline');
    const control=new Int32Array(workerData.shared,0,2),bytes=new Uint8Array(workerData.shared,8);
    let failed=null,waiting=false,stderr='';
    const reply=(text,status=1)=>{if(!waiting)return;waiting=false;const value=Buffer.from(text,'utf8');
      if(value.length>bytes.length){text=JSON.stringify({message:'Native response exceeds 64 MiB.'});status=2;}
      const encoded=Buffer.from(text,'utf8');bytes.set(encoded);Atomics.store(control,1,encoded.length);Atomics.store(control,0,status);Atomics.notify(control,0);};
    const failure=message=>{failed=message;reply(JSON.stringify({message}),2);};
    const child=spawn(workerData.executable,[],{windowsHide:true,stdio:'pipe'});
    child.on('error',error=>failure(error.message));child.on('exit',code=>failure('Native process exited '+code+': '+stderr));
    child.stderr.on('data',data=>{stderr=(stderr+data.toString()).slice(-2000);});
    createInterface({input:child.stdout}).on('line',line=>reply(line));
    parentPort.on('message',request=>{waiting=true;if(failed){failure(failed);return;}child.stdin.write(request+'\\n',error=>{if(error)failure(error.message);});});
    parentPort.on('close',()=>child.kill());
  `;
  const worker=new Worker(source,{eval:true,execArgv:[],workerData:{executable,shared}});
  worker.unref();
  const value:WorkerConnection={worker,control:new Int32Array(shared,0,2),bytes:new Uint8Array(shared,8),transport:'process',...(fallbackReason?{fallbackReason}:{}),contents:new Map<object,string>(),nextContentId:0};
  connections.set(executable,value);return value;
}
/** Both transports execute the same C++ core. A missing addon may fall back to
 * the C++ subprocess, never to TypeScript rules. */
export function nativeRequest(request: unknown, executable = nativeExecutable()): unknown {
  const c=connection(executable);
  if(request&&typeof request==='object'&&'operation' in request&&['create','command','view','evaluate','replay'].includes(String(request.operation))&&'content' in request&&request.content&&typeof request.content==='object'&&immutableJson(request.content)){
    const content=request.content;
    let id=c.contents.get(content);
    if(!id){
      if(c.contents.size>=16){const oldest=c.contents.entries().next().value!;rawRequest(c,{operation:'releaseContent',contentId:oldest[1]},executable);c.contents.delete(oldest[0]);}
      id=`content-${++c.nextContentId}`;
      const registered=rawRequest(c,{operation:'registerContent',contentId:id,content},executable) as {ok:boolean;error?:unknown};
      if(!registered.ok)return registered;
      c.contents.set(content,id);
    }
    const {content:_content,...rest}=request as Record<string,unknown>;
    return rawRequest(c,{...rest,contentId:id},executable);
  }
  return rawRequest(c,request,executable);
}
function rawRequest(c:WorkerConnection,request:unknown,executable:string):unknown {
  if(c.addon)return JSON.parse(c.addon(JSON.stringify(request))) as unknown;
  const control=c.control!,worker=c.worker!,bytes=c.bytes!;
  Atomics.store(control,0,0);
  worker.postMessage(JSON.stringify(request));
  const status=Atomics.wait(control,0,0,30_000);
  if(status==='timed-out'){connections.delete(executable);void worker.terminate();throw new Error('Native runtime request timed out.');}
  const text=new TextDecoder().decode(bytes.subarray(0,Atomics.load(control,1)));
  const response=JSON.parse(text) as unknown;
  if(Atomics.load(control,0)===2)throw new Error((response as {message:string}).message);
  return response;
}
export function nativeTransport(executable=nativeExecutable()):{transport:'addon'|'process';fallbackReason?:string}{const c=connection(executable);return {transport:c.transport,...(c.fallbackReason?{fallbackReason:c.fallbackReason}:{})};}
export function closeNativeRuntimes():void {
  for(const c of connections.values()){
    if(c.addon)for(const id of c.contents.values())c.addon(JSON.stringify({operation:'releaseContent',contentId:id}));
    if(c.worker)void c.worker.terminate();
  }
  connections.clear();
}
export function nativeCreateRun(content: ContentSnapshot, seed: number, options: Parameters<typeof createRun>[2]): CreateResult {
  // JS strings can contain isolated surrogates that cannot cross the strict UTF-8 JSON boundary.
  // Return the same field diagnostic before serialization; C++ also validates every raw caller.
  try { if (content?.project) resolveUserValues(content.project.userFields, options?.userValues); }
  catch (error) { if (error instanceof CoreFault) return { ok: false, error: error.diagnostic() }; throw error; }
  return freeze(nativeRequest({operation:'create',content,seed,options}) as CreateResult);
}
export function nativeDispatchCommand(content: ContentSnapshot, state: RunState, command: Command|unknown, expectedRevision: number): DispatchResult {
  // Preserve invalid JS-only field presence across JSON transport. Dropping an
  // undefined extra key could turn a rejected command into an accepted command.
  let transportCommand:unknown;
  try {transportCommand=JSON.parse(JSON.stringify(command,(_key,value:unknown)=>['undefined','function','symbol','bigint'].includes(typeof value)?null:value));}
  catch {transportCommand=null;}
  const result=nativeRequest({operation:'command',content,state,command:transportCommand,expectedRevision:expectedRevision??null}) as DispatchResult;
  // Rejected transactions preserve the caller's exact immutable state reference.
  return result.ok ? freeze(result) : freeze({...result,state});
}
export function nativeReplay(content:ContentSnapshot,origin:Parameters<typeof createRun>[2]&{seed:number},commands:DeepReadonly<Command[]>):CreateResult {
  return freeze(nativeRequest({operation:'replay',content,origin,commands}) as CreateResult);
}
/** One process for many serialized requests; avoids per-command process startup during analysis. */
export class NativeSession {
  private readonly process: ChildProcessWithoutNullStreams;
  private readonly pending: {resolve:(value:unknown)=>void;reject:(reason:Error)=>void}[]=[];
  private stopped: Error|null=null;
  constructor(executable=nativeExecutable()) {
    this.process=spawn(executable,[],{windowsHide:true,stdio:'pipe'});
    createInterface({input:this.process.stdout}).on('line',line=>{
      const p=this.pending.shift();if(!p)return;
      try {p.resolve(JSON.parse(line));}catch(error){p.reject(error instanceof Error?error:new Error(String(error)));}
    });
    this.process.stderr.resume();
    this.process.on('error',error=>this.fail(error));
    this.process.on('exit',code=>this.fail(new Error(`Native runtime stopped (${code}).`)));
  }
  private fail(error:Error):void {this.stopped=error;for(const p of this.pending.splice(0))p.reject(error);}
  request(request:unknown):Promise<unknown> {
    if(this.stopped)return Promise.reject(this.stopped);
    return new Promise((resolve,reject)=>{this.pending.push({resolve,reject});this.process.stdin.write(JSON.stringify(request)+'\n',error=>{if(error)this.fail(error);});});
  }
  close():void {this.process.stdin.end();}
}
