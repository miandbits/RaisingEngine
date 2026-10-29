import test from 'node:test';
import assert from 'node:assert/strict';
import {readableLayoutForScreen,normalizeReadablePreferences} from '../../dist/packages/visual/readable-layout.js';
import {resolveScreenElements} from '../../dist/packages/visual/components.js';
import {checkReadableLayout} from '../../dist/packages/validation/readable-layout.js';
import {SemanticContext} from '../../dist/packages/validation/context.js';

const element=(id,kind,extra={})=>({id,kind,x:0,y:0,width:160,height:40,...extra});
function fixture(){return {width:1280,height:720,readableLayout:{},screens:[{id:'story',label:'Story',elements:[
  element('clip','group',{width:100,height:20,clipChildren:true,visibleCondition:{op:'flag',flagId:'known',equals:true}}),
  element('speaker','text',{text:'Long speaker name',parentId:'clip',y:200}),
  element('dialogue','dialogue'),element('choice','choices',{visibleWhen:'choice'}),
  element('advance','button',{text:'Continue',action:'advance'}),element('hidden','text',{hidden:true}),
],readableLayout:{items:[{elementId:'speaker',fontSize:24},{elementId:'dialogue'},{elementId:'choice'},{elementId:'hidden'}],actions:['advance']}}]};}

test('physical viewport activation and 200% text leave authored data and legacy layouts unchanged',()=>{
 const design=fixture(),before=structuredClone(design);
 assert.equal(readableLayoutForScreen(design,'story',{width:1280,height:720}),null);
 for(const viewport of [{width:320,height:568},{width:390,height:844},{width:844,height:390}]){
  const result=readableLayoutForScreen(design,'story',viewport,{textScale:2});
  assert.equal(result.config.minTargetSize,44);assert.equal(result.items[0].fontSize,48);assert.equal(result.items[1].fontSize,36);
  assert.equal(result.actions[0].element.id,'advance');assert.equal(result.actions[0].element.action,'advance');
 }
 assert.ok(readableLayoutForScreen(design,'story',{width:1280,height:720},{readingLayout:'on'}));
 for(const viewport of [{width:0,height:800},{width:NaN,height:300},{width:320,height:Infinity}])assert.equal(readableLayoutForScreen(design,'story',viewport,{readingLayout:'on'}),null);
 assert.deepEqual(design,before);
 const legacy=structuredClone(design);delete legacy.readableLayout;
 assert.equal(readableLayoutForScreen(legacy,'story',{width:320,height:568},{readingLayout:'on'}),null);
});

test('reading order removes canvas clipping while retaining authored hidden state and inherited conditions',()=>{
 const design=fixture();assert.equal(resolveScreenElements(design,'story').find(e=>e.id==='speaker').hidden,true);
 const result=readableLayoutForScreen(design,'story',{width:390,height:844});
 assert.deepEqual(result.items.map(i=>i.element.id),['speaker','dialogue','choice','hidden']);
 assert.ok(!result.items[0].element.hidden);assert.equal(result.items[0].element._clipRect,undefined);
 assert.deepEqual(result.items[0].element.visibleCondition,design.screens[0].elements[0].visibleCondition);
 assert.equal(result.items[2].element.visibleWhen,'choice');assert.equal(result.items[3].element.hidden,true);
 assert.equal(resolveScreenElements(design,'story').find(e=>e.id==='speaker').hidden,true);
});

test('component leaves preserve stable prefixed IDs and inherited disable conditions',()=>{
 const design=fixture();design.components=[{id:'card',label:'Card',width:200,height:100,elements:[element('text','text',{text:'Card title'}),element('go','button',{text:'Read',action:'navigate',screenId:'story'})]}];
 design.screens[0].elements=[element('one','component',{componentId:'card',enabledCondition:{op:'flag',flagId:'allowed',equals:true}})];
 design.screens[0].readableLayout={items:[{elementId:'one__text'}],actions:['one__go']};
 const result=readableLayoutForScreen(design,'story',{width:320,height:568});
 assert.equal(result.actions[0].element.id,'one__go');assert.deepEqual(result.actions[0].element.enabledCondition,design.screens[0].elements[0].enabledCondition);
});

test('stored presentation preferences reject malformed or unbounded values',()=>{
 for(const value of [null,{},[],{textScale:'2'},{textScale:20,readingLayout:'off'},{textScale:NaN}])assert.deepEqual(normalizeReadablePreferences(value),{readingLayout:'auto',textScale:1});
 for(const textScale of [1,1.25,1.5,2])assert.deepEqual(normalizeReadablePreferences({textScale,readingLayout:'on'}),{readingLayout:'on',textScale});
});

function diagnostics(design){const ctx=new SemanticContext();checkReadableLayout(ctx,{file:'project.json',kind:'project',data:{visual:design},version:'test',locations:new Map()},design);return ctx.diagnostics;}
test('invalid references, duplicate IDs, unsupported kinds and non-button actions are export-blocking errors',()=>{
 const design=fixture();design.screens[0].readableLayout={items:[{elementId:'missing'},{elementId:'clip'},{elementId:'speaker',imageHeight:100},{elementId:'advance'}],actions:['advance','dialogue']};
 const issues=diagnostics(design);for(const code of ['READABLE_REFERENCE','READABLE_KIND','READABLE_IMAGE_HEIGHT','READABLE_DUPLICATE','READABLE_ACTION_KIND'])assert.ok(issues.some(i=>i.code===code&&i.severity==='error'),code);
 assert.ok(issues.some(i=>i.code==='READABLE_CONTROL_OMITTED'&&i.severity==='warning'));
 const valid=fixture();assert.equal(diagnostics(valid).filter(i=>i.severity==='error').length,0);
 delete valid.readableLayout;assert.ok(diagnostics(valid).some(i=>i.code==='READABLE_CONFIG_REQUIRED'));
 delete valid.screens[0].readableLayout;valid.readableLayout={};assert.ok(diagnostics(valid).some(i=>i.code==='READABLE_SCREEN_MISSING'&&i.severity==='warning'));
});

test('grouped actions and compact schedules retain canonical element identity and never mutate authored actions',()=>{
 const design=fixture(),screen=design.screens[0];
 screen.elements.push(element('save','button',{text:'Save',action:'save'}),element('planner','schedule',{scheduleLayout:'board'}));
 screen.readableLayout.actions.push('save');Object.assign(screen.readableLayout,{actionLayout:'grouped',primaryAction:'advance'});
 screen.readableLayout.items.unshift({elementId:'planner',schedulePresentation:'compact'});
 const before=structuredClone(design),view=readableLayoutForScreen(design,'story',{width:390,height:844},{textScale:2});
 assert.equal(view.actionLayout,'grouped');assert.equal(view.primaryAction,'advance');
 assert.deepEqual(view.actions.map(x=>[x.element.id,x.element.action]),[['advance','advance'],['save','save']]);
 assert.equal(view.items[0].schedulePresentation,'compact');assert.equal(view.items[0].element.scheduleLayout,'board');
 assert.deepEqual(design,before);assert.equal(diagnostics(design).filter(x=>x.severity==='error').length,0);
 delete screen.readableLayout.primaryAction;assert.equal(readableLayoutForScreen(design,'story',{width:390,height:844}).primaryAction,undefined);
});

test('primary action roles and compact planning validate real references and scope',()=>{
 const design=fixture(),layout=design.screens[0].readableLayout;
 layout.primaryAction='missing';layout.items[0].schedulePresentation='compact';
 let issues=diagnostics(design);
 for(const code of ['READABLE_PRIMARY_GROUP','READABLE_PRIMARY_REFERENCE','READABLE_SCHEDULE_KIND'])assert.ok(issues.some(x=>x.code===code&&x.severity==='error'),code);
 layout.actionLayout='grouped';layout.primaryAction='advance';delete layout.items[0].schedulePresentation;
 design.screens[0].elements.push(element('planner','schedule'));layout.items.push({elementId:'planner',schedulePresentation:'compact'});
 const ctx=new SemanticContext();checkReadableLayout(ctx,{file:'project.json',kind:'project',data:{visual:design,roster:{}},version:'test',locations:new Map()},design);
 assert.ok(ctx.diagnostics.some(x=>x.code==='READABLE_COMPACT_ROSTER'));
});


test('first-use planning guidance is optional, compact-only and presentation-only',()=>{
 const design=fixture(),screen=design.screens[0];
 screen.elements.push(element('planner','schedule'));
 const item={elementId:'planner',schedulePresentation:'compact'};screen.readableLayout.items.push(item);
 const resolve=()=>readableLayoutForScreen(design,'story',{width:390,height:844}).items.find(x=>x.element.id==='planner');
 assert.equal(resolve().planningGuidance,undefined);
 item.planningGuidance='first-use';const before=structuredClone(design);
 assert.equal(resolve().planningGuidance,'first-use');assert.deepEqual(design,before);
 assert.equal(diagnostics(design).filter(x=>x.severity==='error').length,0);
 delete item.schedulePresentation;
 assert.equal(resolve().planningGuidance,undefined,'Invalid guide settings never create an unexpected overlay');
 assert.ok(diagnostics(design).some(x=>x.code==='READABLE_GUIDANCE_COMPACT'&&x.severity==='error'));
 item.schedulePresentation='compact';item.elementId='dialogue';
 assert.ok(diagnostics(design).some(x=>x.code==='READABLE_GUIDANCE_COMPACT'));
});

test('plan reuse is opt-in, compact-only and rejects roster context without changing authored IDs',()=>{
 const design=fixture(),screen=design.screens[0];screen.elements.push(element('planner','schedule'));const item={elementId:'planner',schedulePresentation:'compact'};screen.readableLayout.items.push(item);
 const resolve=()=>readableLayoutForScreen(design,'story',{width:390,height:844}).items.find(x=>x.element.id==='planner');assert.equal(resolve().planReuse,undefined);
 item.planReuse='last-committed';const before=structuredClone(design);assert.equal(resolve().planReuse,'last-committed');assert.deepEqual(design,before);assert.equal(diagnostics(design).filter(x=>x.severity==='error').length,0);
 delete item.schedulePresentation;assert.equal(resolve().planReuse,undefined);assert.ok(diagnostics(design).some(x=>x.code==='READABLE_PLAN_REUSE_COMPACT'));
 item.schedulePresentation='compact';item.elementId='dialogue';assert.ok(diagnostics(design).some(x=>x.code==='READABLE_PLAN_REUSE_COMPACT'));
 item.elementId='planner';const ctx=new SemanticContext();checkReadableLayout(ctx,{file:'project.json',kind:'project',data:{visual:design,roster:{}},version:'test',locations:new Map()},design);assert.ok(ctx.diagnostics.some(x=>x.code==='READABLE_COMPACT_ROSTER'));
});
