import {createHash} from 'node:crypto';
import {mkdir,readFile,writeFile,copyFile} from 'node:fs/promises';
import path from 'node:path';
const root=path.resolve(import.meta.dirname,'..'),source=path.resolve(process.argv[2]??path.join(root,'../RaisingEngine-anim'));
const store=path.join(root,'.cache/character-plugin-store/installed/cutout-animation/1.0.0');
await mkdir(path.join(store,'runtime/windows'),{recursive:true});await mkdir(path.join(store,'schema'),{recursive:true});
const files={};
for(const [from,to]of [[path.join(root,'.cache/cutout-runtime-build/cutout-runtime.dll'),'runtime/windows/cutout-runtime.dll'],...['rig','controller'].map(t=>[path.join(source,`plugins/cutout-animation/schema/${t}.schema.json`),`schema/${t}.schema.json`])]){
 const bytes=await readFile(from);await writeFile(path.join(store,to),bytes);files[to]=createHash('sha256').update(bytes).digest('hex');
}
await writeFile(path.join(store,'plugin.json'),JSON.stringify({manifestVersion:1,id:'cutout-animation',name:'Cutout animation',version:'1.0.0',engineApi:{min:1,max:1},documentTypes:['rig','controller'].map(type=>({type,label:type,schema:`schema/${type}.schema.json`})),elementTypes:[{type:'character',label:'Character',refType:'controller'}],modules:[{role:'runtime',platform:'windows-x64',abi:'raising-extension-1',path:'runtime/windows/cutout-runtime.dll'}],exportTargets:['windows'],capabilities:['cutout-animation-v1'],files},null,2));
const capabilitiesPath=path.join(root,'dist/native-player-character/capabilities.json');const capabilities=JSON.parse(await readFile(capabilitiesPath,'utf8'));if(!capabilities.visualFeatures.includes('plugin-elements-v1'))capabilities.visualFeatures.push('plugin-elements-v1');await writeFile(capabilitiesPath,JSON.stringify(capabilities,null,2));
console.log(store);
