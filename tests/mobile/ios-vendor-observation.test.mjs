import assert from 'node:assert/strict';
import test from 'node:test';
import {describeVendorContactProof,verifyVendorContactMarker} from '../../scripts/lib/ios-vendor-observation.mjs';

test('isolated contact evidence requires exactly one accepted marker for the bound launch',()=>{
  const nonce='7'.repeat(64),pid=13057;
  const marker=`FIRST_CONTACT_PROOF_ACCEPTED ${nonce} ${pid}\n`;
  const bound={nonce,pid};
  assert.equal(verifyVendorContactMarker('XCTest started\n'+marker+'Test Passed\n',bound),true);
  assert.throws(()=>verifyVendorContactMarker('Only XCTest exit is known\n',bound),/one bound/);
  assert.throws(()=>verifyVendorContactMarker(marker+marker,bound),/one bound/);
  assert.throws(()=>verifyVendorContactMarker(marker,{...bound,pid:pid+1}),/PID mismatch/);
  assert.throws(()=>verifyVendorContactMarker(marker,{...bound,nonce:'4'.repeat(64)}),/nonce mismatch/);
});

test('failed engine diagnostics retain their own bound reason without implying a functional failure',()=>{
  const context={nonce:'a'.repeat(64),pid:1020,test:'testFilesImportBackupAfterRelaunch'};
  const reason='The native contact record is incomplete';
  const marker='FIRST_CONTACT_PROOF_INCOMPLETE '+JSON.stringify({...context,reason})+'\n';
  assert.deepEqual(describeVendorContactProof(marker,context),
    {proofAccepted:false,proofStatus:'reported-incomplete',proofError:reason});
  assert.equal(describeVendorContactProof(marker,{...context,pid:1021}).proofStatus,'unavailable');
  assert.equal(describeVendorContactProof(marker+marker,context).proofStatus,'unavailable');
});
