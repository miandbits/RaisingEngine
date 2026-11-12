import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {mkdir,readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';

const [order]=process.argv.slice(2);
assert.ok(['U-S-E','E-S-U'].includes(order),'Use one of the two frozen triplet orders');
const root=path.resolve('.cache/ios-vendor-ci/runs',order);
const archive=path.resolve('.cache/ios-vendor-ci/apps');
await mkdir(path.dirname(root),{recursive:true});
await mkdir(root);
const results=[];
const arms=order.split('-');
const wrapper=async(args,env)=>new Promise((resolve,reject)=>{
  const child=spawn(process.execPath,args,{env,stdio:'inherit'});
  child.once('error',reject);
  child.once('close',(code,signal)=>resolve({code,signal}));
});
for(const [index,arm] of arms.entries()) {
  const output=path.join(root,`arm-${index+1}-${arm}`);
  const args=arm==='E'?['scripts/test-ios-system-ui.mjs',path.join(archive,'engine'),
    '--project','examples/my-first-raising-game','--family','iphone','--test','files-roundtrip','--text-scale','2']:
    ['scripts/test-ios-vendor-reference.mjs',path.join(archive,arm+'.app')];
  const {code,signal}=await wrapper(args,{...process.env,RAISING_IOS_VENDOR_ARM:arm,RAISING_IOS_VENDOR_OUTPUT:output});
  const result={index:index+1,arm,attempted:true,code,signal,cleanupVerified:false};
  try {
    const {out}=JSON.parse(await readFile(path.join(output,'latest.json'),'utf8'));
    assert.equal(path.dirname(out),output,'Reference latest evidence is outside the current arm');
    const receipt=JSON.parse(await readFile(path.join(out,'result.json'),'utf8'));
    assert.equal(arm==='E'?receipt.vendorIsolation?.arm:receipt.arm,arm,'Evidence belongs to another vendor arm');
    const createAttempted=arm==='E'?receipt.vendorIsolation?.createAttempted:receipt.createAttempted;
    result.cleanupVerified=receipt.device?.id?receipt.deletedDevice===true:createAttempted!==true;
    result.receipt=path.relative(root,path.join(out,'result.json'));
    result.actualFilesRoundTrip=arm==='E'&&receipt.actualFilesRoundTrip===true;
    if(arm==='E') {
      assert.ok(code!==0||result.actualFilesRoundTrip,'Engine arm must retain the full Files verdict');
      result.functionalPassed=code===0&&receipt.ok===true&&result.cleanupVerified&&result.actualFilesRoundTrip;
      result.contactProofs=(receipt.vendorIsolation.contacts??[]).map(contact=>({
        test:contact.test,accepted:contact.proofAccepted===true,
      }));
      const expectedCases=['testFilesExportBackup','testFilesImportBackupAfterRelaunch'];
      result.diagnosticQualified=result.contactProofs.length===expectedCases.length&&
        result.contactProofs.every((contact,caseIndex)=>contact.test===expectedCases[caseIndex]&&contact.accepted);
      result.passed=result.functionalPassed&&result.diagnosticQualified;
    } else {
      result.diagnosticQualified=receipt.proofAccepted===true;
      result.passed=code===0&&receipt.ok===true&&result.cleanupVerified&&result.diagnosticQualified;
    }
  } catch(error) {result.passed=false;result.evidenceError=String(error);}
  results.push(result);
  await writeFile(path.join(root,'triplet.json'),JSON.stringify({order,results},null,2));
  if(!result.cleanupVerified) {
    for(let j=index+1;j<arms.length;j++)results.push({index:j+1,arm:arms[j],attempted:false,
      reason:'Previous owned-device cleanup was not established'});
    break;
  }
}
await writeFile(path.join(root,'triplet.json'),JSON.stringify({order,results},null,2));
process.exitCode=results.every(result=>result.passed)?0:1;
