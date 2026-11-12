import assert from 'node:assert/strict';
import {constants} from 'node:fs';
import {lstat,mkdtemp,open,rm,writeFile} from 'node:fs/promises';
import path from 'node:path';
import {spawn} from 'node:child_process';
import {setTimeout as delay} from 'node:timers/promises';

const contactByCase={
  testFilesExportBackup:{id:'start',duration:0.20},
  testFilesImportBackupAfterRelaunch:{id:'load',duration:0.05}
};

async function readRegularFile(file,maxBytes,expectedIdentity,allowMissing=false) {
  let handle;
  try {handle=await open(file,constants.O_RDONLY|constants.O_NOFOLLOW|constants.O_NONBLOCK);}
  catch(error) {if(allowMissing&&error.code==='ENOENT')return undefined;throw error;}
  try {
    const identity=await handle.stat({bigint:true});
    assert.ok(identity.isFile()&&identity.size<=BigInt(maxBytes),'idb contact evidence must be a bounded regular file');
    if(expectedIdentity)assert.ok(String(identity.dev)===expectedIdentity.stdoutDevice&&String(identity.ino)===expectedIdentity.stdoutInode,
      'idb contact stdout does not identify the bound player');
    const contents=Buffer.alloc(Number(identity.size));
    const {bytesRead}=await handle.read(contents,0,contents.length,0);
    assert.equal(bytesRead,contents.length,'idb contact evidence could not be read completely');
    const pathname=await lstat(file,{bigint:true});
    assert.ok(pathname.isFile()&&pathname.dev===identity.dev&&pathname.ino===identity.ino,'idb contact evidence pathname changed during read');
    return contents.toString('utf8');
  } finally {await handle.close();}
}

function latestCompleteView(text) {
  assert.ok(text.endsWith('\n'),'The latest bound player stdout record is incomplete');
  for(const line of text.split('\n').slice(0,-1).reverse()) {
    try {
      const value=JSON.parse(line);
      if(value?.event==='editor-view')return value.view;
    } catch {}
  }
  throw Error('No complete player view is available for the idb contact');
}

export function validateIDBContactRequest(request,expected,view,now=Date.now()) {
  const contact=contactByCase[expected.test];
  assert.ok(contact&&request?.version===1&&/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i.test(request.requestID??''),
    'idb contact request version, case or identifier is invalid');
  for(const key of ['nonce','test','pid','stdout','stdoutDevice','stdoutInode'])assert.equal(request[key],expected[key],`idb contact request ${key} differs from the bound launch`);
  assert.ok(Number.isFinite(request.createdAt)&&Number.isFinite(request.expiresAt)&&
    request.createdAt<=now+1000&&Math.abs(request.expiresAt-request.createdAt-45000)<=1&&now<request.expiresAt,
    'idb contact request is stale or has an invalid deadline');
  assert.equal(request.controlID,contact.id,'idb contact must target the original first control');
  assert.equal(request.duration,contact.duration,'idb contact must use the admitted contact duration');
  assert.ok(view?.screenId==='title'&&view.state===null&&Number.isSafeInteger(view.documentGeneration)&&
    view.documentGeneration>0&&request.generation===view.documentGeneration,
    'idb contact must use the current bound title before any game state exists');
  const layout=view.readingLayout;
  const control=layout?.controls?.find(item=>item.id===contact.id);
  const {point,appFrame}=request;
  assert.equal(request.orientation,'portrait','idb contact must retain original portrait orientation');
  assert.ok(control?.enabled===true&&point&&appFrame&&[control.x,control.y,control.width,control.height,
    point.x,point.y,appFrame.x,appFrame.y,appFrame.width,appFrame.height,layout.windowWidth,layout.windowHeight].every(Number.isFinite),
  'idb contact has no current enabled control geometry');
  assert.ok(control.width>=44&&control.height>=44&&control.x>=0&&control.y>=0&&
    control.x+control.width<=layout.windowWidth+1&&control.y+control.height<=layout.windowHeight+1&&
    Math.abs(appFrame.width-layout.windowWidth)<=1&&Math.abs(appFrame.height-layout.windowHeight)<=1&&
    Math.abs(point.x-(appFrame.x+control.x+control.width/2))<=0.5&&
    Math.abs(point.y-(appFrame.y+control.y+control.height/2))<=0.5,
  'idb contact point differs from the original authored control center');
}

export async function runIDBContact({requestPath,responsePath,expected,finished,invoke}) {
  while(!finished()) {
    const contents=await readRegularFile(requestPath,16384,undefined,true);
    if(contents===undefined){await delay(50);continue;}
    assert.ok(!finished(),'XCTest ended before its idb contact was admitted');
    const request=JSON.parse(contents);
    const stdout=await readRegularFile(expected.stdout,8*1024*1024,expected);
    validateIDBContactRequest(request,expected,latestCompleteView(stdout));
    assert.ok(!finished(),'XCTest ended before its idb contact could start');
    await writeFile(path.join(path.dirname(requestPath),'admission.json'),contents,{flag:'wx',mode:0o600});
    assert.ok(!finished(),'XCTest ended before the admitted idb contact could start');
    await invoke();
    const response=JSON.parse(await readRegularFile(responsePath,32768));
    assert.ok(response.version===1&&response.ok===true&&response.backend==='idb'&&response.rpcCompleted===true&&response.streamEvents===3,
      'idb contact must report one completed down, delay and up RPC stream');
    for(const key of ['nonce','requestID','pid','duration'])assert.equal(response[key],request[key],`idb contact response ${key} differs from its request`);
    assert.deepEqual(response.point,request.point,'idb contact response point differs from its request');
    assert.equal(response.deviceUDID,expected.deviceUDID,'idb response belongs to another device');
    assert.equal(response.guestDelivery,'requires-native-proof','An RPC response cannot prove guest delivery');
    assert.ok(/^[0-9]+$/.test(response.rpcStartedNS??'')&&/^[0-9]+$/.test(response.rpcCompletedNS??'')&&
      BigInt(response.rpcCompletedNS)>BigInt(response.rpcStartedNS),'idb response lacks ordered monotonic RPC times');
    return response;
  }
  throw Error('XCTest ended without a idb contact request');
}

export function idbExperimentConfiguration(environment=process.env) {
  const backend=environment.RAISING_IOS_CONTACT_ROUTE;
  if(backend===undefined)return;
  assert.ok(process.platform==='darwin'&&environment.CI==='true'&&environment.GITHUB_ACTIONS==='true'&&
    environment.RUNNER_ENVIRONMENT==='github-hosted'&&environment.GITHUB_REF===(backend==='startup'
      ?'refs/heads/codex/ios-idb-startup-observation':'refs/heads/codex/ios-idb-route-comparison'),
    'The idb experiment runs only on its isolated GitHub-hosted macOS branch');
  assert.ok(['xctest','idb','preflight','startup'].includes(backend),'Unknown iOS contact route');
  assert.ok(environment.DEVELOPER_DIR==='/Applications/Xcode_26.6.app/Contents/Developer'&&
    environment.RAISING_IOS_SIMULATOR_RUNTIME==='com.apple.CoreSimulator.SimRuntime.iOS-26-5'&&
    environment.RAISING_IOS_SIMULATOR_DEVICE_TYPE==='com.apple.CoreSimulator.SimDeviceType.iPhone-17e',
    'Contact comparison must retain its pinned Xcode, Simulator runtime and device');
  if(backend==='xctest')return {backend};
  const {RAISING_IDB_COMPANION:companion,RAISING_IDB_PYTHON:python}=environment;
  assert.ok(path.isAbsolute(companion??'')&&path.isAbsolute(python??''),
    'The idb experiment requires the pinned companion and disposable Python environment');
  return {backend,companion,python};
}

export async function startIDBCompanion(companion,device,out,{observeStartup}={}) {
  assert.match(device,/^[0-9a-f]{8}-(?:[0-9a-f]{4}-){3}[0-9a-f]{12}$/i);
  // sockaddr_un has a short pathname limit. The unique parent is mode 0700.
  const directory=await mkdtemp('/private/tmp/raising-idb-');
  const socket=path.join(directory,'companion.sock');
  const log=path.join(out,'idb-companion.log');
  let child,finished,closed,forced=false,startedAt,startedWallMs,spawnEventAtMs,observer;
  const observationAbort=observeStartup?new AbortController():undefined;
  const elapsed=()=>startedAt===undefined?null:performance.now()-startedAt;
  const stop=async()=>{
    const stoppedAtMs=elapsed();
    observationAbort?.abort();
    try {
      if(child&&!finished) {
        child.kill('SIGTERM');
        const force=setTimeout(()=>{forced=true;child.kill('SIGKILL');},5000);
        try {await closed;} finally {clearTimeout(force);}
      }
    } finally {await rm(directory,{recursive:true,force:true});}
    return {pid:child?.pid??null,exitCode:finished?.code??null,signal:finished?.signal??null,forced,
      ...(observeStartup?{stoppedAtMs,stoppedAfterMs:elapsed()}: {})};
  };
  const finishObservation=async()=>observer?await observer:{attempted:false};
  const saveObservation=async(status,elapsedMs,cleanup)=> {
    if(!observeStartup)return;
    await writeFile(path.join(out,'idb-startup-observation.json'),JSON.stringify({
      status,elapsedMs,startedWallMs,spawnEventAtMs,socketPath:socket,socketPathBytes:Buffer.byteLength(socket),
      cleanup,observation:await finishObservation()
    },null,2),{flag:'wx',mode:0o600});
  };
  try {
    const output=await open(log,'wx',0o600);
    try {
      child=spawn(companion,['--udid',device,'--only','simulator','--grpc-domain-sock',socket,
        '--terminate-offline','1','--log-level','debug'],{stdio:['ignore',output.fd,output.fd]});
      startedAt=performance.now();
      startedWallMs=Date.now();
      closed=new Promise(resolve=>{
        child.once('spawn',()=>{spawnEventAtMs=elapsed();});
        child.once('error',error=>{finished=error;observationAbort?.abort();resolve();});
        child.once('exit',(code,signal)=>{finished??={code,signal};observationAbort?.abort();});
        child.once('close',(code,signal)=>{finished??={code,signal};resolve();});
      });
    } finally {await output.close();}
    const deadline=startedAt+30000;
    do {
      assert.ok(!finished,'Pinned idb companion exited before its Unix socket became available');
      const entry=await lstat(socket).catch(error=>{if(error.code!=='ENOENT')throw error;});
      if(entry) {
        assert.ok(entry.isSocket(),'The pinned companion did not create a Unix socket');
        const readyAtMs=elapsed();
        assert.ok(!finished,'Pinned idb companion exited while checking its Unix socket');
        assert.ok(readyAtMs<=30000,'Pinned idb companion Unix socket was observed after the 30-second deadline');
        observationAbort?.abort();
        return {socket,log,stop,saveObservation:cleanup=>saveObservation('socket-observed',readyAtMs,cleanup)};
      }
      if(observeStartup&&!observer&&child.pid&&elapsed()>=15000&&performance.now()<deadline) {
        observer=Promise.resolve().then(()=>observeStartup({
          pid:child.pid,device,out,startedAt,startedWallMs,deadline,executable:companion,
          isAlive:()=>!finished,signal:observationAbort.signal
        })).catch(error=>({attempted:true,error:String(error)}));
      }
      await delay(100);
    } while(performance.now()<deadline);
    throw Error('Timed out waiting for the pinned idb companion Unix socket');
  } catch(error) {
    const failureAtMs=elapsed();
    const cleanup=await stop();
    await writeFile(path.join(out,'idb-companion-start-failure.json'),
      JSON.stringify({error:String(error),cleanup},null,2),{flag:'wx',mode:0o600});
    if(observeStartup)error.saveStartupObservation=()=>saveObservation('socket-not-observed',failureAtMs,cleanup);
    throw error;
  }
}

export async function verifyIDBPreflight({companion,python,coordinator,device,out,tool}) {
  const context=path.join(out,'idb-preflight-context.json');
  const response=path.join(out,'idb-preflight.json');
  await writeFile(context,JSON.stringify({version:1,deviceUDID:device,appID:null,pid:null,nonce:null}),{flag:'wx',mode:0o600});
  await tool(python,[coordinator,'preflight','--socket',companion.socket,'--context',context,
    '--response',response],'idb-preflight.log',30000);
  const receipt=JSON.parse(await readRegularFile(response,32768));
  assert.ok(receipt.version===1&&receipt.ok===true&&receipt.deviceUDID===device&&receipt.streamEvents===0&&
    receipt.noInputPosted===true&&receipt.rpcCompleted===true&&receipt.guestDelivery==='unobserved'&&
    receipt.guestRegistration==='unobserved','No-input compatibility receipt did not bind the owned device and empty stream');
  const {widthPoints,heightPoints,widthPixels,heightPixels,density}=receipt.screen??{};
  assert.ok([widthPoints,heightPoints,widthPixels,heightPixels,density].every(value=>Number.isFinite(value)&&value>0)&&
    heightPoints>widthPoints&&Math.abs(widthPixels/density-widthPoints)<1e-6&&Math.abs(heightPixels/density-heightPoints)<1e-6,
    'idb preflight did not verify portrait point and pixel dimensions');
  const log=await readRegularFile(companion.log,8*1024*1024);
  const routes=[...log.matchAll(/Negotiated the (\w+) HID transport/g)].map(match=>match[1]);
  assert.deepEqual(routes,['indigo'],'Pinned companion must negotiate the Indigo route exactly once');
  return receipt.screen;
}

export async function readAcceptedContactProof({transcript,stdout,pid,nonce,stdoutDevice,stdoutInode}) {
  const markers=[...transcript.matchAll(/^FIRST_CONTACT_PROOF_ACCEPTED ([a-f0-9]{64}) (\d+)\s*$/gm)];
  assert.equal(markers.length,1,'XCTest must validate one native first-contact proof');
  assert.equal(markers[0][1],nonce,'XCTest validated the wrong first-contact nonce');
  assert.equal(Number(markers[0][2]),pid,'XCTest validated the wrong first-contact PID');
  const records=[];
  const bytes=await readRegularFile(stdout,8*1024*1024,{stdoutDevice,stdoutInode});
  for(const line of bytes.split('\n').slice(0,-1)) {
    try {
      const entry=JSON.parse(line);
      if(entry?.event==='input-proof')records.push(entry);
    } catch {}
  }
  assert.equal(records.length,1,'The app must emit one native first-contact proof');
  const [proof]=records;
  assert.equal(proof.nonce,nonce,'Native first-contact nonce mismatch');
  assert.equal(proof.pid,pid,'Native first-contact PID mismatch');
  assert.ok(proof.source==='SDL_PollEvent'&&proof.samples?.[0]?.phase==='down'&&proof.samples.at(-1).phase==='up');
  const nanos=BigInt(proof.samples.at(-1).timestampNS)-BigInt(proof.samples[0].timestampNS);
  assert.ok(nanos>0n,'Native first-contact timestamps are unordered');
  return {pid,nonce,durationNS:String(nanos),windowID:proof.windowID};
}
