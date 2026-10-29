import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,writeFile,readFile,stat} from 'node:fs/promises';
import {join,resolve} from 'node:path';
import {sharedWidgetsFixture,sharedWidgetIds} from '../fixtures/shared-widgets.mjs';
import {loadProject} from '../../dist/packages/project-service/index.js';
import {validateProject} from '../../dist/packages/validation/index.js';
import {resolveScreenElements} from '../../dist/packages/visual/components.js';
import {plannerEntryError} from '../../dist/packages/visual/planner-entry.js';
import {requiredPlayerFeatures,missingPlayerFeatures} from '../../dist/packages/export-service/compatibility.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {verifyPresentationVisual} from '../../dist/packages/visual/presentation-update.js';
async function fixture(){await mkdir('.cache/shared-widgets',{recursive:true});const out=await mkdtemp(resolve('.cache/shared-widgets/contract-'));return {out,...await sharedWidgetsFixture(join(out,'project'))};}
async function validate(f,p){await writeFile(join(f.root,'project.json'),JSON.stringify(p));return validateProject(await loadProject(f.root));}

test('nested shared schedule, stats and growth preserve widget properties and resolve absolute planner targets without mutation',async()=>{
 const f=await fixture(),p=f.content.project,v=p.visual,before=structuredClone(v),rows=resolveScreenElements(v,'schedule');
 for(const kind of ['schedule','stats','result'])assert.equal(rows.find(e=>e.id===sharedWidgetIds[kind]).kind,kind);
 const board=rows.find(e=>e.kind==='schedule');for(const key of ['scheduleGroups','showPhaseForecast','showActivityRules','showActivityDescription'])assert.deepEqual(board[key],v.components[0].elements[0][key]);
 assert.equal(rows.find(e=>e.kind==='result').resultLayout,'growth');assert.deepEqual(rows.find(e=>e.kind==='stats').attributeIds,['money','stamina','skill']);
 const entry={type:'openPlanner',screenId:'schedule',plannerElementId:sharedWidgetIds.schedule,plannerGroupId:'all',plannerActivityId:'study'},state={phase:'planning',periodIndex:0};
 assert.equal(plannerEntryError(v,state,f.content.activities,entry),null);
 assert.ok(plannerEntryError(v,state,f.content.activities,{...entry,plannerElementId:'schedule'}),'A definition-local name is not a global alias');
 const hidden=structuredClone(v);hidden.screens.find(s=>s.id==='schedule').elements[0].hidden=true;assert.ok(plannerEntryError(hidden,state,f.content.activities,entry));
 const disabled=structuredClone(v);disabled.components[1].elements[0].enabledCondition={op:'not',condition:{op:'always'}};assert.ok(plannerEntryError(disabled,state,f.content.activities,entry,()=>false));
 assert.deepEqual(v,before);assert.equal((await validate(f,p)).valid,true);
});

test('whole-game widgets are rejected in repeated collection rows, including nested definitions',async()=>{
 const f=await fixture(),p=structuredClone(f.content.project);
 p.visual.dataTables=[{id:'rows',name:'Rows',columns:[{id:'name',type:'text',name:'Name'}],rows:[{id:'one',values:{name:'One'}}]}];
 p.visual.screens.find(s=>s.id==='schedule').elements.push({id:'repeated',kind:'collection',componentId:'home-panel',x:0,y:0,width:300,height:300,source:{type:'table',tableId:'rows'}});
 const result=await validate(f,p);assert.ok(result.diagnostics.some(d=>d.code==='UI_COLLECTION_SHARED_WIDGET'),JSON.stringify(result.diagnostics));
});

test('shared widgets require a distinct runtime capability and export refuses older runtimes before writing output',async()=>{
 const f=await fixture(),features=requiredPlayerFeatures(f.content);assert.ok(features.includes('component-widgets-v1'));
 assert.ok(features.includes('nested-components-v1'));assert.ok(!features.includes('ui-composition-v1'));
 assert.deepEqual(missingPlayerFeatures(['nested-components-v1'],['components-v1']),['nested-components-v1']);
 assert.deepEqual(missingPlayerFeatures(['nested-components-v1'],['ui-composition-v1']),[],'Existing native composition runtimes already expand nested definitions');
 const broader=structuredClone(f.content);broader.project.visual.components[0].elements[0].tooltip='Help';assert.ok(requiredPlayerFeatures(broader).includes('ui-composition-v1'),'Nesting support does not promise tooltips or arbitrary modals');
 assert.deepEqual(missingPlayerFeatures(['component-widgets-v1'],['components-v1','activity-board-v1','activity-growth-v1']),['component-widgets-v1']);
 const runtime=join(f.out,'runtime');await mkdir(runtime);await writeFile(join(runtime,'index.html'),'fixture');
 const caps={formatVersion:'1',visualFeatures:features.filter(x=>x!=='component-widgets-v1'),imageExtensions:['.png','.jpg','.jpeg','.webp'],audioExtensions:['.wav','.mp3','.ogg'],fontExtensions:['.ttf','.otf']};await writeFile(join(runtime,'capabilities.json'),JSON.stringify(caps));
 const request={content:f.content,projectRoot:f.root,target:'web',destination:join(f.out,'export'),runtimes:{web:runtime}};
 await assert.rejects(exportGame(request),/component-widgets-v1/);assert.equal(await stat(request.destination).catch(()=>null),null);
 caps.visualFeatures.push('component-widgets-v1');caps.visualFeatures=caps.visualFeatures.filter(x=>x!=='nested-components-v1');await writeFile(join(runtime,'capabilities.json'),JSON.stringify(caps));
 await assert.rejects(exportGame(request),/nested-components-v1/);assert.equal(await stat(request.destination).catch(()=>null),null);
 caps.visualFeatures.push('nested-components-v1');await writeFile(join(runtime,'capabilities.json'),JSON.stringify(caps));await exportGame(request);
 const bundle=JSON.parse(await readFile(join(request.destination,'game.json')));assert.deepEqual(bundle.content.project.visual.components,f.content.project.visual.components);
 const plain=structuredClone(f.content);plain.project.visual.components=[];plain.project.visual.screens=[];assert.ok(!requiredPlayerFeatures(plain).includes('component-widgets-v1'));
});

test('shared widget paint remains presentation-only while structural and gameplay target changes require normal compatibility review',async()=>{
 const {content}=await fixture(),base=content.project.visual,next=structuredClone(base);
 next.components[0].elements[0].x+=12;next.components[0].elements[0].color='#123456';verifyPresentationVisual(base,next);
 for(const change of [v=>{v.components[0].elements[0].scheduleGroups[0].activityIds=['rest'];},v=>{v.components[0].elements[1].attributeIds=['money'];},v=>{v.components[0].elements[0].id='renamed';},v=>{v.components[1].elements[0].componentId='other';}]){const changed=structuredClone(base);change(changed);assert.throws(()=>verifyPresentationVisual(base,changed),/PRESENTATION_CONTENT_CHANGED/);}
});
