import test from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';

for(const transport of ['process','addon'])test(`bounded malformed API corpus survives ${transport} and recovers after every request`,()=>{
  // Isolate the in-process addon too: an abort or hang becomes a failing child,
  // with the exact last payload preserved for reproduction.
  const child=spawnSync(process.execPath,['tests/native/fuzz-runner.mjs',transport],{
    encoding:'utf8',windowsHide:true,timeout:60000,maxBuffer:1024*1024,env:{...process.env,NODE_OPTIONS:''},
  });
  assert.equal(child.status,0,child.stdout+child.stderr+String(child.error??''));
  const report=JSON.parse(child.stdout.trim());assert.equal(report.cases,1000);assert.ok(report.rejected>500);
});
