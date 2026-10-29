import {readFile,writeFile} from 'node:fs/promises';
import {join} from 'node:path';
import {phaseForecastFixture} from './phase-forecast-ui.mjs';
import {loadProject,buildSnapshot} from '../../dist/packages/project-service/index.js';
export function scrollGroupDesign(){return {width:1280,height:720,routing:{defaultScreenId:'title',phases:[]},readableLayout:{background:'#f5efdf',fontSize:18},screens:[{id:'title',label:'Credits',elements:[
 {id:'heading',kind:'text',text:'Project credits',x:50,y:20,width:1100,height:60,color:'#203a3b'},
 {id:'credits',kind:'group',label:'Project credits',x:50,y:100,width:1100,height:470,overflow:'scroll',clipChildren:true,fill:'#eee2c8'},
 {id:'part-one',parentId:'credits',kind:'text',text:Array.from({length:22},(_,i)=>`Team contribution ${i+1}. Carefully prepared credits.`).join('\n'),x:20,y:20,width:1000,height:880,fontSize:24,color:'#203a3b'},
 {id:'nested',parentId:'credits',kind:'group',x:20,y:930,width:1000,height:930,opacity:0.8},
 {id:'part-two',parentId:'nested',kind:'text',text:Array.from({length:20},(_,i)=>`Acknowledgement ${i+1}. Thank you for your work.`).join('\n')+'\nFINAL CREDIT LINE',x:0,y:0,width:960,height:850,fontSize:24,color:'#203a3b'},
 {id:'last-action',parentId:'credits',kind:'button',text:'Finish reading credits',action:'navigate',screenId:'detail',x:20,y:1880,width:500,height:64,color:'#ffffff',fill:'#305b4c'},
 {id:'outside',kind:'button',text:'Outside action',action:'navigate',screenId:'detail',x:50,y:630,width:300,height:64,color:'#ffffff',fill:'#305b4c'}],readableLayout:{items:[{elementId:'heading'},{elementId:'part-one'},{elementId:'part-two'}],actions:['last-action']}},
 {id:'detail',label:'Done',elements:[{id:'done',kind:'text',text:'Credits finished',x:40,y:40,width:1000,height:100,color:'#203a3b'},{id:'back',kind:'button',text:'Back to credits',action:'navigate',screenId:'title',x:40,y:180,width:300,height:64,fill:'#305b4c',color:'#ffffff'}],readableLayout:{items:[{elementId:'done'}],actions:['back']}}]};}
export async function scrollGroupFixture(root){const f=await phaseForecastFixture(root,{opted:false});const p=JSON.parse(await readFile(join(root,'project.json'),'utf8'));p.visual=scrollGroupDesign();await writeFile(join(root,'project.json'),JSON.stringify(p));const built=await buildSnapshot(await loadProject(root));if(!built.ok)throw Error(JSON.stringify(built.diagnostics));return {...f,content:built.snapshot};}
