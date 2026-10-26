import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,writeFile,stat} from 'node:fs/promises';
import {join,resolve} from 'node:path';
import {loadProject,buildSnapshot} from '../../dist/packages/project-service/index.js';
import {validateProject} from '../../dist/packages/validation/index.js';
import {resolveScreenElements} from '../../dist/packages/visual/components.js';
import {requiredPlayerFeatures,missingPlayerFeatures} from '../../dist/packages/export-service/compatibility.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {encodeRgbaPng} from '../../dist/packages/media/png.js';
async function fixture(){
 await mkdir('.cache/activity-board',{recursive:true});const base=await mkdtemp(resolve('.cache/activity-board/contract-')),root=join(base,'game');await cp('examples/my-first-raising-game',root,{recursive:true});
 const project=JSON.parse(await readFile(join(root,'project.json'),'utf8')),screen=project.visual.screens.find(s=>s.id==='schedule'),board=screen.elements.find(e=>e.kind==='schedule');
 Object.assign(board,{scheduleLayout:'activity-board',activityIcons:[{activityId:'study',imageAssetId:'card-art'}],scheduleGroups:[{id:'all',label:'An authored group called all',activityIds:['study'],iconAssetId:'card-art'},{id:'rest',label:'Rest',activityIds:['rest']}],entryFrameAssetId:'card-art',controlStyleId:'board-colors',showPhaseForecast:true,showActivityRules:true,showActivityDescription:true});
 project.visual.styles=[...(project.visual.styles??[]),{id:'board-colors',label:'Board colors',fill:'#eee5d1',color:'#253e37'}];
 const manifest=JSON.parse(await readFile(join(root,'assets/manifest.json'))),pixels=Buffer.alloc(16*16*4,255);await writeFile(join(root,'assets/card.png'),encodeRgbaPng({width:16,height:16,pixels}));
 const wav=Buffer.alloc(44);wav.write('RIFF');wav.writeUInt32LE(36,4);wav.write('WAVEfmt ',8);wav.writeUInt32LE(16,16);wav.writeUInt16LE(1,20);wav.writeUInt16LE(1,22);wav.writeUInt32LE(8000,24);wav.writeUInt32LE(8000,28);wav.writeUInt16LE(1,32);wav.writeUInt16LE(8,34);wav.write('data',36);await writeFile(join(root,'assets/quiet.wav'),wav);
 manifest.assets.push({id:'card-art',kind:'image',path:'assets/card.png'},{id:'not-image',kind:'audio',path:'assets/quiet.wav'});await writeFile(join(root,'assets/manifest.json'),JSON.stringify(manifest));return {base,root,project,board,screen};
}
async function validate(f){await writeFile(join(f.root,'project.json'),JSON.stringify(f.project));return validateProject(await loadProject(f.root));}

test('activity board reuses ordinary widget fields in canvas and reading layouts without mutating source',async()=>{
 const f=await fixture(),before=structuredClone(f.project);assert.equal((await validate(f)).valid,true);
 const resolved=resolveScreenElements(f.project.visual,'schedule').find(e=>e.id===f.board.id);
 for(const field of ['scheduleLayout','scheduleGroups','activityIcons','entryFrameAssetId','controlStyleId','showPhaseForecast'])assert.deepEqual(resolved[field],f.board[field]);
 assert.deepEqual(f.project,before);assert.equal(f.board.scheduleGroups[0].id,'all','The runtime All filter must not reserve an authored group ID.');
 f.project.durationMonths=12;for(const periods of [1,4,12]){f.project.periodsPerMonth=periods;const result=await validate(f);assert.equal(result.valid,true,JSON.stringify(result.diagnostics));}
});

test('missing, wrong-kind and duplicate board artwork/activity references produce actionable locations',async()=>{
 const f=await fixture(),base=structuredClone(f.board);
 const cases=[
  [b=>b.activityIcons[0].activityId='missing-activity',d=>d.pointer.endsWith('/activityIcons/0/activityId')],
  [b=>b.activityIcons[0].imageAssetId='missing-image',d=>d.pointer.endsWith('/activityIcons/0/imageAssetId')],
  [b=>b.activityIcons.push({...b.activityIcons[0]}),d=>d.code==='UI_SCHEDULE_ICON_DUPLICATE'],
  [b=>b.scheduleGroups.push({...b.scheduleGroups[0]}),d=>d.code==='UI_SCHEDULE_GROUP_DUPLICATE'],
  [b=>b.scheduleGroups[0].activityIds=['study','study'],d=>d.code==='SCHEMA_INVALID'&&d.pointer.includes('/scheduleGroups/0/activityIds')],
  [b=>b.scheduleGroups[0].activityIds=['missing-activity'],d=>d.pointer.endsWith('/scheduleGroups/0/activityIds/0')],
  [b=>b.scheduleGroups[0].iconAssetId='missing-image',d=>d.pointer.endsWith('/scheduleGroups/0/iconAssetId')],
  ...['frameAssetId','entryFrameAssetId'].map(field=>[b=>b[field]='not-image',d=>d.code==='UI_IMAGE_KIND'&&d.pointer.endsWith('/'+field)]),
  [b=>b.scheduleGroups[0].iconAssetId='not-image',d=>d.code==='UI_IMAGE_KIND'&&d.pointer.endsWith('/scheduleGroups/0/iconAssetId')],
  [b=>b.activityIcons[0].imageAssetId='not-image',d=>d.code==='UI_IMAGE_KIND'&&d.pointer.endsWith('/activityIcons/0/imageAssetId')]
 ];
 for(const [change,expected]of cases){for(const key of Object.keys(f.board))delete f.board[key];Object.assign(f.board,structuredClone(base));change(f.board);const result=await validate(f);assert.equal(result.valid,false);assert.ok(result.diagnostics.some(expected),JSON.stringify(result.diagnostics));}
});

test('board kind and roster restrictions cannot be bypassed through component declarations or expansion',async()=>{
 const f=await fixture();f.board.kind='panel';let result=await validate(f);assert.ok(result.diagnostics.some(d=>d.code==='UI_ACTIVITY_BOARD_KIND'));
 f.board.kind='schedule';f.project.roster={localAttributeIds:['skill','stamina','stress'],members:[{activityId:'study',characterId:'apprentice',initialAttributes:{skill:0,stamina:80,stress:0}}]};result=await validate(f);assert.ok(result.diagnostics.some(d=>d.code==='UI_ACTIVITY_BOARD_ROSTER'));delete f.project.roster;
 const child={id:'planner',kind:'panel',x:0,y:0,width:300,height:200,scheduleLayout:'activity-board'};f.project.visual.components=[{id:'planner-card',label:'Unsupported board carrier',width:300,height:200,elements:[child]}];f.screen.elements.push({id:'planner-instance',kind:'component',componentId:'planner-card',x:0,y:0,width:300,height:200});
 const expanded=resolveScreenElements(f.project.visual,'schedule').find(e=>e.id==='planner-instance__planner');assert.equal(expanded.kind,'panel');assert.equal(expanded.scheduleLayout,'activity-board');result=await validate(f);assert.ok(result.diagnostics.some(d=>d.code==='UI_ACTIVITY_BOARD_KIND'&&d.pointer.includes('/components/')));
 child.kind='schedule';result=await validate(f);assert.ok(result.diagnostics.some(d=>d.code==='SCHEMA_INVALID'&&d.pointer.includes('/components/')),'Schedule children remain outside supported component authoring.');
});

test('new board and its icons have one independent capability; legacy board and opaque plugin data stay separate',async()=>{
 const f=await fixture(),content={project:f.project,activities:[],events:[]};let features=requiredPlayerFeatures(content);assert.ok(features.includes('activity-board-v1'));assert.ok(!features.includes('schedule-board-icons-v1'));assert.ok(!features.includes('schedule-board-v1'));
 assert.deepEqual(missingPlayerFeatures(['activity-board-v1'],['schedule-board-v1','schedule-board-icons-v1','compact-planning-v1']),['activity-board-v1']);
 f.board.scheduleLayout='board';features=requiredPlayerFeatures(content);assert.ok(!features.includes('activity-board-v1'));assert.ok(features.includes('schedule-board-v1'));assert.ok(features.includes('schedule-board-icons-v1'));
 f.board.scheduleLayout='columns';delete f.board.activityIcons;f.screen.elements.push({id:'opaque',kind:'plugin',extension:{props:{scheduleLayout:'activity-board',activityIcons:[{activityId:'x',imageAssetId:'y'}]}}});assert.ok(!requiredPlayerFeatures(content).includes('activity-board-v1'));
});

test('Web export requires activity-board support without falsely advertising legacy board support',async()=>{
 const f=await fixture();assert.equal((await validate(f)).valid,true);const built=await buildSnapshot(await loadProject(f.root));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));const runtime=join(f.base,'runtime');await mkdir(runtime);await writeFile(join(runtime,'index.html'),'fixture');
 const caps={formatVersion:'1',visualFeatures:requiredPlayerFeatures(built.snapshot).filter(v=>v!=='activity-board-v1'),imageExtensions:['.png','.jpg','.jpeg','.webp'],audioExtensions:['.wav','.mp3','.ogg'],fontExtensions:['.ttf','.otf']};await writeFile(join(runtime,'capabilities.json'),JSON.stringify(caps));
 const request={content:built.snapshot,projectRoot:f.root,target:'web',destination:join(f.base,'export'),runtimes:{web:runtime}};await assert.rejects(exportGame(request),/activity-board-v1/);assert.equal(await stat(request.destination).catch(()=>null),null);
 caps.visualFeatures.push('activity-board-v1');await writeFile(join(runtime,'capabilities.json'),JSON.stringify(caps));await exportGame(request);const bundle=JSON.parse(await readFile(join(request.destination,'game.json')));assert.equal(bundle.content.project.visual.screens.find(s=>s.id==='schedule').elements.find(e=>e.kind==='schedule').scheduleLayout,'activity-board');assert.ok(!caps.visualFeatures.includes('schedule-board-v1'));
});
