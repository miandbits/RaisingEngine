import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,readdir} from 'node:fs/promises';
import Ajv from 'ajv/dist/2020.js';
import {resolveResumeContext,resumeContextEligibility,resumeContextLimits} from '../../dist/packages/visual/resume-context.js';
import {checkResumeContext} from '../../dist/packages/validation/resume-context.js';
import {checkVisual} from '../../dist/packages/validation/visual.js';
import {SemanticContext} from '../../dist/packages/validation/context.js';
import {requiredPlayerFeatures,missingPlayerFeatures} from '../../dist/packages/export-service/compatibility.js';
const leaf=(id,extra={})=>({id,kind:'text',x:0,y:0,width:400,height:80,text:id,...extra});
const design=()=>({width:1280,height:720,resumeContext:{screenId:'recap',elementIds:['second','first'],showAfterLoad:true},screens:[{id:'recap',label:'Recap',elements:[leaf('first'),leaf('second')]}]});
const collect=(d,extra={})=>resolveResumeContext(d,{phase:'planning',evaluate:c=>c.op==='literal'&&c.value===true,text:e=>e.text??'',...extra});
const diagnostics=d=>{const ctx=new SemanticContext();checkResumeContext(ctx,{file:'project.json',kind:'project',data:{visual:d},version:'test',locations:new Map()},d);return ctx.diagnostics;};
test('explicit schema opt-in bounds references and rejects unknown storage or transcript options',async()=>{
 const ajv=new Ajv({strict:true,allErrors:true});for(const f of(await readdir('packages/schema')).filter(f=>f.endsWith('.schema.json')))ajv.addSchema(JSON.parse(await readFile('packages/schema/'+f,'utf8')));
 const validate=ajv.getSchema('https://raising-engine.example/schemas/0.1.0/visual.schema.json'),d=design();assert.equal(validate(d),true,ajv.errorsText(validate.errors));
 for(const config of[{},null,true,{screenId:'recap',elementIds:[]},{screenId:'recap',elementIds:['first','first']},{screenId:'recap',elementIds:Array.from({length:65},(_,i)=>'e'+i)},{...d.resumeContext,showAfterLoad:false},{...d.resumeContext,persistent:true}])assert.equal(validate({...d,resumeContext:config}),false,JSON.stringify(config));
 assert.equal(validate({...d,resumeContext:{screenId:'recap',elementIds:Array.from({length:64},(_,i)=>'e'+i)}}),true);delete d.resumeContext;assert.equal(validate(d),true);
});
test('safe binding whitelist excludes story, selected rows and arbitrary data expressions',()=>{
 for(const binding of[undefined,'none','calendar','calendar-year','calendar-month','calendar-day','calendar-weekday','location','user-field','attribute-value','relationship-value','item-count'])assert.equal(resumeContextEligibility(leaf('a',{binding})),null,binding);
 for(const binding of['story','speaker','narrative-actor','choice-result','selected-field','save-summary','context-name','data-value','schedule-summary','player-notice','ending','title'])assert.notEqual(resumeContextEligibility(leaf('a',{binding})),null,binding);
 assert.equal(resumeContextEligibility(leaf('a',{binding:'relationship-value',valueFormat:'{label}: {value} ({delta})'})),'binding');assert.equal(resumeContextEligibility(leaf('a',{binding:'relationship-value',valueFormat:'{label}: {value}'})),null);
 assert.equal(resumeContextEligibility(leaf('a',{dataBinding:{op:'literal',value:1}})),'selection');assert.equal(resumeContextEligibility(leaf('a',{selectionBindings:[{property:'text',selectionKey:'row',field:'name'}]})),'selection');assert.equal(resumeContextEligibility(leaf('a',{selectionBindings:[]})),null);assert.equal(resumeContextEligibility(leaf('a',{kind:'dialogue'})),'kind');
});
test('semantic diagnostics preserve repairable missing references, unsafe leaves and action dependency',()=>{
 const d=design();assert.deepEqual(diagnostics(d),[]);d.resumeContext.elementIds=['first','first','missing'];assert.deepEqual(diagnostics(d).map(x=>x.code),['RESUME_CONTEXT_DUPLICATE','RESUME_CONTEXT_REFERENCE']);d.resumeContext.screenId='gone';assert.equal(diagnostics(d)[0].code,'RESUME_CONTEXT_SCREEN');
 d.resumeContext={screenId:'recap',elementIds:['first','second']};d.screens[0].elements[0].binding='relationship-value';d.screens[0].elements[0].valueFormat='{delta}';d.screens[0].elements[1].kind='button';assert.deepEqual(diagnostics(d).map(x=>x.code),['RESUME_CONTEXT_BINDING','RESUME_CONTEXT_KIND']);
 const project={gameId:'reminders',visual:{width:1280,height:720,screens:[{id:'title',label:'Title',elements:[leaf('open',{kind:'button',action:'resumeContext'})]}]}},ctx=new SemanticContext();checkVisual(ctx,{file:'project.json',kind:'project',data:project,version:'test',locations:new Map()},project.visual);assert.ok(ctx.diagnostics.some(d=>d.code==='UI_RESUME_CONTEXT_REQUIRED'));
});
test('selection order, inherited conditions and phase visibility apply without source canvas clipping',()=>{
 const d=design();d.screens[0].elements=[leaf('group',{kind:'group',width:20,height:20,clipChildren:true,visibleCondition:{op:'literal',value:true}}),leaf('first',{parentId:'group',x:500,text:'Current commitment'}),leaf('second',{visibleWhen:'planning',text:'Chosen identity'}),leaf('hidden',{hidden:true}),leaf('transparent',{opacity:0}),leaf('wrong-phase',{visibleWhen:'choice'}),leaf('false',{visibleCondition:{op:'literal',value:false}})];d.resumeContext.elementIds=['second','first','hidden','transparent','wrong-phase','false','missing'];
 const before=structuredClone(d);assert.deepEqual(collect(d).entries.map(x=>x.text),['Chosen identity','Current commitment']);assert.deepEqual(d,before);assert.equal(collect(d,{phase:'ready'}).entries.length,2);d.screens[0].elements[0].visibleCondition.value=false;assert.deepEqual(collect(d).entries.map(x=>x.text),['Chosen identity']);d.screens[0].elements[0].hidden=true;assert.equal(collect(d).omittedEntries,0);
});
test('component leaves retain explicit current context and localized authored variants',()=>{
 const d=design();d.components=[{id:'card',label:'Card',width:400,height:100,elements:[leaf('copy',{text:'Original',variants:[{when:{op:'literal',value:true},values:{text:'約束を守る'}}]})]}];d.screens[0].elements=[leaf('card-instance',{kind:'component',componentId:'card'})];d.resumeContext.elementIds=['card-instance__copy'];assert.deepEqual(diagnostics(d),[]);assert.equal(collect(d).entries[0].text,'約束を守る');assert.equal(d.components[0].elements[0].text,'Original');
});
test('64 KiB payload is UTF-8 bounded and reports truncation without counting invisible leaves',()=>{
 const d=design();d.screens[0].elements=Array.from({length:64},(_,i)=>leaf('e'+i,{text:'界🙂'.repeat(1000)}));d.resumeContext.elementIds=d.screens[0].elements.map(e=>e.id);const s=collect(d);assert.ok(s.utf8Bytes<=resumeContextLimits.utf8Bytes);assert.equal(s.truncated,true);assert.ok(s.omittedEntries>0);assert.equal(s.entries.length+s.omittedEntries,64);assert.ok(s.entries.at(-1).truncated);assert.ok(!s.entries.some(e=>e.text.includes('�')));assert.equal(s.utf8Bytes,s.entries.reduce((n,e)=>n+Buffer.byteLength(e.elementId)+Buffer.byteLength(e.text),0));
 d.screens[0].elements.forEach(e=>e.hidden=true);assert.deepEqual(collect(d),{entries:[],utf8Bytes:0,omittedEntries:0,truncated:false});
});
test('opt-in requires its precise exported-player capability; absence retains old projects',()=>{
 const content={project:{visual:design()},activities:[],events:[],endings:[],characters:[],attributes:[],assets:[]};const required=requiredPlayerFeatures(content);assert.ok(required.includes('resume-context-v1'));assert.ok(missingPlayerFeatures(required,['reading-history-v1']).includes('resume-context-v1'));delete content.project.visual.resumeContext;assert.ok(!requiredPlayerFeatures(content).includes('resume-context-v1'));
});
