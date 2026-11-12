import assert from 'node:assert/strict';
import {mkdtemp,readFile,rm,stat,writeFile} from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';
import {readAcceptedContactProof,runIDBContact,validateIDBContactRequest,verifyIDBPreflight} from '../../scripts/lib/ios-idb-contact-test.mjs';
import {runBoundedAppleTool} from '../../scripts/lib/ios-simulator-test.mjs';

const nonce='b'.repeat(64),pid=1042,deviceUDID='51431F8A-57AE-49C3-9658-56ECC48976D4';
const firstTest='testFilesExportBackup';
const view={screenId:'title',state:null,documentGeneration:1,readingLayout:{windowWidth:390,windowHeight:844,
  controls:[{id:'start',enabled:true,x:12,y:534.5,width:358,height:76}]}};

async function fixture(t) {
  const directory=await mkdtemp(path.join(os.tmpdir(),'idb-protocol-'));
  t.after(()=>rm(directory,{recursive:true,force:true}));
  const stdout=path.join(directory,'app.stdout');
  const requestPath=path.join(directory,'request.json'),responsePath=path.join(directory,'response.json');
  await writeFile(stdout,JSON.stringify({event:'editor-view',view})+'\n');
  const {dev,ino}=await stat(stdout,{bigint:true});
  const expected={test:firstTest,stdout,stdoutDevice:String(dev),stdoutInode:String(ino),nonce,pid,deviceUDID};
  const {deviceUDID:_,...identity}=expected;
  const createdAt=Date.now();
  const request={...identity,version:1,requestID:'35011B7E-2CDD-4930-9F39-4F3294D0F1C3',generation:1,
    controlID:'start',point:{x:191,y:572.5},appFrame:{x:0,y:0,width:390,height:844},orientation:'portrait',
    duration:0.20,createdAt,expiresAt:createdAt+45000};
  return {directory,stdout,requestPath,responsePath,expected,request};
}

test('idb admits the bound title center once and binds completion to the same device and request',async t=>{
  const {directory,requestPath,responsePath,expected,request}=await fixture(t);
  await writeFile(requestPath,JSON.stringify(request));
  let invocations=0;
  const options={requestPath,responsePath,expected,finished:()=>false,invoke:async()=>{
    invocations++;
    assert.equal(await readFile(path.join(directory,'admission.json'),'utf8'),JSON.stringify(request));
    await writeFile(responsePath,JSON.stringify({version:1,ok:true,backend:'idb',rpcCompleted:true,streamEvents:3,
      guestDelivery:'requires-native-proof',rpcStartedNS:'123',rpcCompletedNS:'456',deviceUDID,nonce,pid,
      requestID:request.requestID,point:request.point,duration:request.duration}));
  }};
  await runIDBContact(options);
  await assert.rejects(runIDBContact(options),/EEXIST/);
  assert.equal(invocations,1);
});

test('idb fails closed on stale, moved, wrong-generation and unfinished stdout requests',async t=>{
  const {requestPath,responsePath,stdout,expected,request}=await fixture(t);
  assert.throws(()=>validateIDBContactRequest(request,expected,view,request.expiresAt),/stale/);
  assert.throws(()=>validateIDBContactRequest({...request,point:{x:190,y:572.5}},expected,view),/center/);
  assert.throws(()=>validateIDBContactRequest({...request,generation:2},expected,view),/title/);
  await writeFile(requestPath,JSON.stringify(request));
  await writeFile(stdout,JSON.stringify({event:'editor-view',view})+'\n{"event":','utf8');
  await assert.rejects(runIDBContact({requestPath,responsePath,expected,finished:()=>false,
    invoke:()=>assert.fail('An in-flight view cannot admit input')}),/incomplete/);
});

test('guest timing requires an exact Swift validation marker and same launch proof',async t=>{
  const {stdout,expected}=await fixture(t);
  const receipt={event:'input-proof',nonce,pid,source:'SDL_PollEvent',windowID:2,
    samples:[{phase:'down',timestampNS:'100'},{phase:'up',timestampNS:'200000100'}]};
  await writeFile(stdout,JSON.stringify(receipt)+'\n','utf8');
  const marker=`FIRST_CONTACT_PROOF_ACCEPTED ${nonce} ${pid}\n`;
  assert.deepEqual(await readAcceptedContactProof({...expected,transcript:marker}),
    {pid,nonce,durationNS:'200000000',windowID:2});
  await assert.rejects(readAcceptedContactProof({...expected,transcript:marker+marker}),/validate one/);
  await assert.rejects(readAcceptedContactProof({...expected,transcript:'ordinary XCTest passed\n'}),/validate one/);
});

test('zero-message preflight rejects an alternate route despite its matching empty RPC',async t=>{
  const {directory}=await fixture(t);
  const log=path.join(directory,'companion.log');
  await writeFile(log,'Negotiated the dtuhid HID transport\n');
  await assert.rejects(verifyIDBPreflight({companion:{log,socket:'/private/test.sock'},python:'/test/python',
    coordinator:'/test/coordinator',device:deviceUDID,out:directory,tool:async(_python,args)=>{
      assert.equal(args[1],'preflight');
      assert.equal(args.includes('--request'),false);
      await writeFile(args.at(-1),JSON.stringify({version:1,ok:true,deviceUDID,streamEvents:0,noInputPosted:true,
        rpcCompleted:true,guestRegistration:'unobserved',guestDelivery:'unobserved',
        screen:{widthPoints:390,heightPoints:844,widthPixels:1170,heightPixels:2532,density:3}}));
    }}),/Indigo route/);
});

test('an aborted diagnostic process returns a cancellation error',async t=>{
  const {directory}=await fixture(t);
  const cancel=new AbortController();
  cancel.abort();
  await assert.rejects(runBoundedAppleTool(process.execPath,['-e','process.exit(0)'],
    {cwd:directory,logFile:path.join(directory,'abort.log'),abortSignal:cancel.signal}),/tool canceled/);
});
