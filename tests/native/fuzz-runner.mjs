import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { createInterface } from 'node:readline';
import { createRequire } from 'node:module';
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { resolve } from 'node:path';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { nativeExecutable } from '../../dist/packages/native-bridge/index.js';
import { malformedCorpus } from './fuzz-corpus.mjs';

const transport=process.argv[2], built=await buildSnapshot(await loadProject('examples/academy-demo'));
assert.equal(built.ok,true); const corpus=malformedCorpus(built.snapshot);
mkdirSync('.cache/native-fuzz',{recursive:true});
let request, child, stderr='', pending;
if(transport==='addon') {
  const manifest=JSON.parse(readFileSync('dist/native/addon.json','utf8'));
  const addon=createRequire(import.meta.url)(resolve('dist/native',manifest.filename));
  request=async raw=>addon.request(raw);
} else {
  child=spawn(nativeExecutable(),[],{windowsHide:true,stdio:'pipe'});
  child.stderr.on('data',data=>{stderr=(stderr+data).slice(-8000);});
  createInterface({input:child.stdout}).on('line',line=>{pending?.resolve(line);pending=null;});
  child.on('error',error=>pending?.reject(error));
  child.on('exit',code=>pending?.reject(Error(`Native exit ${code}: ${stderr}`)));
  request=raw=>new Promise((resolve,reject)=>{pending={resolve,reject};child.stdin.write(raw+'\n');});
}
const codes={}, accepted=[];
try {
  const valid=JSON.stringify({operation:'create',content:built.snapshot,seed:42,options:{mode:'full'}});
  const baseline=JSON.parse(await request(valid)); assert.equal(baseline.ok,true);
  for(const [index,entry] of corpus.entries()) {
    writeFileSync(`.cache/native-fuzz/last-${transport}.json`,JSON.stringify({index,...entry}));
    const value=JSON.parse(await request(entry.raw));
    assert.equal(typeof value.ok,'boolean',entry.label);
    if(!value.ok) {
      for(const key of ['code','message','path'])assert.equal(typeof value.error?.[key],'string',entry.label);
      codes[value.error.code]=(codes[value.error.code]??0)+1;
    } else accepted.push(entry.label);
    assert.equal(JSON.parse(await request('{"operation":"info"}')).implementation,'C++17',entry.label);
    assert.deepEqual(JSON.parse(await request(valid)),baseline,entry.label+' corrupted the next valid run');
  }
  assert.equal(stderr,'','sanitizer/transport diagnostics');
  const report={transport,cases:corpus.length,rejected:corpus.length-accepted.length,accepted:accepted.length,codes};
  writeFileSync(`.cache/native-fuzz/result-${transport}.json`,JSON.stringify(report,null,2));
  console.log(JSON.stringify(report));
} finally { child?.stdin.end(); }
