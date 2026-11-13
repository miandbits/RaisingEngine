import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile,stat} from 'node:fs/promises';
import {join,resolve,basename} from 'node:path';
import {surfaceFixture} from '../fixtures/surfaces.mjs';
import {loadProject,buildSnapshot} from '../../dist/packages/project-service/index.js';
import {validateProject} from '../../dist/packages/validation/index.js';
import {resolveControlStyle,controlStyleReferences,controlStyleFields} from '../../dist/packages/visual/control-styles.js';
import {resolveScreenElements} from '../../dist/packages/visual/components.js';
import {resolvePresentation} from '../../dist/packages/visual/variants.js';
import {requiredPlayerFeatures} from '../../dist/packages/export-service/compatibility.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

const palette={id:'control-paper',label:'Paper controls',fill:'#f5efdf',color:'#203a3b',borderColor:'#886633',hoverFill:'#e1d4b0',pressedFill:'#cfbc84',disabledColor:'#7f7b70',fontSize:40,padding:50,surface:{mode:'none'}};
const dark={id:'control-dark',label:'Dark controls',fill:'#203a3b',color:'#f5efdf'};
function configure(design){
 design.styles.push(palette,dark,{id:'control-empty',label:'Empty style'});
 design.defaultStyle={fill:'#ff0000',color:'#00ff00',borderColor:'#0000ff'};
 design.readableLayout.controlStyleId='control-paper';
 design.screens[0].elements[0].controlStyleId='control-dark';design.screens[0].elements[0].hidden=true;
 design.components[0].elements[0].controlStyleId='control-dark';
 design.screens[0].elements[6].overrides[0].values.controlStyleId='control-paper';
 design.screens[0].elements[8].variants[0].values.controlStyleId='control-dark';
 design.components[0].elements[0].variants=[{label:'Conditional controls',when:{op:'always'},values:{controlStyleId:'control-paper'}}];
 return design;
}
async function fixture(){await mkdir('.cache/ui-control-colors',{recursive:true});const f=await surfaceFixture(await mkdtemp(resolve('.cache/ui-control-colors/core-'))),project=JSON.parse(await readFile(join(f.root,'project.json'),'utf8'));configure(project.visual);await writeFile(join(f.root,'project.json'),JSON.stringify(project));return {...f,project};}
async function validate(f){await writeFile(join(f.root,'project.json'),JSON.stringify(f.project));return validateProject(await loadProject(f.root));}
const pointers=['/visual/readableLayout/controlStyleId','/visual/screens/0/elements/0/controlStyleId','/visual/components/0/elements/0/controlStyleId','/visual/screens/0/elements/6/overrides/0/values/controlStyleId','/visual/screens/0/elements/8/variants/0/values/controlStyleId','/visual/components/0/elements/0/variants/0/values/controlStyleId'];

test('control colors accept all declared reference slots and preserve component/variant overrides',async()=>{
 const f=await fixture(),validation=await validate(f);assert.equal(validation.valid,true,JSON.stringify(validation.diagnostics));
 assert.deepEqual(controlStyleReferences(f.project.visual).map(x=>x.pointer).sort(),[...pointers].sort());
 const expanded=resolveScreenElements(f.project.visual,'title'),overridden=expanded.find(e=>e.id==='component-tile__panel');assert.equal(overridden.controlStyleId,'control-paper');
 const variant=resolvePresentation(expanded.find(e=>e.id==='variant'),undefined,()=>true);assert.equal(variant.controlStyleId,'control-dark');
 assert.ok(requiredPlayerFeatures({...f.content,project:f.project}).includes('ui-control-colors-v1'));
});
test('only named palette colors resolve; explicit widget references win and reading fallback is mode-dependent',async()=>{
 const f=await fixture(),d=f.project.visual,before=structuredClone(d);const expected=Object.fromEntries(controlStyleFields.map(k=>[k,palette[k]]));
 assert.deepEqual(resolveControlStyle(d,undefined,false),{});assert.deepEqual(resolveControlStyle(d,undefined,true),expected);
 assert.deepEqual(resolveControlStyle(d,'control-dark',true),{fill:dark.fill,color:dark.color});assert.deepEqual(resolveControlStyle(d,'control-dark',false),{fill:dark.fill,color:dark.color});
 for(const id of ['control-empty','missing',''])assert.deepEqual(resolveControlStyle(d,id,true),{},`${id} must not inherit default or reading colors`);
 const result=resolveControlStyle(d,'control-paper',false);assert.deepEqual(result,expected);result.fill='#000000';assert.deepEqual(d,before);assert.equal(resolveControlStyle(d,'control-paper',false).fill,palette.fill);
 delete d.readableLayout.controlStyleId;assert.deepEqual(resolveControlStyle(d,undefined,true),{});
});
test('hidden elements, unused components, overrides and variants report exact missing control-style references',async()=>{
 const f=await fixture();for(const ref of controlStyleReferences(f.project.visual)){let owner=f.project;const segments=ref.pointer.slice(1).split('/');for(const key of segments.slice(0,-1))owner=owner[key];owner[segments.at(-1)]='missing-controls';}
 const result=await validate(f);assert.equal(result.valid,false);assert.deepEqual(result.diagnostics.filter(x=>x.code==='UI_CONTROL_STYLE_REFERENCE').map(x=>x.pointer).sort(),[...pointers].sort());
});
test('empty IDs and indirect inheritance slots are invalid, while opaque plugin props are not references',async()=>{
 const f=await fixture();for(const value of ['',null,42]){f.project.visual.readableLayout.controlStyleId=value;const r=await validate(f);assert.ok(r.diagnostics.some(d=>d.code==='SCHEMA_INVALID'&&d.pointer.includes('controlStyleId')),JSON.stringify(r.diagnostics));}
 delete f.project.visual.readableLayout.controlStyleId;f.project.visual.defaultStyle.controlStyleId='control-paper';let r=await validate(f);assert.equal(r.valid,false);delete f.project.visual.defaultStyle.controlStyleId;f.project.visual.styles[0].controlStyleId='control-paper';r=await validate(f);assert.equal(r.valid,false);
 const design={screens:[{id:'title',elements:[{id:'extension',kind:'plugin',extension:{plugin:'custom',props:{controlStyleId:'plugin-owned',nested:{controlStyleId:'also-opaque'}}}}]}]};assert.deepEqual(controlStyleReferences(design),[]);assert.ok(!requiredPlayerFeatures({project:{visual:design},activities:[],events:[]}).includes('ui-control-colors-v1'));
});
test('exports require control-color capability and reject forged missing references before publication',async()=>{
 const f=await fixture(),built=await buildSnapshot(await loadProject(f.root));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));const content=built.snapshot,runtime=join(f.root,'runtime');await mkdir(runtime);await writeFile(join(runtime,'index.html'),'fixture');const capabilities={formatVersion:'1',visualFeatures:requiredPlayerFeatures(content).filter(x=>x!=='ui-control-colors-v1'),imageExtensions:['.png','.jpg','.jpeg','.webp'],audioExtensions:['.wav','.ogg','.mp3'],fontExtensions:['.ttf','.otf']};await writeFile(join(runtime,'capabilities.json'),JSON.stringify(capabilities));let index=0;const request=value=>({content:value,projectRoot:f.root,target:'web',destination:join(f.root,'..',basename(f.root)+'-export-'+index++),runtimes:{web:runtime}});
 let q=request(content);await assert.rejects(exportGame(q),/ui-control-colors-v1/);assert.equal(await stat(q.destination).catch(()=>null),null);
 capabilities.visualFeatures.push('ui-control-colors-v1');await writeFile(join(runtime,'capabilities.json'),JSON.stringify(capabilities));q=request(content);await exportGame(q);assert.ok(await stat(join(q.destination,'game.json')));
 const forged=structuredClone(content);forged.project.visual.readableLayout.controlStyleId='missing';q=request(forged);await assert.rejects(exportGame(q),/Generated control style does not exist/);assert.equal(await stat(q.destination).catch(()=>null),null);
});
