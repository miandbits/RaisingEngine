import {writeIOSIconCatalog} from '../../dist/packages/export-service/branding.js';
import {validateReleaseIcon,releaseHash,RELEASE_LIMITS} from '../../dist/packages/export-service/release-metadata.js';
import { spawn } from 'node:child_process';
import { createWriteStream } from 'node:fs';
import { cp, mkdir, readFile, writeFile, rename, lstat } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
const repository = fileURLToPath(new URL('../..', import.meta.url));
const applicationPattern = /^[A-Za-z][A-Za-z0-9-]*(?:\.[A-Za-z][A-Za-z0-9-]*)+$/;
// The exported launcher and service use exactly the same relocation preflight.
// Preserve stale generated output; never delete prior builds or follow a build link.
const iosConfigureScript = `set -eu
export PATH="$PATH:/opt/homebrew/bin:/usr/local/bin"
cd -P -- .
sdk="\${1:-iphonesimulator}"
team="\${2:-}"
case "$sdk" in iphonesimulator|iphoneos) ;; *) print -u2 -- "Choose iphonesimulator or iphoneos."; exit 1 ;; esac
if [[ -n "$team" && ! "$team" =~ '^[A-Z0-9]{10}$' ]]; then
  print -u2 -- "Apple development team must contain 10 uppercase letters or digits."
  exit 1
fi
source_dir="$PWD"
build_dir="$source_dir/build-$sdk"
cache="$build_dir/CMakeCache.txt"
if [[ -L "$build_dir" || ( -e "$build_dir" && ! -d "$build_dir" ) ]]; then
  print -u2 -- "Xcode build folder must be a real directory, not a symbolic link or file: $build_dir"
  exit 1
fi
if [[ -L "$cache" || ( -e "$cache" && ! -f "$cache" ) ]]; then
  print -u2 -- "CMake cache must be a regular file, not a symbolic link: $cache"
  exit 1
fi
if [[ -f "$cache" ]]; then
  cached_source=$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$cache")
  cached_build=$(sed -n 's/^CMAKE_CACHEFILE_DIR:INTERNAL=//p' "$cache")
  if [[ -z "$cached_source" || -z "$cached_build" ]]; then
    print -u2 -- "Existing CMake cache has no source/build identity. Preserve this build folder elsewhere before regenerating: $build_dir"
    exit 1
  fi
  if [[ "$cached_source" != "$source_dir" || "$cached_build" != "$build_dir" ]]; then
    backup=$(mktemp -d "$source_dir/.raising-moved-$sdk.XXXXXX")
    mv -- "$build_dir" "$backup/build-$sdk"
    print -- "Preserved the previous location's entire build folder at: $backup/build-$sdk"
    print -- "The regenerated project uses the exported defaults. Reapply local signing or Xcode settings if needed."
  fi
fi
typeset -a configure_args
configure_args=(--preset "$sdk")
if [[ -n "$team" ]]; then
  configure_args+=("-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=$team" -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=YES)
fi
"\${CMAKE:-cmake}" "\${configure_args[@]}"
`;

/** Bounded numeric strings accepted by Apple's release/build version keys. */
export function validateIOSVersions({version='0.1.0', buildNumber='1'} = {}) {
  if (typeof version !== 'string' || !/^[0-9]{1,9}(?:\.[0-9]{1,9}){2}$/.test(version)) throw new Error('Use an iOS release version with three numbers, such as 1.2.0. Preview suffixes are not supported by iOS.');
  if (typeof buildNumber !== 'string' || !/^[0-9]{1,9}(?:\.[0-9]{1,9}){0,2}$/.test(buildNumber)) throw new Error('Use an iOS build number with one to three numbers, such as 1 or 1.2.3 (up to nine digits each).');
  return {version, buildNumber};
}

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
export async function prepareIOSProject(directory, {root=repository, applicationId='org.raisingengine.firstgame', version='0.1.0', buildNumber='1'} = {}) {
  if (!applicationPattern.test(applicationId)) throw new Error('Use a reverse-domain iOS application ID, for example org.example.mygame.');
  validateIOSVersions({version,buildNumber});
  const bundle = JSON.parse(await readFile(path.join(directory,'game.json'),'utf8'));
  if (bundle.formatVersion !== '1' || !bundle.content || !bundle.assets || bundle.extensions) throw new Error('iOS requires a validated, plugin-free game export.');
  const branding=await readFile(path.join(directory,'release-branding.json'),'utf8').then(JSON.parse).catch(e=>{if(e.code==='ENOENT')return null;throw e;});
  if(branding?.displayName&&(typeof branding.displayName!=='string'||branding.displayName.length>80||/[\u0000-\u001f\u007f]/.test(branding.displayName)))throw Error('Invalid application display name.');
  if(branding?.icon){const master=path.join(directory,'branding/app-icon.png');const info=await lstat(master);if(!info.isFile()||info.size>RELEASE_LIMITS.iconBytes)throw Error('Application icon must be a regular file up to 20 MiB.');const bytes=await readFile(master);validateReleaseIcon(bytes);if(releaseHash(bytes)!==branding.icon.sha256)throw Error('Packaged application icon changed.');await writeIOSIconCatalog(directory,bytes);}
  await mkdir(path.join(directory,'game'));
  for (const entry of ['game.json','assets','fonts','licenses','release-branding.json']) {
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
  // User text is never interpolated into CMake/Xcode source. A target post-build
  // command reads an owned UTF-8 file after ordinary plist expansion, before signing.
  if(branding?.displayName){
    await writeFile(path.join(directory,'Application-DisplayName.txt'),branding.displayName);
    await writeFile(path.join(directory,'ApplyReleaseName.cmake'),`string(REPLACE "\\\${EFFECTIVE_PLATFORM_NAME}" "$ENV{EFFECTIVE_PLATFORM_NAME}" APP_BUNDLE "\${APP_BUNDLE}")
if(NOT DEFINED APP_BUNDLE OR NOT EXISTS "\${APP_BUNDLE}/Info.plist")
  message(FATAL_ERROR "Built application Info.plist is missing")
endif()
file(READ "\${CMAKE_CURRENT_LIST_DIR}/Application-DisplayName.txt" app_name)
foreach(key CFBundleName CFBundleDisplayName)
  execute_process(COMMAND /usr/bin/plutil -replace "\${key}" -string "\${app_name}" "\${APP_BUNDLE}/Info.plist" COMMAND_ERROR_IS_FATAL ANY)
endforeach()
`);
  }
  const nameHook=branding?.displayName?'set(RAISING_IOS_RELEASE_NAME_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/ApplyReleaseName.cmake")\n':'';
  await writeFile(path.join(directory,'CMakeLists.txt'), `cmake_minimum_required(VERSION 3.21.1)\nproject(RaisingGame LANGUAGES C CXX)\nset(RAISING_GAME_BUNDLE "\u0024{CMAKE_CURRENT_SOURCE_DIR}/game" CACHE PATH "Game resources")\nset(RAISING_APP_ID "${applicationId}" CACHE STRING "Application identifier")\nset(RAISING_IOS_VERSION "${version}" CACHE STRING "iOS release version")\nset(RAISING_IOS_BUILD_NUMBER "${buildNumber}" CACHE STRING "iOS build number")\n${nameHook}add_subdirectory(engine/native/player native-player)\n`);
  if(branding?.icon){
    const extra='target_sources(raising-player PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/branding/AppIcon.xcassets")\nset_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/branding/AppIcon.xcassets" TARGET_DIRECTORY raising-player PROPERTIES MACOSX_PACKAGE_LOCATION Resources)\nset_target_properties(raising-player PROPERTIES XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon)\n';
    const cmake=path.join(directory,'CMakeLists.txt');await writeFile(cmake,await readFile(cmake,'utf8')+extra);
  }
  const presets = ['iphonesimulator','iphoneos'].map(sdk => ({name:sdk,generator:'Xcode',binaryDir:'${sourceDir}/build-'+sdk,cacheVariables:{CMAKE_SYSTEM_NAME:'iOS',CMAKE_OSX_SYSROOT:sdk,CMAKE_OSX_ARCHITECTURES:sdk==='iphonesimulator'&&process.arch==='x64'?'x86_64':'arm64',CMAKE_OSX_DEPLOYMENT_TARGET:'16.0',CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED:'NO'}}));
  await writeFile(path.join(directory,'CMakePresets.json'),JSON.stringify({version:3,configurePresets:presets},null,2));
  await writeFile(path.join(directory,'Generate Xcode project.command'), '#!/bin/zsh\nset -eu\ncd -- "${0:A:h}"\n'+iosConfigureScript+'open "build-$sdk/RaisingGame.xcodeproj"\n',{mode:0o755});
  await writeFile(path.join(directory,'IOS-README.txt'), 'Release version: '+version+'\nBuild number: '+buildNumber+'\nIncrease the build number for each new build you distribute. These values come from Export. To change these release values, export a new project with the new version and build number. Existing exports and their receipts remain unchanged.\n\nOpen build-iphonesimulator/RaisingGame.xcodeproj and choose raising-player plus an iPhone or iPad simulator. If you move this folder, double-click Generate Xcode project.command to regenerate it. A build cache from a different location is preserved intact inside a uniquely named .raising-moved-iphonesimulator folder before a fresh project is generated. Same-location builds remain incremental. Reapply any local signing or Xcode changes to the new project; the preserved project retains those old settings. Symbolic-link build folders are refused. For a moved device build, run this command with iphoneos as its first argument.\n\nDevice: configure the iphoneos preset, select your Apple development team, and enable code signing. A simulator or unsigned device build cannot be uploaded to the App Store. An explicitly imported icon is compiled from branding/AppIcon.xcassets. Signing, provisioning, device testing and store review remain release work. No credentials are stored in this project.\n\nGames with reading layouts support portrait and landscape, physical-size controls and text enlargement. Other games retain their authored orientation. The player supports touch input and application-private saves. Original engine and dependency licenses are included.\n');
  await writeFile(path.join(directory,'ios-build.json'),JSON.stringify({version:1,status:'source-prepared',applicationId,releaseVersion:version,buildNumber,contentFingerprint:bundle.content.contentFingerprint,minimumIOS:'16.0',...(branding?.displayName?{displayName:branding.displayName}:{}),appIcon:!!branding?.icon},null,2));
}
// Published releases keep their hashed source metadata immutable; standalone builds
// record their local configure status by default. Generated Xcode files remain local.
export async function configureIOSProject(directory, {sdk='iphonesimulator',team,recordStatus=true} = {}) {
  if (process.platform !== 'darwin') throw new Error('Generating an iOS Xcode project requires macOS and Xcode.');
  if (!['iphonesimulator','iphoneos'].includes(sdk)) throw new Error('iOS SDK must be iphonesimulator or iphoneos.');
  if (team && !/^[A-Z0-9]{10}$/.test(team)) throw new Error('Apple development team must contain 10 uppercase letters or digits.');
  await runAppleTool('/bin/zsh',['-c',iosConfigureScript,'raising-ios-configure',sdk,team??''],directory,{logFile:path.join(directory,'xcode-configure.log')});
  if (recordStatus) {
    const file=path.join(directory,'ios-build.json'), metadata=JSON.parse(await readFile(file,'utf8'));
    await writeFile(file,JSON.stringify({...metadata,status:'xcode-project-generated',sdk,signed:!!team},null,2));
  }
  return path.join(directory,'build-'+sdk,'RaisingGame.xcodeproj');
}


/** Inspect the built app, not merely the generated source or Xcode settings. */
export async function verifyIOSBundleMetadata(directory, {sdk='iphonesimulator'} = {}) {
  if(process.platform!=='darwin')throw new Error('Inspecting iOS app metadata requires macOS.');
  if(!['iphonesimulator','iphoneos'].includes(sdk))throw new Error('iOS SDK must be iphonesimulator or iphoneos.');
  const metadata=JSON.parse(await readFile(path.join(directory,'ios-build.json'),'utf8'));
  const expected={applicationId:metadata.applicationId,...validateIOSVersions({version:metadata.releaseVersion,buildNumber:metadata.buildNumber})};
  const plist=path.join(directory,'build-'+sdk,'native-player','Release-'+sdk,'RaisingPlayer.app','Info.plist');
  const observed={};
  for(const [field,key] of [['applicationId','CFBundleIdentifier'],['version','CFBundleShortVersionString'],['buildNumber','CFBundleVersion']]){
    observed[field]=(await runAppleTool('/usr/bin/plutil',['-extract',key,'raw','-o','-',plist],directory)).trim();
    if(observed[field]!==expected[field])throw new Error(`Built iOS ${key} differs from exported metadata: expected ${expected[field]}, found ${observed[field]}`);
  }
  if(metadata.displayName){observed.displayName=(await runAppleTool('/usr/bin/plutil',['-extract','CFBundleDisplayName','raw','-o','-',plist],directory)).replace(/\r?\n$/,'');if(observed.displayName!==metadata.displayName)throw Error('Built iOS display name differs from release settings.');}
  if(metadata.appIcon){const app=path.dirname(plist);if(!(await lstat(path.join(app,'Assets.car'))).isFile())throw Error('Built iOS icon asset catalog is missing.');const iconName=(await runAppleTool('/usr/bin/plutil',['-extract','CFBundleIcons.CFBundlePrimaryIcon.CFBundleIconName','raw','-o','-',plist],directory)).trim();if(iconName!=='AppIcon')throw Error('Built iOS primary icon is missing.');}
  const builtPlist=JSON.parse(await runAppleTool('/usr/bin/plutil',['-convert','json','-o','-',plist],directory));
  for(const [key,value] of Object.entries(builtPlist))if((key.startsWith('DT')||key==='BuildMachineOSBuild'||key==='MinimumOSVersion')&&typeof value==='string'&&/\$[({]/.test(value))throw Error(`Built iOS ${key} contains an unresolved build setting.`);
  return {sdk,plist,...(metadata.displayName?{displayName:observed.displayName}:{}),...(metadata.appIcon?{appIconVerified:true}:{}),applicationId:observed.applicationId,releaseVersion:observed.version,buildNumber:observed.buildNumber};
}
