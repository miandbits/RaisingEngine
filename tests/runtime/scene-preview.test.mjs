import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,writeFile,access,readdir} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import path from 'node:path';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';

const features=['renderer-preview-v1','scene-preview-v1','scene-portrait-transform-v1'];
const scene={backgroundAssetId:'visual-library',portraits:[{characterId:'mentor',portraitId:'visual-neutral',position:'left',scale:.625123456,offsetX:-.2,offsetY:.1}]};
// Draft recovery is intentional local metadata; authored files and other journals stay unchanged.
async function hashes(root){
 const result={};
 async function scan(directory){for(const entry of await readdir(directory,{withFileTypes:true})){const file=path.join(directory,entry.name);if(path.relative(root,file).split(path.sep).join('/')==='.studio/recovery')continue;if(entry.isDirectory())await scan(file);else result[path.relative(root,file)]=createHash('sha256').update(await readFile(file)).digest('hex');}}
 await scan(root);return result;
}
async function fixture(capabilities=features){
 await mkdir('.cache/scene-preview-tests',{recursive:true});const work=await mkdtemp(path.resolve('.cache/scene-preview-tests/run-'));
 const root=path.join(work,'project'),runtime=path.join(work,'runtime');await cp('examples/academy-visual',root,{recursive:true});await mkdir(runtime);
 // Never launched: packaging/capability test stub only.
 await writeFile(path.join(runtime,'RaisingPlayer.exe'),'scene-preview service contract fixture');
 await writeFile(path.join(runtime,'capabilities.json'),JSON.stringify({formatVersion:'1',imageExtensions:['.png'],audioExtensions:[],visualFeatures:capabilities}));
 const service=new QtStudioService({nativeTarget:'windows',runtimes:{windows:runtime,web:runtime}});await service.handle({method:'open',params:{root}});
 const replaceTitle=title=>{const data=structuredClone(service.editor.documents.find(d=>d.kind==='project').data);data.title=title;service.editor.edit({type:'replace',file:'project.json',data},service.editor.revision);};
 return {root,runtime,service,replaceTitle};
}
const stable=view=>Object.fromEntries(['revision','dirty','undo','redo','draftVersion','diskVersion','documents'].map(key=>[key,view[key]]));

test('unsaved scene transforms require the transform capability even when the project has none',async()=>{
 const legacyFeatures=['renderer-preview-v1','scene-preview-v1'];
 const {root,runtime,service,replaceTitle}=await fixture(legacyFeatures),editor=service.editor,disk=await hashes(root);
 replaceTitle('Unsaved scene capability test');const before=stable(editor.view());
 const legacyScene={portraits:[{characterId:'mentor',portraitId:'visual-neutral',position:'left'}]};
 const preview=scene=>service.handle({method:'preview',params:{revision:editor.revision,scene}});
 assert.ok((await preview(legacyScene)).play.scene);
 for(const [key,value] of [['scale',.2],['offsetX',.1],['offsetY',-.3],['scale',1]]){
  const draft=structuredClone(legacyScene);draft.portraits[0][key]=value;
  await assert.rejects(preview(draft),/scene-portrait-transform-v1/);
  assert.deepEqual(stable(editor.view()),before);assert.deepEqual(await hashes(root),disk);
 }
 const capabilityFile=path.join(runtime,'capabilities.json'),caps=JSON.parse(await readFile(capabilityFile,'utf8'));
 caps.visualFeatures.push('scene-portrait-transform-v1');await writeFile(capabilityFile,JSON.stringify(caps));
 const result=await preview(scene);assert.deepEqual(JSON.parse(await readFile(result.play.scene,'utf8')),scene);
 assert.deepEqual(stable(editor.view()),before);assert.deepEqual(await hashes(root),disk);
});

test('scene preview exports an unchanged draft with a separate scene file and preserves project, history and disk',async()=>{
 const {root,service,replaceTitle}=await fixture(),disk=await hashes(root),editor=service.editor;
 replaceTitle('Scene preview draft');replaceTitle('Redo survives scene preview');editor.edit({type:'undo'},editor.revision);
 const before=stable(editor.view()),snapshot=await editor.snapshot(editor.revision);assert.equal(snapshot.ok,true);
 assert.equal(before.dirty,true);assert.equal(before.undo,true);assert.equal(before.redo,true);
 const requestScene=structuredClone(scene),result=await service.handle({method:'preview',params:{revision:editor.revision,scene:requestScene}});
 assert.deepEqual(stable(result.view),before);assert.deepEqual(await hashes(root),disk);assert.deepEqual(requestScene,scene);
 assert.equal(result.play.preview,true);assert.equal(result.play.gameplayValidated,false);assert.equal(result.play.previewSource,'initial-state');
 assert.equal(result.play.fingerprint,snapshot.snapshot.contentFingerprint);
 assert.equal(path.dirname(result.play.scene),path.dirname(result.play.directory));assert.equal(path.resolve(result.play.scene).startsWith(path.resolve(root)+path.sep),false);
 assert.deepEqual(JSON.parse(await readFile(result.play.scene,'utf8')),scene);
 const bundle=JSON.parse(await readFile(path.join(result.play.directory,'game.json'),'utf8'));assert.deepEqual(bundle.content,snapshot.snapshot);
 assert.notDeepEqual(bundle.content.project.visual.defaultScene,scene);await assert.rejects(access(result.play.save),{code:'ENOENT'});
 const clearedScene={backgroundAssetId:null,portraits:[]},cleared=await service.handle({method:'preview',params:{revision:editor.revision,scene:clearedScene}});
 assert.deepEqual(JSON.parse(await readFile(cleared.play.scene,'utf8')),clearedScene);assert.deepEqual(stable(cleared.view),before);
 editor.edit({type:'redo'},editor.revision);assert.equal(editor.documents.find(d=>d.kind==='project').data.title,'Redo survives scene preview');
 editor.edit({type:'undo'},editor.revision);assert.equal(editor.documents.find(d=>d.kind==='project').data.title,'Scene preview draft');
 assert.deepEqual(await hashes(root),disk);
});

test('scene preview rejects malformed scenes and missing references without changing clean editor state',async()=>{
 const {root,service}=await fixture(),editor=service.editor,disk=await hashes(root),before=stable(editor.view());assert.equal(before.dirty,false);
 for(const invalid of [null,{}, {backgroundAssetId:'missing-image'}, {portraits:[{characterId:'missing-character',portraitId:'visual-neutral',position:'center'}]}, {portraits:[{characterId:'mentor',portraitId:'missing-portrait',position:'center'}]}, {portraits:[{...scene.portraits[0],scale:4.1}]}, {backgroundAssetId:'visual-library',unexpected:true}]){
  await assert.rejects(service.handle({method:'preview',params:{revision:editor.revision,scene:invalid}}),/장면 미리보기 오류/);
  assert.deepEqual(stable(editor.view()),before);assert.deepEqual(await hashes(root),disk);
 }
});

test('scene preview rejects old capabilities and stale revisions while ordinary renderer preview remains available',async()=>{
 const {root,service,replaceTitle}=await fixture(['renderer-preview-v1']),editor=service.editor,disk=await hashes(root);
 const revision=editor.revision,before=stable(editor.view());await assert.rejects(service.handle({method:'preview',params:{revision,scene}}),/장면 미리보기/);
 assert.deepEqual(stable(editor.view()),before);assert.deepEqual(await hashes(root),disk);
 const ordinary=await service.handle({method:'preview',params:{revision}});assert.equal(ordinary.play.preview,true);assert.equal(Object.hasOwn(ordinary.play,'scene'),false);
 replaceTitle('Keep current draft');const edited=stable(editor.view());await assert.rejects(service.handle({method:'preview',params:{revision,scene}}),/갱신/);
 assert.deepEqual(stable(editor.view()),edited);assert.deepEqual(await hashes(root),disk);
});
