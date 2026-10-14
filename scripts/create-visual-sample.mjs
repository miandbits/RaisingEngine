import { cp, mkdir, readFile, writeFile, readdir } from 'node:fs/promises';
import { resolve, join } from 'node:path';
import { createVisual } from '../dist/packages/visual/model.js';
const root=resolve(process.argv[2]??'examples/academy-visual');
const source=resolve(process.argv[3]??'examples/academy-year');
try { if((await readdir(root)).length)throw Error(`Refusing to overwrite nonempty project: ${root}`); } catch(e){if(e.code!=='ENOENT')throw e;}
await cp(source,root,{recursive:true,filter:path=>!path.split(/[\\/]/).includes('.studio')});
await mkdir(join(root,'assets/visual'),{recursive:true});
const files=['courtyard','library','haru','yuna'];
for(const name of files)await cp(resolve(`assets/visual-sample/${name}.png`),join(root,`assets/visual/${name}.png`));
const read=async file=>JSON.parse(await readFile(join(root,file),'utf8'));
const write=async(file,data)=>writeFile(join(root,file),JSON.stringify(data,null,2)+'\n');
const project=await read('project.json');const manifest=await read('assets/manifest.json');
for(const name of files){if(manifest.assets.some(a=>a.id===`visual-${name}`))throw Error(`Asset ID already exists: visual-${name}`);manifest.assets.push({id:`visual-${name}`,kind:'image',path:`assets/visual/${name}.png`});}
await write('assets/manifest.json',manifest);
for(const [id,name] of [['apprentice','haru'],['mentor','yuna']]){const c=await read(`characters/${id}.json`);c.portraits={...c.portraits,'visual-neutral':`visual-${name}`};await write(`characters/${id}.json`,c);}
const portraits=[{characterId:'apprentice',portraitId:'visual-neutral',position:'center'}];
project.visual=createVisual();project.visual.defaultScene={backgroundAssetId:'visual-courtyard',portraits};
project.title=`${project.title} · 비주얼`;await write('project.json',project);
for(const file of (await readdir(join(root,'activities'))).filter(f=>f.endsWith('.json'))){const a=await read(`activities/${file}`);a.presentation={...a.presentation,backgroundAssetId:['read','research','craft'].includes(a.id)?'visual-library':'visual-courtyard',portraits};await write(`activities/${file}`,a);}
for(const folder of ['events','endings'])for(const file of (await readdir(join(root,folder))).filter(f=>f.endsWith('.json'))){const d=await read(`${folder}/${file}`);let id='visual-intro';for(let i=2;d.nodes.some(n=>n.id===id);i++)id=`visual-intro-${i}`;d.nodes.unshift({id,type:'scene',scene:{backgroundAssetId:/scholar|exam|research|study/.test(d.id)?'visual-library':'visual-courtyard',portraits:folder==='events'?[{characterId:'apprentice',portraitId:'visual-neutral',position:'left'},{characterId:'mentor',portraitId:'visual-neutral',position:'right'}]:portraits},next:d.entryNodeId});d.entryNodeId=id;await write(`${folder}/${file}`,d);}
console.log(`Visual project created: ${root}`);
