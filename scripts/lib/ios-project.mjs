import { spawn } from 'node:child_process';
import { createWriteStream } from 'node:fs';
import { cp, mkdir, readFile, writeFile, rename, lstat } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
const repository = fileURLToPath(new URL('../..', import.meta.url));
const applicationPattern = /^[A-Za-z][A-Za-z0-9-]*(?:\.[A-Za-z][A-Za-z0-9-]*)+$/;
export function runAppleTool(command, args, cwd, {logFile} = {}) {
  return new Promise((resolve, reject) => {
    const log = logFile ? createWriteStream(logFile) : null;
    log?.on('error', reject);
    const child = spawn(command, args, {cwd, env:{...process.env, PATH:[process.env.PATH,'/opt/homebrew/bin','/usr/local/bin'].filter(Boolean).join(path.delimiter)}, stdio:['ignore','pipe','pipe']});
    let output = '', errors = '';
    for (const stream of [child.stdout,child.stderr]) stream.on('data', data => {
      log?.write(data); const text=data.toString();output = (output + text).slice(-18000);
      errors=(errors+text.split('\n').filter(line=>/error:|CMake Error/.test(line)).join('\n')).slice(-8000);
    });
    child.on('error', error => {log?.end();reject(error);});
    child.on('close', code => {log?.end();code === 0 ? resolve(output) : reject(new Error(`${command} failed (${code})\n${errors}\n${output}${logFile?'\nFull log: '+logFile:''}`));});
  });
}
/** Called inside the exporter's owned staging directory. No source project is modified. */
export async function prepareIOSProject(directory, {root=repository, applicationId='org.raisingengine.firstgame'} = {}) {
  if (!applicationPattern.test(applicationId)) throw new Error('Use a reverse-domain iOS application ID, for example org.example.mygame.');
  const bundle = JSON.parse(await readFile(path.join(directory,'game.json'),'utf8'));
  if (bundle.formatVersion !== '1' || !bundle.content || !bundle.assets || bundle.extensions) throw new Error('iOS requires a validated, plugin-free game export.');
  await mkdir(path.join(directory,'game'));
  for (const entry of ['game.json','assets','fonts','licenses']) {
    try { await rename(path.join(directory,entry),path.join(directory,'game',entry)); }
    catch(e) { if(e.code !== 'ENOENT' || entry === 'game.json') throw e; }
  }
  const filter = async source => {
    if (['.git','.cache','dist','build','Xcode'].includes(path.basename(source))) return false;
    if ((await lstat(source)).isSymbolicLink()) throw new Error(`iOS source contains a symbolic link: ${source}`);
    return true;
  };
  for (const entry of ['CMakeLists.txt','include','src','vendor','player','mobile/ios']) {
    const target = path.join(directory,'engine/native',entry); await mkdir(path.dirname(target),{recursive:true});
    await cp(path.join(root,'native',entry),target,{recursive:true,filter});
  }
  await mkdir(path.join(directory,'game/licenses'),{recursive:true});
  await cp(path.join(root,'native/player/vendor/SDL-ios/LICENSE.txt'),path.join(directory,'game/licenses/SDL.txt'));
  await cp(path.join(root,'LICENSE'),path.join(directory,'ENGINE-LICENSE.txt'));
  await writeFile(path.join(directory,'CMakeLists.txt'), `cmake_minimum_required(VERSION 3.21.1)\nproject(RaisingGame LANGUAGES C CXX)\nset(RAISING_GAME_BUNDLE "\u0024{CMAKE_CURRENT_SOURCE_DIR}/game" CACHE PATH "Game resources")\nset(RAISING_APP_ID "${applicationId}" CACHE STRING "Application identifier")\nadd_subdirectory(engine/native/player native-player)\n`);
  const presets = ['iphonesimulator','iphoneos'].map(sdk => ({name:sdk,generator:'Xcode',binaryDir:'${sourceDir}/build-'+sdk,cacheVariables:{CMAKE_SYSTEM_NAME:'iOS',CMAKE_OSX_SYSROOT:sdk,CMAKE_OSX_ARCHITECTURES:sdk==='iphonesimulator'&&process.arch==='x64'?'x86_64':'arm64',CMAKE_OSX_DEPLOYMENT_TARGET:'16.0',CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED:'NO'}}));
  await writeFile(path.join(directory,'CMakePresets.json'),JSON.stringify({version:3,configurePresets:presets},null,2));
  await writeFile(path.join(directory,'Generate Xcode project.command'), '#!/bin/zsh\nset -eu\ncd -- "${0:A:h}"\nexport PATH="$PATH:/opt/homebrew/bin:/usr/local/bin"\ncmake --preset iphonesimulator\nopen build-iphonesimulator/RaisingGame.xcodeproj\n',{mode:0o755});
  await writeFile(path.join(directory,'IOS-README.txt'), 'Open build-iphonesimulator/RaisingGame.xcodeproj and choose raising-player plus an iPhone or iPad simulator. If you move this folder, double-click Generate Xcode project.command to regenerate it.\n\nDevice: configure the iphoneos preset, select your Apple development team, and enable code signing. A simulator or unsigned device build cannot be uploaded to the App Store. Icons, signing, provisioning, device testing and store review are release work. No credentials are stored in this project.\n\nThe game uses landscape orientation, touch input and application-private saves. Original engine and dependency licenses are included.\n');
  await writeFile(path.join(directory,'ios-build.json'),JSON.stringify({version:1,status:'source-prepared',applicationId,contentFingerprint:bundle.content.contentFingerprint,minimumIOS:'16.0'},null,2));
}
export async function configureIOSProject(directory, {sdk='iphonesimulator',team} = {}) {
  if (process.platform !== 'darwin') throw new Error('Generating an iOS Xcode project requires macOS and Xcode.');
  if (!['iphonesimulator','iphoneos'].includes(sdk)) throw new Error('iOS SDK must be iphonesimulator or iphoneos.');
  if (team && !/^[A-Z0-9]{10}$/.test(team)) throw new Error('Apple development team must contain 10 uppercase letters or digits.');
  const args = ['--preset',sdk];
  if (team) args.push('-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM='+team,'-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=YES');
  await runAppleTool(process.env.CMAKE ?? 'cmake',args,directory,{logFile:path.join(directory,'xcode-configure.log')});
  const file=path.join(directory,'ios-build.json'), metadata=JSON.parse(await readFile(file,'utf8'));
  await writeFile(file,JSON.stringify({...metadata,status:'xcode-project-generated',sdk,signed:!!team},null,2));
  return path.join(directory,'build-'+sdk,'RaisingGame.xcodeproj');
}
