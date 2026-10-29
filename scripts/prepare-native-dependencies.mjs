import {execFileSync} from 'node:child_process';
import {readFile,mkdir,access,readdir,realpath} from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {inspectIOSKeyboardPatch,applyIOSKeyboardPatch} from './lib/ios-sdl-keyboard-patch.mjs';

const repository=fileURLToPath(new URL('..',import.meta.url));
export async function prepareNativeDependencies(root=repository, {ios=process.platform==='darwin'}={}){
  const lock=JSON.parse(await readFile(path.join(root,'config/native-dependencies.json'),'utf8'));
  const vendor=path.resolve(root,'native/player/vendor');await mkdir(vendor,{recursive:true});
  const git=(args,options={})=>execFileSync('git',args,{encoding:'utf8',windowsHide:true,...options});
  for(const dependency of [...lock.dependencies,...(ios?lock.iosDependencies??[]:[])]){
    const target=path.resolve(vendor,dependency.path);
    if(!target.startsWith(vendor+path.sep)||!/^[a-f0-9]{40}$/.test(dependency.commit))throw Error('Invalid native dependency lock');
    // Never reset an existing dependency checkout or overwrite local source changes.
    let exists=true;try{await access(path.join(target,'.git'));}catch{exists=false;}
    if(!exists){
      const entries=await readdir(target).catch(e=>{if(e.code==='ENOENT')return [];throw e;});
      if(entries.length)throw Error(`Dependency directory is not empty: ${target}`);
      await mkdir(path.dirname(target),{recursive:true});
      const parent=await realpath(path.dirname(target));
      if(parent!==vendor&&!parent.startsWith((await realpath(vendor))+path.sep))throw Error('Dependency path escapes vendor directory');
      git(['clone','--depth','1','--branch',dependency.ref,'--single-branch',dependency.url,target],{stdio:'inherit'});
    }
    const head=git(['-C',target,'rev-parse','HEAD']).trim();
    if(head!==dependency.commit)throw Error(`Dependency version mismatch: ${dependency.path}. Expected ${dependency.commit}, found ${head}; existing files were preserved.`);
    const dirty=git(['-C',target,'status','--porcelain','--untracked-files=no','--ignore-submodules=all']).trim();
    const keyboardPatch=dependency.path==='SDL-ios'?await inspectIOSKeyboardPatch(target):null;
    const reviewedPatch=keyboardPatch&&!git(['-C',target,'diff','HEAD','--summary']).trim()&&dirty.split('\n').every(line=>keyboardPatch.files.some(file=>file.patched&&line.trim()===`M ${file.source}`));
    if(dirty&&!reviewedPatch)throw Error(`Dependency has local changes: ${dependency.path}; existing files were preserved.`);
    if(keyboardPatch)console.log(`SDL-ios keyboard isolation patch: ${JSON.stringify(await applyIOSKeyboardPatch(target))}`);
    console.log(`${dependency.path}: ${head}`);
  }
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url))await prepareNativeDependencies();
