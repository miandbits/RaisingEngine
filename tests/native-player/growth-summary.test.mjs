import test from 'node:test';import assert from 'node:assert/strict';import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';import {resolve,join} from 'node:path';import {spawnSync} from 'node:child_process';
import {loadProject,buildSnapshot} from '../../dist/packages/project-service/index.js';
import {phaseForecastFixture} from '../fixtures/phase-forecast-ui.mjs';import {exportGame} from '../../dist/packages/export-service/export.js';import {desktopTarget,gameDirectory,playerExecutable} from '../../dist/packages/export-service/targets.js';import {activityGrowthRows} from '../../dist/packages/visual/growth-summary.js';
test('native activity presentation reveals grouped committed growth and waits for explicit advance after skip/finish and restored saves',{timeout:180000},async()=>{
 await mkdir('.cache/growth-summary/native',{recursive:true});const out=await mkdtemp(resolve('.cache/growth-summary/native/run-')),fixture=await phaseForecastFixture(join(out,'project'),{opted:false}),content=structuredClone(fixture.content);delete content.project.gameplay;content.project.schedulePlayback='continuous';const visual=content.project.visual;visual.activityTimelineCompletion='reveal-result';visual.timelines=[{id:'study-scene',name:'Study scene',clips:[{id:'one',durationMs:1000,text:'{activity} · {outcome}'}]}];visual.activityTimelines=[{activityId:'study',timelineId:'study-scene'}];for(const s of visual.screens)for(const e of s.elements)if(e.kind==='result'){e.resultLayout='growth';e.color='#203a3b';e.height=300;e.y=300;}
 const activity=content.activities.find(a=>a.id==='study');activity.costs=[];activity.successCheck={chance:{op:'literal',value:10000}};activity.successEffects=[];activity.failureEffects=[];activity.baseEffects=[{op:'add',target:{kind:'attribute',attributeId:'knowledge'},value:{op:'literal',value:8}},{op:'add',target:{kind:'attribute',attributeId:'knowledge'},value:{op:'literal',value:-3}}];const attr=content.attributes.find(a=>a.id==='knowledge')??content.attributes[0];for(const e of activity.baseEffects)e.target.attributeId=attr.id;content.attributes.push({...structuredClone(attr),id:'focus',label:'Focus'});content.project.initialState.attributes.focus=0;for(const a of content.attributes.filter(a=>a.id!==attr.id).slice(0,4))activity.baseEffects.push({op:'add',target:{kind:'attribute',attributeId:a.id},value:{op:'literal',value:1}});
 const target=desktopTarget(),destination=join(out,'game'),runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app');await exportGame({content,projectRoot:fixture.root,target,destination,runtimes:{windows:runtime,macos:runtime,web:runtime}});
 const proofs=[];for(const locale of ['en','ko','ja'])for(const ending of ['skip','finish']){const name=locale+'-'+ending,dir=join(out,name);await mkdir(dir);const save=join(dir,'save.json');const steps=[{clickWindow:'start'},...[0,1,2,3].map(i=>({select:'plan-'+i,value:'study'})),{clickWindow:'advance'},{clickWindow:'advance'},{checkpoint:'performance'},...(ending==='skip'?[{clickWindow:'timeline-skip'}]:[{timelineAdvanceMs:2000}]),{checkpoint:'growth',screenshot:join(out,name+'.png')},{timelineRealtime:true},{waitFrames:90},{timelineRealtime:false},{checkpoint:'stable'},{resize:{width:390,height:844}},{clickWindow:'engine-reading-options'},{click:'engine-reading-settings-text-200'},{click:'engine-reading-close'},...Array.from({length:8},()=>({touchDrag:{fromX:190,fromY:350,toX:190,toY:100}})),{checkpoint:'phone',screenshot:join(out,name+'-phone.png')},{key:'F5'},{key:'F9'},{checkpoint:'restored'},{clickWindow:'advance'},{checkpoint:'acknowledged'}];const input=join(out,name+'.json'),report=join(out,name+'-report.json');await writeFile(input,JSON.stringify(steps));const run=spawnSync(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--save',save,'--automation',input,'--report',report,'--report-checkpoints-only','--hidden','--locale',locale],{encoding:'utf8',timeout:45000});assert.equal(run.status,0,run.stderr||run.error?.message);const r=JSON.parse(await readFile(report,'utf8'));assert.equal(r.ok,true,r.error);const get=tag=>r.steps.find(s=>s.command.checkpoint===tag).ui,p=get('performance'),g=get('growth'),s=get('stable'),restored=get('restored'),phone=get('phone');assert.equal(phone.readingLayout.active,true);assert.equal(phone.readingLayout.textScale,2);const pane=phone.readingLayout['reading-main'];assert.ok(pane.scrollWidth<=pane.clientWidth+1,JSON.stringify(pane));assert.ok(pane.scrollTop>0,JSON.stringify(pane));assert.ok(pane.scrollTop+pane.clientHeight>=pane.scrollHeight-2,JSON.stringify(pane));assert.deepEqual(phone.state,g.state);assert.deepEqual(phone.commands,g.commands);assert.equal(p.state.phase,'activity-result');assert.deepEqual(g.state,p.state);assert.equal(g.timeline.cursor.completed,true);assert.deepEqual(s.state,g.state);assert.deepEqual(s.commands,g.commands);assert.deepEqual(restored.state,g.state);assert.deepEqual(restored.timeline.cursor,g.timeline.cursor);const rows=activityGrowthRows(g.state.lastActivityResult);assert.equal(rows.length,5);assert.equal(rows[0].delta,5);const resultNode=g.nodes.find(n=>n.id==='result');assert.ok(resultNode,JSON.stringify(g.nodes.map(n=>n.id)));assert.ok(resultNode.scrollHeight<=resultNode.clientHeight+1,JSON.stringify(resultNode));assert.ok(g.texts.result.includes(String(rows[0].before)));assert.ok(g.texts.result.includes(String(rows[0].after)));assert.match(g.texts.result,/\+5/);if(locale==='en')assert.doesNotMatch(g.texts.result,/[가-힣]/);assert.equal(get('acknowledged').commands.filter(c=>c.type==='acknowledgeResult').length,1);assert.equal(g.commands.filter(c=>c.type==='executeActivity').length,1);proofs.push({locale,ending,rows,text:g.texts.result,phone200:{pane,canonicalUnchanged:true}});}
 await writeFile(join(out,'proof.json'),JSON.stringify({ok:true,cases:proofs},null,2));
});

test('reading activity presentations show actual media and text at 200%, retain accessible controls and restore the cursor through authored records', {timeout:240000}, async t => {
  await mkdir('.cache/growth-summary/native', {recursive:true});
  const out=await mkdtemp(resolve('.cache/growth-summary/native/reading-'));
  t.diagnostic(`Evidence: ${out}`);
  const fixture=await phaseForecastFixture(join(out,'project'), {opted:false});
  let content=structuredClone(fixture.content);delete content.project.gameplay;
  const visual=content.project.visual;visual.activityTimelineCompletion='reveal-result';content.project.schedulePlayback='continuous';
  for(const [id,file] of [['presentation-a','courtyard.png'],['presentation-b','library.png']]){
    await writeFile(join(fixture.root,'assets',file),await readFile(resolve('examples/academy-journey/assets/visual',file)));
    content.assets.push({id,kind:'image',path:'assets/'+file});
  }
  const stage={width:400,height:200,layers:[{id:'room',x:0,y:0,width:400,height:200,frames:[{imageAssetId:'presentation-a',durationMs:300},{imageAssetId:'presentation-b',durationMs:300}],loop:true}]};
  visual.timelines=[{id:'presentation',name:'Presentation',clips:[{id:'image',durationMs:15000,transition:'fade',imageAssetId:'presentation-a',text:'{activity} · {outcome}'},{id:'stage',durationMs:15000,stage,text:'{activity} · {outcome}'}]}];
  visual.activityTimelines=[{activityId:'study',timelineId:'presentation'}];
  visual.saveSystem={manualSlots:2,autoSlots:1};
  for(const s of visual.screens)for(const e of s.elements)if(e.kind==='result')e.resultLayout='growth';
  for(const loading of [false,true])visual.screens.push({id:loading?'load-records':'records',label:'Records',modal:true,elements:[{id:'slots',kind:loading?'loadSlots':'saveSlots',x:20,y:20,width:800,height:500},{id:'close',kind:'button',action:'closeModal',text:'Close records',x:20,y:600,width:250,height:60}],readableLayout:{items:[{elementId:'slots'}],actions:['close']}});
  await writeFile(join(fixture.root,'project.json'),JSON.stringify(content.project));await writeFile(join(fixture.root,'assets/manifest.json'),JSON.stringify({formatVersion:'0.1.0',assets:content.assets}));
  const built=await buildSnapshot(await loadProject(fixture.root));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));content=built.snapshot;
  const target=desktopTarget(),destination=join(out,'game'),runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app');
  await exportGame({content,projectRoot:fixture.root,target,destination,runtimes:{windows:runtime,macos:runtime,web:runtime}});
  const touch=id=>[{scrollIntoView:id,waitFrames:2},{touchWindow:id,waitFrames:2}];
  const get=(r,tag)=>r.steps.find(s=>s.command.checkpoint===tag).ui;
  const proofs=[];
  for(const width of [300,390])for(const locale of ['en','ko','ja']){
    const name=`${width}-${locale}`,dir=join(out,name);await mkdir(dir);
    const height=width===300?568:844,ending=width===300?'skip':'finish';
    const steps=[{clickWindow:'start'},...[0,1,2,3].map(i=>({select:'plan-'+i,value:'study'})),{clickWindow:'advance'},
      {resize:{width,height}},{safeArea:{x:0,y:20,width,height:height-40}},...touch('engine-reading-options'),...touch('engine-reading-settings-text-200'),...touch('engine-reading-close'),...touch('advance'),
      {timelineAdvanceMs:350},{checkpoint:'image',screenshot:join(dir,'image.png')},{touchWindow:'timeline-pause',waitFrames:2},{checkpoint:'paused'},{timelineAdvanceMs:500},{checkpoint:'still-paused'},
      ...touch('timeline-speed'),{checkpoint:'faster-paused'},...touch('timeline-save'),{checkpoint:'save-modal'},{timelineAdvanceMs:500},{checkpoint:'save-modal-stable'},...touch('records__slots-request'),...touch('records__close'),
      {touchWindow:'timeline-pause',waitFrames:2},{timelineAdvanceMs:100},...touch('timeline-load'),{checkpoint:'load-modal'},{timelineAdvanceMs:500},{checkpoint:'load-modal-stable'},...touch('load-records__slots-request'),...touch('load-records__slots-confirm'),{checkpoint:'restored'},
      ...touch('engine-reading-options'),{checkpoint:'options'},{timelineAdvanceMs:500},...touch('engine-reading-close'),{checkpoint:'options-return'},
      {touchWindow:'timeline-pause',waitFrames:2},{timelineRealtime:true},{waitFrames:8},{timelineRealtime:false},{checkpoint:'clock'},
      {timelineAdvanceMs:3750},{scrollIntoView:'timeline-status'},{timelineAdvanceMs:350},{checkpoint:'stage',screenshot:join(dir,'stage.png')},
      ...Array.from({length:12},()=>({touchDrag:{fromX:width/2,fromY:260,toX:width/2,toY:110}})),{checkpoint:'end',screenshot:join(dir,'end.png')},
      ...(ending==='skip'?[{touchWindow:'timeline-skip'}]:[{timelineAdvanceMs:15000}]),{checkpoint:'growth',screenshot:join(dir,'growth.png')},
      {scheduleAdvanceMs:5000},{checkpoint:'growth-wait'},...touch('advance'),{checkpoint:'acknowledged'}];
    const input=join(dir,'steps.json'),report=join(dir,'report.json');await writeFile(input,JSON.stringify(steps));
    const run=spawnSync(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--save',join(dir,'save.json'),'--automation',input,'--report',report,'--report-checkpoints-only','--hidden','--locale',locale],{encoding:'utf8',timeout:45000});
    await writeFile(join(dir,'process.log'),(run.stdout??'')+'\n'+(run.stderr??''));assert.equal(run.status,0,run.error?.message??run.stderr);
    const r=JSON.parse(await readFile(report,'utf8'));assert.equal(r.ok,true,r.error);
    const image=get(r,'image'),staged=get(r,'stage'),growth=get(r,'growth');
    for(const ui of [image,staged]){
      assert.equal(ui.readingLayout.active,true);assert.equal(ui.readingLayout.textScale,2);assert.equal(ui.timeline.cursor.completed,false);
      assert.equal(ui.timelinePresentation.reading,true);const text=ui.timelinePresentation['timeline-text'],media=ui.timelinePresentation['timeline-visual'],pane=ui.readingLayout['reading-main'];
      assert.ok(text.rendered&&text.visible&&text.textMeasured,JSON.stringify(text));assert.match(text.rml,/Workshop/);assert.equal(text.clipped,false);assert.equal(text.overflowX,false);
      assert.ok(media.rendered&&media.visible&&media.height>=72,JSON.stringify(media));assert.equal(Number(media.opacity),1);assert.match(media.rml,/image\(/);
      assert.ok(media.x>=pane.x-.5&&media.x+media.width<=pane.x+pane.clientWidth+.5,JSON.stringify({media,pane}));
      assert.ok(media.y<pane.y+pane.clientHeight&&media.y+media.height>pane.y,'Actual media intersects the reading viewport');
      assert.ok(text.y<pane.y+pane.clientHeight&&text.y+text.height>pane.y,'Actual clip text intersects the reading viewport');
      assert.ok(pane.scrollWidth<=pane.clientWidth+1,JSON.stringify(pane));assert.ok(!ui.texts.result,'Growth must remain absent while playback owns progression');
      for(const id of ['timeline-pause','timeline-skip']){const c=ui.readingLayout.controls.find(c=>c.id===id);assert.ok(c&&c.width>=43.5&&c.height>=43.5&&c.enabled,JSON.stringify(c));assert.ok(c.y>=ui.readingLayout['reading-actions'].y-.5&&c.y+c.height<=height-20+.5,JSON.stringify(c));}
    }
    assert.equal(image.timelineStage,null);assert.equal(staged.timelineStage.layers.length,1);assert.match(staged.timelineStage.layers[0].rml,/image\(/);
    assert.deepEqual(get(r,'paused').timeline.cursor,get(r,'still-paused').timeline.cursor);
    assert.equal(get(r,'faster-paused').timeline.cursor.speed,4);
    for(const [tag,modal]of [['save-modal','records'],['load-modal','load-records']]){const u=get(r,tag);assert.equal(u.uiModal,modal);assert.equal(u.timelinePresentation,null);assert.ok(u.texts[modal+'__slots']);}
    assert.deepEqual(get(r,'save-modal').timeline.cursor,get(r,'save-modal-stable').timeline.cursor);
    assert.equal(get(r,'load-modal').timeline.cursor.paused,false);assert.deepEqual(get(r,'load-modal').timeline.cursor,get(r,'load-modal-stable').timeline.cursor,'A visible record dialog suspends an otherwise running cursor');
    assert.deepEqual(get(r,'restored').timeline.cursor,get(r,'faster-paused').timeline.cursor,'Authored record restore preserves exact paused cursor and speed');
    assert.equal(get(r,'options').timelinePresentation,null);assert.deepEqual(get(r,'options-return').timeline.cursor,get(r,'restored').timeline.cursor);
    assert.ok(get(r,'clock').timeline.cursor.elapsedMs>get(r,'restored').timeline.cursor.elapsedMs,'Real frame time advances the reading presentation');
    const end=get(r,'end').readingLayout['reading-main'];assert.ok(end.scrollTop>0&&end.scrollTop+end.clientHeight>=end.scrollHeight-2,JSON.stringify(end));
    assert.equal(growth.timelinePresentation,null);assert.equal(growth.timeline.cursor.completed,true);assert.ok(growth.texts.result);assert.deepEqual(get(r,'growth-wait').state,growth.state);
    for(const tag of ['paused','still-paused','faster-paused','save-modal','save-modal-stable','load-modal','load-modal-stable','restored','options','options-return','clock','stage','end','growth','growth-wait']){assert.deepEqual(get(r,tag).state,image.state,tag);assert.deepEqual(get(r,tag).commands,image.commands,tag);}
    assert.equal(get(r,'acknowledged').commands.filter(c=>c.type==='acknowledgeResult').length,1);
    proofs.push({width,height,locale,ending,image:image.timelinePresentation,stage:staged.timelineStage,controls:image.readingLayout.controls,end,cursorRestored:true,canonicalUnchanged:true});
  }
  await writeFile(join(out,'proof.json'),JSON.stringify({ok:true,cases:proofs},null,2));
});

test('reading inline result playback updates shaped text and retains legacy completion unless result review is enabled', {timeout:90000}, async t => {
  await mkdir('.cache/growth-summary/native',{recursive:true});const out=await mkdtemp(resolve('.cache/growth-summary/native/inline-'));t.diagnostic(`Evidence: ${out}`);
  for(const review of [false,true]){
    const dir=join(out,review?'review':'legacy');await mkdir(dir);const fixture=await phaseForecastFixture(join(dir,'project'),{opted:false}),content=structuredClone(fixture.content);delete content.project.gameplay;content.project.schedulePlayback='manual';
    content.project.visual.resultPlayback={leadInMs:800,changeMs:800,holdMs:1000};if(review)content.project.visual.activityTimelineCompletion='reveal-result';
    for(const screen of content.project.visual.screens)for(const element of screen.elements)if(element.kind==='result')element.resultLayout='growth';
    const target=desktopTarget(),destination=join(dir,'game'),runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app');await exportGame({content,projectRoot:fixture.root,target,destination,runtimes:{windows:runtime,macos:runtime,web:runtime}});
    const steps=[{clickWindow:'start'},...[0,1,2,3].map(i=>({select:'plan-'+i,value:'study'})),{clickWindow:'advance'},{clickWindow:'advance'},{resize:{width:300,height:568}},{checkpoint:'start',screenshot:join(dir,'start.png')},{timelineAdvanceMs:1600},{checkpoint:'progress'}, {touchWindow:'timeline-pause'},{key:'F5'},{checkpoint:'saved'},{key:'F9'},{checkpoint:'restored'},{touchWindow:'timeline-skip'},{checkpoint:'complete',screenshot:join(dir,'complete.png')},{click:review?'advance':'timeline-continue'},{checkpoint:'acknowledged'}];
    const input=join(dir,'steps.json'),report=join(dir,'report.json');await writeFile(input,JSON.stringify(steps));const run=spawnSync(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--save',join(dir,'save.json'),'--automation',input,'--report',report,'--report-checkpoints-only','--hidden','--locale','en','--text-scale','2'],{encoding:'utf8',timeout:30000});assert.equal(run.status,0,run.error?.message??run.stderr);const r=JSON.parse(await readFile(report,'utf8'));assert.equal(r.ok,true,r.error);const get=tag=>r.steps.find(s=>s.command.checkpoint===tag).ui;
    assert.equal(get('start').readingLayout.textScale,2);assert.ok(get('start').timelinePresentation['timeline-text'].textMeasured);assert.equal(get('start').timelinePresentation['timeline-visual'],undefined);assert.notEqual(get('start').timelinePresentation['timeline-text'].rml,get('progress').timelinePresentation['timeline-text'].rml,'The actual reading text updates as committed values interpolate');assert.deepEqual(get('saved').timeline.cursor,get('restored').timeline.cursor);
    assert.equal(get('complete').timeline.cursor.completed,true);assert.equal(get('complete').timelinePresentation===null,review);if(review)assert.ok(get('complete').texts.result);else assert.ok(get('complete').readingLayout.controls.some(c=>c.id==='timeline-continue'&&c.enabled));
    for(const tag of ['progress','saved','restored','complete']){assert.deepEqual(get(tag).state,get('start').state);assert.deepEqual(get(tag).commands,get('start').commands);}assert.equal(get('acknowledged').commands.filter(c=>c.type==='acknowledgeResult').length,1);
  }
  await writeFile(join(out,'proof.json'),JSON.stringify({ok:true,inlineLiveText:true,legacyCompletionPreserved:true,explicitReview:true,cursorRestored:true},null,2));
});
