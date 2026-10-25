import {createHash} from 'node:crypto';
import {lstat,readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';

export const iosKeyboardSource='src/video/uikit/SDL_uikitviewcontroller.m';
export const iosKeyboardOriginalHash='9171a88ccbb72f5f041d38c86ded597f461a9999f324167c16bcaf1a227b5bb0';
export const iosKeyboardPatchedHash='beb639555d9f8a7aaca50039274f80dbf27238e9ad14244cb2143dfc71c98e90';
export const iosSceneSource='src/video/uikit/SDL_uikitvideo.m';
export const iosSceneOriginalHash='1d11a9118b5a01af13593e96e49c32cd1fc1d92f93ec4915cb3fb0b3ed24a26d';
export const iosScenePatchedHash='9e172e9e6e665fe0718dfc12a046b43642a33b3dfef69072ce9b8a40b071d29d';
const hash=bytes=>createHash('sha256').update(bytes).digest('hex');

/** SDL 3.4.18 observes global UIKit keyboard notifications. During a Files
 * picker rotation, an unrelated hide writes the SDL view's portrait frame into
 * a still-landscape parent; UIKit then applies the rotation delta twice.
 * Keep normal SDL text input, including its final hide after resigning, intact.
 * Exact hashes make this idempotent and preserve all unknown local changes.
 */
async function inspectKeyboardSource(target){
 const file=path.join(target,iosKeyboardSource),info=await lstat(file);
 if(!info.isFile()||info.isSymbolicLink())throw Error('Pinned iOS SDL keyboard source must be an ordinary file; existing files were preserved.');
 const bytes=await readFile(file),digest=hash(bytes);
 if(digest===iosKeyboardPatchedHash)return {file,source:iosKeyboardSource,patched:true,bytes};
 if(digest!==iosKeyboardOriginalHash)throw Error('Pinned iOS SDL keyboard source differs from the reviewed original or patch; existing files were preserved.');
 const text=bytes.toString('utf8').replace(
  '- (void)keyboardWillShow:(NSNotification *)notification\n{\n#ifndef SDL_PLATFORM_TVOS\n',
  '- (void)keyboardWillShow:(NSNotification *)notification\n{\n#ifndef SDL_PLATFORM_TVOS\n    // RaisingEngine: another UIKit controller (for example Files) owns its\n    // keyboard. Do not resize the SDL view in that controller\'s transition.\n    if (!textFieldFocused) {\n        return;\n    }\n\n'
 ).replace(
  '- (void)keyboardWillHide:(NSNotification *)notification\n{\n    [self setKeyboardHeight:0];',
  '- (void)keyboardWillHide:(NSNotification *)notification\n{\n    // A hide after SDL resigns must still clear its own previous offset.\n    if (!textFieldFocused && keyboardHeight == 0) {\n        return;\n    }\n    [self setKeyboardHeight:0];'
 );
 if(hash(text)!==iosKeyboardPatchedHash)throw Error('Pinned iOS SDL keyboard patch did not match the reviewed source.');
 return {file,source:iosKeyboardSource,patched:false,bytes:Buffer.from(text)};
}
async function inspectSceneSource(target){
 const file=path.join(target,iosSceneSource),info=await lstat(file);
 if(!info.isFile()||info.isSymbolicLink())throw Error('Pinned iOS SDL scene source must be an ordinary file; existing files were preserved.');
 const bytes=await readFile(file),digest=hash(bytes);
 if(digest===iosScenePatchedHash)return {file,source:iosSceneSource,patched:true,bytes};
 if(digest!==iosSceneOriginalHash)throw Error('Pinned iOS SDL scene source differs from the reviewed original or patch; existing files were preserved.');
 const text=bytes.toString('utf8').replace(
  '    UIInterfaceOrientation orient = [UIApplication sharedApplication].statusBarOrientation;',
  "    // RaisingEngine: scene-based apps can report Unknown from the deprecated\n    // application orientation. Use this SDL window's scene for keyboard layout.\n    UIWindowScene *scene = data.uiwindow.windowScene;\n    UIInterfaceOrientation orient = scene ? scene.interfaceOrientation : [UIApplication sharedApplication].statusBarOrientation;"
 );
 if(hash(text)!==iosScenePatchedHash)throw Error('Pinned iOS SDL scene patch did not match the reviewed source.');
 return {file,source:iosSceneSource,patched:false,bytes:Buffer.from(text)};
}
export async function inspectIOSKeyboardPatch(target){
 // Validate both files before writing either. An unknown local change is never
 // partially overwritten while preparing the other half of the correction.
 return {files:await Promise.all([inspectKeyboardSource(target),inspectSceneSource(target)])};
}
export async function applyIOSKeyboardPatch(target){
 const patch=await inspectIOSKeyboardPatch(target);
 for(const file of patch.files)if(!file.patched)await writeFile(file.file,file.bytes);
 return {keyboard:iosKeyboardPatchedHash,scene:iosScenePatchedHash};
}
