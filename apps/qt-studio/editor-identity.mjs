import {readFile,lstat,realpath} from 'node:fs/promises';
import {execFile} from 'node:child_process';
import {promisify} from 'node:util';
import path from 'node:path';

const execute=promisify(execFile),packagedNames=new Set(['raising-studio-service','raising-studio-services']);
const host=()=>({platform:process.platform,arch:process.arch,nodeVersion:process.version});
const unknownSource=()=>({commit:null,tree:null,dirty:null,origin:'unavailable',context:'service-and-packaging-source',binaryVerified:false});
const hash=value=>value===null||typeof value==='string'&&/^[a-f0-9]{40}$/.test(value);
async function json(file){const s=await lstat(file);if(!s.isFile()||s.size>65536)throw Error('Invalid editor metadata file.');return JSON.parse(await readFile(file,'utf8'));}
async function optionalJson(file){try{return await json(file);}catch(error){if(error.code==='ENOENT')return null;throw error;}}
const version=value=>typeof value==='string'&&/^\d+\.\d+\.\d+(?:[-+][a-zA-Z0-9.-]+)?$/.test(value)?value:null;
function preview(config){return config&&version(config.version)&&config.channel==='unsigned-preview'&&config.platform==='windows'&&config.architecture==='x64'?{version:config.version,channel:config.channel,platform:config.platform,architecture:config.architecture}:null;}
const launch=platform=>platform==='darwin'?{kind:'application',entryPoint:'Contents/MacOS/RaisingStudioQt',command:null}:{kind:'application',entryPoint:platform==='win32'?'RaisingStudioQt.exe':'RaisingStudioQt',command:null};

/** The launcher and support information use the same current Qt entry point. */
export function sourceEditorLaunch(engineRoot,{platform=process.platform,output='dist/qt-studio',args=[]}={}){
  const paths=platform==='win32'?path.win32:path.posix,root=paths.resolve(engineRoot),directory=paths.resolve(root,output);
  return {executable:paths.join(directory,platform==='darwin'?'RaisingStudioQt.app/Contents/MacOS/RaisingStudioQt':platform==='win32'?'RaisingStudioQt.exe':'RaisingStudioQt'),args:['--engine-root',root,...args],cwd:root};
}

function storedIdentity(value){
  // Check only our small provenance contract. Do not mistake arbitrary project
  // metadata or an enclosing repository's Git HEAD for the installed editor.
  if(value?.schemaVersion!==1||value.product!=='Raising Studio'||!version(value.engineVersion)||value.distribution?.kind!=='package'
    ||!['development','unsigned-preview'].includes(value.distribution.channel)
    ||!(value.distribution.releaseVersion===null||version(value.distribution.releaseVersion))
    ||!value.source||!hash(value.source.commit)||!hash(value.source.tree)||![true,false,null].includes(value.source.dirty)
    ||value.source.origin!=='package'||value.source.binaryVerified!==false||!Array.isArray(value.warnings)||value.warnings.some(x=>typeof x!=='string')
    ||!value.launch||typeof value.launch.kind!=='string')throw Error('Invalid packaged editor identity.');
  return value;
}

/** Read current services/packaging provenance, without claiming a binary rebuild. */
export async function readEditorIdentity(engineRoot){
  const root=path.resolve(engineRoot),warnings=[];
  let pkg=null;try{pkg=await optionalJson(path.join(root,'package.json'));}catch{warnings.push('Editor package metadata could not be read.');}
  const packaged=packagedNames.has(pkg?.name);
  if(packaged){
    try{
      const value=storedIdentity(await json(path.join(root,'editor-identity.json')));
      if(value.engineVersion!==version(pkg.version))throw Error('Editor identity and service package versions differ.');
      return {...value,host:host()};
    }catch{warnings.push('This installed editor has no valid source identity receipt. Its source revision is unknown.');}
  }
  const identity={schemaVersion:1,product:'Raising Studio',engineVersion:version(pkg?.version),
    distribution:{kind:packaged?'package':pkg?.name==='raising-engine'?'source':'unknown',releaseVersion:null,channel:pkg?.name==='raising-engine'?'source':'unknown'},
    source:unknownSource(),nativeCore:{coreVersion:null,sourceRevision:null},host:host(),publicPreview:null,
    launch:packaged?launch(process.platform):pkg?.name==='raising-engine'?{kind:'source',entryPoint:'scripts/start-qt-studio.mjs',command:'npm run studio',outputVariable:'QT_STUDIO_OUT'}:{kind:'unknown',entryPoint:null,command:null},
    support:{stage:'authoring-alpha',guide:'docs/development/platform-support.md',summary:'Runtime capability checks and automated tests do not certify physical devices, accessibility, signing, stores or asset rights.'},warnings};
  try{identity.publicPreview=preview(await optionalJson(path.join(root,'config/preview-release.json')));}catch{warnings.push('Published preview policy could not be read.');}
  try{
    const core=await optionalJson(path.join(root,'dist/native/runtime.json'));
    if(core?.runtime==='raising-native')identity.nativeCore={coreVersion:typeof core.coreVersion==='string'?core.coreVersion:null,sourceRevision:typeof core.sourceRevision==='string'?core.sourceRevision:null};
  }catch{warnings.push('Native core metadata could not be read.');}
  if(identity.distribution.kind==='source'){
    try{
      await lstat(path.join(root,'.git'));
      const git=async args=>(await execute('git',['-C',root,...args],{encoding:'utf8',timeout:2500,maxBuffer:1024*1024,windowsHide:true})).stdout.trim();
      const top=await git(['rev-parse','--show-toplevel']);if(await realpath(top)!==await realpath(root))throw Error('Not the editor repository root.');
      const [commit,tree,status]=await Promise.all([git(['rev-parse','HEAD']),git(['rev-parse','HEAD^{tree}']),git(['status','--porcelain','--untracked-files=normal'])]);
      if(!hash(commit)||!hash(tree)||!commit||!tree)throw Error('Invalid source revision.');
      identity.source={...unknownSource(),commit,tree,dirty:!!status,origin:'git'};
    }catch{warnings.push('Source revision is unavailable; a source archive or missing Git is not a verified commit.');}
  }
  warnings.push('Source metadata does not prove that separately built editor or player binaries match this commit.');
  return identity;
}

/** Embed this before package inventory/signing; relocations keep the same source. */
export async function createPackagedEditorIdentity(engineRoot,{platform=process.platform,arch=process.arch,releaseVersion=null,channel='development'}={}){
  const identity=await readEditorIdentity(engineRoot);
  if(!identity.engineVersion)throw Error('The editor package version is required before packaging.');
  if(!['win32','darwin','linux'].includes(platform)||!['development','unsigned-preview'].includes(channel)||!(releaseVersion===null||version(releaseVersion)))throw Error('Invalid editor distribution identity.');
  if(channel==='unsigned-preview'&&platform!=='win32')throw Error('The unsigned-preview policy is scoped to Windows.');
  return {...identity,distribution:{kind:'package',platform,arch,releaseVersion,channel},source:{...identity.source,origin:'package'},buildHost:identity.host,launch:launch(platform)};
}
