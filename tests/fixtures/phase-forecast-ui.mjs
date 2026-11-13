import {cp,rm,mkdir,readFile,writeFile} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {loadProject,buildSnapshot} from '../../dist/packages/project-service/index.js';
const add=(id,value)=>({op:'add',target:{kind:'attribute',attributeId:id},value:{op:'literal',value}});
export async function phaseForecastFixture(root,{story=false,opted=true,layout='columns',calendar=true,locale='en'}={}){
 await cp(resolve('examples/my-first-raising-game'),root,{recursive:true});
 await rm(join(root,'events'),{recursive:true,force:true});await mkdir(join(root,'events'));
 await rm(join(root,'scenarios'),{recursive:true,force:true});
 await rm(join(root,'activities'),{recursive:true,force:true});await mkdir(join(root,'activities'));
 const activities=[{id:'study',label:'Workshop',description:'Practice carefully, then leave time to recover.',costs:[{attributeId:'money',amount:5},{attributeId:'stamina',amount:10}],baseEffects:[add('skill',10)]},{id:'rest',label:'Rest',description:'Recover energy.',costs:[],baseEffects:[add('stamina',20)]},{id:'costly',label:'Expensive workshop',description:'This workshop requires more money than the starting resources.',costs:[{attributeId:'money',amount:1000}],baseEffects:[]}];
 for(const a of activities)await writeFile(join(root,'activities',a.id+'.json'),JSON.stringify({formatVersion:'0.1.0',requirements:{op:'always'},...a}));
 if(story)await writeFile(join(root,'events/story.json'),JSON.stringify({formatVersion:'0.1.0',id:'story',label:'HIDDEN STORY LABEL',trigger:'after-activity',activityId:'study',priority:1,requirements:{op:'always'},window:{fromPeriod:0,throughPeriod:7},entryNodeId:'fee',nodes:[{id:'fee',type:'effect',effects:[add('money',-3)],next:'talk'},{id:'talk',type:'dialogue',speakerId:'apprentice',text:'HIDDEN STORY TEXT',next:'end'},{id:'end',type:'end'}]}));
 const p=JSON.parse(await readFile(join(root,'project.json'),'utf8'));p.durationMonths=2;p.periodsPerMonth=4;p.initialState.attributes={money:40,stamina:80,skill:0,stress:0};
 if(calendar)p.calendar={monthsPerYear:12,daysPerMonth:28,periodLabel:'Week'};else delete p.calendar;
 p.localization={defaultLocale:locale,fallbackLocale:'en',locales:[{id:'en',label:'English'},{id:'ja',label:'日本語'},{id:'ko',label:'한국어'}],entries:[]};
 p.gameplay={startLocationId:'home',locations:[{id:'home',name:'Home',connections:[]}],items:[],shops:[],interactions:[],actions:[{id:'upkeep',name:'Upkeep',costs:[{attributeId:'money',amount:2}],effects:[add('stamina',1)]},{id:'phase-tax',name:'Phase upkeep',costs:[],effects:[add('money',-4)]}],rules:[{id:'upkeep',name:'Upkeep',trigger:{type:'periodEnd'},actionId:'upkeep'},{id:'phase-tax',name:'Phase upkeep',trigger:{type:'monthEnd'},actionId:'phase-tax'}]};
 const text=(id,extra={})=>({id,kind:'text',x:40,y:24,width:900,height:70,text:id,fontSize:24,...extra});
 const button=(id,action,x)=>({id,kind:'button',action,text:id,x,y:640,width:220,height:60,fontSize:22,fill:'#305b4c',color:'#ffffff'});
 const reading=(items,actions)=>({items:items.map(x=>typeof x==='string'?{elementId:x}:x),actions});
 p.visual={width:1280,height:720,readableLayout:{background:'#f5efdf',fontSize:18},screens:[
  {id:'title',label:'Title',elements:[text('intro',{text:'Forecast fixture'}),button('start','start',40)],readableLayout:reading(['intro'],['start'])},
  {id:'schedule',label:'Plan',elements:[text('plan-title',{text:'Your phase plan'}),{id:'schedule',kind:'schedule',scheduleLayout:layout,scheduleGroups:[{id:'all',label:'Activities',activityIds:['study','rest','costly']}],x:40,y:110,width:864,height:300,color:'#203a3b',fill:'#f3ead4',fontSize:18,showActivityDescription:true,showActivityRules:true,...(opted?{showPhaseForecast:true}:{}),visibleWhen:'planning'},text('result',{kind:'result',visibleWhen:'result',y:110,height:300}),button('advance','advance',40),button('save','save',280),button('load','load',520)],readableLayout:reading(['plan-title',{elementId:'schedule',schedulePresentation:'compact'},'result'],['advance','save','load'])},
  {id:'story',label:'Story',elements:[text('dialogue',{kind:'dialogue',height:400}),text('choices',{kind:'choices',y:430,height:150}),button('advance','advance',40)],readableLayout:reading(['dialogue','choices'],['advance'])},
  {id:'ending',label:'Ending',elements:[text('ending',{binding:'ending'}),button('start','start',40)],readableLayout:reading(['ending'],['start'])}
 ]};
 await writeFile(join(root,'project.json'),JSON.stringify(p));const built=await buildSnapshot(await loadProject(root));if(!built.ok)throw Error(JSON.stringify(built.diagnostics));return {root,content:built.snapshot};
}
