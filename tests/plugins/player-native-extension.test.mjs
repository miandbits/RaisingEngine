import test from 'node:test';
import assert from 'node:assert/strict';
import {copyFile,mkdir,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {makeGame,referenceRuntime,rig,controller,pluginElement,EVIDENCE,meshRig,meshController,pngPixel,close,RED,BLUE} from './player-extension-fixture.mjs';

const INSTANCE='home/hero-anim';
async function run(f,name,steps,args=[]){
  const input=resolve(f.root,`${name}-steps.json`),output=resolve(f.root,`${name}-report.json`),save=resolve(f.root,`${name}-save.json`);
  await writeFile(input,JSON.stringify(steps));
  const child=spawnSync(resolve(f.game,'RaisingPlayer.exe'),['--bundle',f.game,'--save',save,'--seed','42','--hidden','--automation',input,'--report',output,...args],{encoding:'utf8',timeout:90000,windowsHide:true});
  assert.equal(child.status,0,child.error?.message??child.stderr);
  const report=JSON.parse(await readFile(output,'utf8'));assert.equal(report.ok,true,report.error);
  const ui=tag=>{const step=report.steps.find(s=>s.command.checkpoint===tag);assert.ok(step,`checkpoint ${tag}`);return step.ui;};
  return {report,save,ui,instance:tag=>ui(tag).extensions.instances.find(i=>i.instance===INSTANCE)};
}
const track=(instance,n)=>instance.state.tracks.find(t=>t.track===n);
const near=(a,b,message)=>assert.ok(a.length===b.length&&a.every((v,i)=>Math.abs(v-b[i])<1e-6),`${message}: ${JSON.stringify(a)} vs ${JSON.stringify(b)}`);
/** Same op sequence through the Wasm build of the same core gives the expected draw items. */
async function reference(updates,observeAfter=-1){
  const request=await referenceRuntime();
  assert.equal(request({op:'init',documents:[{type:'rig',id:'hero',data:rig},{type:'controller',id:'hero-main',data:controller}]}).ok,true);
  assert.equal(request({op:'mount',instance:INSTANCE,element:{type:'character',ref:'hero-main',width:pluginElement.width,height:pluginElement.height,props:pluginElement.extension.props}}).ok,true);
  updates.forEach((dt,i)=>{if(i===observeAfter)request({op:'observe',before:{},after:{},command:{type:'executeAction',actionId:'cheer',quantity:1},error:null});request({op:'update',dtMs:dt});});
  return request({op:'frame',instance:INSTANCE}).items;
}
const beginning=[{click:'start'},{checkpoint:'home'},{extensionAdvanceMs:500,extensionSteps:5},{checkpoint:'idle'}];
const cheer=[{click:'cheer'},{click:'confirm-purchase'},{checkpoint:'joy'}];

test('native player draws extension items, binds committed actions to clips and restores clip state',{timeout:180000},async t=>{
  const f=await makeGame({target:'windows',name:'native'});t.diagnostic(`fixture: ${f.root}`);
  await mkdir(EVIDENCE,{recursive:true});
  const steps=[...beginning,...cheer,{extensionAdvanceMs:300},{checkpoint:'joy-300',screenshot:resolve(f.root,'joy-300.png')},{key:'F5'},
    {extensionAdvanceMs:700},{checkpoint:'back-to-idle'},{key:'F9'},{checkpoint:'loaded',screenshot:resolve(f.root,'loaded.png')},
    {extensionAdvanceMs:400},{checkpoint:'loaded-done'}];
  const r=await run(f,'main',steps);
  const home=r.instance('home');
  assert.equal(home.available,true,home.error);
  // props.skin 'summer' reaches the runtime through the host's element spec untouched.
  assert.deepEqual(home.items.map(i=>i.imageAssetId),['anim-torso-summer','anim-face','anim-eyes-open']);
  // Rig 200x300 fits the 300x450 element at scale 1.5; torso pivot (0.5,1) sits on the body bone 100px above the origin.
  near(home.items[0].matrix,[1.5,0,0,1.5,90,90],'torso setup matrix');
  near(home.items[0].matrix,(await reference([])).find(i=>i.imageAssetId==='anim-torso-summer').matrix,'parity with core');
  assert.equal(r.ui('home').texts['hero-anim'].includes('raising-extension'),true);
  // Idle loop advanced on the presentation clock only.
  const idle=r.instance('idle');assert.equal(track(idle,0).clip,'idle');assert.equal(track(idle,0).timeMs,500);
  near(idle.items[0].matrix,[1.5,0,0,1.5,90,84],'idle body y=-104 at 500ms');
  near(idle.items[0].matrix,(await reference([100,100,100,100,100])).find(i=>i.imageAssetId==='anim-torso-summer').matrix,'idle parity');
  assert.deepEqual(r.ui('idle').state,r.ui('home').state,'animation never changes game state');
  // executeAction success -> controller binding -> joy one-shot on track 0.
  const joy=r.instance('joy');assert.equal(track(joy,0).clip,'joy');
  // The same binding swaps the outfit (skin): identical geometry, the winter image, same items as the core.
  assert.deepEqual(joy.items.map(i=>i.imageAssetId),['anim-torso-winter','anim-face','anim-eyes-open']);
  assert.equal(joy.state.skin,'winter');
  assert.deepEqual(joy.items.map(i=>i.imageAssetId),(await reference([100,100,100,100,100,0],5)).map(i=>i.imageAssetId),'skin swap parity');
  const observed=r.ui('joy').extensions.lastObserve;
  assert.equal(observed.command.type,'executeAction');
  assert.deepEqual(observed.plugins['cutout-animation'].matched,[`${INSTANCE}:cheer-joy`]);
  assert.equal(r.ui('joy').state.attributes.money,r.ui('idle').state.attributes.money+1);
  const peak=r.instance('joy-300');assert.equal(track(peak,0).clip,'joy');assert.equal(track(peak,0).timeMs,300);
  for(const [a,b] of peak.items.map((item,i)=>[item.matrix,(r.instance('joy').items[i]).matrix]))assert.notDeepEqual(a,b,'joy pose differs from rest pose');
  near(peak.items[1].matrix,(await reference([100,100,100,100,100,300],5)).find(i=>i.imageAssetId==='anim-face').matrix,'joy parity');
  assert.equal(track(r.instance('back-to-idle'),0).clip,'idle','one-shot returns to the controller default clip');
  // Clip event (plugin 1.8.0): reported with its payload; its sound goes to the player's audio once.
  const fired=r.ui('back-to-idle').extensions;
  assert.equal(r.ui('joy-300').extensions.eventCount,0,'the 300 ms event fires on the following advance (half-open interval)');
  assert.deepEqual(fired.events.map(e=>[e.instance,e.plugin,e.clip,e.name,e.int,e.string,e.audio,e.volume,e.balance]),[[INSTANCE,'cutout-animation','joy','cheer',7,'peak','anim-cheer',0.5,0.25]]);
  assert.deepEqual([fired.eventCount,fired.soundCount],[1,1]);
  const sound=r.ui('back-to-idle').audio;
  assert.deepEqual([sound.extensionSounds+sound.extensionSoundErrors,sound.extensionSoundMissing],[1,0],JSON.stringify(sound));
  // Save carries opaque plugin state; load restores the one-shot at its saved time.
  const saved=JSON.parse(await readFile(r.save,'utf8'));
  const state=saved.playerPresentation.extensions['cutout-animation'];
  assert.equal(state.instances[INSTANCE].ref,'hero-main');
  assert.equal(saved.playerPresentation.timeline,null);
  const loaded=r.instance('loaded');assert.equal(track(loaded,0).clip,'joy');assert.equal(track(loaded,0).timeMs,300);
  assert.equal(state.instances[INSTANCE].state.skin,'winter');
  assert.equal(loaded.items[0].imageAssetId,'anim-torso-winter','skin restored with the clip');
  assert.equal(track(r.instance('loaded-done'),0).clip,'idle');
  assert.deepEqual(r.ui('loaded').state,r.ui('joy-300').state);
  for(const name of ['joy-300','loaded'])await copyFile(resolve(f.root,`${name}.png`),resolve(EVIDENCE,`windows-${name}.png`));
});

test('native game state and saves are identical regardless of presentation frame pacing',{timeout:240000},async t=>{
  const f=await makeGame({target:'windows',name:'pacing'});t.diagnostic(`fixture: ${f.root}`);
  const tail=[{click:'cheer'},{click:'confirm-purchase'},{click:'schedule-link'},{checkpoint:'end'},{key:'F5'},{checkpoint:'saved'}];
  const runs=[
    await run(f,'one-step',[{click:'start'},{extensionAdvanceMs:1000},...cheer,{extensionAdvanceMs:2000},...tail]),
    await run(f,'many-steps',[{click:'start'},{extensionAdvanceMs:1000,extensionSteps:50},...cheer,{extensionAdvanceMs:2000,extensionSteps:97},...tail]),
    await run(f,'realtime',[{click:'start'},{timelineRealtime:true},{waitFrames:40},...cheer,{waitFrames:25},...tail],['--frames','4000']),
  ];
  const strip=save=>{const {playerPresentation,...rest}=save;return rest;};
  const saves=await Promise.all(runs.map(async r=>JSON.parse(await readFile(r.save,'utf8'))));
  for(let i=1;i<runs.length;i++){
    assert.deepEqual(runs[i].ui('end').state,runs[0].ui('end').state,'state does not depend on frame pacing');
    assert.deepEqual(strip(saves[i]),strip(saves[0]),'save minus presentation is identical');
  }
  for(const [i,r] of runs.entries()){
    const ext=r.ui('saved').extensions;
    assert.equal(ext.observations,saves[i].commands.length,'observe runs exactly once per submitted command');
    assert.ok(ext.clockMs>0);
  }
  for(const r of runs.slice(0,2))assert.ok(Math.abs(r.ui('end').extensions.clockMs-3000)<1e-6,'manual presentation clock');
});

test('missing runtime module shows a placeholder and the game stays playable',{timeout:120000},async t=>{
  const f=await makeGame({target:'windows',name:'missing',runtime:false});t.diagnostic(`fixture: ${f.root}`);
  const r=await run(f,'missing',[{click:'start'},{checkpoint:'home',screenshot:resolve(f.root,'placeholder.png')},...cheer,{key:'F5'},{key:'F9'},{checkpoint:'loaded'}]);
  const home=r.instance('home');
  assert.equal(home.available,false);assert.deepEqual(home.items,[]);
  assert.match(r.ui('home').texts['hero-anim'],/플러그인 필요: cutout-animation/);
  assert.equal(r.ui('home').extensions.plugins[0].available,false);assert.match(r.ui('home').extensions.plugins[0].error,/확장 모듈 파일이 없습니다/);
  assert.equal(r.ui('joy').state.attributes.money,r.ui('home').state.attributes.money+1,'actions still commit');
  assert.deepEqual(r.ui('loaded').state,r.ui('joy').state);
  const saved=JSON.parse(await readFile(r.save,'utf8'));assert.equal(saved.playerPresentation,undefined,'no extension state without a runtime');
  await mkdir(EVIDENCE,{recursive:true});await copyFile(resolve(f.root,'placeholder.png'),resolve(EVIDENCE,'windows-placeholder.png'));
});

test('native player draws weighted mesh items bent by their bones (plugin-mesh-v1)',{timeout:120000},async t=>{
  const f=await makeGame({target:'windows',name:'native-mesh',mesh:true});t.diagnostic(`fixture: ${f.root}`);
  const shot=resolve(f.root,'mesh-bent.png');
  const r=await run(f,'mesh',[{click:'start'},{extensionAdvanceMs:100},{checkpoint:'bent',screenshot:shot}]);
  const bent=r.instance('bent');
  assert.equal(bent.available,true,bent.error);
  const [arm]=bent.items;
  assert.equal(arm.imageAssetId,'anim-arm');
  assert.equal(arm.mesh.positions.length,36);assert.equal(arm.mesh.indices.length,48);
  // Same triangles as the core compiled to Wasm (element-local px after placement).
  const request=await referenceRuntime();
  request({op:'init',documents:[{type:'rig',id:'hero',data:meshRig()},{type:'controller',id:'hero-main',data:meshController}]});
  request({op:'mount',instance:INSTANCE,element:{type:'character',ref:'hero-main',width:pluginElement.width,height:pluginElement.height,props:pluginElement.extension.props}});
  request({op:'update',dtMs:100});
  near(arm.mesh.positions,request({op:'frame',instance:INSTANCE}).items[0].mesh.positions,'mesh positions parity');
  // Pixels: element at (440,130), element px = rig px. Red shoulder half stays put, the blue
  // forearm half hangs below the elbow (180,200), and nothing is left where the straight arm was.
  const png=await readFile(shot),at=(x,y)=>pngPixel(png,pluginElement.x+x,pluginElement.y+y);
  assert.ok(close(at(90,200),RED),`shoulder half: ${at(90,200)}`);
  assert.ok(close(at(180,290),BLUE),`bent forearm: ${at(180,290)}`);
  assert.ok(close(at(172,240),BLUE)&&close(at(188,300),BLUE),`forearm body: ${at(172,240)} ${at(188,300)}`);
  assert.ok(!close(at(270,200),BLUE)&&!close(at(270,200),RED),`no straight forearm: ${at(270,200)}`);
  await mkdir(resolve(EVIDENCE,'v2'),{recursive:true});await copyFile(shot,resolve(EVIDENCE,'v2','windows-mesh-bent.png'));
});
