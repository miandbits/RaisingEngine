import test from 'node:test';import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile,readdir,stat} from 'node:fs/promises';import {resolve,join,basename} from 'node:path';import {createHash} from 'node:crypto';
import {surfaceFixture,nineSurface} from '../fixtures/surfaces.mjs';
import {loadProject} from '../../dist/packages/project-service/index.js';import {validateProject} from '../../dist/packages/validation/index.js';
import {surfaceReferences,surfaceInsetsProblem} from '../../dist/packages/visual/surfaces.js';import {resolveElementStyle} from '../../dist/packages/visual/styles.js';import {resolveScreenElements} from '../../dist/packages/visual/components.js';
import {validatedPngDimensions,encodeRgbaPng,decodePng,PNG_LIMITS,PNG_DIMENSION_CACHE_LIMIT} from '../../dist/packages/media/png.js';import {decodePng as legacyDecode} from '../../dist/packages/studio-service/png.js';
import {requiredPlayerFeatures} from '../../dist/packages/export-service/compatibility.js';import {exportGame} from '../../dist/packages/export-service/export.js';
const sha=bytes=>createHash('sha256').update(bytes).digest('hex');
async function fixture(){await mkdir('.cache/ui-surfaces',{recursive:true});return surfaceFixture(await mkdtemp(resolve('.cache/ui-surfaces/contract-')));}
const png=(width,height,value=100)=>encodeRgbaPng({width,height,pixels:Buffer.alloc(width*height*4,value)});
const project=async f=>JSON.parse(await readFile(join(f.root,'project.json'),'utf8'));
async function save(f,p){await writeFile(join(f.root,'project.json'),JSON.stringify(p));return validateProject(await loadProject(f.root));}

test('surface contract covers every declared root and resolves local, named, default and explicit none without aliasing',async()=>{
 const f=await fixture(),p=await project(f),d=p.visual;d.defaultStyle.surface=nineSurface;d.styles.push({id:'unused',label:'Unused',surface:{mode:'tile',imageAssetId:'surface-tile'}});d.screens[0].elements[0].hidden=true;
 const validation=await save(f,p);assert.equal(validation.valid,true,JSON.stringify(validation.diagnostics));const refs=surfaceReferences(d);for(const root of ['/defaultStyle/surface','/readableLayout/surface','/styles/2/surface','/components/0/elements/0/surface','/screens/0/elements/6/overrides/0/values/surface','/screens/0/elements/8/variants/0/values/surface'])assert.ok(refs.some(r=>r.pointer==='/visual'+root),root);
 const leaf={id:'x',kind:'panel',x:0,y:0,width:20,height:20};const inherited=resolveElementStyle(d,leaf);assert.deepEqual(inherited.surface,nineSurface);inherited.surface.insets.left=0;assert.equal(d.defaultStyle.surface.insets.left,12);
 assert.equal(resolveElementStyle(d,{...leaf,styleId:'paper'}).surface.mode,'tile');assert.deepEqual(resolveElementStyle(d,{...leaf,styleId:'frame',surface:{mode:'none'}}).surface,{mode:'none'});
 const expanded=resolveScreenElements(d,'title');assert.equal(expanded.find(e=>e.id==='component-tile__panel').surface.mode,'tile');assert.equal(expanded.find(e=>e.id==='component-none__panel').surface.mode,'none');assert.ok(requiredPlayerFeatures({...f.content,project:p}).includes('ui-surfaces-v1'));
});
test('surface schema rejects unsupported modes, incomplete cuts, atlas data and invalid integers',async()=>{
 const f=await fixture(),p=await project(f);for(const surface of [{mode:'stretch',imageAssetId:'surface-nine'},{mode:'tile'},{mode:'none',imageAssetId:'surface-nine'},{mode:'tile',imageAssetId:'surface-nine',insets:{top:0,right:0,bottom:0,left:0}},{...nineSurface,insets:{...nineSurface.insets,left:-1}},{...nineSurface,insets:{...nineSurface.insets,left:0.5}},{...nineSurface,insets:{top:0,right:0,bottom:0}},{...nineSurface,atlasRegion:'frame'}]){p.visual.defaultStyle.surface=surface;const r=await save(f,p);assert.equal(r.valid,false,JSON.stringify(surface));assert.ok(r.diagnostics.some(d=>d.code==='SCHEMA_INVALID'&&d.pointer.includes('/surface')),JSON.stringify(r.diagnostics));}
});
test('missing assets are validated even in hidden, unused, overridden and conditional surface roots',async()=>{
 const f=await fixture(),p=await project(f);for(const ref of surfaceReferences(p.visual))if(ref.surface.mode!=='none')ref.surface.imageAssetId='missing-surface';p.visual.defaultStyle.surface={mode:'tile',imageAssetId:'missing-default'};p.visual.screens[0].elements[0].hidden=true;
 const r=await save(f,p);assert.equal(r.valid,false);for(const path of ['/visual/defaultStyle/surface/imageAssetId','/visual/readableLayout/surface/imageAssetId','/visual/styles/0/surface/imageAssetId','/visual/components/0/elements/0/surface/imageAssetId','/visual/screens/0/elements/6/overrides/0/values/surface/imageAssetId','/visual/screens/0/elements/8/variants/0/values/surface/imageAssetId'])assert.ok(r.diagnostics.some(d=>d.pointer===path),path);
});
test('full PNG validation rejects malformed bytes, non-PNG extension and cuts without positive center; changed same-path pixels invalidate dimensions',async()=>{
 const f=await fixture(),p=await project(f),file=join(f.root,'assets/surface-nine.png');p.visual.defaultStyle.surface={...nineSurface,insets:{top:12,right:18,bottom:12,left:18}};let r=await save(f,p);assert.ok(r.diagnostics.some(d=>d.code==='UI_SURFACE_INSETS'));
 p.visual.defaultStyle.surface=nineSurface;await writeFile(file,png(20,20));r=await save(f,p);assert.ok(r.diagnostics.some(d=>d.code==='UI_SURFACE_INSETS'),'Same asset path and ID must not retain old36x36 dimensions');
 await writeFile(file,f.textures.nine.subarray(0,45));r=await save(f,p);assert.ok(r.diagnostics.some(d=>d.code==='UI_SURFACE_PNG_INVALID'),'Valid signature/header alone does not prove the PNG can decode');
 await writeFile(file,f.textures.nine);const manifest=JSON.parse(await readFile(join(f.root,'assets/manifest.json'),'utf8'));manifest.assets.find(a=>a.id==='surface-nine').path='assets/disguised.jpg';await writeFile(join(f.root,'assets/disguised.jpg'),f.textures.nine);await writeFile(join(f.root,'assets/manifest.json'),JSON.stringify(manifest));r=await save(f,p);assert.ok(r.diagnostics.some(d=>d.code==='UI_SURFACE_PNG_REQUIRED'));
 assert.ok(surfaceInsetsProblem({...nineSurface,insets:{top:0,right:36,bottom:0,left:0}},{width:36,height:36}));assert.equal(surfaceInsetsProblem({...nineSurface,insets:{top:0,right:35,bottom:0,left:0}},{width:36,height:36}),undefined);
});
test('PNG dimensions cache uses immutable byte hashes, bounded LRU and the full legacy decoder contract',()=>{
 const bytes=png(8,6),first=validatedPngDimensions(bytes);assert.deepEqual(first,{width:8,height:6});assert.equal(validatedPngDimensions(Buffer.from(bytes)),first);assert.ok(Object.isFrozen(first));assert.deepEqual(decodePng(bytes),legacyDecode(bytes));assert.throws(()=>{first.width=100});
 assert.deepEqual(validatedPngDimensions(png(4,6)),{width:4,height:6});const corrupt=Buffer.from(bytes);corrupt[45]^=1;assert.throws(()=>validatedPngDimensions(corrupt));assert.throws(()=>validatedPngDimensions(bytes.subarray(0,45)));assert.throws(()=>validatedPngDimensions(Buffer.alloc(PNG_LIMITS.maxBytes+1)));assert.throws(()=>encodeRgbaPng({width:8193,height:1,pixels:Buffer.alloc(0)}));
 for(let i=0;i<=PNG_DIMENSION_CACHE_LIMIT;i++)validatedPngDimensions(png(1,1,i));assert.notEqual(validatedPngDimensions(bytes),first,'Old metadata is evicted after the bounded cache fills');
});
test('export requires surface capability and revalidates immutable PNG bytes and insets before atomic publication',async()=>{
 const f=await fixture(),base=join(f.root,'..'),runtime=await mkdtemp(join(base,'runtime-'));await writeFile(join(runtime,'index.html'),'fixture');const caps={formatVersion:'1',visualFeatures:requiredPlayerFeatures(f.content).filter(x=>x!=='ui-surfaces-v1'),imageExtensions:['.png','.jpg','.jpeg','.webp'],audioExtensions:['.wav','.ogg','.mp3'],fontExtensions:['.ttf','.otf']};await writeFile(join(runtime,'capabilities.json'),JSON.stringify(caps));let index=0;const run=content=>{const destination=join(base,'export-'+basename(f.root)+'-'+index++);return {destination,request:{content,projectRoot:f.root,target:'web',destination,runtimes:{web:runtime}}};};
 let q=run(f.content);await assert.rejects(exportGame(q.request),/ui-surfaces-v1/);assert.equal(await stat(q.destination).catch(()=>null),null);caps.visualFeatures.push('ui-surfaces-v1');await writeFile(join(runtime,'capabilities.json'),JSON.stringify(caps));q=run(f.content);await exportGame(q.request);assert.deepEqual(await readFile(join(q.destination,'assets/surface-nine.png')),f.textures.nine);
 const invalid=structuredClone(f.content);invalid.project.visual.defaultStyle.surface={...nineSurface,insets:{top:18,right:12,bottom:18,left:12}};q=run(invalid);await assert.rejects(exportGame(q.request),/positive center/);assert.equal(await stat(q.destination).catch(()=>null),null);
 await writeFile(join(f.root,'assets/surface-nine.png'),'corrupt');q=run(f.content);await assert.rejects(exportGame(q.request));assert.equal(await stat(q.destination).catch(()=>null),null);
 const forged=structuredClone(f.content);forged.assetHashes['surface-nine']=sha(Buffer.from('corrupt'));q=run(forged);await assert.rejects(exportGame(q.request),/PNG/);assert.equal(await stat(q.destination).catch(()=>null),null);assert.ok(!(await readdir(base)).some(name=>name.startsWith('.'+basename(q.destination))),'Failed export does not leave a published or staging directory');
});
test('opaque plugin props named surface are not interpreted as engine paint or capabilities',async()=>{
 const f=await fixture(),content=structuredClone(f.content);const clear=value=>{if(!value||typeof value!=='object')return;delete value.surface;for(const child of Object.values(value))clear(child);};clear(content.project.visual);
 content.project.visual.screens[0].elements.push({id:'opaque-plugin',kind:'plugin',x:0,y:0,width:10,height:10,extension:{plugin:'fixture',props:{surface:{mode:'custom',imageAssetId:'plugin-owned'},nested:{surface:{mode:'nine-slice'}}}}});
 assert.deepEqual(surfaceReferences(content.project.visual),[]);assert.ok(!requiredPlayerFeatures(content).includes('ui-surfaces-v1'));
});
