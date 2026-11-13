import {readFile,writeFile} from 'node:fs/promises';
import {join} from 'node:path';
import {surfaceFixture,nineSurface,surfaceColours} from './surfaces.mjs';
import {loadProject,buildSnapshot} from '../../dist/packages/project-service/index.js';
export const background=[32,48,64],fill=[200,210,220],red=[240,40,60],green=[40,180,100],border=[40,160,192];
const mix=(under,over,alpha)=>under.map((v,i)=>v*(1-alpha)+over[i]*alpha);
export const opacitySamples=[];
export function scrollOpacityDesign(){
 opacitySamples.length=0;
 const elements=[{id:'backdrop',kind:'panel',x:0,y:0,width:660,height:520,fill:'#203040'}];
 const sample=(id,x,y,expected)=>opacitySamples.push({id,x,y,expected:expected.map(Math.round)});
 const group=(id,x,y,opacity,childOpacity=1,extra={})=>{
  elements.push({id,kind:'group',label:id,x,y,width:180,height:180,opacity,fill:'#c8d2dc',borderWidth:4,borderColor:'#28a0c0',overflow:'scroll',...extra},
   {id:id+'-child',parentId:id,kind:'panel',x:30,y:30,width:70,height:70,opacity:childOpacity,fill:'#f0283c'});
  const painted=mix(background,fill,opacity);
  sample(id+'-fill',x+16,y+130,painted);sample(id+'-border',x+2,y+130,mix(background,border,opacity));
  sample(id+'-child',x+50,y+50,mix(painted,red,opacity*childOpacity));
 };
 group('opaque',20,20,1);group('partial',230,20,.5);group('child-alpha',440,20,.5,.5);
 group('nested',20,240,.5);elements.pop();
 // Scroll parents retain cumulative per-element opacity; they do not composite
 // the whole subtree as a single translucent layer. Native and flat Web agree.
 const nestedBase=mix(mix(background,fill,.5),green,.25),nestedChild=mix(nestedBase,red,.125);
 opacitySamples.splice(opacitySamples.findIndex(s=>s.id==='nested-child'),1);
 elements.push({id:'nested-inner',parentId:'nested',kind:'group',x:30,y:30,width:120,height:120,opacity:.5,fill:'#28b464',overflow:'scroll'},
  {id:'nested-top',parentId:'nested-inner',kind:'panel',x:20,y:20,width:40,height:40,opacity:.5,fill:'#f0283c'},
  {id:'nested-bottom',parentId:'nested-inner',kind:'panel',x:20,y:330,width:40,height:40,opacity:.5,fill:'#f0283c'});
 sample('nested-inner',60,350,nestedBase);sample('nested-top',80,300,nestedChild);
 for(const [id,x,surface,colour] of [['tile',230,{mode:'tile',imageAssetId:'surface-tile'},[245,235,210]],['nine',440,nineSurface,surfaceColours[4]]]){
  elements.push({id,kind:'group',label:id,x,y:240,width:180,height:180,opacity:.5,overflow:'scroll',surface});
  sample(id+'-surface',x+129,369,mix(background,colour,.5));
 }
 elements.push({id:'finish',kind:'button',x:230,y:455,width:250,height:48,text:'Finish',action:'navigate',screenId:'done'});
 return {width:660,height:520,routing:{defaultScreenId:'title',phases:[]},screens:[{id:'title',label:'Opacity',elements},{id:'done',label:'Done',elements:[{id:'done-copy',kind:'text',x:20,y:20,width:600,height:80,text:'Finished'}]}]};
}
export async function scrollOpacityFixture(root){const f=await surfaceFixture(root),p=JSON.parse(await readFile(join(root,'project.json'),'utf8'));p.visual=scrollOpacityDesign();await writeFile(join(root,'project.json'),JSON.stringify(p));const built=await buildSnapshot(await loadProject(root));if(!built.ok)throw Error(JSON.stringify(built.diagnostics));return {...f,content:built.snapshot,design:p.visual};}
export function assertSamples(assert,pixelAt,samples=opacitySamples){const measured=[];for(const sample of samples){const actual=pixelAt(sample.x,sample.y);measured.push({...sample,actual});assert.ok(actual.every((v,i)=>Math.abs(v-sample.expected[i])<=2),JSON.stringify({sample,actual}));}return measured;}
export const nestedEndColour=mix(mix(mix(background,fill,.5),green,.25),red,.125).map(Math.round);
