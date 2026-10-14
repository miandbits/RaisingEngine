import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp} from 'node:fs/promises';
import path from 'node:path';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {resolveScreenElements} from '../../dist/packages/visual/components.js';

test('instance quantity stays inherited unless deliberately overridden',()=>{
 const button={id:'purchase',kind:'button',x:0,y:0,width:100,height:40,action:'buy',shopId:'shop',itemId:'tea'};
 const design={width:640,height:360,components:[{id:'card',label:'Card',width:100,height:40,elements:[button]}],screens:[{id:'home',label:'Home',elements:[
  {id:'shared',kind:'component',componentId:'card',x:0,y:0,width:100,height:40},
  {id:'custom',kind:'component',componentId:'card',x:100,y:0,width:100,height:40,overrides:[{elementId:'purchase',values:{quantity:3}}]}
 ]}]};
 const initial=resolveScreenElements(design,'home');
 assert.equal(initial[0].quantity,undefined);assert.equal(initial[1].quantity,3);
 button.quantity=5;
 const changed=resolveScreenElements(design,'home');
 assert.equal(changed[0].quantity,5);assert.equal(changed[1].quantity,3);
 assert.equal(design.screens[0].elements[0].overrides,undefined);
});

test('reusable UI definitions, per-instance overrides and anchors survive save, stale edits and undo',async()=>{
 await mkdir('.cache/components-tests',{recursive:true});
 const root=await mkdtemp(path.resolve('.cache/components-tests/project-'));
 await cp('examples/academy-visual',root,{recursive:true});
 const service=new QtStudioService();await service.handle({method:'open',params:{root}});
 const editor=service.editor,doc=editor.documents.find(d=>d.kind==='project');
 const original=structuredClone(doc.data),data=structuredClone(original),v=data.visual;
 v.viewportMode='responsive';v.components=[{id:'confirm-card',label:'확인창',width:300,height:150,elements:[
  {id:'background',kind:'panel',x:0,y:0,width:300,height:150,fill:'#202020',anchorX:'stretch',anchorY:'stretch'},
  {id:'message',kind:'text',x:16,y:16,width:268,height:48,text:'새 게임을 시작할까요?',anchorX:'stretch'},
  {id:'confirm',kind:'button',x:184,y:90,width:100,height:40,text:'확인',action:'start',anchorX:'right',anchorY:'bottom'}
 ]}];
 for(const id of ['title','ending']) {
  const screen=v.screens.find(s=>s.id===id);
  screen.elements=screen.elements.filter(e=>!(e.kind==='button'&&e.action==='start'));
  screen.elements.push({id:'confirmation',kind:'component',componentId:'confirm-card',x:20,y:20,width:360,height:180,anchorX:'center',anchorY:'center',overrides:id==='ending'?[{elementId:'message',values:{text:'다시 시작할까요?'}}]:[]});
 }
 const put=(value=data,revision=editor.revision)=>editor.edit({type:'replace',file:doc.file,data:structuredClone(value)},revision);
 put();assert.equal((await editor.snapshot(editor.revision)).ok,true,JSON.stringify(editor.diagnostics));
 await editor.save(editor.revision);const stale=editor.revision,saved=structuredClone(v);
 v.components[0].elements[2].text='시작하기';put();
 assert.throws(()=>put(original,stale));
 editor.edit({type:'undo'},editor.revision);
 assert.equal(editor.documents.find(d=>d.kind==='project').data.visual.components[0].elements[2].text,'확인');
 await editor.open(root);assert.deepEqual(editor.documents.find(d=>d.kind==='project').data.visual,saved);
 const valid=structuredClone(editor.documents.find(d=>d.kind==='project').data);
 const diagnose=async(change)=>{const next=structuredClone(valid);change(next.visual);put(next);return (await editor.validate(editor.revision)).view.diagnostics;};
 const expect=async(code,change)=>{const ds=await diagnose(change);assert.ok(ds.some(d=>d.code===code),code+': '+JSON.stringify(ds));return ds;};
 await expect('UI_COMPONENT_DUPLICATE',v=>v.components.push(structuredClone(v.components[0])));
 await expect('UI_ELEMENT_DUPLICATE',v=>v.components[0].elements.push(structuredClone(v.components[0].elements[0])));
 await expect('UI_COMPONENT_REFERENCE',v=>v.screens[0].elements.find(e=>e.kind==='component').componentId='missing');
 await expect('UI_COMPONENT_OVERRIDE_REFERENCE',v=>v.screens[0].elements.find(e=>e.kind==='component').overrides=[{elementId:'missing',values:{text:'x'}}]);
 await expect('UI_COMPONENT_OVERRIDE_DUPLICATE',v=>v.screens[0].elements.find(e=>e.kind==='component').overrides=[{elementId:'message',values:{text:'a'}},{elementId:'message',values:{text:'b'}}]);
 // The reserved expansion separator cannot appear in authored IDs.
 await expect('SCHEMA_INVALID',v=>v.screens[0].elements.push({id:'confirmation__confirm',kind:'text',text:'collision',x:0,y:0,width:100,height:30}));
 const ds=await expect('UI_ACTION_REFERENCE',v=>v.screens[0].elements.find(e=>e.kind==='component').overrides=[{elementId:'confirm',values:{action:'navigate',screenId:'missing-screen'}}]);
 assert.ok(ds.some(d=>d.code==='UI_ACTION_REFERENCE'&&JSON.stringify(d).includes('/overrides/0/values/screenId')));
 await expect('UI_ACTION_REQUIRED',v=>v.screens[0].elements.find(e=>e.kind==='component').overrides=[{elementId:'confirm',values:{action:'buy'}}]);
 await expect('UI_STYLE_REFERENCE',v=>v.screens[0].elements.find(e=>e.kind==='component').overrides=[{elementId:'confirm',values:{styleId:'missing'}}]);
 await expect('UI_BOUNDS',v=>v.components[0].elements[0].width=301);
 await expect('UI_NAVIGATION',v=>v.screens[0].elements.find(e=>e.kind==='component').hidden=true);
 for(const change of [v=>v.components[0].elements[0].kind='component',v=>v.components[0].elements[0].kind='shop',v=>v.components[0].elements[0].anchorX='invalid',v=>v.screens[0].elements.find(e=>e.kind==='component').overrides=[{elementId:'confirm',values:{quantity:0}}]]) {
  const ds=await diagnose(change);assert.ok(ds.some(d=>d.severity==='error'),JSON.stringify(ds));
 }
 put(valid);assert.equal((await editor.snapshot(editor.revision)).ok,true);
});
