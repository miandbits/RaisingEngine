// Shared fixture for player extension-host tests: a hand-built exported game directory whose
// game.json carries a plugin screen element and the `extensions` block (CONTRACT.md §5-§6).
import {cp,mkdir,mkdtemp,readFile,writeFile,readdir} from 'node:fs/promises';
import {existsSync} from 'node:fs';
import {resolve,extname} from 'node:path';
import {deflateSync} from 'node:zlib';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';

export const NATIVE_APP=resolve(process.env.NATIVE_PLAYER_OUT??'dist/native-player-app');
export const WEB_APP=resolve(process.env.WEB_OUT??'dist/web');
export const EVIDENCE=resolve('.cache/anim-evidence');
const CRC=Array.from({length:256},(_,n)=>{let c=n;for(let k=0;k<8;k++)c=c&1?0xedb88320^(c>>>1):c>>>1;return c>>>0;});
const crc32=bytes=>{let c=0xffffffff;for(const b of bytes)c=CRC[(c^b)&255]^(c>>>8);return (c^0xffffffff)>>>0;};
/** Solid-colour RGBA PNG with a 2px darker border, so placement is visible in screenshots. */
export function png(width,height,[r,g,b,a=255]){
  const raw=Buffer.alloc((width*4+1)*height);
  for(let y=0;y<height;y++)for(let x=0;x<width;x++){const edge=x<2||y<2||x>=width-2||y>=height-2,o=y*(width*4+1)+1+x*4;raw.set(edge?[r>>1,g>>1,b>>1,a]:[r,g,b,a],o);}
  const chunk=(type,data)=>{const head=Buffer.alloc(8);head.writeUInt32BE(data.length,0);head.write(type,4,'ascii');const crc=Buffer.alloc(4);crc.writeUInt32BE(crc32(Buffer.concat([head.subarray(4),data])),0);return Buffer.concat([head,data,crc]);};
  const ihdr=Buffer.alloc(13);ihdr.writeUInt32BE(width,0);ihdr.writeUInt32BE(height,4);ihdr.set([8,6,0,0,0],8);
  return Buffer.concat([Buffer.from([137,80,78,71,13,10,26,10]),chunk('IHDR',ihdr),chunk('IDAT',deflateSync(raw)),chunk('IEND',Buffer.alloc(0))]);
}
export const IMAGES={'anim-torso':[80,120,[220,120,80]],'anim-face':[60,60,[250,214,170]],'anim-eyes-open':[40,10,[40,40,90]],'anim-eyes-closed':[40,10,[200,40,40]],
  'anim-torso-summer':[80,120,[235,200,60]],'anim-torso-winter':[80,120,[70,110,220]]};
export const rig={schemaVersion:1,canvas:{width:200,height:300,originX:100,originY:280},
  bones:[{id:'root',parent:null,x:100,y:280},{id:'body',parent:'root',x:0,y:-100},{id:'head',parent:'body',x:0,y:-80}],
  slots:[{id:'torso',bone:'body',attachment:'torso'},{id:'face',bone:'head',attachment:'face'},{id:'eyes',bone:'head',attachment:'open'}],
  attachments:[{slot:'torso',name:'torso',imageAssetId:'anim-torso',width:80,height:120,pivotX:0.5,pivotY:1},
    {slot:'face',name:'face',imageAssetId:'anim-face',width:60,height:60,pivotX:0.5,pivotY:1},
    {slot:'eyes',name:'open',imageAssetId:'anim-eyes-open',width:40,height:10,pivotX:0.5,pivotY:3},
    {slot:'eyes',name:'closed',imageAssetId:'anim-eyes-closed',width:40,height:10,pivotX:0.5,pivotY:3}],
  clips:[{id:'idle',durationMs:1000,loop:true,tracks:[{target:'bone:body',property:'y',keys:[{timeMs:0,value:-100},{timeMs:500,value:-104},{timeMs:1000,value:-100}]}]},
    {id:'blink',durationMs:2000,loop:true,tracks:[{target:'slot:eyes',property:'attachment',keys:[{timeMs:0,value:'open'},{timeMs:1800,value:'closed'},{timeMs:1900,value:'open'}]}]},
    {id:'joy',durationMs:600,loop:false,tracks:[{target:'bone:head',property:'rotation',keys:[{timeMs:0,value:0},{timeMs:300,value:20},{timeMs:600,value:0}]},
      {target:'bone:body',property:'scaleY',keys:[{timeMs:0,value:1},{timeMs:300,value:1.15},{timeMs:600,value:1}]}]}],
  mix:{defaultMs:0},
  // Outfits (plugin 1.2.0): same torso geometry, other images. The placed element wears summer
  // (props.skin); the cheer binding swaps to winter, and a save/load keeps it.
  skins:[{id:'summer',attachments:[{slot:'torso',name:'torso',imageAssetId:'anim-torso-summer',width:80,height:120,pivotX:0.5,pivotY:1}]},
    {id:'winter',attachments:[{slot:'torso',name:'torso',imageAssetId:'anim-torso-winter',width:80,height:120,pivotX:0.5,pivotY:1}]}]};
export const controller={schemaVersion:1,rig:'hero',defaultClip:'idle',layers:[{track:1,clip:'blink'}],
  bindings:[{id:'cheer-joy',signal:'actionResult',match:{actionId:'cheer',outcome:'success'},clip:'joy',skin:'winter',then:'default'}]};
export const pluginElement={id:'hero-anim',kind:'plugin',x:440,y:130,width:300,height:450,
  extension:{plugin:'cutout-animation',type:'character',ref:'hero-main',props:{scale:1,flipX:false,skin:'summer'}}};

/**
 * target: 'windows' | 'web'. runtime: true copies the runtime module, false leaves it missing.
 * Content is a real project snapshot; the plugin element is injected after the snapshot because
 * project schema support is owned by another change.
 */
export async function makeGame({target,name,runtime=true}){
  await mkdir(resolve('.cache/anim-tests'),{recursive:true});
  const root=await mkdtemp(resolve(`.cache/anim-tests/${name}-`)),projectRoot=resolve(root,'project'),game=resolve(root,'game');
  await cp(resolve('examples/academy-journey'),projectRoot,{recursive:true});
  const file=resolve(projectRoot,'project.json'),project=JSON.parse(await readFile(file,'utf8'));
  project.gameplay.actions=[{id:'cheer',name:'응원하기',costs:[],effects:[{op:'add',target:{kind:'attribute',attributeId:'money'},value:{op:'literal',value:1}}]}];
  project.visual.screens.find(s=>s.id==='home').elements.push({id:'cheer',kind:'button',x:56,y:600,width:300,height:48,text:'응원하기',action:'executeAction',actionId:'cheer'});
  await writeFile(file,JSON.stringify(project));
  const built=await buildSnapshot(await loadProject(projectRoot));
  if(!built.ok)throw new Error(JSON.stringify(built.diagnostics));
  const content=structuredClone(built.snapshot);
  content.project.visual.screens.find(s=>s.id==='home').elements.push(structuredClone(pluginElement));
  await cp(target==='windows'?NATIVE_APP:WEB_APP,game,{recursive:true});
  await mkdir(resolve(game,'assets'),{recursive:true});
  const assets={};
  for(const asset of content.assets){const relative=`assets/${asset.id}${extname(asset.path).toLowerCase()}`;await cp(resolve(projectRoot,asset.path),resolve(game,relative));assets[asset.id]=relative;}
  for(const [id,[w,h,colour]] of Object.entries(IMAGES)){const relative=`assets/${id}.png`;await writeFile(resolve(game,relative),png(w,h,colour));assets[id]=relative;}
  const module=target==='windows'?'cutout-runtime.dll':'cutout-runtime.mjs';
  const extensions={version:1,plugins:[{id:'cutout-animation',version:'1.0.0',abi:'raising-extension-1',entry:`plugins/cutout-animation/${module}`,
    documents:[{type:'rig',id:'hero',data:rig},{type:'controller',id:'hero-main',data:controller}]}]};
  if(runtime){
    await mkdir(resolve(game,'plugins/cutout-animation'),{recursive:true});
    const files=target==='windows'?['build-win/cutout-runtime.dll']:['build-wasm/cutout-runtime.mjs','build-wasm/cutout-runtime.wasm'];
    for(const f of files)await cp(resolve('dist/cutout',f),resolve(game,'plugins/cutout-animation',f.split('/')[1]));
  }
  await writeFile(resolve(game,'game.json'),JSON.stringify({formatVersion:'1',content,assets,extensions}));
  return {root,game,content,extensions};
}
/** Reference results from the same C++ core compiled to Wasm (Node), for parity checks. */
export async function referenceRuntime(){
  const {default:factory}=await import(new URL('../../dist/cutout/build-wasm/cutout-runtime.mjs',import.meta.url).href);
  const module=await factory();const request=module.cwrap('raising_extension_request','string',['string']);
  return op=>JSON.parse(request(JSON.stringify(op)));
}
export async function listFiles(dir){return existsSync(dir)?readdir(dir):[];}
