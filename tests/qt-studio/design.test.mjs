import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {resolveElementStyle} from '../../dist/packages/visual/styles.js';

test('project font import, shared styles, stale edits, validation and undo/save roundtrip',async()=>{
 await mkdir('.cache/design-tests',{recursive:true});const root=await mkdtemp(path.resolve('.cache/design-tests/project-'));
 await cp('examples/academy-visual',root,{recursive:true});const service=new QtStudioService();await service.handle({method:'open',params:{root}});
 const revision=service.editor.revision,source=path.resolve('dist/native-player-app/fonts/NotoSansKR.ttf');
 const imported=await service.handle({method:'importFont',params:{file:source,revision}}),id=imported.importedAssetId;
 assert.match(id,/^font-[a-f0-9]{24}$/);assert.deepEqual(await readFile(path.join(root,'assets',id+'.ttf')),await readFile(source));
 await assert.rejects(service.handle({method:'importFont',params:{file:source,revision}}));
 const current=service.editor.revision;await service.handle({method:'importFont',params:{file:source,revision:current}});assert.equal(service.editor.revision,current);
 const editor=service.editor,doc=editor.documents.find(d=>d.kind==='project'),data=structuredClone(doc.data),v=data.visual;
 v.defaultStyle={fontAssetId:id,fontSize:22,color:'#ffffff'};v.styles=[{id:'primary',label:'주요 버튼',fill:'#543c74',fontSize:28,padding:8,hoverFill:'#765b96',pressedFill:'#392850',disabledColor:'#999999'}];
 const buttons=v.screens.flatMap(s=>s.elements).filter(e=>e.kind==='button');assert.ok(buttons.length>1);
 for(const e of buttons){delete e.fontSize;delete e.fill;delete e.color;e.styleId='primary';}
 const before=structuredClone(buttons[0]);const effective=resolveElementStyle(v,buttons[0]);assert.equal(effective.fontSize,28);assert.equal(effective.fontAssetId,id);assert.equal(effective.color,'#ffffff');assert.deepEqual(buttons[0],before);
 assert.equal(resolveElementStyle(v,{...buttons[0],fontSize:32}).fontSize,32);
 const put=()=>editor.edit({type:'replace',file:doc.file,data:structuredClone(data)},editor.revision);
 put();assert.equal((await editor.snapshot(editor.revision)).ok,true,JSON.stringify(editor.diagnostics));await editor.save(editor.revision);
 v.styles[0].fontSize=34;put();assert.equal(resolveElementStyle(editor.documents.find(d=>d.kind==='project').data.visual,buttons[1]).fontSize,34);
 editor.edit({type:'undo'},editor.revision);assert.equal(editor.documents.find(d=>d.kind==='project').data.visual.styles[0].fontSize,28);
 v.styles.push({...v.styles[0]});v.defaultStyle.fontAssetId='visual-courtyard';buttons[0].styleId='missing-style';buttons[1].fontAssetId='missing-font';put();
 const diagnostics=(await editor.validate(editor.revision)).view.diagnostics;
 for(const code of ['UI_STYLE_DUPLICATE','UI_FONT_KIND','UI_STYLE_REFERENCE','REFERENCE_MISSING'])assert.ok(diagnostics.some(d=>d.code===code),code);
 assert.equal((await editor.snapshot(editor.revision)).ok,false);
 await editor.open(root);assert.equal(editor.documents.find(d=>d.kind==='project').data.visual.styles[0].fontSize,28);assert.equal((await editor.snapshot(editor.revision)).ok,true);
 const bad=path.join(root,'broken.ttf');await writeFile(bad,Buffer.alloc(20));await assert.rejects(service.handle({method:'importFont',params:{file:bad,revision:editor.revision}}),/TTF/);
 const damaged=Buffer.from(await readFile(source));damaged.writeUInt32BE(0xffffffff,20);await writeFile(bad,damaged);await assert.rejects(service.handle({method:'importFont',params:{file:bad,revision:editor.revision}}),/범위/);
});
