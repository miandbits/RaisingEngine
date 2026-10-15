import test from 'node:test';
import assert from 'node:assert/strict';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {resolveScreenElements,flowChildren} from '../../dist/packages/visual/components.js';
import {collectionRows,resolveCollectionRow} from '../../dist/packages/visual/data-ui.js';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';

test('nested component paths preserve repeated instances, shared edits and reject cycles',()=>{
 const visual={width:1280,height:720,components:[{id:'inner',width:200,height:100,elements:[{id:'label',kind:'text',text:'Shared',x:0,y:0,width:180,height:30}]},{id:'outer',width:200,height:100,elements:[{id:'nested',kind:'component',componentId:'inner',x:10,y:20,width:200,height:100}]}],screens:[{id:'a',elements:[{id:'one',kind:'component',componentId:'outer',x:50,y:50,width:200,height:100},{id:'two',kind:'component',componentId:'outer',x:500,y:50,width:200,height:100}]}]};
 let rows=resolveScreenElements(visual,'a');assert.deepEqual(rows.map(e=>e.id),['one__nested__label','two__nested__label']);assert.deepEqual(rows.map(e=>e.x),[60,510]);visual.components[0].elements[0].text='Updated';assert.ok(resolveScreenElements(visual,'a').every(e=>e.text==='Updated'));
 visual.components[0].elements=[{id:'back',kind:'component',componentId:'outer',x:0,y:0,width:200,height:100}];assert.throws(()=>resolveScreenElements(visual,'a'),/cycle/);
});
test('flow layout uses stable sibling order, responsive widths and does not mutate source',()=>{
 const source=[{id:'a',kind:'button',x:99,y:99,width:100,height:40},{id:'b',kind:'button',x:99,y:99,width:100,height:60},{id:'c',kind:'button',x:99,y:99,width:100,height:20}],before=structuredClone(source);
 const parent={id:'group',kind:'group',x:0,y:0,width:400,height:200,flowLayout:{direction:'column',gap:10,padding:20}};
 let rows=flowChildren(source,parent);assert.deepEqual(rows.map(e=>[e.x,e.y,e.width]),[[20,20,360],[20,70,360],[20,140,360]]);
 parent.flowLayout.direction='grid';parent.flowLayout.columns=2;rows=flowChildren(source,parent);assert.deepEqual(rows.map(e=>[e.x,e.y,e.width]),[[20,20,175],[205,20,175],[20,90,175]]);assert.deepEqual(source,before);
});
test('collection filtering and stable sorting bind into nested component leaves',()=>{
 const design={width:1280,height:720,screens:[],dataTables:[{id:'items',columns:[],rows:[{id:'a',values:{name:'A',score:2,show:true}},{id:'b',values:{name:'B',score:1,show:true}},{id:'c',values:{name:'C',score:1,show:true}},{id:'d',values:{name:'D',score:0,show:false}}]}],components:[{id:'inner',width:200,height:100,elements:[{id:'title',kind:'text',x:0,y:0,width:100,height:30,text:'?'}]},{id:'outer',width:200,height:100,elements:[{id:'nested',kind:'component',componentId:'inner',x:0,y:0,width:200,height:100}]}]};
 const collection={id:'list',kind:'collection',x:0,y:0,width:300,height:100,componentId:'outer',source:{type:'table',tableId:'items'},rowFilter:{field:'show',operator:'eq',value:true},sort:{field:'score',direction:'asc'},bindings:[{elementId:'nested__title',property:'text',field:'name'}]};
 const rows=collectionRows(design,collection,{});assert.deepEqual(rows.map(r=>r.id),['b','c','a']);assert.equal(resolveCollectionRow(design,collection,rows[0],300)[0].text,'B');assert.equal(resolveCollectionRow(design,collection,rows[0],300)[0].id,'list__b__nested__title');
});
test('old visual sample still validates; invalid nested/modal/selection drafts produce located diagnostics',async()=>{
 const root=new URL('../../projects/data-ui-lab/',import.meta.url).pathname.replace(/^\/(\w:)/,'$1');assert.equal((await buildSnapshot(await loadProject(root))).ok,true);
 const service=new QtStudioService();await service.handle({method:'open',params:{root}});const editor=service.editor,doc=editor.documents.find(d=>d.kind==='project'),data=structuredClone(doc.data);
 data.visual.components[0].elements.push({id:'cycle',kind:'component',componentId:data.visual.components[0].id,x:0,y:0,width:100,height:100});
 data.visual.screens.find(s=>s.id==='home').elements.push({id:'invalid-modal',kind:'button',action:'openModal',screenId:'home',x:0,y:0,width:100,height:40},{id:'invalid-selection',kind:'text',binding:'selected-field',selectionKey:'absent',selectionField:'name',x:0,y:0,width:100,height:40});
 editor.edit({type:'replace',file:doc.file,data},editor.revision);const result=await editor.snapshot(editor.revision);assert.equal(result.ok,false);const codes=result.diagnostics.map(d=>d.code);assert.ok(codes.includes('UI_COMPONENT_CYCLE'));assert.ok(codes.includes('UI_MODAL_REFERENCE'));assert.ok(codes.includes('UI_SELECTION_REFERENCE'));
});
