import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,writeFile,readFile,access} from 'node:fs/promises';
import path from 'node:path';
import {content,doc,loaded,event,replaceEvents,temporary} from './helpers.mjs';
import {validateProject} from '../../dist/packages/validation/index.js';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/session.js';
import {encodeSave,decodeSave} from '../../dist/packages/runtime-service/save.js';
import {nativeCreateRun,nativeDispatchCommand,nativeRequest,closeNativeRuntimes} from '../../dist/packages/native-bridge/index.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

const portrait={characterId:'mentor',portraitId:'neutral',position:'center'};
const transform={scale:0.6251234567,offsetX:-0.234567891,offsetY:0.3456789123};
const must=r=>{assert.equal(r.ok,true,JSON.stringify(r.error??r.diagnostics));return r;};
function configure(p,root,props={}){
 p.root=root;doc(p,'assets').assets=[{id:'image',kind:'image',path:'assets/image.png'}];
 doc(p,'character','mentor').portraits={neutral:'image'};
 doc(p,'activity','rest').presentation={portraits:[{...portrait,...props}]};
 replaceEvents(p,[event('transforms',[
  {id:'pose',type:'scene',scene:{portraits:[{...portrait,...props}]},next:'say'},
  {id:'say',type:'dialogue',speakerId:null,text:'Pose',next:'reset'},
  {id:'reset',type:'scene',scene:{portraits:[{...portrait}]},next:'plain'},
  {id:'plain',type:'dialogue',speakerId:null,text:'Default pose',next:'end'},
  {id:'end',type:'end'},
 ])]);
}
async function fixture(t){const root=await temporary(t);await mkdir(path.join(root,'assets'));await writeFile(path.join(root,'assets/image.png'),'contract fixture; no rendering');return root;}

test('portrait transform schema accepts exact boundaries and rejects invalid values at the scene field',async t=>{
 const root=await fixture(t);
 for(const props of [{},{scale:0.1,offsetX:-2,offsetY:2},{scale:4,offsetX:2,offsetY:-2},transform]){
  const c=await content(p=>configure(p,root,props));assert.deepEqual(c.events[0].nodes[0].scene.portraits[0],{...portrait,...props});
 }
 for(const [key,value] of [['scale',0],['scale',0.099],['scale',4.001],['offsetX',-2.001],['offsetY',2.001],['scale','1'],['offsetX',null]]){
  const p=structuredClone(loaded);configure(p,root,{[key]:value});
  const validation=await validateProject(p);assert.equal(validation.valid,false);
  assert.ok(validation.diagnostics.some(d=>d.code==='SCHEMA_INVALID'&&d.file==='events/transforms.json'&&d.pointer===`/nodes/0/scene/portraits/0/${key}`),JSON.stringify(validation.diagnostics));
 }
});

test('scene transforms preserve precision across TS/native commands and saves; replacing portraits clears old transforms',async t=>{
 const root=await fixture(t),plain=await content(p=>configure(p,root)),decorated=await content(p=>configure(p,root,transform));
 let a=must(createSession(plain,517)).session,b=must(createSession(decorated,517)).session;
 const comparable=state=>{const {contentFingerprint,presentation,...rest}=state;return rest;};
 const exactPose=[{...portrait,...transform}],legacyPose=[portrait];
 assert.deepEqual(a.state.presentation,{backgroundAssetId:null,musicAssetId:null,portraits:[]});
 assert.deepEqual(must(nativeCreateRun(decorated,517,{mode:'full'})).state,b.state);
 let transformedSeen=false,resetSeen=false,saveBoundsChecked=false;
 const commands=[{type:'setSchedule',activityIds:['rest','rest','rest']},{type:'executeActivity'},{type:'acknowledgeResult'},{type:'resolvePeriodEnd'},...Array.from({length:5},()=>({type:'advanceNarrative'}))];
 for(const command of commands){
  const cpp=must(nativeDispatchCommand(decorated,b.state,command,b.state.stateRevision));
  const ar=must(dispatchSession(plain,a,command,a.state.stateRevision)),br=must(dispatchSession(decorated,b,command,b.state.stateRevision));
  a=ar.session;b=br.session;assert.deepEqual(cpp.state,b.state);assert.deepEqual(cpp.transition,br.transition);assert.deepEqual(comparable(a.state),comparable(b.state));
  for(const p of a.state.presentation.portraits)assert.deepEqual(p,portrait);
  if(b.state.activeNarrative?.nodeId==='say'){assert.deepEqual(b.state.presentation.portraits,exactPose);transformedSeen=true;}
  if(b.state.activeNarrative?.nodeId==='plain'){assert.deepEqual(b.state.presentation.portraits,legacyPose);resetSeen=true;}
  for(const [c,s] of [[plain,a],[decorated,b]]){
   const save=must(await encodeSave(c,s)),restored=must(await decodeSave(c,save.text));assert.deepEqual(restored.session,s);assert.equal(must(await encodeSave(c,restored.session)).text,save.text);
   if(c===decorated&&s.state.presentation.portraits.length&&!saveBoundsChecked){
    for(const [key,value] of [['scale',0.09],['scale',4.01],['offsetX',-2.01],['offsetY',2.01],['offsetX',null]]){
     const bad=JSON.parse(save.text);bad.state.presentation.portraits[0][key]=value;
     const rejected=await decodeSave(c,JSON.stringify(bad));assert.equal(rejected.ok,false);assert.equal(rejected.error.code,'SAVE_SCHEMA');assert.equal(rejected.error.path,`/state/presentation/portraits/0/${key}`);
    }saveBoundsChecked=true;
   }
   const replay=must(nativeRequest({operation:'replay',content:c,origin:s.origin,commands:s.commands}));assert.deepEqual(replay.state,s.state);
  }
 }
 assert.equal(transformedSeen,true);assert.equal(resetSeen,true);assert.equal(saveBoundsChecked,true);assert.deepEqual(a.commands,b.commands);assert.deepEqual(a.origin,b.origin);closeNativeRuntimes();
});

test('export gates transformed scenes for both targets while legacy scenes remain compatible',async t=>{
 const root=await fixture(t),plain=await content(p=>configure(p,root)),decorated=await content(p=>configure(p,root,transform));
 for(const target of ['web','windows']){
  const runtime=path.join(root,`${target}-runtime`);await mkdir(runtime);await writeFile(path.join(runtime,target==='web'?'index.html':'RaisingPlayer.exe'),'packaging fixture');
  const cap={formatVersion:'1',imageExtensions:['.png'],audioExtensions:[],visualFeatures:[]};
  await writeFile(path.join(runtime,'capabilities.json'),JSON.stringify(cap));
  const base={target,projectRoot:root,runtimes:{web:runtime,windows:runtime}};
  await exportGame({...base,content:plain,destination:path.join(root,`${target}-legacy`)});
  const old=path.join(root,`${target}-old`);await assert.rejects(exportGame({...base,content:decorated,destination:old}),/scene-portrait-transform-v1/);await assert.rejects(access(old),{code:'ENOENT'});
  cap.visualFeatures.push('scene-portrait-transform-v1');await writeFile(path.join(runtime,'capabilities.json'),JSON.stringify(cap));
  const destination=path.join(root,`${target}-new`);await exportGame({...base,content:decorated,destination});
  const bundle=JSON.parse(await readFile(path.join(destination,'game.json'),'utf8'));assert.deepEqual(bundle.content.events[0].nodes[0].scene.portraits,[{...portrait,...transform}]);
 }
});
