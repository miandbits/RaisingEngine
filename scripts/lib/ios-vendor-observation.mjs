import assert from 'node:assert/strict';
import {stat} from 'node:fs/promises';
import path from 'node:path';

export function vendorIsolationConfiguration(environment=process.env) {
  if(environment.RAISING_IOS_VENDOR_ARM===undefined)return;
  assert.ok(process.platform==='darwin'&&environment.CI==='true'&&environment.GITHUB_ACTIONS==='true'&&
    environment.RUNNER_ENVIRONMENT==='github-hosted'&&environment.GITHUB_REF==='refs/heads/codex/ios-vendor-isolation',
  'Vendor isolation may only run on its isolated GitHub-hosted macOS branch');
  assert.ok(environment.DEVELOPER_DIR==='/Applications/Xcode_26.6.app/Contents/Developer'&&
    environment.RAISING_IOS_SIMULATOR_RUNTIME==='com.apple.CoreSimulator.SimRuntime.iOS-26-5'&&
    environment.RAISING_IOS_SIMULATOR_DEVICE_TYPE==='com.apple.CoreSimulator.SimDeviceType.iPhone-17e',
  'Vendor isolation must use the predeclared Simulator toolchain, runtime and device');
  const arm=environment.RAISING_IOS_VENDOR_ARM;
  const output=environment.RAISING_IOS_VENDOR_OUTPUT;
  assert.ok(['U','S','E'].includes(arm)&&path.isAbsolute(output??''),
    'Vendor isolation requires a predeclared arm and unique evidence root');
  return {arm,output};
}

export async function captureVendorTouchLog({device,start,executable,out,tool}) {
  const file='postcase-touch.log';
  try {
    assert.match(executable,/^[A-Za-z0-9_-]+$/);
    await tool('/usr/bin/xcrun',['simctl','spawn',device,'log','show','--start','@'+start,
      '--timezone','UTC','--style','compact','--info','--debug','--predicate',
      `(process == "${executable}" AND subsystem == "com.apple.UIKit" AND category == "EventDispatch") OR (process == "testmanagerd" AND (subsystem == "com.apple.dt.xctest" OR subsystem == "com.apple.recap")) OR (process == "backboardd" AND category == "TouchEvents")`],file,30000);
    return {file,bytes:(await stat(path.join(out,file))).size,queryCompleted:true};
  } catch(error) {
    return {file,queryCompleted:false,error:String(error)};
  }
}

export function verifyVendorContactMarker(transcript,{pid,nonce}) {
  const markers=[...transcript.matchAll(/^FIRST_CONTACT_PROOF_ACCEPTED ([a-f0-9]{64}) (\d+)[ \t]*\r?$/gm)];
  assert.equal(markers.length,1,'XCTest must validate one bound contact proof');
  assert.equal(markers[0][1],nonce,'XCTest contact proof nonce mismatch');
  assert.equal(Number(markers[0][2]),pid,'XCTest contact proof PID mismatch');
  return true;
}

export function describeVendorContactProof(transcript,contact) {
  try {
    verifyVendorContactMarker(transcript,contact);
    return {proofAccepted:true};
  } catch(error) {
    const incomplete=[...transcript.matchAll(/^FIRST_CONTACT_PROOF_INCOMPLETE (\{.*\})\r?$/gm)];
    if(incomplete.length===1) {
      try {
        const {pid,nonce,test,reason}=JSON.parse(incomplete[0][1]);
        if(pid===contact.pid&&nonce===contact.nonce&&test===contact.test&&typeof reason==='string')
          return {proofAccepted:false,proofStatus:'reported-incomplete',proofError:reason};
      } catch {}
    }
    return {proofAccepted:false,proofStatus:'unavailable',proofError:String(error)};
  }
}
