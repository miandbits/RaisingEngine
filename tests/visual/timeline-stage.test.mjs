import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,readdir,writeFile,mkdtemp,rm} from 'node:fs/promises';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import Ajv2020 from 'ajv/dist/2020.js';
import {fixture} from '../gameplay/fixture.mjs';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/session.js';
import {encodeSave,decodeSave} from '../../dist/packages/runtime-service/save.js';
import {planActivityTimeline,createTimelineCursor,advanceTimeline,timelineLayerFrameIndex} from '../../dist/packages/visual/timeline.js';

const stage=()=>({width:320,height:128,layers:[
  {id:'background',x:0,y:0,width:320,height:128,frames:[{imageAssetId:'frame-a',durationMs:100}]},
  {id:'actor',x:-8,y:20,width:64,height:96,loop:true,frames:[{imageAssetId:'frame-a',durationMs:100},{imageAssetId:'frame-b',durationMs:200}]},
]});
const configure=p=>{
  p.visual.timelines=[{id:'animation',name:'Layered animation',clips:[{id:'clip',durationMs:650,stage:stage()}]}];
  p.visual.activityTimelines=[{activityId:'rest',timelineId:'animation'}];
};
async function images(root){
  const png=Buffer.from('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aDasAAAAASUVORK5CYII=','base64');
  await writeFile(path.join(root,'assets/frame.png'),png);
  await writeFile(path.join(root,'assets/manifest.json'),JSON.stringify({formatVersion:'0.1.0',assets:[
    {id:'frame-a',kind:'image',path:'assets/frame.png'},
    {id:'frame-b',kind:'image',path:'assets/frame.png'},
    {id:'wrong-kind',kind:'audio',path:'assets/frame.png'},
  ]}));
}
function activityResult(content){
  let s=createSession(content,42,{mode:'activities-only'}).session;
  for(const command of [{type:'setSchedule',activityIds:['rest','rest','rest']},{type:'executeActivity'}]){
    const result=dispatchSession(content,s,command,s.state.stateRevision);assert.equal(result.ok,true);s=result.session;
  }
  return s;
}

test('stage schema accepts layered and legacy clips, rejects geometry, lengths, and time bounds',async()=>{
  const ajv=new Ajv2020({strict:true,allErrors:true});
  for(const file of (await readdir('packages/schema')).filter(x=>x.endsWith('.schema.json')))
    ajv.addSchema(JSON.parse(await readFile(path.join('packages/schema',file),'utf8')));
  const validate=ajv.getSchema('https://raising-engine.example/schemas/0.1.0/visual.schema.json');
  const visual={width:1280,height:720,screens:['title','schedule','story','ending'].map(id=>({id,label:id,elements:[]})),timelines:[{id:'test',name:'Test',clips:[{id:'clip',durationMs:650,stage:stage()}]}]};
  assert.equal(validate(visual),true,ajv.errorsText(validate.errors));
  const before=JSON.stringify(visual);validate(visual);assert.equal(JSON.stringify(visual),before,'validation does not inject loop defaults');
  for(const [label,mutate] of [
    ['zero stage width',s=>s.width=0],['too large stage',s=>s.height=8193],['infinite stage',s=>s.width=Infinity],
    ['empty layers',s=>s.layers=[]],['many layers',s=>s.layers=Array.from({length:33},()=>s.layers[0])],
    ['non-finite position',s=>s.layers[0].x=NaN],['infinite position',s=>s.layers[0].y=Infinity],
    ['negative size',s=>s.layers[0].width=-1],['empty frames',s=>s.layers[0].frames=[]],
    ['many frames',s=>s.layers[0].frames=Array.from({length:501},()=>s.layers[0].frames[0])],
    ['zero time',s=>s.layers[0].frames[0].durationMs=0],['large time',s=>s.layers[0].frames[0].durationMs=60001],
    ['fractional time',s=>s.layers[0].frames[0].durationMs=1.5],
  ]){const v=structuredClone(visual);mutate(v.timelines[0].clips[0].stage);assert.equal(validate(v),false,label);}
  const old=structuredClone(visual);delete old.timelines[0].clips[0].stage;old.timelines[0].clips[0].imageAssetId='frame-a';
  assert.equal(validate(old),true,ajv.errorsText(validate.errors));
  old.timelines[0].clips[0].stage=stage();assert.equal(validate(old),true,'legacy image may be kept as fallback');
});

test('frame intervals, loop boundaries and nonloop end hold use only elapsed time',()=>{
  const layer=stage().layers[1],times=[0,99,100,299,300,399,400,650];
  assert.deepEqual(times.map(t=>timelineLayerFrameIndex(layer,t)),[0,0,1,1,0,0,1,0]);
  delete layer.loop;
  assert.deepEqual(times.map(t=>timelineLayerFrameIndex(layer,t)),[0,0,1,1,1,1,1,1]);
  assert.equal(timelineLayerFrameIndex(layer,-100),0);
  assert.equal(timelineLayerFrameIndex(stage().layers[0],60000),0);
});

test('stage plan survives save/restore and timing controls without changing committed state',async t=>{
  const f=await fixture(t,configure,images);assert.equal(f.built.ok,true,JSON.stringify(f.built.diagnostics));
  const content=f.built.snapshot,session=activityResult(content),before=JSON.stringify(session),plan=planActivityTimeline(content,session.state);
  assert.deepEqual(plan.clips[0].stage,stage());assert.equal(plan.clips[0].imageAssetId,undefined);
  const cursor=advanceTimeline(plan,createTimelineCursor(plan),400),encoded=await encodeSave(content,session);
  assert.equal(encoded.ok,true);const envelope=JSON.parse(encoded.text);envelope.playerPresentation={version:1,timeline:cursor};
  const decoded=await decodeSave(content,JSON.stringify(envelope));
  assert.equal(decoded.ok,true,JSON.stringify(decoded));assert.deepEqual(decoded.playerPresentation.timeline,cursor);
  const restoredPlan=planActivityTimeline(content,decoded.session.state);
  const frame=(p,c)=>timelineLayerFrameIndex(p.clips[c.clipIndex].stage.layers[1],c.elapsedMs);
  assert.equal(frame(plan,cursor),1);assert.equal(frame(restoredPlan,decoded.playerPresentation.timeline),1);
  const paused={...cursor,paused:true};assert.deepEqual(advanceTimeline(plan,paused,100),paused);
  const fast=advanceTimeline(plan,{...createTimelineCursor(plan),speed:4},100);assert.equal(frame(plan,fast),frame(plan,cursor));
  assert.equal(JSON.stringify(session),before);
});

test('stage semantic diagnostics point to duplicate layers, missing frames and wrong asset kinds',async t=>{
  const f=await fixture(t,p=>{configure(p);const layers=p.visual.timelines[0].clips[0].stage.layers;layers[1].id='background';layers[0].frames[0].imageAssetId='missing';layers[1].frames[1].imageAssetId='wrong-kind';},images);
  assert.equal(f.built.ok,false);
  const diagnostics=f.built.diagnostics;
  assert.ok(diagnostics.some(x=>x.pointer==='/visual/timelines/0/clips/0/stage/layers/1/id'));
  assert.ok(diagnostics.some(x=>x.pointer==='/visual/timelines/0/clips/0/stage/layers/0/frames/0/imageAssetId'));
  assert.ok(diagnostics.some(x=>x.code==='TIMELINE_IMAGE_KIND'&&x.pointer.endsWith('/layers/1/frames/1/imageAssetId')));
});

test('web renderer uses stage order, exact frame, clipped geometry and legacy image fallback',async t=>{
  const {build}=await import('esbuild'),React=await import('react'),{renderToStaticMarkup}=await import('react-dom/server');
  const directory=await mkdtemp(path.resolve('.cache/timeline-stage-render-'));t.after(()=>rm(directory,{recursive:true,force:true}));
  const outfile=path.join(directory,'overlay.mjs');
  await build({entryPoints:['apps/player/renderer/TimelineOverlay.tsx'],outfile,bundle:true,platform:'node',format:'esm',external:['react'],loader:{'.css':'empty'},logLevel:'silent'});
  const {TimelineOverlay}=await import(pathToFileURL(outfile).href);
  const previous=globalThis.window;globalThis.window={raisingAssetUrl:id=>`/images/${id}.png`};
  t.after(()=>{if(previous===undefined)delete globalThis.window;else globalThis.window=previous;});
  const clip={id:'clip',clipId:'clip',durationMs:650,text:'',transition:'none',day:null,stage:stage(),imageAssetId:'legacy'};
  const plan={clips:[clip]},cursor={clipIndex:0,elapsedMs:400,completed:false,speed:1,paused:false};
  const render=()=>renderToStaticMarkup(React.createElement(TimelineOverlay,{timeline:{plan,cursor},generation:1,busy:false,act:()=>{},save:()=>{},load:()=>{}}));
  let html=render();assert.match(html,/data-layer-id="background"/);assert.match(html,/data-layer-id="actor" data-frame-index="1"/);
  assert.ok(html.indexOf('data-layer-id="background"')<html.indexOf('data-layer-id="actor"'));
  assert.match(html,/left:-2.5%/);assert.match(html,/src="\/images\/frame-b.png"/);assert.doesNotMatch(html,/id="timeline-image"/);
  cursor.completed=true;cursor.elapsedMs=0;html=render();assert.match(html,/data-layer-id="actor" data-frame-index="0"/,'completed clip uses duration 650, not cursor zero');
  delete clip.stage;html=render();assert.match(html,/id="timeline-image"/);assert.match(html,/src="\/images\/legacy.png"/);
});
