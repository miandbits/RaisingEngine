import test from 'node:test';
import assert from 'node:assert/strict';
import {execFileSync,spawnSync} from 'node:child_process';
import {mkdtemp,mkdir,writeFile,readFile,chmod,symlink,unlink} from 'node:fs/promises';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {prepareNativeDependencies} from '../../scripts/prepare-native-dependencies.mjs';
import {inspectIOSKeyboardPatch,applyIOSKeyboardPatch,iosKeyboardSource,iosKeyboardOriginalHash,iosKeyboardPatchedHash,iosSceneSource,iosSceneOriginalHash,iosSceneLegacyPatchedHash,iosScenePatchedHash} from '../../scripts/lib/ios-sdl-keyboard-patch.mjs';
const digest=bytes=>createHash('sha256').update(bytes).digest('hex');
const git=(directory,args)=>execFileSync('git',['-C',directory,...args],{encoding:'utf8',windowsHide:true});

const modernBoundsGuard='    // Backport SDL 2ee7eb62120b6cd2ceb6ecc24052fc994a4cbd9a: modern\n    // UIWindow bounds already follow the scene, even before orientation is known.\n    if (UIKit_IsSystemVersionAtLeast(13.0)) {\n        return frame;\n    }\n\n';
async function executeProductionFrame(source,root,label){
 const signature='CGRect UIKit_ComputeViewFrame(SDL_Window *window, UIScreen *screen)';
 assert.equal(source.split(signature).length,2,'Extract exactly one actual production function');
 const start=source.indexOf(signature),end=source.indexOf('\n}\n#endif',start)+2;
 assert.ok(end>start);
 const production=source.slice(start,end);
 const environment=await readFile(new URL('./fixtures/ios-sdl-view-frame.mm',import.meta.url),'utf8');
 const file=path.join(root,label+'.mm'),binary=path.join(root,label);
 await writeFile(file,environment.replace('// PRODUCTION_FUNCTION',production));
 execFileSync('xcrun',['clang++','-std=c++17','-fobjc-arc','-framework','Foundation','-framework','CoreGraphics',file,'-o',binary],{timeout:30000});
 const result=spawnSync(binary,[],{encoding:'utf8',timeout:10000});
 await writeFile(path.join(root,label+'.log'),(result.stdout??'')+(result.stderr??''));
 assert.ifError(result.error);assert.equal(result.signal,null);
 return result;
}

// The actual pinned UIKit file is prepared by the macOS native dependency lane.
// This tests byte ownership/idempotence, not a substitute for genuine UIKit UI tests.
test('pinned iOS corrections preserve modern bounds and safely migrate reviewed caches',{skip:process.platform!=='darwin'},async()=>{
 const vendor=path.resolve('native/player/vendor/SDL-ios'),original=git(vendor,['show','HEAD:'+iosKeyboardSource]);
 assert.equal(digest(original),iosKeyboardOriginalHash);const sceneOriginal=git(vendor,['show','HEAD:'+iosSceneSource]);assert.equal(digest(sceneOriginal),iosSceneOriginalHash);
 await mkdir('.cache/ios-keyboard-patch',{recursive:true});const root=await mkdtemp(path.resolve('.cache/ios-keyboard-patch/run-')),target=path.join(root,'native/player/vendor/SDL-ios'),file=path.join(target,iosKeyboardSource);
 await mkdir(path.dirname(file),{recursive:true});await writeFile(file,original);const sceneFile=path.join(target,iosSceneSource);await writeFile(sceneFile,sceneOriginal);await writeFile(path.join(target,'other.txt'),'preserve other dependency files\n');
 git(target,['init','-q']);git(target,['add','.']);git(target,['-c','user.name=Dependency test','-c','user.email=dependency-test@example.invalid','commit','-qm','Pinned fixture']);const commit=git(target,['rev-parse','HEAD']).trim();
 await mkdir(path.join(root,'config'));await writeFile(path.join(root,'config/native-dependencies.json'),JSON.stringify({version:1,dependencies:[],iosDependencies:[{path:'SDL-ios',commit}]}));
 await writeFile(sceneFile,sceneOriginal+'\n// Unknown local scene edit\n');await assert.rejects(prepareNativeDependencies(root,{ios:true}),/differs.*preserved/);assert.equal(await readFile(file,'utf8'),original,'all inputs are verified before either patch is written');await writeFile(sceneFile,sceneOriginal);
 await prepareNativeDependencies(root,{ios:true});const scenePatched=await readFile(sceneFile);assert.equal(digest(scenePatched),iosScenePatchedHash);const patched=await readFile(file);assert.equal(digest(patched),iosKeyboardPatchedHash);
 await prepareNativeDependencies(root,{ios:true});assert.deepEqual(await readFile(sceneFile),scenePatched);assert.deepEqual(await readFile(file),patched,'normal repeated preparation accepts only reviewed patch bytes');
 // Execute the complete function read from the actual prepared dependency.
 const current=await executeProductionFrame(scenePatched.toString('utf8'),root,'modern-bounds');
 assert.equal(current.status,0,current.stdout+current.stderr);
 // The producer supplies the exact exported dependency it just compiled. Do not
 // silently substitute the fresh fixture when that build-source check is requested.
 if(process.env.RAISING_IOS_BUILT_SDL_SCENE_SOURCE!==undefined){
  const builtPath=process.env.RAISING_IOS_BUILT_SDL_SCENE_SOURCE;
  assert.ok(builtPath && path.isAbsolute(builtPath),'Built SDL scene source must be an explicit absolute path');
  const builtSource=await readFile(builtPath);
  assert.equal(digest(builtSource),iosScenePatchedHash,'Actual built SDL scene source must have the reviewed modern-bounds patch');
  assert.deepEqual(builtSource,scenePatched,'Actual built source must equal the fresh prepared source');
  const built=await executeProductionFrame(builtSource.toString('utf8'),root,'built-modern-bounds');
  assert.equal(built.status,0,built.stdout+built.stderr);
  console.log('Built SDL scene regression: '+JSON.stringify({source:builtPath,sha256:digest(builtSource),result:'passed',log:path.join(root,'built-modern-bounds.log')}));
 }
 const legacyScene=scenePatched.toString('utf8').replace(modernBoundsGuard,'');
 assert.equal(digest(legacyScene),iosSceneLegacyPatchedHash,'Negative control is the exact previously shipped prepared source');
 const legacy=await executeProductionFrame(legacyScene,root,'legacy-bounds');
 assert.equal(legacy.status,1,'The previous production transform must fail the modern-bounds contract');
 assert.match(legacy.stdout,/FAIL modern unknown full landscape actual=0,0,390,844 expected=0,0,844,390/);
 // A fully reviewed prior cache must migrate through the normal dirty allowlist.
 await writeFile(sceneFile,legacyScene);await prepareNativeDependencies(root,{ios:true});
 assert.deepEqual(await readFile(sceneFile),scenePatched);assert.deepEqual(await readFile(file),patched);
 await prepareNativeDependencies(root,{ios:true});assert.deepEqual(await readFile(sceneFile),scenePatched);
 // Every recognized partial preparation state safely converges after interruption.
 for(const [keyboard,scene] of [[original,legacyScene],[original,scenePatched],[patched,sceneOriginal]]){
  await writeFile(file,keyboard);await writeFile(sceneFile,scene);
  await prepareNativeDependencies(root,{ios:true});
  assert.deepEqual(await readFile(file),patched);assert.deepEqual(await readFile(sceneFile),scenePatched);
  await prepareNativeDependencies(root,{ios:true});
  assert.deepEqual(await readFile(file),patched);assert.deepEqual(await readFile(sceneFile),scenePatched);
 }
 await writeFile(file,patched);await writeFile(sceneFile,scenePatched);
 // The production post-dirty-check inspection must detect intervening changes.
 await writeFile(file,original);await writeFile(sceneFile,sceneOriginal);
 let validated=await inspectIOSKeyboardPatch(target);
 await chmod(sceneFile,0o755);
 await assert.rejects(applyIOSKeyboardPatch(target,validated),/changed after validation.*preserved/);
 assert.equal(await readFile(file,'utf8'),original);assert.equal(await readFile(sceneFile,'utf8'),sceneOriginal);
 await chmod(sceneFile,0o644);validated=await inspectIOSKeyboardPatch(target);
 await writeFile(sceneFile,sceneOriginal+'\n// intervening edit\n');
 await assert.rejects(applyIOSKeyboardPatch(target,validated),/differs.*preserved/);
 assert.equal(await readFile(file,'utf8'),original);
 await writeFile(file,patched);await writeFile(sceneFile,scenePatched);
 // Do not follow dependency-source symlinks, even to recognized bytes.
 const preserved=path.join(root,'preserved-scene.m');await writeFile(preserved,scenePatched);
 await unlink(sceneFile);await symlink(preserved,sceneFile);
 await assert.rejects(prepareNativeDependencies(root,{ios:true}),/ordinary file.*preserved/);
 assert.deepEqual(await readFile(preserved),scenePatched);assert.deepEqual(await readFile(file),patched);
 await unlink(sceneFile);await writeFile(sceneFile,scenePatched);

 const changed=Buffer.concat([patched,Buffer.from('\n// Designer local dependency experiment\n')]);await writeFile(file,changed);
 await assert.rejects(prepareNativeDependencies(root,{ios:true}),/differs.*preserved/);assert.deepEqual(await readFile(file),changed,'unknown local source is not replaced');
 await writeFile(file,patched);await writeFile(path.join(target,'other.txt'),'unrelated local change\n');
 await assert.rejects(prepareNativeDependencies(root,{ios:true}),/local changes.*preserved/);assert.equal(await readFile(path.join(target,'other.txt'),'utf8'),'unrelated local change\n');assert.deepEqual(await readFile(file),patched);
 await writeFile(path.join(target,'other.txt'),'preserve other dependency files\n');await prepareNativeDependencies(root,{ios:true});
 await chmod(file,0o755);git(target,['add',iosKeyboardSource]);assert.match(git(target,['diff','HEAD','--summary']),/mode change 100644 => 100755/);
 await assert.rejects(prepareNativeDependencies(root,{ios:true}),/local changes.*preserved/);assert.deepEqual(await readFile(file),patched,'staged permission changes are preserved rather than silently accepted');
 console.log('iOS keyboard dependency-patch evidence: '+root);
});
