import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,writeFile,mkdir,symlink,readFile,access,cp} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {execFileSync} from 'node:child_process';
import {validateCocoaRepairManifest,prepareCocoaRepair,deployedCocoaRepair,verifyCocoaSourceKit,cocoaRepair,sha256} from '../../scripts/lib/qt-cocoa-repair.mjs';

const identity={sha256:'a'.repeat(64),uuid:'00000000-0000-0000-0000-000000000000'};
const valid=()=>({schemaVersion:1,qtVersion:'6.12.0',arch:'arm64',minimumMacOS:'14.4',patchCommit:cocoaRepair.commit,
  plugin:{file:'plugin.dylib',sha256:sha256(Buffer.from('plugin'))},sourceKit:{file:'source.tar.gz',sha256:sha256(Buffer.from('source'))},
  frameworks:{QtCore:{...identity},QtGui:{...identity},QtWidgets:{...identity}},
  nativeRegression:{passed:true,case:'tableViewSelectionAfterModelChange',logFile:'native.log',logSha256:'b'.repeat(64)}});

test('repair is opt-in and exact official Qt/ABI manifest is required',async()=>{
  assert.equal(await prepareCocoaRepair(undefined),undefined);
  assert.equal(validateCocoaRepairManifest(valid()).qtVersion,'6.12.0');
  for(const [key,value] of [['qtVersion','6.10.1'],['arch','x64'],['minimumMacOS','27.0'],['patchCommit','arbitrary']])assert.throws(()=>validateCocoaRepairManifest({...valid(),[key]:value}),/Qt Cocoa repair/);
  for(const value of [null,[],{},'not-json'])assert.throws(()=>validateCocoaRepairManifest(value));
  assert.throws(()=>validateCocoaRepairManifest({...valid(),frameworks:{QtCore:identity}}),/framework/);
  assert.throws(()=>validateCocoaRepairManifest({...valid(),nativeRegression:{passed:false}}),/regression/);
});

test('repair rejects empty, absolute, escaping and ambiguous file paths',()=>{
  for(const file of ['', '/tmp/plugin', '../plugin', 'nested/../plugin', 'nested\\plugin', 'nested//plugin', './plugin']) {
    const manifest=valid();manifest.plugin.file=file;assert.throws(()=>validateCocoaRepairManifest(manifest),/file identity/);
  }
});

test('malformed, missing and checksum-mismatched repair fails without output mutation',async()=>{
  const dir=await mkdtemp(path.join(tmpdir(),'qt-cocoa-repair-')),manifest=path.join(dir,'repair.json'),untouched=path.join(dir,'candidate.app');
  await mkdir(untouched);await writeFile(path.join(untouched,'sentinel'),'unchanged');
  const options={platform:'darwin',arch:'arm64',qt:'/no-kit-needed',libraryRoot:'/no-kit-needed'};
  await assert.rejects(prepareCocoaRepair(manifest,options),/ENOENT/);
  await writeFile(manifest,'{broken');await assert.rejects(prepareCocoaRepair(manifest,options),/Qt Cocoa repair/);
  await writeFile(manifest,JSON.stringify(valid()));await assert.rejects(prepareCocoaRepair(manifest,options),/ENOENT/);
  await writeFile(path.join(dir,'plugin.dylib'),'changed');await writeFile(path.join(dir,'source.tar.gz'),'source');
  await assert.rejects(prepareCocoaRepair(manifest,options),/checksum/);
  assert.equal(await readFile(path.join(untouched,'sentinel'),'utf8'),'unchanged');
  await assert.rejects(prepareCocoaRepair(manifest,{...options,arch:'x64'}),/macOS arm64/);
});

test('repair refuses symlinked manifest and plugin inputs',{skip:process.platform==='win32'?'macOS-only repair; Windows may lack symlink privilege':false},async()=>{
  const dir=await mkdtemp(path.join(tmpdir(),'qt-cocoa-links-')),manifest=path.join(dir,'repair.json');
  await writeFile(manifest,JSON.stringify(valid()));await symlink('repair.json',path.join(dir,'manifest-link.json'));
  const options={platform:'darwin',arch:'arm64',qt:'/unused',libraryRoot:'/unused'};
  await assert.rejects(prepareCocoaRepair(path.join(dir,'manifest-link.json'),options),/regular file/);
  await writeFile(path.join(dir,'actual'),'plugin');await symlink('actual',path.join(dir,'plugin.dylib'));
  await assert.rejects(prepareCocoaRepair(manifest,options),/regular file/);
});

test('source archive requires exact source/patch/license entries, never inferred notices',()=>{
  assert.throws(()=>verifyCocoaSourceKit('unused',()=>Buffer.from('../escape\n')),/unsafe/);
  assert.throws(()=>verifyCocoaSourceKit('unused',()=>Buffer.from('qt-cocoa-source-kit/README.md\n')),/source\/license mismatch/);
  const entry='qt-cocoa-source-kit/downloads/qtbase-everywhere-src-6.12.0.tar.xz';
  assert.throws(()=>verifyCocoaSourceKit('unused',(_tool,args)=>Buffer.from(args[0]==='-tzf'?entry+'\n':'modified source')),/source\/license mismatch/);
});

test('actual exact-kit repair preflight accepts built native regression and source archive',{skip:!process.env.RAISING_QT_COCOA_REPAIR_TEST_MANIFEST||process.platform!=='darwin'},async()=>{
  const qt=process.env.QT_ROOT??'/opt/homebrew/opt/qt';
  const repair=await prepareCocoaRepair(process.env.RAISING_QT_COCOA_REPAIR_TEST_MANIFEST,{qt,libraryRoot:path.join(qt,'lib')});
  assert.equal(repair.manifest.patchCommit,cocoaRepair.commit);await access(repair.sourceKit);
  const substituted=await mkdtemp(path.join(tmpdir(),'qt-cocoa-stock-substitution-'));
  await mkdir(path.join(substituted,'Contents/PlugIns/platforms'),{recursive:true});
  const plugins=execFileSync(path.join(qt,'bin/qtpaths'),['--query','QT_INSTALL_PLUGINS'],{encoding:'utf8'}).trim();
  await cp(path.join(plugins,'platforms/libqcocoa.dylib'),path.join(substituted,'Contents/PlugIns/platforms/libqcocoa.dylib'));
  await assert.rejects(deployedCocoaRepair(repair,substituted),/deployed plugin UUID mismatch/);
  const changed={...repair.manifest,frameworks:{...repair.manifest.frameworks,QtGui:{...repair.manifest.frameworks.QtGui,sha256:'0'.repeat(64)}}};
  const original=process.env.RAISING_QT_COCOA_REPAIR_TEST_MANIFEST,temp=path.join(path.dirname(original),'test-wrong-abi.json');
  await writeFile(temp,JSON.stringify(changed));
  await assert.rejects(prepareCocoaRepair(temp,{qt,libraryRoot:path.join(qt,'lib')}),/private ABI kit hash mismatch/);
});
