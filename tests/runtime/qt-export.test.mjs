import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,writeFile,access,readdir,unlink} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import path from 'node:path';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
const stable=view=>Object.fromEntries(['revision','dirty','undo','redo','draftVersion','diskVersion','documents'].map(key=>[key,view[key]]));
// Draft recovery is intentional local metadata; authored files and other journals stay unchanged.
async function hashes(root){const result={};async function visit(dir){for(const e of await readdir(dir,{withFileTypes:true})){const file=path.join(dir,e.name);if(path.relative(root,file).split(path.sep).join('/')==='.studio/recovery')continue;if(e.isDirectory())await visit(file);else result[path.relative(root,file)]=createHash('sha256').update(await readFile(file)).digest('hex');}}await visit(root);return result;}
// Only successful publication may persist the exact chosen target. Failed
// operations below still compare every local metadata file without exclusions.
async function unchangedAfterExport(root,before,target){const key=path.join('.studio','release','settings.json');assert.deepEqual(JSON.parse(await readFile(path.join(root,key),'utf8')),{version:1,target});const actual=await hashes(root),expected={...before};delete actual[key];delete expected[key];assert.deepEqual(actual,expected);}
async function fixture(){
 await mkdir('.cache/qt-export-tests',{recursive:true});const work=await mkdtemp(path.resolve('.cache/qt-export-tests/run-'));
 const root=path.join(work,'project'),runtime=path.join(work,'runtime');await cp('examples/academy-visual',root,{recursive:true});await mkdir(runtime);
 // Packaging stubs only; never execute these files.
 await writeFile(path.join(runtime,'RaisingPlayer.exe'),'export fixture');await writeFile(path.join(runtime,'index.html'),'export fixture');
 await writeFile(path.join(runtime,'capabilities.json'),JSON.stringify({formatVersion:'1',imageExtensions:['.png'],audioExtensions:[],visualFeatures:[]}));
 const service=new QtStudioService({runtimes:{windows:runtime,web:runtime}});await service.handle({method:'open',params:{root}});
 const change=fn=>{const data=structuredClone(service.editor.documents.find(d=>d.kind==='project').data);fn(data);service.editor.edit({type:'replace',file:'project.json',data},service.editor.revision);};
 return {root,work,service,change};
}

test('Qt export includes the dirty draft in both targets and preserves source disk, revision and undo/redo',async()=>{
 const {root,work,service,change}=await fixture(),editor=service.editor,disk=await hashes(root);
 change(p=>p.title='Unsaved export');change(p=>p.title='Redo remains');editor.edit({type:'undo'},editor.revision);
 const before=stable(editor.view()),snapshot=await editor.snapshot(editor.revision);assert.equal(snapshot.ok,true);assert.equal(before.dirty,true);assert.equal(before.undo,true);assert.equal(before.redo,true);
 for(const target of ['windows','web']){
  const destination=path.join(work,target),reply=await service.handle({method:'export',params:{revision:editor.revision,target,destination}});
  assert.equal(path.resolve(reply.exported.directory),destination);assert.equal(reply.exported.target,target);assert.equal(reply.exported.contentFingerprint,snapshot.snapshot.contentFingerprint);assert.equal(reply.exported.assetCount,snapshot.snapshot.assets.length);
  const bundle=JSON.parse(await readFile(path.join(destination,'game.json'),'utf8'));assert.deepEqual(bundle.content,snapshot.snapshot);assert.equal(bundle.content.project.title,'Unsaved export');
  for(const asset of Object.values(bundle.assets))await access(path.join(destination,asset));
  assert.deepEqual(stable(reply.view),before);await unchangedAfterExport(root,disk,target);
 }
 editor.edit({type:'redo'},editor.revision);assert.equal(editor.documents.find(d=>d.kind==='project').data.title,'Redo remains');editor.edit({type:'undo'},editor.revision);assert.equal(editor.documents.find(d=>d.kind==='project').data.title,'Unsaved export');await unchangedAfterExport(root,disk,'web');
});

test('Qt export rejects invalid targets, relative/existing destinations, stale revisions and invalid content without losing edits',async()=>{
 const {root,work,service,change}=await fixture(),editor=service.editor,disk=await hashes(root),original=stable(editor.view());
 for(const params of [{target:'android',destination:path.join(work,'invalid-target')},{target:'windows',destination:'relative-path'}]){
  await assert.rejects(service.handle({method:'export',params:{revision:editor.revision,...params}}));assert.deepEqual(stable(editor.view()),original);assert.deepEqual(await hashes(root),disk);
 }
 const existing=path.join(work,'existing');await mkdir(existing);await writeFile(path.join(existing,'keep.txt'),'original output');const outputBefore=await hashes(existing);
 await assert.rejects(service.handle({method:'export',params:{revision:editor.revision,target:'windows',destination:existing}}),/이미 있습니다/);assert.deepEqual(await hashes(existing),outputBefore);
 const oldRevision=editor.revision;change(p=>p.title='Current draft');const changed=stable(editor.view()),stalePath=path.join(work,'stale');
 await assert.rejects(service.handle({method:'export',params:{revision:oldRevision,target:'windows',destination:stalePath}}),/갱신/);assert.deepEqual(stable(editor.view()),changed);await assert.rejects(access(stalePath),{code:'ENOENT'});
 change(p=>p.visual.defaultScene.backgroundAssetId='missing-background');const invalid=stable(editor.view()),invalidPath=path.join(work,'invalid-content');
 await assert.rejects(service.handle({method:'export',params:{revision:editor.revision,target:'windows',destination:invalidPath}}),/오류/);assert.deepEqual(stable(editor.view()),invalid);await assert.rejects(access(invalidPath),{code:'ENOENT'});assert.deepEqual(await hashes(root),disk);
});

test('Qt export rejects a changed revision before publication and retries the same destination',async()=>{
 const {root,work,service,change}=await fixture(),editor=service.editor,disk=await hashes(root);change(p=>p.title='Export snapshot');
 const revision=editor.revision,originalCheck=editor.assertDiskUnchanged.bind(editor);let checks=0;
 editor.assertDiskUnchanged=async()=>{if(++checks===2)change(p=>p.title='Concurrent newer draft');await originalCheck();};
 await assert.rejects(service.handle({method:'export',params:{revision,target:'windows',destination:path.join(work,'concurrent')}}),/갱신/);
 assert.equal(checks,2);assert.equal(editor.documents.find(d=>d.kind==='project').data.title,'Concurrent newer draft');assert.equal(editor.view().dirty,true);assert.deepEqual(await hashes(root),disk);
 const destination=path.join(work,'concurrent');await assert.rejects(access(destination),{code:'ENOENT'});
 assert.deepEqual((await readdir(work)).filter(name=>name.startsWith('.raising-export-')),[]);
 editor.assertDiskUnchanged=originalCheck;
 const before=stable(editor.view());
 await service.handle({method:'export',params:{revision:editor.revision,target:'windows',destination}});
 assert.equal(JSON.parse(await readFile(path.join(destination,'game.json'),'utf8')).content.project.title,'Concurrent newer draft');
 assert.deepEqual(stable(editor.view()),before);await unchangedAfterExport(root,disk,'windows');
});

test('Qt export disk conflicts and packaging failures leave no output and permit same-path retry',async()=>{
 for(const target of ['windows','web']){
  const {root,work,service}=await fixture(),editor=service.editor,before=stable(editor.view()),disk=await hashes(root);
  const file=path.join(root,'project.json'),original=await readFile(file),check=editor.assertDiskUnchanged.bind(editor);let checks=0;
  editor.assertDiskUnchanged=async()=>{if(++checks===2)await writeFile(file,Buffer.concat([original,Buffer.from('\n')]));await check();};
  const destination=path.join(work,target),request=()=>service.handle({method:'export',params:{revision:editor.revision,target,destination}});
  await assert.rejects(request());assert.equal(checks,2);await assert.rejects(access(destination),{code:'ENOENT'});
  assert.deepEqual((await readdir(work)).filter(name=>name.startsWith('.raising-export-')),[]);
  assert.deepEqual(stable(editor.view()),before);
  await writeFile(file,original);editor.assertDiskUnchanged=check;
  // A runtime collision fails after copying has begun. Only the owned staging directory is removed.
  const collision=path.join(work,'runtime','game.json');await writeFile(collision,'runtime collision');
  await assert.rejects(request());await assert.rejects(access(destination),{code:'ENOENT'});
  assert.deepEqual((await readdir(work)).filter(name=>name.startsWith('.raising-export-')),[]);
  await unlink(collision);await request();await access(path.join(destination,'game.json'));
  assert.deepEqual(stable(editor.view()),before);await unchangedAfterExport(root,disk,target);
 }
});
