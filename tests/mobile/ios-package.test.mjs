import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,writeFile,readFile,readdir,symlink} from 'node:fs/promises';
import path from 'node:path';
import {prepareIOSProject} from '../../scripts/lib/ios-project.mjs';

async function fixture() {
  await mkdir('.cache/ios-package-tests',{recursive:true});
  const root=await mkdtemp(path.resolve('.cache/ios-package-tests/run-'));
  for(const file of ['LICENSE','native/CMakeLists.txt','native/include/core.hpp','native/src/core.cpp','native/vendor/json.hpp','native/player/main.cpp','native/player/vendor/SDL-ios/LICENSE.txt','native/mobile/ios/Info.plist.in']) {
    await mkdir(path.dirname(path.join(root,file)),{recursive:true});await writeFile(path.join(root,file),file);
  }
  const out=path.join(root,'export');await mkdir(path.join(out,'assets'),{recursive:true});await mkdir(path.join(out,'fonts'));
  await writeFile(path.join(out,'game.json'),JSON.stringify({formatVersion:'1',content:{contentFingerprint:'sha256:test'},assets:{}}));
  return {root,out};
}

test('iOS export is relocatable, preserves resources and licenses, and defaults to unsigned Xcode presets',async()=>{
  const {root,out}=await fixture();
  await prepareIOSProject(out,{root,applicationId:'org.example.garden'});
  assert.equal(JSON.parse(await readFile(path.join(out,'game/game.json'),'utf8')).content.contentFingerprint,'sha256:test');
  assert.equal(await readFile(path.join(out,'game/licenses/SDL.txt'),'utf8'),'native/player/vendor/SDL-ios/LICENSE.txt');
  const cmake=await readFile(path.join(out,'CMakeLists.txt'),'utf8');assert.ok(cmake.includes('${CMAKE_CURRENT_SOURCE_DIR}/game'));assert.ok(!cmake.includes(root));
  const presets=JSON.parse(await readFile(path.join(out,'CMakePresets.json'),'utf8')).configurePresets;
  assert.deepEqual(presets.map(p=>p.name),['iphonesimulator','iphoneos']);
  assert.ok(presets.every(p=>p.cacheVariables.CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED==='NO'));
  assert.equal(JSON.parse(await readFile(path.join(out,'ios-build.json'),'utf8')).applicationId,'org.example.garden');
});

test('iOS preparation rejects invalid identifiers and source symlinks',async()=>{
  const {root,out}=await fixture();const before=await readdir(out);
  await assert.rejects(prepareIOSProject(out,{root,applicationId:'../invalid'}),/application ID/);assert.deepEqual(await readdir(out),before);
  await symlink(path.join(root,'LICENSE'),path.join(root,'native/include/external'));
  await assert.rejects(prepareIOSProject(out,{root}),/symbolic link/);
});
