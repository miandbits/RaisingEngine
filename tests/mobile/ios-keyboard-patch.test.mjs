import test from 'node:test';
import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {mkdtemp,mkdir,writeFile,readFile,chmod} from 'node:fs/promises';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {prepareNativeDependencies} from '../../scripts/prepare-native-dependencies.mjs';
import {iosKeyboardSource,iosKeyboardOriginalHash,iosKeyboardPatchedHash,iosSceneSource,iosSceneOriginalHash,iosScenePatchedHash} from '../../scripts/lib/ios-sdl-keyboard-patch.mjs';
const digest=bytes=>createHash('sha256').update(bytes).digest('hex');
const git=(directory,args)=>execFileSync('git',['-C',directory,...args],{encoding:'utf8',windowsHide:true});

// The actual pinned UIKit file is prepared by the macOS native dependency lane.
// This tests byte ownership/idempotence, not a substitute for genuine UIKit UI tests.
test('pinned iOS keyboard correction is reproducible and preserves unknown dependency edits',{skip:process.platform!=='darwin'},async()=>{
 const vendor=path.resolve('native/player/vendor/SDL-ios'),original=git(vendor,['show','HEAD:'+iosKeyboardSource]);
 assert.equal(digest(original),iosKeyboardOriginalHash);const sceneOriginal=git(vendor,['show','HEAD:'+iosSceneSource]);assert.equal(digest(sceneOriginal),iosSceneOriginalHash);
 await mkdir('.cache/ios-keyboard-patch',{recursive:true});const root=await mkdtemp(path.resolve('.cache/ios-keyboard-patch/run-')),target=path.join(root,'native/player/vendor/SDL-ios'),file=path.join(target,iosKeyboardSource);
 await mkdir(path.dirname(file),{recursive:true});await writeFile(file,original);const sceneFile=path.join(target,iosSceneSource);await writeFile(sceneFile,sceneOriginal);await writeFile(path.join(target,'other.txt'),'preserve other dependency files\n');
 git(target,['init','-q']);git(target,['add','.']);git(target,['-c','user.name=Dependency test','-c','user.email=dependency-test@example.invalid','commit','-qm','Pinned fixture']);const commit=git(target,['rev-parse','HEAD']).trim();
 await mkdir(path.join(root,'config'));await writeFile(path.join(root,'config/native-dependencies.json'),JSON.stringify({version:1,dependencies:[],iosDependencies:[{path:'SDL-ios',commit}]}));
 await writeFile(sceneFile,sceneOriginal+'\n// Unknown local scene edit\n');await assert.rejects(prepareNativeDependencies(root,{ios:true}),/differs.*preserved/);assert.equal(await readFile(file,'utf8'),original,'all inputs are verified before either patch is written');await writeFile(sceneFile,sceneOriginal);
 await prepareNativeDependencies(root,{ios:true});const scenePatched=await readFile(sceneFile);assert.equal(digest(scenePatched),iosScenePatchedHash);const patched=await readFile(file);assert.equal(digest(patched),iosKeyboardPatchedHash);
 await prepareNativeDependencies(root,{ios:true});assert.deepEqual(await readFile(sceneFile),scenePatched);assert.deepEqual(await readFile(file),patched,'normal repeated preparation accepts only reviewed patch bytes');
 const changed=Buffer.concat([patched,Buffer.from('\n// Designer local dependency experiment\n')]);await writeFile(file,changed);
 await assert.rejects(prepareNativeDependencies(root,{ios:true}),/differs.*preserved/);assert.deepEqual(await readFile(file),changed,'unknown local source is not replaced');
 await writeFile(file,patched);await writeFile(path.join(target,'other.txt'),'unrelated local change\n');
 await assert.rejects(prepareNativeDependencies(root,{ios:true}),/local changes.*preserved/);assert.equal(await readFile(path.join(target,'other.txt'),'utf8'),'unrelated local change\n');assert.deepEqual(await readFile(file),patched);
 await writeFile(path.join(target,'other.txt'),'preserve other dependency files\n');await prepareNativeDependencies(root,{ios:true});
 await chmod(file,0o755);git(target,['add',iosKeyboardSource]);assert.match(git(target,['diff','HEAD','--summary']),/mode change 100644 => 100755/);
 await assert.rejects(prepareNativeDependencies(root,{ios:true}),/local changes.*preserved/);assert.deepEqual(await readFile(file),patched,'staged permission changes are preserved rather than silently accepted');
 console.log('iOS keyboard dependency-patch evidence: '+root);
});
