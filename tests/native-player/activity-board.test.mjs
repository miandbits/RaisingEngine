import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,writeFile,rm} from 'node:fs/promises';
import {existsSync} from 'node:fs';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject,buildSnapshot} from '../../dist/packages/project-service/index.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {desktopTarget,gameDirectory,playerExecutable} from '../../dist/packages/export-service/targets.js';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/index.js';
import {encodeSave} from '../../dist/packages/runtime-service/save.js';
import {decodePng,encodeRgbaPng} from '../../dist/packages/studio-service/png.js';
const at=(r,k)=>r.steps.find(s=>s.command.checkpoint===k)?.ui;
const hit=id=>[{scrollIntoView:id,waitFrames:2},{clickWindow:id,waitFrames:2}];
const touch=id=>[{scrollIntoView:id,waitFrames:2},{touchWindow:id,waitFrames:2}];
const bid=(suffix,owner='planner')=>owner+'__board-'+suffix;
const node=(u,id)=>u.nodes.find(n=>n.id===id);
const same=(a,b)=>{assert.deepEqual(a.state,b.state);assert.deepEqual(a.commands,b.commands);assert.deepEqual(a.scheduleDraft,b.scheduleDraft);};
async function fixture(out,{periods=4,reading=false,locale='en',disabled=false,two=false,intermission=false,many=false,options=false,contextual=false}={}){
 const root=join(out,'project');await cp(resolve('examples/my-first-raising-game'),root,{recursive:true});await rm(join(root,'events'),{recursive:true,force:true});await mkdir(join(root,'events'));await rm(join(root,'scenarios'),{recursive:true,force:true});
 const p=JSON.parse(await readFile(join(root,'project.json'),'utf8'));p.durationMonths=3;p.periodsPerMonth=periods;delete p.calendar;p.localization={defaultLocale:locale,fallbackLocale:'en',locales:[{id:'en',label:'English'},{id:'ko',label:'한국어'},{id:'ja',label:'日本語'}],entries:[]};
 const button=(id,action,x,extra={})=>({id,kind:'button',action,text:id,x,y:640,width:200,height:56,fontSize:20,...extra});
 const planner={id:'planner',kind:'schedule',scheduleLayout:'activity-board',x:40,y:110,width:908,height:390,fontSize:18,color:'#203a34',fill:'#f5f1e8',controlStyleId:'controls',showPhaseForecast:true,showActivityDescription:true,showActivityRules:true,activityIcons:[{activityId:'study',imageAssetId:'board-study'},{activityId:'rest',imageAssetId:'board-rest'}],scheduleGroups:[{id:'learning',label:'Learning',activityIds:['study','work']},{id:'recovery',label:'Recovery',activityIds:['rest'],iconAssetId:'board-rest'},{id:'all',label:'Authored all category',activityIds:['walk']}],entryFrameAssetId:'board-frame',...(disabled?{enabledCondition:{op:'not',condition:{op:'always'}}}:{})};
 if(options)Object.assign(planner,{x:24,y:176,width:1232,height:450,activityBoard:{density:'compact',showSummary:true,advanceAfterApply:true,forecast:'inline',summaryAttributeIds:['stamina','money','skill']}});
 const tools=[button('advance','advance',40),button('save','save',260),button('load','load',480),button('restart','start',700)];
 const readingLayout={items:[{elementId:'planner',schedulePresentation:'compact',planReuse:'last-committed'}],actions:['advance','save','load','restart'],actionLayout:'grouped',primaryAction:'advance'};
 p.visual={width:1280,height:720,styles:[{id:'controls',label:'Board controls',fill:'#e5eade',color:'#203a34',borderColor:'#667766',hoverFill:'#cbd9c5',pressedFill:'#bacbb0',disabledColor:'#59665a'}],...(reading?{readableLayout:{background:'#f5f1e8',fontSize:18}}:{}),screens:[{id:'title',label:'Board fixture',elements:[button('start','start',40),button('load','load',260)],...(reading?{readableLayout:{items:[],actions:['start','load']}}:{})},{id:'schedule',label:'Plan',elements:[planner,...(two?[{...planner,id:'other',x:980,y:110,width:260,height:390,activityIcons:[],scheduleGroups:[]}]:[]),...tools],...(reading?{readableLayout:readingLayout}:{})},{id:'story',label:'Story',elements:[{id:'dialogue',kind:'dialogue',x:40,y:40,width:1000,height:400},{id:'choices',kind:'choices',x:40,y:440,width:1000,height:170},button('advance','advance',40)]},{id:'ending',label:'Ending',elements:[button('restart','start',40)]}]};
 if(intermission){p.periodFlow={intermission:{screenId:'schedule',completionFlagId:'garden-visited',entryEffects:[]}};p.gameplay={startLocationId:'home',locations:[{id:'home',name:'Home',connections:[]}],items:[],shops:[],interactions:[],actions:[{id:'finish-break',name:'Finish break',costs:[],effects:[{op:'setFlag',flagId:'garden-visited',value:true}]}]};}
 if(options){p.visual.screens[1].elements.push(button('map','navigate',930,{screenId:'directory'}));p.visual.routing={defaultScreenId:'schedule',phases:[{phase:'title',screenId:'title'},{phase:'dialogue',screenId:'story'},{phase:'choice',screenId:'story'},{phase:'finished',screenId:'ending'}]};p.visual.screens.push({id:'directory',label:'Activity map',elements:[button('open-planner','openPlanner',40,{screenId:'schedule',plannerElementId:'planner',plannerGroupId:'learning',plannerActivityId:'study'}),button('open-disabled','openPlanner',260,{screenId:'disabled-plan',plannerElementId:'disabled-board'})]}, {id:'disabled-plan',label:'Disabled planner',elements:[{...planner,id:'disabled-board',enabledCondition:{op:'not',condition:{op:'always'}}}]});}
 if(options){const directory=p.visual.screens.find(s=>s.id==='directory');for(const [id,base,active,x]of [['hidden-variant',1,0,480],['visible-variant',0,1,700]]){directory.elements.push(button('open-'+id,'openPlanner',x,{screenId:id,plannerElementId:id+'-board',plannerGroupId:'learning',plannerActivityId:'study'}));p.visual.screens.push({id,label:id,elements:[{...planner,id:id+'-board',opacity:base,variants:[{label:'Active planning visibility',when:{op:'always'},values:{opacity:active}}]}]});}}
 if(contextual){p.visual.routing.defaultScreenId='directory';p.visual.screens.find(s=>s.id==='directory').readableLayout={items:[],actions:['open-planner','open-disabled']};p.visual.screens[1].readableLayout.items[0].planningGuidance='first-use';}
 const studyFile=join(root,'activities/study.json'),study=JSON.parse(await readFile(studyFile,'utf8'));study.description='Practice with patient attention and review every instruction. '.repeat(18)+'FINAL COMPLETE DESCRIPTION.';study.requirements={op:'all',conditions:[{op:'compare',left:{op:'read',target:{kind:'attribute',attributeId:'money'}},comparator:'gte',right:{op:'literal',value:5}},{op:'compare',left:{op:'read',target:{kind:'attribute',attributeId:'stamina'}},comparator:'gte',right:{op:'literal',value:10}}]};await writeFile(studyFile,JSON.stringify(study));
 if(locale==='ja'){study.label='時間をかけて工房の仕事と仲間の計画を丁寧に確認する';await writeFile(studyFile,JSON.stringify(study));}
 if(many){const base=JSON.parse(await readFile(join(root,'activities/walk.json'),'utf8'));for(let i=0;i<15;i++){const id='extra-'+String(i).padStart(2,'0');await writeFile(join(root,'activities',id+'.json'),JSON.stringify({...base,id,label:'Practice '+(i+1)}));planner.activityIcons.push({activityId:id,imageAssetId:i%2?'board-rest':'board-study'});}planner.scheduleGroups.push({id:'craft',label:'Craft and maintenance',activityIds:['work'],iconAssetId:'board-study'},{id:'care',label:'Community and care',activityIds:['walk','rest'],iconAssetId:'board-rest'});}
 const manifest=JSON.parse(await readFile(join(root,'assets/manifest.json'),'utf8'));for(const [id,color]of[['board-study',[40,100,180]],['board-rest',[100,150,70]],['board-frame',[180,150,70]]]){const pixels=Buffer.alloc(32*32*4);for(let y=0;y<32;y++)for(let x=0;x<32;x++){const i=(y*32+x)*4;pixels.set(color,i);pixels[i+3]=id==='board-frame'&&x>1&&y>1&&x<30&&y<30?0:255;}await writeFile(join(root,'assets',id+'.png'),encodeRgbaPng({width:32,height:32,pixels}));manifest.assets.push({id,kind:'image',path:'assets/'+id+'.png'});}await writeFile(join(root,'assets/manifest.json'),JSON.stringify(manifest));await writeFile(join(root,'project.json'),JSON.stringify(p));
 const built=await buildSnapshot(await loadProject(root));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));const content=built.snapshot,target=desktopTarget(),runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??'dist/native-player-app'),destination=join(out,'game');await exportGame({content,projectRoot:root,target,destination,runtimes:{windows:runtime,macos:runtime,web:runtime}});
 const save=join(out,'progress.json');return {content,root,save,run:async(name,steps,extra=[])=>{const script=join(out,name+'.json'),report=join(out,name+'-report.json');await writeFile(script,JSON.stringify(steps.flat()));const child=spawnSync(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--save',save,'--locale',locale,'--automation',script,'--report',report,'--report-checkpoints-only','--hidden',...extra],{encoding:'utf8',timeout:90000,windowsHide:true});await writeFile(join(out,name+'-process.txt'),(child.stdout??'')+'\n'+(child.stderr??''));assert.equal(child.status,0,child.stderr??child.error?.message);const r=JSON.parse(await readFile(report,'utf8'));assert.equal(r.ok,true,r.error);return r;}};
}
async function workspace(name){await mkdir('.cache/activity-board/native',{recursive:true});return mkdtemp(resolve('.cache/activity-board/native/'+name+'-'));}

test('native activity board reviews visual cards without changing draft, commands or saves, then applies one slot',{timeout:180000},async()=>{
 const out=await workspace('desktop'),f=await fixture(out,{two:true,many:true});const session=createSession(f.content,42,{mode:'full'}).session,saved=await encodeSave(f.content,session);assert.equal(saved.ok,true);await writeFile(f.save,saved.text);
 const r=await f.run('journey',[{resize:{width:1280,height:720}},hit('load'),{checkpoint:'initial',screenshot:join(out,'initial.png')},hit(bid('slot-2')),{checkpoint:'period'},hit(bid('group-option-all')),{checkpoint:'category'},hit(bid('group-all')),hit(bid('activity-study')),{checkpoint:'inspection',screenshot:join(out,'inspected.png')},{wheelWindow:{x:700,y:315,deltaY:-4},waitFrames:2},{checkpoint:'detail-wheel'},{key:'End'},{checkpoint:'detail-end'},hit(bid('cancel')),{checkpoint:'cancel'},hit(bid('slot-2')),hit(bid('activity-rest')),hit(bid('apply')),{checkpoint:'applied',screenshot:join(out,'applied.png')},hit(bid('slot-1')),hit(bid('activity-work')),{key:'Escape'},{checkpoint:'escape'},hit(bid('slot-0','other')),hit(bid('activity-walk','other')),{checkpoint:'other'},hit(bid('cancel','other')),{checkpoint:'other-cancel'},hit(bid('help')),{checkpoint:'canvas-help'},hit('engine-planning-guide-skip'),hit('phase-forecast-planner'),{checkpoint:'canvas-forecast'},hit('engine-phase-forecast-close'),{checkpoint:'canvas-return'}]);
 const initial=at(r,'initial');for(const key of ['period','category','inspection','detail-wheel','detail-end','cancel'])same(at(r,key),initial);
 assert.equal(at(r,'period').activityBoard.slot,2);assert.equal(at(r,'period').scheduleDraft.slot,initial.scheduleDraft.slot,'Inspecting a period does not alter persisted slot');
 assert.ok(at(r,'category').elements.some(e=>e.id===bid('activity-walk')));assert.ok(!at(r,'category').elements.some(e=>e.id===bid('activity-study')),'Authored all is distinct from synthetic All');
 assert.match(at(r,'inspection').texts.planner,/FINAL COMPLETE DESCRIPTION/);assert.match(at(r,'inspection').texts.planner,/Requirements/);assert.match(at(r,'inspection').texts.planner,/Money.*≥ 5.*and.*Energy.*≥ 10/s);
 assert.ok(node(at(r,'detail-wheel'),bid('details')).scrollTop>0,'Mouse wheel scrolls the actual detail pane');assert.ok(node(at(r,'detail-end'),bid('details')).scrollTop>0,'End reaches full inspected details');
 const applied=at(r,'applied');assert.deepEqual(applied.state,initial.state);assert.deepEqual(applied.commands,initial.commands);assert.deepEqual(applied.scheduleDraft,{...initial.scheduleDraft,slot:2,selected:4,activities:['','','rest','']});assert.match(applied.texts.planner,/Period 3/);assert.equal(applied.elements.find(e=>e.id===bid('apply')).enabled,false,'Applied assignment is read-only until a new inspection');same(at(r,'escape'),applied);same(at(r,'other'),applied);same(at(r,'other-cancel'),applied);for(const k of ['canvas-help','canvas-forecast','canvas-return'])same(at(r,k),applied);assert.equal(at(r,'canvas-help').readingLayout.guidanceOpen,true);assert.equal(at(r,'canvas-forecast').phaseForecast.open,true);
 assert.equal(await readFile(f.save,'utf8'),saved.text,'Review and draft-only Apply never rewrite player save');
 for(const id of [bid('slot-0'),bid('activity-study'),bid('apply')])assert.equal(node(at(r,'inspection'),id).ownerId,'planner','Generated targets retain authored owner for diagnostics');
 const cards=initial.nodes.filter(n=>n.id===bid('activity-study')||n.id===bid('activity-rest'));assert.equal(cards.length,2);const catalog=node(initial,bid('catalog')),viewport=node(initial,bid('viewport'));assert.ok(catalog.clientHeight>=150,JSON.stringify(catalog));assert.ok(viewport.scrollHeight<=viewport.clientHeight+1,'Wide board never scrolls tickets away');assert.equal(initial.elements.filter(e=>e.id.startsWith(bid('activity-'))).length,19);assert.ok(node(initial,bid('activity-extra-00')).clickableAtCenter,'Artwork cards are immediately usable with five categories');assert.notEqual(cards[0].x,cards[1].x,'Wide catalog uses two card columns');
 assert.equal(node(at(r,'inspection'),bid('apply')).clickableAtCenter,true,'Apply remains visible beside independently scrolling detail');
 assert.equal(node(initial,bid('slot-0')).color.toLowerCase(),'#203a34');await writeFile(join(out,'proof.json'),JSON.stringify({ok:true,draftOnly:true,saveBytesUnchanged:true,ownerScoped:true},null,2));console.log('Activity board desktop evidence: '+out);
});

test('native activity board supports localized one/twelve periods, phone 200%, keyboard, touch and exact confirmation',{timeout:240000},async()=>{
 for(const [locale,periods,width,height] of [['en',1,1280,720],['ko',12,1280,720],['ja',4,300,568]]){
  const out=await workspace('layout-'+locale),phone=width===300,f=await fixture(out,{periods,reading:phone,locale});const input=phone?touch:hit,last=periods-1;
  const steps=[{resize:{width,height}},input('start'),{checkpoint:'initial',screenshot:join(out,'initial.png')},input(bid('slot-'+last)),input(bid('activity-study')),{checkpoint:'review',screenshot:join(out,'review.png')},{key:'End'},{checkpoint:'end',screenshot:join(out,'end.png')},{key:'Home'},{checkpoint:'top'},...(phone?[{touchDrag:{fromX:150,fromY:200,toX:150,toY:80},waitFrames:2}]:[{wheelWindow:{x:700,y:315,deltaY:-4},waitFrames:2}]),{checkpoint:'scrolled'},{key:'Tab'},{checkpoint:'after-tab'},input(bid('cancel')),{checkpoint:'cancel'}];
  for(let i=0;i<periods;i++)steps.push(input(bid('slot-'+i)),input(bid('activity-rest')),input(bid('apply')));
  steps.push({checkpoint:'filled'},input('advance'),{checkpoint:'confirmed'},input('advance'),{checkpoint:'executed',screenshot:join(out,'executed.png')},input(bid('slot-'+last)),{checkpoint:'disabled'});
  const r=await f.run('layout',steps,phone?['--text-scale','2']:[]),initial=at(r,'initial');for(const k of ['review','top','scrolled','end','after-tab','cancel'])same(at(r,k),initial);
  assert.match(initial.elements.find(e=>e.id===bid('slot-'+last)).label,new RegExp((locale==='en'?'Period':locale==='ko'?'일정':'予定')+' '+periods));assert.equal(at(r,'review').activityBoard.slot,last);assert.equal(at(r,'cancel').focusedId,bid('slot-'+last));
  const u=at(r,'review');for(const c of u.readingLayout.controls.filter(c=>c.id.includes('__board-')))assert.ok(c.width>=43.5&&c.height>=43.5,JSON.stringify(c));
  if(phone){for(const id of [bid('apply'),bid('cancel')]){const control=node(u,id);assert.equal(control.overflowY,false,'Wrapped action text stays inside its full button: '+JSON.stringify(control));assert.equal(control.overflowX,false);assert.ok(Math.abs(control.visibleHitbox.height-control.hitbox.height)<.1,'Full wrapped button remains visible');assert.ok(control.height>=44,'Wrapped Japanese 200% labels retain full touch targets');}const main=u.readingLayout['reading-main'],footer=u.readingLayout['reading-actions'];assert.equal(u.readingLayout.active,true);assert.ok(at(r,'scrolled').readingLayout['reading-main'].scrollTop>at(r,'top').readingLayout['reading-main'].scrollTop,'Actual touch drag scrolls inspected text');assert.ok(main.scrollWidth<=main.clientWidth+1);assert.ok(main.y+main.height<=footer.y+1);assert.ok(footer.scrollHeight<=footer.clientHeight+1);const end=at(r,'end').readingLayout['reading-main'];assert.ok(end.scrollTop+end.clientHeight>=end.scrollHeight-1,'Keyboard End reaches the final complete rule/notice line');assert.equal(at(r,'scrolled').activityBoard.activityId,u.activityBoard.activityId,'A touch drag never selects a card');assert.equal(node(u,bid('apply'))?.clickableAtCenter??u.elements.find(e=>e.id===bid('apply')).enabled,true);}
  else if(periods===12)assert.ok(node(at(r,'cancel'),bid('periods')).scrollLeft>0,'Last period stays in view after Cancel');
  assert.deepEqual(at(r,'confirmed').commands,[{type:'setSchedule',activityIds:Array(periods).fill('rest')}]);assert.deepEqual(at(r,'executed').commands,[{type:'setSchedule',activityIds:Array(periods).fill('rest')},{type:'executeActivity'}]);same(at(r,'disabled'),at(r,'executed'));
  console.log('Activity board layout evidence: '+out);
 }
});

// Inspect actual text ink, not just an un-clipped button box: narrow buttons
// can be geometrically valid while splitting ordinary English words in half.
function boardInkBands(image,viewport,box) {
 const sx=image.width/viewport.width,sy=image.height/viewport.height,rows=[];
 for(let y=Math.ceil((box.y+8)*sy);y<Math.floor((box.y+box.height-8)*sy);y++){
  let ink=0;for(let x=Math.ceil((box.x+8)*sx);x<Math.floor((box.x+box.width-8)*sx);x++){
   const at=(y*image.width+x)*4;
   if(Math.abs(image.pixels[at]-32)<24&&Math.abs(image.pixels[at+1]-58)<24&&Math.abs(image.pixels[at+2]-52)<24)ink++;
  }
  if(ink>=Math.max(1,Math.floor(sx)))rows.push(y);
 }
 const bands=[];for(const y of rows){const last=bands.at(-1);if(last&&y-last.bottom<=Math.ceil(3*sy))last.bottom=y;else bands.push({top:y,bottom:y});}
 return bands.map(b=>({top:b.top/sy,height:(b.bottom-b.top+1)/sy}));
}

test('native readable board scales ticket and helper widths with text, preserving whole words and touch browsing',{timeout:240000},async()=>{
 for(const [locale,width,height,periods]of [['en',390,844,4],['en',300,568,12],['ja',300,568,4]]){
  const out=await workspace('readable-width-'+locale+'-'+width),f=await fixture(out,{periods,reading:true,locale});
  const session=createSession(f.content,42,{mode:'full'}).session,saved=await encodeSave(f.content,session);assert.equal(saved.ok,true);await writeFile(f.save,saved.text);
  const steps=[{resize:{width,height}},touch('load'),{checkpoint:'initial',screenshot:join(out,'initial.png')},
   {touchDrag:{fromX:width-48,fromY:80,toX:40,toY:80},waitFrames:2},{checkpoint:'swiped'}];
  for(let i=0;i<periods;i++)steps.push(touch(bid('slot-'+i)),{checkpoint:'period-'+i},touch(bid('cancel')),{checkpoint:'cancel-'+i});
  steps.push(touch(bid('slot-'+(periods-1))),touch(bid('group-option-learning')),{checkpoint:'category'},touch(bid('activity-study')),{checkpoint:'inspected',screenshot:join(out,'inspected.png')},touch(bid('cancel')),{checkpoint:'cancel'},
   {scrollIntoView:'phase-forecast-planner',waitFrames:2},{checkpoint:'forecast-label',screenshot:join(out,'forecast-label.png')},touch('phase-forecast-planner'),{checkpoint:'forecast'},touch('engine-phase-forecast-close'),
   {scrollIntoView:bid('reuse'),waitFrames:2},{checkpoint:'reuse-label',screenshot:join(out,'reuse-label.png')},
   {scrollIntoView:bid('help'),waitFrames:2},{checkpoint:'help-label',screenshot:join(out,'help-label.png')},touch(bid('help')),{checkpoint:'help'},touch('engine-planning-guide-skip'),{checkpoint:'returned'});
  const r=await f.run('readable-width',steps,['--text-scale','2']),initial=at(r,'initial');
  for(const step of r.steps)if(step.command.checkpoint)same(step.ui,initial);
  const strip=node(initial,bid('periods')),ticket=node(initial,bid('slot-0'));
  assert.equal(initial.readingLayout.active,true);assert.equal(ticket.fontSize,'36px');
  assert.ok(ticket.width>=strip.clientWidth*.8&&ticket.width<=strip.clientWidth,JSON.stringify({ticket,strip}));
  assert.ok(Math.abs(ticket.visibleHitbox.width-ticket.hitbox.width)<.1&&Math.abs(ticket.visibleHitbox.height-ticket.hitbox.height)<.1,'One complete ticket remains visible at 200%');
  assert.ok(node(at(r,'swiped'),bid('periods')).scrollLeft>strip.scrollLeft,'Real horizontal touch drag browses the strip without choosing an activity');
  const image=decodePng(await readFile(join(out,'initial.png'))),bands=boardInkBands(image,{width,height},ticket);
  if(locale==='en'){
   assert.equal(ticket.label,'Period 1 · Choose activity');
   assert.ok(bands.length>=2&&bands.length<=3,'Period and the two complete English words use two or three lines, never four fragmented word lines: '+JSON.stringify(bands));
   assert.ok(bands.every(b=>b.height>=18),'Every ticket text line retains full glyph height: '+JSON.stringify(bands));
  }
  for(let i=0;i<periods;i++){assert.equal(at(r,'period-'+i).activityBoard.slot,i);assert.equal(at(r,'cancel-'+i).focusedId,bid('slot-'+i));}
  assert.equal(at(r,'category').activityBoard.group,'learning');assert.equal(at(r,'inspected').activityBoard.activityId,'study');
  const helpers=[];
  for(const [checkpoint,id]of [['forecast-label','phase-forecast-planner'],['reuse-label',bid('reuse')],['help-label',bid('help')]]){
   const u=at(r,checkpoint),control=node(u,id),main=u.readingLayout['reading-main'];helpers.push({checkpoint,control});
   assert.ok(control.width>=main.clientWidth-10,'Large-text helpers use independent full-width rows: '+JSON.stringify(control));
   assert.ok(Math.abs(control.visibleHitbox.width-control.hitbox.width)<.1&&Math.abs(control.visibleHitbox.height-control.hitbox.height)<=1,'Full helper target is reachable within one point of scroll rounding: '+JSON.stringify(control));
   assert.equal(control.overflowX,false);assert.equal(control.overflowY,false);
   assert.ok(main.scrollWidth<=main.clientWidth+1,'Only the period strip scrolls horizontally');
  }
  assert.equal(at(r,'forecast').phaseForecast.open,true);assert.equal(at(r,'help').readingLayout.guidanceOpen,true);
  assert.equal(await readFile(f.save,'utf8'),saved.text,'Browsing, categories and helpers never rewrite a player save');
  await writeFile(join(out,'readable-width-proof.json'),JSON.stringify({ok:true,locale,width,height,periods,textScale:2,ticket,strip,bands,helpers,saveBytesUnchanged:true,canonicalAndDraftUnchanged:true},null,2));
  console.log('Activity board readable width evidence: '+out);
 }
});

test('native activity board locks completed/intermission periods and keeps forecast, help and opted reuse available',{timeout:180000},async()=>{
 const out=await workspace('helpers'),f=await fixture(out,{intermission:true,reading:true});let session=createSession(f.content,42,{mode:'full'}).session;for(const command of [{type:'setSchedule',activityIds:['rest','work','study','walk']},{type:'executeActivity'},{type:'acknowledgeResult'}]){const reply=dispatchSession(f.content,session,command,session.state.stateRevision);assert.equal(reply.ok,true,JSON.stringify(reply.error));session=reply.session;}assert.equal(session.state.activityIntermission,true);const saved=await encodeSave(f.content,session);assert.equal(saved.ok,true);await writeFile(f.save,saved.text);
 const r=await f.run('helpers',[hit('load'),{checkpoint:'loaded'},hit(bid('slot-0')),{checkpoint:'past'},hit(bid('slot-1')),hit(bid('activity-rest')),hit(bid('apply')),{checkpoint:'draft'},hit('phase-forecast-planner'),{checkpoint:'forecast'},hit('engine-phase-forecast-close'),hit(bid('help')),{checkpoint:'help'},hit('engine-planning-guide-skip'),hit(bid('reuse')),{checkpoint:'reuse'},hit('board-reuse-cancel'),{checkpoint:'reuse-cancel'}]);
 same(at(r,'past'),at(r,'loaded'));assert.equal(at(r,'loaded').elements.find(e=>e.id===bid('slot-0')).enabled,false);assert.equal(at(r,'draft').scheduleDraft.activities[0],'rest');for(const key of ['forecast','help','reuse','reuse-cancel'])same(at(r,key),at(r,'draft'));assert.equal(at(r,'forecast').phaseForecast.open,true);assert.equal(at(r,'help').readingLayout.guidanceOpen,true);assert.equal(at(r,'reuse').readingLayout.planReuse.open,true);assert.equal(await readFile(f.save,'utf8'),saved.text);
 console.log('Activity board helpers evidence: '+out);
});

test('native activity board rejects stale owner, replacement, draft and availability tokens without mutation',{timeout:150000},async()=>{
 const out=await workspace('guards'),f=await fixture(out),input=join(out,'snapshot.json'),source=join(out,'guards.cpp'),exe=join(out,process.platform==='win32'?'guards.exe':'guards');await writeFile(input,JSON.stringify(f.content));
 await writeFile(source,`#include <SDL3/SDL_timer.h>
#include "screen.hpp"
#include <iostream>
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(int argc,char** argv){GameSession g;g.content=read_json(fs::u8path(argv[1]));g.start();ScreenMarkup m(g,Json::object(),fs::path());m.display_content=g.content;m.locale="en";m.screen_id="schedule";for(const auto& s:g.content.at("project").at("visual").at("screens"))if(s.at("id")=="schedule")m.render_elements=s.at("elements");m.ensure_plan();m.activity_board_open=true;m.activity_board_owner=m.screen_id;m.activity_board_element="planner";m.activity_board_slot=2;m.activity_board_activity="rest";m.activity_board_snapshot=m.activity_board_token();check(m.activity_board_valid(),"Initial valid review rejected");const auto state=g.state,commands=g.commands,draft=m.plan_save();
++m.reuse_generation;check(!m.activity_board_valid(),"Same-state replacement accepted");--m.reuse_generation;
g.state["stateRevision"]=g.state.at("stateRevision").get<int>()+1;check(!m.activity_board_valid(),"Changed revision accepted");g.state=state;
m.plan[2]="work";check(!m.activity_board_valid(),"Changed draft accepted");m.restore_plan(draft);
m.plan_slot=1;check(!m.activity_board_valid(),"Changed persisted slot accepted");m.restore_plan(draft);
m.plan_selected=4;check(!m.activity_board_valid(),"Changed selected mask accepted");m.restore_plan(draft);
m.screen_id="story";check(!m.activity_board_valid(),"Changed screen accepted");m.screen_id="schedule";
m.ui_modal="help";check(!m.activity_board_valid(),"Authored modal accepted");m.ui_modal.clear();
for(auto& e:m.render_elements)if(e.at("id")=="planner")e["_boundEnabled"]=false;check(!m.activity_board_valid(),"Disabled owner accepted");for(auto& e:m.render_elements)if(e.at("id")=="planner")e.erase("_boundEnabled");
for(auto& e:m.render_elements)if(e.at("id")=="planner")e["opacity"]=0;check(!m.activity_board_valid(),"Invisible owner accepted");for(auto& e:m.render_elements)if(e.at("id")=="planner")e.erase("opacity");
for(auto& e:m.render_elements)if(e.at("id")=="planner")e["scheduleLayout"]="board";check(!m.activity_board_valid(),"Changed widget kind accepted");for(auto& e:m.render_elements)if(e.at("id")=="planner")e["scheduleLayout"]="activity-board";
g.state["activityIntermission"]=true;g.state["periodIndex"]=2;check(!m.activity_board_valid(),"Already executed period accepted");g.state=state;
check(m.activity_board_valid(),"Original review no longer valid");auto widget=*m.activity_board_widget("planner");m.activity_board_activity="study";const auto shown=m.activity_board_details(widget);check(shown.find("FINAL COMPLETE DESCRIPTION")!=std::string::npos,"Description positive control missing");check(shown.find("Requirements")!=std::string::npos,"Rules positive control missing");widget["showActivityDescription"]=false;widget["showActivityRules"]=false;const auto hidden=m.activity_board_details(widget);check(hidden.find("FINAL COMPLETE DESCRIPTION")==std::string::npos,"Description opt-out ignored");check(hidden.find("Requirements")==std::string::npos,"Rules opt-out ignored");check(g.state==state&&g.commands==commands&&m.plan_save()==draft,"Read-only checks changed live state or draft");m.close_activity_board();check(m.plan_save()==draft&&g.state==state&&g.commands==commands,"Cancel changed draft or canonical state");std::cout<<"{\\"ok\\":true,\\"guards\\":16}";}
`);
 const mingw=join(process.env.QT_COMPILER??'C:/Program Files/mingw64/bin','g++.exe'),compiler=process.env.CXX??(process.platform==='win32'?(existsSync(mingw)?mingw:'g++'):'c++'),library=resolve(process.env.NATIVE_PLAYER_BUILD??'dist/native-player','core/libraising_core.a');
 const built=spawnSync(compiler,['-std=c++17','-O1','-I',resolve('native/player'),'-I',resolve('native/include'),'-I',resolve('native/vendor'),'-I',resolve('native/player/vendor/SDL/include'),source,library,'-o',exe],{encoding:'utf8',timeout:90000});assert.equal(built.status,0,built.stderr||String(built.error));const run=spawnSync(exe,[input],{encoding:'utf8',timeout:30000});assert.equal(run.status,0,run.stderr);assert.equal(JSON.parse(run.stdout).guards,16);await writeFile(join(out,'proof.json'),run.stdout);console.log('Activity board guards evidence: '+out);
});


test('native compact board shows rule chips and shared-core forecast while Apply alone advances to the next unfilled period',{timeout:240000},async()=>{
 for(const [locale,width,height]of [['en',1280,720],['en',300,568],['ja',390,844]]){
  const out=await workspace('compact-'+locale+'-'+width),phone=width<640,f=await fixture(out,{reading:phone,locale,many:true,options:true}),input=phone?touch:hit;
  const session=createSession(f.content,42,{mode:'full'}).session,saved=await encodeSave(f.content,session);assert.equal(saved.ok,true);await writeFile(f.save,saved.text);
  const steps=[{resize:{width,height}},input('load'),{checkpoint:'initial',screenshot:join(out,'initial.png')},input(bid('slot-2')),input(bid('activity-study')),{checkpoint:'inspect',screenshot:join(out,'inspect.png')},input(bid('cancel')),{checkpoint:'cancel'}];
  for(const i of [2,3,0,1])steps.push(input(bid('slot-'+i)),input(bid('activity-rest')),input(bid('apply')),{checkpoint:'apply-'+i});
  steps.push({checkpoint:'complete',screenshot:join(out,'complete.png')},input('phase-forecast-planner'),{checkpoint:'forecast'},input('engine-phase-forecast-close'),{checkpoint:'return'});
  const r=await f.run('compact',steps,phone?['--text-scale','2']:[]),initial=at(r,'initial');same(at(r,'inspect'),initial);same(at(r,'cancel'),initial);
  for(const [slot,next]of [[2,3],[3,0],[0,1],[1,1]]){const u=at(r,'apply-'+slot);assert.equal(u.scheduleDraft.slot,next);assert.equal(u.focusedId,bid('slot-'+next));assert.deepEqual(u.state,initial.state);assert.deepEqual(u.commands,initial.commands);}
  const complete=at(r,'complete');assert.deepEqual(complete.scheduleDraft.activities,Array(4).fill('rest'));assert.equal(complete.scheduleDraft.selected,15);same(at(r,'forecast'),complete);same(at(r,'return'),complete);assert.equal(at(r,'forecast').phaseForecast.requests,complete.phaseForecast.requests,'Details reuses the exact inline core result');assert.equal(at(r,'forecast').phaseForecast.result.ok,true);assert.equal(at(r,'forecast').phaseForecast.result.forecast.complete,true);
  assert.equal(await readFile(f.save,'utf8'),saved.text);for(const control of at(r,'inspect').readingLayout.controls.filter(c=>c.id.includes('__board-')))assert.ok(control.width>=43.5&&control.height>=43.5,JSON.stringify(control));
  if(!phone){assert.match(initial.texts.planner,/Incomplete plan/);assert.match(at(r,'inspect').texts.planner,/Cost Money -/);assert.match(complete.texts.planner,/End of this phase/);assert.match(complete.texts.planner,/Selected/);
   const catalog=node(initial,bid('catalog')),cards=initial.nodes.filter(n=>n.id.startsWith(bid('activity-'))&&n.visibleHitbox.height>=n.hitbox.height-1);assert.ok(cards.length>=4,'At least two complete compact card rows are immediately visible: '+JSON.stringify({catalog,cards}));assert.ok(node(initial,bid('viewport')).scrollHeight<=node(initial,bid('viewport')).clientHeight+1);assert.equal(node(at(r,'inspect'),bid('apply')).clickableAtCenter,true);
   for(const row of at(r,'forecast').phaseForecast.result.forecast.attributes.filter(a=>a.delta!==0).slice(0,4))assert.ok(complete.texts.planner.includes(String(row.after)),'Inline values come from the same core reply');
  }else{const u=at(r,'inspect');assert.ok(u.readingLayout['reading-main'].scrollWidth<=u.readingLayout['reading-main'].clientWidth+1);for(const id of [bid('apply'),bid('cancel')]){assert.equal(node(u,id).overflowX,false);assert.equal(node(u,id).overflowY,false);}}
  await writeFile(join(out,'compact-proof.json'),JSON.stringify({ok:true,locale,width,height,coreForecast:at(r,'forecast').phaseForecast.result,requests:complete.phaseForecast.requests,saveBytesUnchanged:true,canonicalCommandsUnchanged:true},null,2));console.log('Compact board native evidence: '+out);
 }
});


test('native contextual entry navigates once, rejects disabled targets before mutation, and keeps inspection draft-only',{timeout:120000},async()=>{
 const out=await workspace('contextual'),f=await fixture(out,{options:true});const r=await f.run('context',[{resize:{width:1280,height:720}},hit('start'),hit(bid('slot-2')),hit(bid('activity-rest')),hit(bid('apply')),{checkpoint:'draft'},hit('map'),{checkpoint:'map'},hit('open-disabled'),{checkpoint:'rejected'},hit('open-planner'),{checkpoint:'entry',screenshot:join(out,'entry.png')},hit(bid('cancel')),{checkpoint:'cancel'},hit('map'),hit('open-planner'),hit(bid('apply')),{checkpoint:'applied'},hit('map'),{checkpoint:'variant-map'},hit('open-hidden-variant'),{checkpoint:'variant-rejected',screenshot:join(out,'hidden-variant-rejected.png')},hit('open-visible-variant'),{checkpoint:'variant-entry',screenshot:join(out,'visible-variant-entry.png')},hit(bid('cancel','visible-variant-board')),{checkpoint:'variant-cancel'}]);
 const draft=at(r,'draft'),map=at(r,'map'),entry=at(r,'entry');same(at(r,'rejected'),map);assert.equal(map.state.uiScreenId,'directory');assert.equal(entry.state.uiScreenId,'schedule');assert.equal(entry.commands.length,map.commands.length+1);assert.deepEqual(entry.commands.at(-1),{type:'navigate',screenId:'schedule'});assert.deepEqual(entry.scheduleDraft,draft.scheduleDraft);assert.equal(entry.activityBoard.group,'learning');assert.equal(entry.activityBoard.activityId,'study');assert.equal(entry.activityBoard.slot,3);same(at(r,'cancel'),entry);assert.equal(at(r,'applied').scheduleDraft.activities[3],'study');assert.equal(at(r,'applied').scheduleDraft.slot,0);assert.equal(at(r,'applied').commands.at(-1).type,'navigate');assert.deepEqual(entry.state.attributes,map.state.attributes);assert.deepEqual(entry.state.rng,map.state.rng);
 const variantMap=at(r,'variant-map'),variantEntry=at(r,'variant-entry');assert.equal(variantMap.elements.find(e=>e.id==='open-hidden-variant').enabled,false,'Active opacity-zero variant disables the destination button');assert.equal(variantMap.elements.find(e=>e.id==='open-visible-variant').enabled,true,'Active visible variant overrides base opacity zero');same(at(r,'variant-rejected'),variantMap);assert.equal(variantEntry.screenId,'visible-variant');assert.equal(variantEntry.commands.length,variantMap.commands.length+1);assert.deepEqual(variantEntry.commands.at(-1),{type:'navigate',screenId:'visible-variant'});assert.deepEqual(variantEntry.scheduleDraft,variantMap.scheduleDraft);assert.equal(variantEntry.activityBoard.valid,true);assert.equal(variantEntry.activityBoard.group,'learning');assert.equal(variantEntry.activityBoard.activityId,'study');assert.ok(node(variantEntry,bid('apply','visible-variant-board')).clickableAtCenter,'Visible variant opens the actual usable board');same(at(r,'variant-cancel'),variantEntry);console.log('Contextual planner native evidence: '+out);
});


test('native phone contextual entry waits for first-use guidance, then opens the requested inspection without another command',{timeout:120000},async()=>{
 const out=await workspace('contextual-guide'),f=await fixture(out,{options:true,contextual:true,reading:true});const r=await f.run('context-guide',[{resize:{width:390,height:844}},touch('start'),{checkpoint:'map'},touch('open-planner'),{checkpoint:'guide',screenshot:join(out,'guide.png')},touch('engine-planning-guide-skip'),{checkpoint:'entry',screenshot:join(out,'entry.png')},touch(bid('cancel')),{checkpoint:'cancel'}],['--text-scale','2']);
 const map=at(r,'map'),guide=at(r,'guide'),entry=at(r,'entry');assert.equal(map.screenId,'directory');assert.equal(guide.readingLayout.guidanceOpen,true);assert.deepEqual(guide.commands,[{type:'navigate',screenId:'schedule'}]);same(entry,guide);assert.equal(entry.activityBoard.open,true);assert.equal(entry.activityBoard.group,'learning');assert.equal(entry.activityBoard.activityId,'study');assert.equal(entry.readingLayout.guidanceOpen,false);assert.doesNotMatch(entry.notice??entry.message??'',/unavailable/);same(at(r,'cancel'),entry);console.log('Contextual guide native evidence: '+out);
});
