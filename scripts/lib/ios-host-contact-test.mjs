import assert from 'node:assert/strict';
import {constants} from 'node:fs';
import {lstat,open,writeFile} from 'node:fs/promises';
import path from 'node:path';
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
    assert.ok(identity.isFile()&&identity.size<=BigInt(maxBytes),'Host contact evidence must be a bounded regular file');
    if(expectedIdentity)assert.ok(String(identity.dev)===expectedIdentity.stdoutDevice&&String(identity.ino)===expectedIdentity.stdoutInode,
      'Host contact stdout does not identify the bound player');
    const contents=Buffer.alloc(Number(identity.size));
    const {bytesRead}=await handle.read(contents,0,contents.length,0);
    assert.equal(bytesRead,contents.length,'Host contact evidence could not be read completely');
    const pathname=await lstat(file,{bigint:true});
    assert.ok(pathname.isFile()&&pathname.dev===identity.dev&&pathname.ino===identity.ino,'Host contact evidence pathname changed during read');
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
  throw Error('No complete player view is available for the host contact');
}

export function validateHostContactRequest(request,expected,view,now=Date.now()) {
  const contact=contactByCase[expected.test];
  assert.ok(contact&&request?.version===1&&/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i.test(request.requestID??''),
    'Host contact request version, case or identifier is invalid');
  for(const key of ['nonce','test','pid','stdout','stdoutDevice','stdoutInode'])assert.equal(request[key],expected[key],`Host contact request ${key} differs from the bound launch`);
  assert.ok(Number.isFinite(request.createdAt)&&Number.isFinite(request.expiresAt)&&
    request.createdAt<=now+1000&&Math.abs(request.expiresAt-request.createdAt-45000)<=1&&now<request.expiresAt,
    'Host contact request is stale or has an invalid deadline');
  assert.equal(request.controlID,contact.id,'Host contact must target the original first control');
  assert.equal(request.duration,contact.duration,'Host contact must use the original contact duration');
  assert.ok(view?.screenId==='title'&&view.state===null&&Number.isSafeInteger(view.documentGeneration)&&
    view.documentGeneration>0&&request.generation===view.documentGeneration,
    'Host contact must use the current bound title before any game state exists');
  const layout=view.readingLayout;
  const control=layout?.controls?.find(item=>item.id===contact.id);
  const {point,appFrame}=request;
  assert.ok(control?.enabled===true&&point&&appFrame&&[control.x,control.y,control.width,control.height,
    point.x,point.y,appFrame.x,appFrame.y,appFrame.width,appFrame.height,layout.windowWidth,layout.windowHeight].every(Number.isFinite),
  'Host contact has no current enabled control geometry');
  assert.ok(control.width>=44&&control.height>=44&&control.x>=0&&control.y>=0&&
    control.x+control.width<=layout.windowWidth+1&&control.y+control.height<=layout.windowHeight+1&&
    Math.abs(appFrame.width-layout.windowWidth)<=1&&Math.abs(appFrame.height-layout.windowHeight)<=1&&
    Math.abs(point.x-(appFrame.x+control.x+control.width/2))<=0.5&&
    Math.abs(point.y-(appFrame.y+control.y+control.height/2))<=0.5,
  'Host contact point differs from the original authored control center');
}

export async function runHostContact({requestPath,responsePath,expected,finished,invoke}) {
  while(!finished()) {
    const contents=await readRegularFile(requestPath,16384,undefined,true);
    if(contents===undefined){await delay(50);continue;}
    assert.ok(!finished(),'XCTest ended before its host contact was admitted');
    const request=JSON.parse(contents);
    const stdout=await readRegularFile(expected.stdout,8*1024*1024,expected);
    validateHostContactRequest(request,expected,latestCompleteView(stdout));
    assert.ok(!finished(),'XCTest ended before its host contact could start');
    await writeFile(path.join(path.dirname(requestPath),'admission.json'),contents,{flag:'wx',mode:0o600});
    assert.ok(!finished(),'XCTest ended before the admitted host contact could start');
    await invoke();
    const response=JSON.parse(await readRegularFile(responsePath,32768));
    assert.ok(response.version===1&&response.ok===true&&response.postedDown===true&&response.postedUp===true,
      'Host contact must report exactly one completed down and up pair');
    for(const key of ['nonce','requestID','pid','duration'])assert.equal(response[key],request[key],`Host contact response ${key} differs from its request`);
    assert.deepEqual(response.point,request.point,'Host contact response point differs from its request');
    assert.ok(Number.isFinite(response.downUptime)&&Number.isFinite(response.upUptime)&&response.upUptime>response.downUptime,
      'Host contact response is missing ordered post timestamps');
    return response;
  }
  throw Error('XCTest ended without a host contact request');
}
