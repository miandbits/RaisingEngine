import {readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {fixture} from '../gameplay/fixture.mjs';
const actor={kind:'actor'};
const flag=flagId=>({op:'flag',flagId,equals:true,owner:actor});
const set=(flagId,value)=>({op:'setFlag',flagId,value,owner:actor});
export async function actorStoryFixture(t){return fixture(t,p=>{
 p.roster={stateModel:'character-v1',localAttributeIds:['skill'],members:['apprentice','mentor'].map(characterId=>({characterId,initialAttributes:{skill:10},activityId:'study',actionIds:['talk','bad']}))};
 p.gameplay.actions=[{id:'talk',name:'Talk',costs:[],effects:[],eventId:'personal-choice'},{id:'bad',name:'Bad',costs:[],effects:[set('personal-promise',true),set('missing-flag',true)]}];
 p.initialState.flags['personal-promise']=true; p.initialState.flags['personal-choice-record']=false;
 // The invalid action is used only in a detached snapshot below.
 p.gameplay.actions[1].effects=[];
},async root=>{
 const flagsPath=resolve(root,'flags.json'),defs=JSON.parse(await readFile(flagsPath,'utf8'));
 defs.flags.push(...['personal-promise','personal-choice-record'].map(id=>({id,label:id,initial:false})));await writeFile(flagsPath,JSON.stringify(defs));
 const common={formatVersion:'0.1.0',scope:'actor',priority:80,window:{fromPeriod:0,throughPeriod:35},entryNodeId:'intro',requirements:{op:'always'}};
 const source={...common,id:'personal-choice',label:'Personal choice',trigger:'interaction',repeatable:true,nodes:[
 {id:'intro',type:'dialogue',speakerId:null,text:'Will you return?',next:'ask'},
 {id:'ask',type:'choice',prompt:'Your promise',choices:[
 {id:'yes',text:'Return',requirements:{op:'always'},effects:[set('personal-promise',true),set('personal-choice-record',true)],next:'end'},
 {id:'other',text:'Another promise',requirements:{op:'always'},effects:[set('personal-promise',true)],next:'end'}]},
 {id:'end',type:'end'}]};
 const follow={...common,id:'personal-follow',label:'Follow',trigger:'after-event',requirements:{op:'all',conditions:[{op:'eventCompleted',eventId:source.id,owner:actor},flag('personal-promise'),flag('personal-choice-record')]},nodes:[{id:'intro',type:'dialogue',speakerId:null,text:'Your own promise.',next:'reward'},{id:'reward',type:'effect',effects:[{op:'add',target:{kind:'attribute',attributeId:'skill',owner:actor},value:{op:'literal',value:7}}],next:'end'},{id:'end',type:'end'}]};
 const chained={...common,id:'personal-coda',label:'Coda',trigger:'after-event',requirements:{op:'eventCompleted',eventId:follow.id,owner:actor},nodes:[{id:'intro',type:'dialogue',speakerId:null,text:'Remembered.',next:'end'},{id:'end',type:'end'}]};
 for(const e of [source,follow,chained])await writeFile(resolve(root,`events/${e.id}.json`),JSON.stringify(e));
});}
