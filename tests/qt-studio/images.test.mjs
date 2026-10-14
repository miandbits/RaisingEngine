import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,writeFile,readFile} from 'node:fs/promises';
import path from 'node:path';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';

test('PNG import shares revision checks, immutable assets and editor undo/save history',async()=>{
 await mkdir('.cache/image-import-tests',{recursive:true});const root=await mkdtemp(path.resolve('.cache/image-import-tests/project-'));
 await cp('examples/academy-adventure',root,{recursive:true});const service=new QtStudioService();await service.handle({method:'open',params:{root}});
 const source=path.resolve('.cache/pm2-approved/pm2-logo.png'),revision=service.editor.revision;
 const imported=await service.handle({method:'importImage',params:{file:source,revision}});const id=imported.importedAssetId;
 assert.match(id,/^image-[a-f0-9]{24}$/);const file=path.join(root,'assets',id+'.png');assert.deepEqual(await readFile(file),await readFile(source));
 await assert.rejects(service.handle({method:'importImage',params:{file:source,revision}}));
 const current=service.editor.revision;await service.handle({method:'importImage',params:{file:source,revision:current}});assert.equal(service.editor.revision,current,'deduplicate is not an edit');
 await service.handle({method:'edit',params:{revision:current,edit:{type:'undo'}}});assert.ok(!service.editor.documents.find(d=>d.kind==='assets').data.assets.some(a=>a.id===id));
 await service.handle({method:'edit',params:{revision:service.editor.revision,edit:{type:'redo'}}});await service.handle({method:'save',params:{revision:service.editor.revision}});
 assert.ok(JSON.parse(await readFile(path.join(root,'assets/manifest.json'),'utf8')).assets.some(a=>a.id===id));
 const bad=path.join(root,'bad.png');await writeFile(bad,'not png');await assert.rejects(service.handle({method:'importImage',params:{file:bad,revision:service.editor.revision}}),/PNG/);
});
