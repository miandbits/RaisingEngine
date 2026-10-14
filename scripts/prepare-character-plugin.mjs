// Install the real cutout-animation package into the character sample's plugin store.
//   node scripts/prepare-character-plugin.mjs [path/to/cutout-animation-x.y.z.raiseplugin]
// Without an argument the package is (re)built from dist/cutout by
// plugins/cutout-animation/tools/package.mjs, so first run
//   node plugins/cutout-animation/tools/build.mjs win wasm studio
// Store: $RAISING_PLUGIN_STORE or .cache/character-plugin-store (Edit-Magical-Character.cmd uses the same).
import {spawnSync} from 'node:child_process';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
const root=path.resolve(import.meta.dirname,'..');
const store=path.resolve(process.env.RAISING_PLUGIN_STORE??path.join(root,'.cache/character-plugin-store'));
let archive=process.argv[2]&&path.resolve(process.argv[2]);
if(!archive){
 const built=spawnSync(process.execPath,[path.join(root,'plugins/cutout-animation/tools/package.mjs')],{cwd:root,encoding:'utf8',stdio:['ignore','pipe','inherit']});
 if(built.status!==0)process.exit(built.status??1);
 archive=JSON.parse(built.stdout.trim().split('\n').at(-1)).package;
}
const {inspectPackage,installPackage,uninstallPlugin}=await import(pathToFileURL(path.join(root,'dist/packages/plugin-host/index.js')).href);
const {manifest}=await inspectPackage(archive);
let result;
try{result=await installPackage(store,archive);}
catch(error){
 // Installed versions are immutable; a rebuilt development package replaces its own pin explicitly.
 if(error.code!=='PLUGIN_VERSION_CONFLICT')throw error;
 await uninstallPlugin(store,manifest.id,manifest.version);result=await installPackage(store,archive);
}
console.log(JSON.stringify({store,package:archive,id:manifest.id,version:manifest.version,alreadyInstalled:result.alreadyInstalled,directory:result.plugin.directory}));
