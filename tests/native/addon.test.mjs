import test from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createRequire } from 'node:module';
import { Worker } from 'node:worker_threads';
import { readFile, mkdtemp, rm, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { tmpdir } from 'node:os';
import { copyNativeResources } from '../../scripts/copy-native-resources.mjs';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { probeAddon } from './addon-probe.mjs';
import { nativeRequest, nativeTransport, closeNativeRuntimes } from '../../dist/packages/native-bridge/index.js';
const require = createRequire(import.meta.url);

test('Node host loads stable C ABI and matches TS through every sample route and replay', async () => {
  const report = await probeAddon();
  assert.equal(report.routes, 7); assert.ok(report.commands > 700);
});

test('same immutable addon binary works inside actual Electron main process', () => {
  const env = { ...process.env, NODE_OPTIONS:'', RAISING_NATIVE_TRANSPORT:'addon' };
  delete env.ELECTRON_RUN_AS_NODE;
  const result = spawnSync(require('electron'), ['tests/native/addon-electron.cjs'], {
    encoding:'utf8', timeout:120_000, windowsHide:true,
    env,
  });
  assert.equal(result.status, 0, result.stdout + result.stderr);
  const report = JSON.parse(result.stdout.split(/\r?\n/).find(x => x.startsWith('ADDON_RESULT=')).slice(13));
  assert.match(report.versions.electron, /^44\./); assert.equal(report.routes, 7);
  assert.equal(report.transport.transport, 'addon');
});

test('unavailable or invalid addon falls back to actual C++ subprocess and reports reason', () => {
  const previous = process.env.RAISING_NATIVE_ADDON;
  try {
    for (const filename of [resolve('.cache/nonexistent-addon.node'), resolve('package.json')]) {
      closeNativeRuntimes(); process.env.RAISING_NATIVE_ADDON = filename;
      assert.equal(nativeTransport().transport, 'process');
      assert.ok(nativeTransport().fallbackReason);
      assert.equal(nativeRequest({ operation:'info' }).implementation, 'C++17');
    }
  } finally { closeNativeRuntimes(); if (previous === undefined) delete process.env.RAISING_NATIVE_ADDON; else process.env.RAISING_NATIVE_ADDON = previous; }
});

test('packaged native resources load addon and reject a mismatched manifest safely', async t => {
  const folder = await mkdtemp(resolve(tmpdir(),'raising-addon-package-'));
  t.after(() => rm(folder,{recursive:true,force:true}));
  await copyNativeResources(folder);
  const runtime = JSON.parse(await readFile(resolve(folder,'runtime.json'),'utf8'));
  const addon = JSON.parse(await readFile(resolve(folder,'addon.json'),'utf8'));
  assert.ok((await readFile(resolve(folder,'NODE-API-LICENSE.txt'),'utf8')).includes('Node.js'));
  const executable = resolve(folder,runtime.executable);
  const bridge = new URL('../../dist/packages/native-bridge/index.js',import.meta.url).href;
  const script = `import {nativeRequest,nativeTransport,closeNativeRuntimes} from ${JSON.stringify(bridge)};const executable=${JSON.stringify(executable)};console.log(JSON.stringify({transport:nativeTransport(executable),response:nativeRequest({operation:'info'},executable)}));closeNativeRuntimes();`;
  const check = () => {
    const child = spawnSync(process.execPath,['--input-type=module','-e',script],{encoding:'utf8',timeout:30000,windowsHide:true,env:{...process.env,NODE_OPTIONS:'',RAISING_NATIVE_TRANSPORT:'auto'}});
    assert.equal(child.status,0,child.stderr); return JSON.parse(child.stdout);
  };
  assert.equal(check().transport.transport,'addon');
  addon.executable = 'raising-native-0000000000000000.exe';
  await writeFile(resolve(folder,'addon.json'),JSON.stringify(addon));
  const fallback=check(); assert.equal(fallback.transport.transport,'process');
  assert.match(fallback.transport.fallbackReason,/does not match/);
  assert.equal(fallback.response.implementation,'C++17');
});

test('addon checks JavaScript boundary and worker threads isolate protocol caches', async () => {
  const manifest = JSON.parse(await readFile('dist/native/addon.json', 'utf8'));
  const filename = resolve('dist/native', manifest.filename), addon = require(filename);
  for (const value of [undefined, null, 5, {}, []]) assert.throws(() => addon.request(value), /JSON string/);
  assert.equal(JSON.parse(addon.request('not JSON')).ok, false);
  const {snapshot} = await buildSnapshot(await loadProject('examples/academy-demo'));
  const workers = Array.from({ length:8 }, (_, index) => new Promise((done, reject) => {
    const worker = new Worker(`const {parentPort,workerData}=require('node:worker_threads');const a=require(workerData.filename);const content=workerData.snapshot;content.attributes.find(x=>x.id==='skill').initial=workerData.index;const request=x=>JSON.parse(a.request(JSON.stringify(x)));request({operation:'registerContent',contentId:'shared-worker-id',content});setTimeout(()=>parentPort.postMessage(request({operation:'create',contentId:'shared-worker-id',seed:0,options:{mode:'full'}})),100);`, { eval:true, workerData:{filename,index,snapshot} });
    worker.once('error', reject); worker.once('message', done);
  }));
  const results = await Promise.all(workers);
  results.forEach((result, index) => { assert.equal(result.ok,true); assert.equal(result.state.attributes.skill,index); });
});
