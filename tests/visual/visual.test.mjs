import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdtemp,cp,mkdir } from 'node:fs/promises';
import { resolve } from 'node:path';
import { StudioEditor } from '../../dist/packages/studio-service/editor.js';
import { createVisual } from '../../dist/packages/visual/model.js';
import { createRun,dispatchCommand } from '../../dist/packages/core/index.js';
import { createSession } from '../../dist/packages/runtime-service/session.js';
import { saveRun,loadRun } from '../../dist/packages/runtime-service/save.js';
import { McpWorkspace } from '../../dist/packages/mcp-service/workspace.js';
const base=resolve('.cache/visual-contracts');
async function fixture(source='examples/academy-demo'){await mkdir(base,{recursive:true});const root=await mkdtemp(`${base}/project-`);await cp(source,root,{recursive:true});const editor=new StudioEditor();await editor.open(root);return {root,editor};}
function put(editor,visual){const doc=editor.documents.find(d=>d.kind==='project');return editor.edit({type:'replace',file:doc.file,data:{...doc.data,visual}},editor.revision);}
test('calendar groups and compact stats validate references and roundtrip through editor history',async()=>{
 const {editor}=await fixture();const v=createVisual();const schedule=v.screens.flatMap(s=>s.elements).find(e=>e.kind==='schedule');
 const activity=editor.documents.find(d=>d.kind==='activity').data.id;
 schedule.scheduleLayout='menu';schedule.scheduleGroups=[{id:'study',label:'교육',activityIds:[activity]}];
 v.widgetTheme='ornate';v.confirmPurchases=true;
 put(editor,v);assert.equal((await editor.snapshot(editor.revision)).ok,true);await editor.save(editor.revision);
 schedule.scheduleGroups.push({id:'study',label:'중복 분류',activityIds:['missing-activity']});schedule.entryFrameAssetId='missing-frame';
 const stats=v.screens.flatMap(s=>s.elements).find(e=>e.kind==='stats');stats.statsLayout='compact';stats.attributeIds=['missing-stat'];
 put(editor,v);const checked=await editor.validate(editor.revision);const d=checked.view.diagnostics;
 assert.ok(d.some(x=>x.code==='UI_SCHEDULE_GROUP_DUPLICATE'));
 for(const field of ['activityIds/0','entryFrameAssetId','attributeIds/0'])assert.ok(d.some(x=>x.pointer.endsWith(field)),field);
 assert.equal((await editor.snapshot(editor.revision)).ok,false);
 editor.edit({type:'undo'},editor.revision);assert.equal((await editor.snapshot(editor.revision)).ok,true);
 await editor.open(editor.root);assert.equal(editor.documents.find(d=>d.kind==='project').data.visual.widgetTheme,'ornate');
});
test('visual design optional compatibility, roundtrip and designer history',async()=>{
 const {editor}=await fixture();assert.equal((await editor.snapshot(editor.revision)).ok,true);
 put(editor,createVisual());assert.equal((await editor.snapshot(editor.revision)).ok,true);await editor.save(editor.revision);
 const visual=structuredClone(editor.documents.find(d=>d.kind==='project').data.visual);visual.screens[1].elements.find(e=>e.id==='stats').x=60;put(editor,visual);editor.edit({type:'undo'},editor.revision);assert.equal(editor.documents.find(d=>d.kind==='project').data.visual.screens[1].elements.find(e=>e.id==='stats').x,42);editor.edit({type:'redo'},editor.revision);await editor.save(editor.revision);await editor.open(editor.root);assert.equal(editor.documents.find(d=>d.kind==='project').data.visual.screens[1].elements.find(e=>e.id==='stats').x,60);
});
test('visual bounds, duplicate IDs, missing assets and unreachable UI navigation produce precise diagnostics',async()=>{
 const {editor}=await fixture();const v=createVisual();v.screens[1].elements.find(e=>e.id==='stats').x=1270;v.screens[0].elements.push({...v.screens[0].elements[0]});v.screens[2].elements=v.screens[2].elements.filter(e=>e.kind!=='choices');v.screens[0].elements.push({id:'bad-image',kind:'image',x:0,y:0,width:100,height:100,imageSource:'asset',imageAssetId:'missing-image'});put(editor,v);const r=await editor.validate(editor.revision);
 for(const code of ['UI_BOUNDS','UI_ELEMENT_DUPLICATE','UI_NAVIGATION','REFERENCE_MISSING'])assert.ok(r.view.diagnostics.some(d=>d.code===code),code);
 assert.ok(r.view.diagnostics.every(d=>d.file==='project.json'&&d.pointer.startsWith('/visual/')));assert.equal((await editor.snapshot(editor.revision)).ok,false);
});
test('screen identities, color values and button actions cannot bypass data contracts',async()=>{
 const {editor}=await fixture();const v=createVisual();v.screens[0].elements.find(e=>e.kind==='button').action='javascript:alert(1)';put(editor,v);const invalid=await editor.validate(editor.revision);assert.ok(invalid.view.diagnostics.some(d=>d.pointer.endsWith('/action')));assert.equal((await editor.snapshot(editor.revision)).ok,false);
 const dup=createVisual();dup.screens[1].id='title';put(editor,dup);const checked=await editor.validate(editor.revision);assert.ok(checked.view.diagnostics.some(d=>d.code==='UI_SCREEN_DUPLICATE'));assert.ok(checked.view.diagnostics.some(d=>d.code==='UI_SCREEN_MISSING'));
});
test('generated assets and initial scene survive core determinism and save/load',async()=>{
 const {root,editor}=await fixture('examples/academy-visual');const built=await editor.snapshot(editor.revision);assert.equal(built.ok,true,JSON.stringify(editor.diagnostics));const content=built.snapshot;
 const first=createRun(content,42,{mode:'full'});const second=createRun(content,42,{mode:'full'});assert.equal(first.ok,true);assert.deepEqual(first,second);assert.equal(first.state.presentation.backgroundAssetId,'visual-courtyard');assert.equal(first.state.presentation.portraits[0].characterId,'apprentice');assert.equal(Object.keys(content.assetHashes).length,4);
 const a=dispatchCommand(content,first.state,{type:'setSchedule',activityIds:['rest','rest','rest']},first.state.stateRevision);const b=dispatchCommand(content,second.state,{type:'setSchedule',activityIds:['rest','rest','rest']},second.state.stateRevision);assert.deepEqual(a,b);
 const created=createSession(content,42);assert.equal(created.ok,true);const file=resolve(root,'visual-save.json');assert.equal((await saveRun(file,content,created.session)).ok,true);const loaded=await loadRun(file,content);assert.equal(loaded.ok,true);assert.deepEqual(loaded.session.state,created.session.state);
});
test('MCP edits the same visual data and rejects stale versions without losing designer drafts',async()=>{
 const {root,editor}=await fixture('examples/academy-visual');const workspace=new McpWorkspace(root);const baseline=await workspace.readProject();
 const doc=editor.documents.find(d=>d.kind==='project');const machine=structuredClone(doc.data);machine.visual.screens[0].elements.find(e=>e.id==='start').text='새로운 여정';
 const human=structuredClone(doc.data.visual);human.screens[1].elements.find(e=>e.id==='stats').x=55;put(editor,human);
 const saved=await workspace.apply(baseline.version,[{type:'replace',file:doc.file,data:machine}],'Visual button wording');assert.equal(saved.ok,true);
 await assert.rejects(()=>editor.save(editor.revision));assert.equal(editor.documents.find(d=>d.kind==='project').data.visual.screens[1].elements.find(e=>e.id==='stats').x,55);
 await assert.rejects(()=>workspace.apply(baseline.version,[{type:'replace',file:doc.file,data:machine}],'Stale visual edit'));
 const current=new StudioEditor();await current.open(root);assert.equal(current.documents.find(d=>d.kind==='project').data.visual.screens[0].elements.find(e=>e.id==='start').text,'새로운 여정');
});
