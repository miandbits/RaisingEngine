import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {phaseForecastFixture} from '../fixtures/phase-forecast-ui.mjs';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {desktopTarget,gameDirectory,playerExecutable} from '../../dist/packages/export-service/targets.js';
const at=(r,n)=>r.steps.find(s=>s.command.checkpoint===n).ui;
const touch=id=>({scrollIntoView:id,touchWindow:id,waitFrames:3});
const unchanged=(a,b)=>{assert.deepEqual(a.state,b.state);assert.deepEqual(a.commands,b.commands);};

test('save controls fit physical targets and confirmation text without changing cancel/save/load semantics',{timeout:240000},async()=>{
 await mkdir('.cache/save-slots-layout/native',{recursive:true});const out=await mkdtemp(resolve('.cache/save-slots-layout/native/run-')),f=await phaseForecastFixture(join(out,'project'),{opted:false});
 f.content=structuredClone(f.content);const visual=f.content.project.visual;
 visual.saveSystem={manualSlots:8,autoSlots:3};
 const button=(id,action,x,y,extra={})=>({id,kind:'button',action,text:id,autoLabel:false,x,y,width:250,height:60,...extra});
 const home=visual.screens.find(s=>s.id==='schedule');home.label='A long named reading room beside the eastern archive, where old stories are saved for future seasons. '.repeat(4)+'FINAL PLACE DETAIL';
 home.elements=home.elements.filter(e=>!['save','load'].includes(e.id));home.elements.push(button('save','openModal',300,640,{screenId:'records'}),button('load','openModal',560,640,{screenId:'load-records'}));
 for(const load of [false,true])visual.screens.push({id:load?'load-records':'records',label:'Records',modal:true,elements:[{id:'slots',kind:load?'loadSlots':'saveSlots',x:80,y:80,width:1120,height:530,fill:'#f5efdf',color:'#263c36'},button('close','closeModal',80,630)],readableLayout:{items:[{elementId:'slots'}],actions:['close']}});
 const runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app'),target=desktopTarget(),destination=join(out,'game');await exportGame({content:f.content,projectRoot:f.root,target,destination,runtimes:{windows:runtime,macos:runtime,web:runtime}});
 const wrapped=join(out,'wrapped-game'),narrow=structuredClone(f.content);for(const s of narrow.project.visual.screens)if(s.modal)s.elements.find(e=>e.id==='slots').width=500;await exportGame({content:narrow,projectRoot:f.root,target,destination:wrapped,runtimes:{windows:runtime,macos:runtime,web:runtime}});
 const run=async(name,steps,locale='en',extra=[],game=destination)=>{const script=join(out,name+'.json'),report=join(out,name+'-report.json');await writeFile(script,JSON.stringify(steps));const p=spawnSync(playerExecutable(target,game),['--bundle',gameDirectory(target,game),'--save',join(out,locale+'-save.json'),'--automation',script,'--report',report,'--report-checkpoints-only','--hidden','--locale',locale,...extra],{encoding:'utf8',timeout:60000});assert.equal(p.status,0,p.stderr||p.error?.message);const r=JSON.parse(await readFile(report,'utf8'));assert.equal(r.ok,true,r.error);return r;};
 const records=[],check=(ui,owner)=>{
  const nodes=ui.nodes.filter(n=>n.kind==='runtime-button'&&n.ownerId===owner&&!n.id.includes('-slot-'));
  assert.ok(nodes.length>0);
  for(const n of nodes){const point=ui.readingLayout.controls.find(c=>c.id===n.id);assert.ok(point&&point.width>=43.5&&point.height>=43.5,'Window-point target: '+JSON.stringify(point));assert.ok(n.scrollHeight<=n.clientHeight+1,'Wrapped label fits button: '+JSON.stringify(n));assert.ok(n.scrollWidth<=n.clientWidth+1,'Label fits width: '+JSON.stringify(n));}
 };
 for(const locale of ['en','ko','ja']){
  await run(locale+'-seed',[touch('start'),touch('save'),touch('records__slots-request')],locale);
  const saved=await readFile(join(out,locale+'-save.json'));
  for(const [name,width,height,scale]of[['desktop',1280,720,1],['short',1024,640,1],['wrapped',1280,720,1],['phone',390,844,2]]){
   const prefix=locale+'-'+name;
   const steps=[{resize:{width,height}},{waitFrames:4},touch('start'),{checkpoint:'before'},touch('save'),{checkpoint:'list',screenshot:join(out,prefix+'-list.png')},touch('records__slots-next'),{checkpoint:'next-page'},touch('records__slots-prev'),touch('records__slots-slot-5'),{checkpoint:'selected-last'},touch('records__slots-slot-0'),touch('records__slots-request'),{checkpoint:'overwrite',screenshot:join(out,prefix+'-overwrite.png')},{key:'Tab'},{waitFrames:3},{checkpoint:'confirm-focus'},{key:'ShiftTab'},{waitFrames:3},{checkpoint:'cancel-focus'},touch('records__slots-cancel'),{checkpoint:'cancelled'},touch('records__close'),touch('load'),touch('load-records__slots-request'),{checkpoint:'load'},touch('load-records__slots-cancel'),{checkpoint:'load-cancelled'},touch('load-records__slots-request'),touch('load-records__slots-confirm'),{checkpoint:'restored'}];
   // Readable record lists show all entries, so desktop paging is unnecessary.
   if(name==='wrapped'&&locale==='en')steps[steps.findIndex(s=>s.touchWindow==='records__slots-cancel')]={clickPoint:{id:'records__slots-cancel',x:20,y:40},waitFrames:3};
   if(name==='phone'){
    steps.splice(steps.findIndex(s=>s.touchWindow==='records__slots-next'),5);
    // A complete long record can be taller than a200% phone viewport. Click its
    // visible first line rather than asking automation to hit an offscreen center.
    steps[steps.findIndex(s=>s.touchWindow==='records__slots-slot-0')]={scrollIntoView:'records__slots-slot-0',clickPoint:{id:'records__slots-slot-0',x:18,y:18},waitFrames:3};
   }
   const r=await run(prefix,steps,locale,['--text-scale',String(scale)],name==='wrapped'?wrapped:destination);
   if(name!=='phone'){const selected=at(r,'selected-last').nodes.find(n=>n.id==='records__slots-slot-5');assert.equal(at(r,'selected-last').focusedId,selected.id);assert.ok(selected.visibleHitbox.height>=selected.height-1,'Selected last-row card stays fully visible after rebuilding');unchanged(at(r,'selected-last'),at(r,'before'));}
   if(name==='desktop'||name==='short'){const request=at(r,'list').nodes.find(n=>n.id==='records__slots-request');assert.equal(request.clickableAtCenter,true,'Primary save action stays visible while cards scroll');assert.ok(request.visibleHitbox.height>=request.height-1);}
   for(const [label,owner]of[['list','records__slots'],['overwrite','records__slots'],['load','load-records__slots']])check(at(r,label),owner);
   assert.equal(at(r,'overwrite').focusedId,'records__slots-cancel');assert.equal(at(r,'overwrite').nodes.find(n=>n.id==='records__slots-cancel').clickableAtCenter,true,prefix+': initial Cancel focus must be visible');assert.equal(at(r,'confirm-focus').focusedId,'records__slots-confirm');assert.equal(at(r,'cancel-focus').focusedId,'records__slots-cancel');
   for(const label of ['cancelled','load-cancelled'])unchanged(at(r,label),at(r,'before'));
   assert.deepEqual(await readFile(join(out,locale+'-save.json')),saved,'Review/cancel/load preserve stored bytes');const prior=JSON.parse(saved);assert.deepEqual(at(r,'restored').state,prior.state);assert.deepEqual(at(r,'restored').commands,prior.commands);
   const confirmation=at(r,'overwrite');assert.match(confirmation.texts['records__slots'],/FINAL PLACE DETAIL/);
   const confirm=at(r,'confirm-focus').nodes.find(n=>n.id==='records__slots-confirm');assert.equal(confirm.clickableAtCenter,true,'Keyboard focus brings Confirm into the visible hit area');
   const cancel=at(r,'cancel-focus').nodes.find(n=>n.id==='records__slots-cancel');assert.equal(cancel.clickableAtCenter,true);
   assert.equal(at(r,'cancelled').nodes.some(n=>n.id==='records__slots-cancel'),false,'The real Cancel pointer action exits confirmation');
   if(name==='wrapped'&&locale==='en'){
    assert.ok(cancel.height>65,'Narrow case actually wraps the English cancel label');
    const panel=confirmation.nodes.find(n=>n.id==='records__slots'),point=confirmation.readingLayout.controls.find(c=>c.id==='records__slots-cancel'),logical=confirmation.nodes.find(n=>n.id==='records__slots-cancel');
    const scale=point.width/logical.width,ox=point.x-logical.x*scale,oy=point.y-logical.y*scale,x=ox+(panel.x+panel.width/2)*scale,top=oy+(panel.y+40)*scale,bottom=oy+(panel.y+panel.height-40)*scale;
    const drags=(fromY,toY,n)=>Array.from({length:n},()=>({touchDrag:{fromX:x,fromY,toX:x,toY},waitFrames:2}));
    const drag=await run('canvas-drag',[{resize:{width,height}},touch('start'),touch('save'),touch('records__slots-request'),{checkpoint:'begin'},...drags(top,bottom,12),{checkpoint:'top',screenshot:join(out,'canvas-drag-top.png')},...drags(bottom,top,40),{checkpoint:'bottom',screenshot:join(out,'canvas-drag-bottom.png')},touch('records__slots-cancel'),{checkpoint:'cancelled'}],locale,['--text-scale','1'],wrapped);
    const node=label=>at(drag,label).nodes.find(n=>n.id==='records__slots-cancel');
    assert.equal(at(drag,'begin').readingLayout.active,false,'Touch scroll proof uses canvas mode');assert.ok(node('top').y>node('bottom').y+20,'Real canvas touch drags scroll between earlier text and the action rows');assert.ok(node('bottom').visibleHitbox.height>=node('bottom').height-1,'Real upward drags reach the entire action target');
    for(const label of ['top','bottom','cancelled'])unchanged(at(drag,label),at(drag,'begin'));assert.deepEqual(await readFile(join(out,locale+'-save.json')),saved,'Canvas dragging and cancel preserve stored bytes');
   }
   records.push({locale,name,reading:confirmation.readingLayout.active,controls:confirmation.readingLayout.controls.filter(c=>c.id.startsWith('records__slots-')),focus:at(r,'confirm-focus').focusedId});
  }
  const accepted=await run(locale+'-accept-overwrite',[touch('start'),{checkpoint:'before'},touch('save'),touch('records__slots-request'),touch('records__slots-confirm'),{checkpoint:'saved'}],locale,['--text-scale','1']);
  unchanged(at(accepted,'saved'),at(accepted,'before'));const overwritten=JSON.parse(await readFile(join(out,locale+'-save.json'),'utf8'));assert.deepEqual(overwritten.state,at(accepted,'before').state);assert.deepEqual(overwritten.commands,at(accepted,'before').commands);assert.ok(overwritten.playerPresentation.saveRecord.sequence>JSON.parse(saved).playerPresentation.saveRecord.sequence,'Confirmed overwrite records a new save');
 }
 await writeFile(join(out,'proof.json'),JSON.stringify({ok:true,records,limits:['Automated desktop pointer/keyboard and phone-sized native renderer; physical iOS device acceptance remains separate.']},null,2));console.log(JSON.stringify({evidence:out}));
});
