import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,writeFile,readFile,readdir} from 'node:fs/promises';
import path from 'node:path';
import {spawn} from 'node:child_process';
import {once} from 'node:events';
import {randomUUID} from 'node:crypto';
import {acquireAuthoringLock} from '../../apps/qt-studio/authoring-lock.mjs';

async function fixture(){await mkdir('.cache/authoring-lock',{recursive:true});return mkdtemp(path.resolve('.cache/authoring-lock/run-'));}
function child(code,args){const process=spawn(globalThis.process.execPath,['--input-type=module','-e',code,...args],{stdio:['ignore','pipe','pipe']});let stdout='',stderr='';process.stdout.on('data',x=>stdout+=x);process.stderr.on('data',x=>stderr+=x);const done=new Promise((resolve,reject)=>{process.on('error',reject);process.on('close',(code,signal)=>resolve({code,signal,stdout,stderr}));});return {process,done,ready:async()=>{while(!stdout.includes('READY')){if(process.exitCode!==null)throw Error(stderr||stdout);await once(process.stdout,'data');}}};}
const imports="import {acquireAuthoringLock} from './apps/qt-studio/authoring-lock.mjs';import {mkdir,writeFile,readFile,unlink} from 'node:fs/promises';import {setTimeout as delay} from 'node:timers/promises';import path from 'node:path';const dir=process.argv[1];";

test('a live writer stays protected; a force-killed writer is recovered without manual file removal',{timeout:15000},async t=>{
 const root=await fixture(),dir=path.join(root,'writers'),held=child(imports+"const release=await acquireAuthoringLock(dir);console.log('READY');setInterval(()=>{},1000);",[dir]);t.after(()=>held.process.kill('SIGKILL'));await held.ready();
 await assert.rejects(acquireAuthoringLock(dir),/Another editor/);assert.equal((await readdir(dir)).length,1);
 held.process.kill('SIGKILL');await held.done;const release=await acquireAuthoringLock(dir);await release();assert.deepEqual(await readdir(dir),[]);
});

test('a crash during ticket preparation recovers, while a live preparing writer blocks entry',{timeout:15000},async t=>{
 const root=await fixture(),dir=path.join(root,'writers'),held=child(imports+"await mkdir(dir,{recursive:true});await mkdir(path.join(dir,process.pid+'_00000000-0000-0000-0000-000000000000'));console.log('READY');setInterval(()=>{},1000);",[dir]);t.after(()=>held.process.kill('SIGKILL'));await held.ready();
 await assert.rejects(acquireAuthoringLock(dir),/Another editor/);held.process.kill('SIGKILL');await held.done;
 const release=await acquireAuthoringLock(dir);await release();assert.deepEqual(await readdir(dir),[]);
});

test('six independent writers keep critical sections exclusive through repeated contention',{timeout:30000},async t=>{
 const root=await fixture(),dir=path.join(root,'writers'),counter=path.join(root,'counter');await writeFile(counter,'0');
 const code=imports+`console.log('READY');while(true){try{await readFile(path.join(dir,'..','go'));break;}catch{}await delay(5);}
 for(let n=0;n<4;n++){let release;for(let attempt=0;!release&&attempt<100;attempt++){try{release=await acquireAuthoringLock(dir);}catch(error){if(!error.message.includes('Another editor'))throw error;await delay(5);}}if(!release)throw Error('Lock never became available');try{const sentinel=path.join(dir,'..','exclusive');await writeFile(sentinel,'owned',{flag:'wx'});try{const file=path.join(dir,'..','counter'),number=Number(await readFile(file,'utf8'));await delay(8);await writeFile(file,String(number+1));}finally{await unlink(sentinel);}}finally{await release();}}`;
 const children=Array.from({length:6},()=>child(code,[dir]));t.after(()=>children.forEach(x=>x.process.kill('SIGKILL')));await Promise.all(children.map(x=>x.ready()));await writeFile(path.join(root,'go'),'go');
 for(const result of await Promise.all(children.map(x=>x.done)))assert.equal(result.code,0,result.stderr);
 assert.equal(await readFile(counter,'utf8'),'24');assert.deepEqual(await readdir(dir),[]);
});

test('a backlog of more than128 dead writer leases is reclaimed before the live-writer limit',async()=>{
 const root=await fixture(),dir=path.join(root,'writers'),dead=child("console.log(process.pid)",[]),result=await dead.done,pid=Number(result.stdout.trim());assert.equal(result.code,0);assert.ok(pid>0);
 await mkdir(dir);for(let i=0;i<140;i++)await mkdir(path.join(dir,pid+'_'+randomUUID()));
 const release=await acquireAuthoringLock(dir);await release();assert.deepEqual(await readdir(dir),[]);
});

test('transient Windows ticket read denial never admits a contender past a live owner',async()=>{
 const {default:fs}=await import('node:fs/promises'),{syncBuiltinESMExports}=await import('node:module');
 const root=await fixture(),dir=path.join(root,'writers'),release=await acquireAuthoringLock(dir),[owner]=await readdir(dir),ticket=path.join(dir,owner,'ticket.json');
 const original=fs.readFile;let denied=0;
 fs.readFile=async(file,...args)=>{if(String(file)===ticket){denied++;throw Object.assign(Error('delete pending'),{code:'EPERM'});}return original(file,...args);};syncBuiltinESMExports();
 try{await assert.rejects(acquireAuthoringLock(dir),/Another editor/);assert.ok(denied>1);assert.deepEqual(await readdir(dir),[owner]);}
 finally{fs.readFile=original;syncBuiltinESMExports();await release();}
 const next=await acquireAuthoringLock(dir);await next();assert.deepEqual(await readdir(dir),[]);
});

test('a participant removed between ticket stat and read is retired after transient denial',async()=>{
 const {default:fs}=await import('node:fs/promises'),{syncBuiltinESMExports}=await import('node:module');
 const root=await fixture(),dir=path.join(root,'writers'),release=await acquireAuthoringLock(dir),[owner]=await readdir(dir),ticket=path.join(dir,owner,'ticket.json');
 const original=fs.readFile;let retired=false;
 fs.readFile=async(file,...args)=>{if(String(file)===ticket&&!retired){retired=true;await release();throw Object.assign(Error('delete pending'),{code:'EPERM'});}return original(file,...args);};syncBuiltinESMExports();
 try{const next=await acquireAuthoringLock(dir);assert.ok(retired);await next();assert.deepEqual(await readdir(dir),[]);}
 finally{fs.readFile=original;syncBuiltinESMExports();if(!retired)await release();}
});
