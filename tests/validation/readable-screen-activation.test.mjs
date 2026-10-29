import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,writeFile,stat} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {loadProject,buildSnapshot} from '../../dist/packages/project-service/index.js';
import {validateProject} from '../../dist/packages/validation/index.js';
import {readableLayoutForScreen} from '../../dist/packages/visual/readable-layout.js';
import {requiredPlayerFeatures,missingPlayerFeatures} from '../../dist/packages/export-service/compatibility.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
async function fixture(){await mkdir('.cache/readable-screen-activation',{recursive:true});const base=await mkdtemp(resolve('.cache/readable-screen-activation/core-')),root=join(base,'game');await cp('examples/my-first-raising-game',root,{recursive:true});return {base,root,project:JSON.parse(await readFile(join(root,'project.json'),'utf8'))};}
async function validate(f){await writeFile(join(f.root,'project.json'),JSON.stringify(f.project));return validateProject(await loadProject(f.root));}

test('always reading is per-screen, content-sized, and leaves preferences and authored rectangles unchanged',async()=>{
 const f=await fixture(),design=f.project.visual,title=design.screens.find(s=>s.id==='title'),other=design.screens.find(s=>s.id==='schedule'),desktop={width:1280,height:720},prefs={readingLayout:'auto',textScale:2};
 assert.equal(readableLayoutForScreen(design,title.id,desktop,prefs),null);title.readableLayout.activation='auto';assert.equal(readableLayoutForScreen(design,title.id,desktop,prefs),null);
 title.readableLayout.activation='always';const before=structuredClone(design),saved=structuredClone(prefs);
 for(const viewport of [desktop,{width:2560,height:1440},{width:390,height:844},{width:844,height:390}]){const view=readableLayoutForScreen(design,title.id,viewport,prefs);assert.ok(view);assert.equal(view.textScale,2);assert.deepEqual(view.actions.map(x=>x.element.id),title.readableLayout.actions);assert.deepEqual(view.items.map(x=>x.element.id),title.readableLayout.items.map(x=>x.elementId));}
 assert.equal(readableLayoutForScreen(design,other.id,desktop,prefs),null);assert.ok(readableLayoutForScreen(design,other.id,desktop,{readingLayout:'on'}));assert.ok(readableLayoutForScreen(design,other.id,{width:390,height:844},prefs));
 assert.deepEqual(design,before);assert.deepEqual(prefs,saved);
 for(const viewport of [{width:0,height:720},{width:Infinity,height:720}])assert.equal(readableLayoutForScreen(design,title.id,viewport,prefs),null);
 delete design.readableLayout;assert.equal(readableLayoutForScreen(design,title.id,desktop,prefs),null);
});

test('screen activation accepts only auto/always on configured screens and retains existing semantic checks',async()=>{
 const f=await fixture(),layout=f.project.visual.screens[0].readableLayout;
 for(const value of ['auto','always']){layout.activation=value;assert.equal((await validate(f)).valid,true);}
 for(const value of ['on','off','',false,2,null]){layout.activation=value;const v=await validate(f);assert.ok(v.diagnostics.some(d=>d.code==='SCHEMA_INVALID'&&d.pointer.includes('/activation')),JSON.stringify(v.diagnostics));}
 delete layout.activation;f.project.visual.readableLayout.activation='always';assert.equal((await validate(f)).valid,false);delete f.project.visual.readableLayout.activation;
 layout.activation='always';delete f.project.visual.readableLayout;assert.ok((await validate(f)).diagnostics.some(d=>d.code==='READABLE_CONFIG_REQUIRED'));
});

test('activation capability examines only declared screen settings and never aliases older reading support',async()=>{
 const f=await fixture(),content={project:f.project,activities:[],events:[]};
 assert.ok(!requiredPlayerFeatures(content).includes('readable-screen-activation-v1'));
 for(const activation of ['auto','always']){f.project.visual.screens[0].readableLayout.activation=activation;assert.ok(requiredPlayerFeatures(content).includes('readable-screen-activation-v1'));}
 assert.deepEqual(missingPlayerFeatures(['readable-screen-activation-v1'],['readable-layout-v1','ui-composition-v1']),['readable-screen-activation-v1']);
 delete f.project.visual.screens[0].readableLayout.activation;f.project.visual.screens[0].elements.push({id:'opaque',kind:'plugin',extension:{props:{readableLayout:{activation:'always'}}}});assert.ok(!requiredPlayerFeatures(content).includes('readable-screen-activation-v1'));
});

test('export blocks older runtimes for explicit screen activation before publishing and permits supporting players',async()=>{
 const f=await fixture();f.project.visual.screens[0].readableLayout.activation='always';assert.equal((await validate(f)).valid,true);const built=await buildSnapshot(await loadProject(f.root));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
 const runtime=join(f.base,'runtime');await mkdir(runtime);await writeFile(join(runtime,'index.html'),'fixture');const caps={formatVersion:'1',visualFeatures:requiredPlayerFeatures(built.snapshot).filter(x=>x!=='readable-screen-activation-v1'),imageExtensions:['.png','.jpg','.jpeg','.webp'],audioExtensions:['.wav','.mp3','.ogg'],fontExtensions:['.ttf','.otf']};await writeFile(join(runtime,'capabilities.json'),JSON.stringify(caps));
 const request={content:built.snapshot,projectRoot:f.root,target:'web',destination:join(f.base,'export'),runtimes:{web:runtime}};await assert.rejects(exportGame(request),/readable-screen-activation-v1/);assert.equal(await stat(request.destination).catch(()=>null),null);
 caps.visualFeatures.push('readable-screen-activation-v1');await writeFile(join(runtime,'capabilities.json'),JSON.stringify(caps));await exportGame(request);const bundle=JSON.parse(await readFile(join(request.destination,'game.json'),'utf8'));assert.equal(bundle.content.project.visual.screens[0].readableLayout.activation,'always');
});
