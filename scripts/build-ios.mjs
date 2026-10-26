import { mkdir, cp, readFile, writeFile, lstat, realpath } from 'node:fs/promises';
import { existsSync } from 'node:fs';
import path from 'node:path';
import { prepareNativeDependencies } from './prepare-native-dependencies.mjs';
import { prepareIOSProject, configureIOSProject, runAppleTool, validateIOSVersions, verifyIOSBundleMetadata } from './lib/ios-project.mjs';
const args=process.argv.slice(2);
const option=(name,fallback)=>{const i=args.indexOf(name);if(i<0)return fallback;if(!args[i+1]||args[i+1].startsWith('--'))throw Error(`Missing ${name}`);return args[i+1];};
if(process.platform!=='darwin')throw Error('iOS builds require macOS with Xcode.');
const projectArg=option('--project');
const output=path.resolve(projectArg??option('--out','dist/ios-game'));
const sdk=option('--sdk','iphonesimulator'),team=option('--team');
if(projectArg && (args.includes('--version') || args.includes('--build-number')))throw Error('An existing iOS project owns its version metadata. Export a new project to change its release version or build number.');
if(!projectArg){
 const versions=validateIOSVersions({version:option('--version','0.1.0'),buildNumber:option('--build-number','1')});
 const input=option('--bundle');if(!input)throw Error('Use --bundle with an exported game, or --project with a previously exported iOS project.');
 if(existsSync(output))throw Error('Output already exists; choose a new --out directory.');
 let source=await realpath(input);if(existsSync(path.join(source,'RaisingPlayer.app/Contents/Resources/game.json')))source=path.join(source,'RaisingPlayer.app/Contents/Resources');
 const game=JSON.parse(await readFile(path.join(source,'game.json'),'utf8'));
 if(game.formatVersion!=='1'||!game.content||!game.assets||game.extensions)throw Error('Expected a validated plugin-free export.');
 for(const asset of Object.values(game.assets))if(typeof asset!=='string'||!/^assets\/[a-zA-Z0-9_.-]+$/.test(asset))throw Error('Invalid exported asset path.');
 await prepareNativeDependencies();
 const relative=path.relative(source,output);if(!relative||(!relative.startsWith('..'+path.sep)&&relative!=='..'&&!path.isAbsolute(relative)))throw Error('Output must be outside the source bundle.');
 await mkdir(output,{recursive:true});
 const filter=async file=>{if((await lstat(file)).isSymbolicLink())throw Error('Bundle symlinks are unsupported.');return true;};
 for(const file of ['game.json','fonts','licenses','branding','release-branding.json',...new Set(Object.values(game.assets))]){
  if(!existsSync(path.join(source,file))) { if(['licenses','branding','release-branding.json'].includes(file))continue; throw Error(`Missing game resource: ${file}`); }
  await mkdir(path.dirname(path.join(output,file)),{recursive:true});await cp(path.join(source,file),path.join(output,file),{recursive:true,filter});
 }
 await prepareIOSProject(output,{applicationId:option('--application-id','org.raisingengine.'+game.content.project.gameId),...versions});
}
const xcode=await configureIOSProject(output,{sdk,team});console.log(`Xcode project: ${xcode}`);
if(!args.includes('--prepare-only')){
 await runAppleTool(process.env.CMAKE??'cmake',['--build','build-'+sdk,'--config','Release','--target','raising-player','--parallel',process.env.BUILD_JOBS??'2'],output,{logFile:path.join(output,'xcode-build.log')});
 const verifiedMetadata=await verifyIOSBundleMetadata(output,{sdk});
 const file=path.join(output,'ios-build.json'),metadata=JSON.parse(await readFile(file,'utf8'));
 await writeFile(file,JSON.stringify({...metadata,verifiedMetadata,status:sdk==='iphonesimulator'?'simulator-built':'device-built-not-device-verified'},null,2));
 console.log(`iOS ${sdk} build completed: ${path.join(output,'build-'+sdk)}`);
}
