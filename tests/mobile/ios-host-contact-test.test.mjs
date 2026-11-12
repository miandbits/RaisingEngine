import assert from 'node:assert/strict';
import {mkdtemp,rm,stat,writeFile} from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';
import {runHostContact,validateHostContactRequest} from '../../scripts/lib/ios-host-contact-test.mjs';

const initialView={screenId:'title',state:null,documentGeneration:1,readingLayout:{windowWidth:390,windowHeight:844,controls:[
  {id:'start',enabled:true,x:16,y:535.4,width:350,height:74.2},
  {id:'load',enabled:true,x:16,y:621.6,width:350,height:74.2}
]}};

function requestFor(expected,now=Date.now()) {
  const exported=expected.test==='testFilesExportBackup';
  return {version:1,requestID:'12345678-1234-1234-1234-123456789abc',...expected,generation:1,
    controlID:exported?'start':'load',duration:exported?0.20:0.05,point:{x:191,y:exported?572.5:658.7},
    appFrame:{x:0,y:0,width:390,height:844},createdAt:now,expiresAt:now+45000};
}

test('host contact admission retains the original Start and Load points and durations',()=>{
  const now=100000;
  for(const test of ['testFilesExportBackup','testFilesImportBackupAfterRelaunch']) {
    const expected={test,nonce:'a'.repeat(64),pid:42,stdout:'/example/player.stdout',stdoutDevice:'5',stdoutInode:'27'};
    const request=requestFor(expected,now);
    validateHostContactRequest(request,expected,initialView,now);
    for(const altered of [{pid:43},{nonce:'b'.repeat(64)},{stdoutInode:'28'},{controlID:'other'},
      {duration:1},{point:{x:194,y:request.point.y}},{expiresAt:now},{generation:2}])
      assert.throws(()=>validateHostContactRequest({...request,...altered},expected,initialView,now));
    assert.throws(()=>validateHostContactRequest({...request,generation:undefined},expected,{...initialView,documentGeneration:undefined},now));
    assert.throws(()=>validateHostContactRequest(request,expected,{...initialView,state:{}},now));
    assert.throws(()=>validateHostContactRequest(request,expected,initialView,request.expiresAt));
  }
});

test('the host contact can be claimed exactly once for the same bound launch',async()=>{
  const directory=await mkdtemp(path.join(os.tmpdir(),'raising-host-contact-'));
  try {
    const stdout=path.join(directory,'player.stdout'),requestPath=path.join(directory,'request.json'),responsePath=path.join(directory,'response.json');
    await writeFile(stdout,JSON.stringify({event:'editor-view',view:initialView})+'\n');
    const identity=await stat(stdout,{bigint:true});
    const expected={test:'testFilesExportBackup',nonce:'a'.repeat(64),pid:42,stdout,stdoutDevice:String(identity.dev),stdoutInode:String(identity.ino)};
    const request=requestFor(expected);
    await writeFile(requestPath,JSON.stringify(request));
    let invocations=0;
    const response={version:1,ok:true,postedDown:true,postedUp:true,requestID:request.requestID,
      nonce:request.nonce,pid:request.pid,duration:request.duration,point:request.point,downUptime:1,upUptime:1.2};
    const options={requestPath,responsePath,expected,finished:()=>false,invoke:async()=>{
      invocations++;
      await writeFile(responsePath,JSON.stringify(response));
    }};
    await assert.rejects(runHostContact({...options,finished:()=>true}),/XCTest ended/);
    await writeFile(stdout,JSON.stringify({event:'editor-view',view:initialView})+'\n{"event":');
    await assert.rejects(runHostContact(options),/record is incomplete/);
    await writeFile(stdout,JSON.stringify({event:'editor-view',view:initialView})+'\n');
    assert.deepEqual(await runHostContact(options),response);
    await assert.rejects(runHostContact(options),{code:'EEXIST'});
    assert.equal(invocations,1);
  } finally {await rm(directory,{recursive:true,force:true});}
});
