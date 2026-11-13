import {cp,readFile,writeFile} from 'node:fs/promises';
import {join,resolve} from 'node:path';
import {encodeRgbaPng,decodePng} from '../../dist/packages/studio-service/png.js';
import {loadProject,buildSnapshot} from '../../dist/packages/project-service/index.js';
export const surfaceColours=[[220,30,40],[240,130,30],[30,190,60],[30,190,210],[80,85,90],[220,40,180],[40,60,220],[230,220,40],[140,50,200]];
export function surfaceTextures(){
 const nine=Buffer.alloc(36*36*4),tile=Buffer.alloc(8*8*4);
 for(let y=0;y<36;y++)for(let x=0;x<36;x++){nine.set(surfaceColours[Math.floor(y/12)*3+Math.floor(x/12)],(y*36+x)*4);nine[(y*36+x)*4+3]=255;}
 for(let y=0;y<8;y++)for(let x=0;x<8;x++){tile.set((x<4)===(y<4)?[245,235,210]:[180,210,225],(y*8+x)*4);tile[(y*8+x)*4+3]=255;}
 return {nine:encodeRgbaPng({width:36,height:36,pixels:nine}),tile:encodeRgbaPng({width:8,height:8,pixels:tile})};
}
export const nineSurface={mode:'nine-slice',imageAssetId:'surface-nine',insets:{top:12,right:12,bottom:12,left:12}};
export function surfaceDesign(){
 const leaf=(id,kind,x,y,width,height,more={})=>({id,kind,x,y,width,height,...more});
 const button=leaf('open','button',16,120,250,70,{text:'Open this page',action:'navigate',screenId:'detail',fill:'#eeeeee',color:'#111111',borderWidth:3,borderColor:'#00ffff',padding:4,styleId:'frame',hoverFill:'#337744',pressedFill:'#992244'});
 const paper=leaf('paper','text',320,16,280,64,{styleId:'paper',text:'Continuous readable text. '.repeat(14),fontSize:18,padding:6,color:'#111111',borderWidth:2,borderColor:'#00ffff'});
 return {width:640,height:360,defaultStyle:{surface:{mode:'none'}},components:[{id:'card',label:'Reusable surface',width:80,height:70,elements:[leaf('panel','panel',0,0,80,70,{surface:nineSurface,fill:'#778899'})]}],styles:[{id:'frame',label:'Frame',surface:nineSurface},{id:'paper',label:'Paper',surface:{mode:'tile',imageAssetId:'surface-tile'}}],readableLayout:{maxShortEdge:600,fontSize:18,background:'#eeeeee',surface:{mode:'tile',imageAssetId:'surface-tile'}},routing:{defaultScreenId:'title',phases:[]},screens:[
  {id:'title',label:'Surfaces',elements:[leaf('tile','panel',16,16,96,64,{styleId:'paper',borderWidth:2,borderColor:'#00ffff'}),leaf('nine','panel',128,16,120,64,{styleId:'frame',borderWidth:2,borderColor:'#00ffff'}),leaf('small','panel',270,16,16,16,{surface:nineSurface}),leaf('cleared','panel',270,48,30,30,{styleId:'frame',surface:{mode:'none'},fill:'#778899'}),paper,button,leaf('component-tile','component',320,120,80,70,{componentId:'card',overrides:[{elementId:'panel',values:{surface:{mode:'tile',imageAssetId:'surface-tile'}}}]}),leaf('component-none','component',420,120,80,70,{componentId:'card',overrides:[{elementId:'panel',values:{surface:{mode:'none'}}}]}),leaf('variant','panel',520,120,80,70,{styleId:'paper',variants:[{label:'Nine-slice during this condition',when:{op:'always'},values:{surface:nineSurface}}]}),leaf('long','text',16,220,584,110,{styleId:'frame',padding:16,text:'A wrapped caption keeps its border and live text. '.repeat(8),fontSize:18,color:'#ffffff'})],readableLayout:{items:[{elementId:'paper'},{elementId:'long'}],actions:['open']}},
  {id:'detail',label:'Other page',elements:[leaf('back','button',16,16,300,70,{text:'Back',action:'navigate',screenId:'title',surface:nineSurface}),leaf('root-copy','text',16,120,600,200,{text:'Live text over the readable root surface.',surface:{mode:'none'},fontSize:18,color:'#111111'})],readableLayout:{items:[{elementId:'root-copy'}],actions:['back']}}
 ]};
}
export async function surfaceFixture(root){await cp(resolve('examples/my-first-raising-game'),root,{recursive:true});const textures=surfaceTextures(),p=JSON.parse(await readFile(join(root,'project.json'),'utf8'));p.visual=surfaceDesign();await writeFile(join(root,'project.json'),JSON.stringify(p));const manifest=JSON.parse(await readFile(join(root,'assets/manifest.json'),'utf8'));for(const[id,bytes]of Object.entries(textures)){const imageAssetId='surface-'+id;await writeFile(join(root,'assets',imageAssetId+'.png'),bytes);manifest.assets.push({id:imageAssetId,kind:'image',path:'assets/'+imageAssetId+'.png'});}await writeFile(join(root,'assets/manifest.json'),JSON.stringify(manifest));const built=await buildSnapshot(await loadProject(root));if(!built.ok)throw Error(JSON.stringify(built.diagnostics));return {root,content:built.snapshot,design:p.visual,textures};}
export const pixel=(image,x,y)=>[...image.pixels.subarray((Math.floor(y)*image.width+Math.floor(x))*4,(Math.floor(y)*image.width+Math.floor(x))*4+3)];
export const readRaster=async file=>decodePng(await readFile(file));
