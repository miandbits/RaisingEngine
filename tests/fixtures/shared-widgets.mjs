import {writeFile} from 'node:fs/promises';
import {join} from 'node:path';
import {phaseForecastFixture} from './phase-forecast-ui.mjs';
import {loadProject,buildSnapshot} from '../../dist/packages/project-service/index.js';
export const sharedWidgetIds={schedule:'home-section__content__schedule',stats:'home-section__content__stats',result:'home-section__content__result'};
export async function sharedWidgetsFixture(root,{layout='activity-board',reading=true,two=false}={}){
 const base=await phaseForecastFixture(root,{layout,calendar:false}),p=structuredClone(base.content.project),v=p.visual;
 delete p.gameplay;p.schedulePlayback='continuous';
 v.activityTimelineCompletion='reveal-result';v.timelines=[{id:'study-scene',name:'Study scene',clips:[{id:'practice',durationMs:1000,text:'{activity} · {outcome}'}]}];v.activityTimelines=[{activityId:'study',timelineId:'study-scene'}];
 const screen=v.screens.find(s=>s.id==='schedule'),schedule=screen.elements.find(e=>e.kind==='schedule'),result=screen.elements.find(e=>e.kind==='result');
 Object.assign(schedule,{x:0,y:0,width:1000,height:390});
 Object.assign(result,{x:0,y:0,width:1000,height:390,resultLayout:'growth'});
 const stats={id:'stats',kind:'stats',x:0,y:400,width:1000,height:100,attributeIds:['money','stamina','skill'],fontSize:18};
 v.components=[{id:'game-widgets',label:'Reusable game widgets',width:1000,height:510,elements:[schedule,stats,result]},{id:'home-panel',label:'Nested home section',width:1000,height:510,elements:[{id:'content',kind:'component',componentId:'game-widgets',x:0,y:0,width:1000,height:510}]}];
 const instance={id:'home-section',kind:'component',componentId:'home-panel',x:40,y:100,width:1000,height:510};
 const button=(id,action,x,extra={})=>({id,kind:'button',action,text:id,x,y:640,width:200,height:56,fontSize:20,...extra});
 screen.elements=[instance,...screen.elements.filter(e=>!['schedule','result','text'].includes(e.kind)),button('map','navigate',760,{screenId:'outings'})];
 if(two){instance.width=600;v.components[0].elements.forEach(e=>e.width=560);v.components[0].width=600;v.components[1].width=600;v.components[1].elements[0].width=600;screen.elements.unshift({...instance,id:'other-section',x:660});}
 screen.readableLayout={items:[{elementId:sharedWidgetIds.schedule,schedulePresentation:'compact',planningGuidance:'first-use',planReuse:'last-committed'},{elementId:sharedWidgetIds.stats},{elementId:sharedWidgetIds.result}],actions:['advance','save','load','map']};
 v.screens.push({id:'outings',label:'Outings',elements:[button('open-planner','openPlanner',40,{screenId:'schedule',plannerElementId:sharedWidgetIds.schedule,plannerGroupId:'all',plannerActivityId:'study'})],readableLayout:{items:[],actions:['open-planner']}});
 v.routing={defaultScreenId:'outings',phases:[{phase:'title',screenId:'title'},{phase:'ready',screenId:'schedule'},{phase:'activity-result',screenId:'schedule'},{phase:'dialogue',screenId:'story'},{phase:'choice',screenId:'story'},{phase:'finished',screenId:'ending'}]};
 if(layout!=='activity-board'){v.routing.defaultScreenId='schedule';v.screens.find(s=>s.id==='outings').elements=[];v.screens.find(s=>s.id==='outings').readableLayout.actions=[];}
 if(!reading){delete v.readableLayout;for(const s of v.screens)delete s.readableLayout;}
 await writeFile(join(root,'project.json'),JSON.stringify(p));
 const built=await buildSnapshot(await loadProject(root));if(!built.ok)throw Error(JSON.stringify(built.diagnostics));return {root,content:built.snapshot};
}
