import {phaseForecastFixture} from './phase-forecast-ui.mjs';
export async function readingActivationFixture(root){
 const f=await phaseForecastFixture(root),content=structuredClone(f.content),d=content.project.visual,home=d.screens.find(s=>s.id==='schedule');
 d.routing={defaultScreenId:'title',phases:[{phase:'planning',screenId:'schedule'},{phase:'ready',screenId:'schedule'},{phase:'activity-result',screenId:'schedule'},{phase:'finished',screenId:'ending'}]};
 const button=(id,text,screenId,x=800)=>({id,kind:'button',action:'navigate',screenId,text,x,y:640,width:360,height:60,color:'#fff2d5',fill:'#234342'});
 home.elements.push(button('open-page','Read field notes','page'));home.readableLayout.actions.push('open-page');
 const longText=Array.from({length:24},(_,i)=>`Field note ${i+1}: a deliberately oversized authored text box must flow naturally in this reading page.`).join('\n')+'\nFINAL FIELD NOTE';
 d.screens.push({id:'page',label:'Field notes',elements:[{id:'page-heading',kind:'text',text:'Field notes',x:40,y:20,width:1000,height:70,fontSize:28,color:'#234342'},{id:'page-body',kind:'text',text:longText,x:40,y:100,width:1000,height:2100,fontSize:18,color:'#234342'},{...home.elements.find(e=>e.id==='schedule'),id:'page-schedule',x:40,y:300},{...button('back-page','Back to plan','schedule',40)},{...button('page-action','Keep reading','page',440)}],readableLayout:{activation:'always',items:[{elementId:'page-heading'},{elementId:'page-body'},{elementId:'page-schedule',schedulePresentation:'compact'}],actions:['back-page','page-action'],actionLayout:'grouped',primaryAction:'back-page'}});
 return {...f,content};
}
