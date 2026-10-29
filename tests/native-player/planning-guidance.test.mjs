import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,writeFile,stat,rename} from 'node:fs/promises';
import {join,resolve} from 'node:path';
import {spawn,spawnSync} from 'node:child_process';
import {setTimeout as delay} from 'node:timers/promises';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {desktopTarget,gameDirectory,playerExecutable} from '../../dist/packages/export-service/targets.js';

import {decodePng,encodeRgbaPng} from '../../dist/packages/studio-service/png.js';

const touch=id=>({scrollIntoView:id,touchWindow:id,waitFrames:2});
const at=(r,name)=>r.steps.find(s=>s.command.checkpoint===name)?.ui;
const button=(id,action,text=id,extra={})=>({id,kind:'button',action,text,autoLabel:false,x:24,y:620,width:230,height:48,...extra});
const leaf=(id,kind,extra={})=>({id,kind,x:24,y:40,width:1200,height:530,...extra});
const reading=(items,actions,primaryAction)=>({items:items.map(item=>typeof item==='string'?{elementId:item}:item),actions,actionLayout:'grouped',...(primaryAction?{primaryAction}:{})});
async function fixture(out,{periods=3,action='advance',locale='en',disabled=false,guided=true,categories=true,glyphs=false,invisiblePrimary=false}={}){
  const key=`${periods}-${action}-${locale}-${disabled}-${guided}-${categories}-${glyphs}-${invisiblePrimary}`,root=join(out,'project-'+key);await cp(resolve('examples/my-first-raising-game'),root,{recursive:true});
  const file=join(root,'project.json'),p=JSON.parse(await readFile(file,'utf8'));p.periodsPerMonth=periods;if(periods===5)p.calendar={monthsPerYear:12,daysPerMonth:30,periodLabel:"Watch / 見張り"};
  if(!p.localization.locales.some(l=>l.id===locale))p.localization.locales.push({id:locale,label:locale});
  const utilities=[button('save','save','Save'),button('load','load','Load')];
  p.visual={width:1280,height:720,readableLayout:{background:'#f5efdf'},saveSystem:{manualSlots:6,autoSlots:3},screens:[
    {id:'title',label:'Title',elements:[leaf('title-text','text',{text:glyphs?'説込戻':'A readable planning fixture.'}),...(glyphs?[leaf('missing-glyphs','text',{text:'\u0378\u0378\u0378'})]:[]),button('start','start','Begin'),...utilities],readableLayout:reading(['title-text',...(glyphs?['missing-glyphs']:[])],['start','save','load'],'start')},
    {id:'schedule',label:'Garden workshop',elements:[leaf('planner','schedule',{scheduleLayout:'board',visibleWhen:'planning',...(disabled?{enabledCondition:{op:'not',condition:{op:'always'}}}:{}),scheduleGroups:categories?[{id:'learning',label:'Learn / 学ぶ',activityIds:['study','walk']},{id:'recovery',label:'Rest and work',activityIds:['rest','work']}]:[]}),leaf('result','result',{visibleWhen:'result'}),button('advance',action,action==='commitPlan'?'Launch this plan':'Confirm my plan',invisiblePrimary?{opacity:0}:{}),...(invisiblePrimary?[button('visible-advance','advance','Confirm visible plan')]:[]),...(action==='commitPlan'?[button('continue','advance','Continue')]:[]),...utilities],readableLayout:reading([{elementId:'planner',schedulePresentation:'compact',...(guided?{planningGuidance:'first-use'}:{})},'result'],['advance','save','load',...(invisiblePrimary?['visible-advance']:[]),...(action==='commitPlan'?['continue']:[])],'advance')},
    {id:'story',label:'Story',elements:[leaf('story','dialogue'),leaf('choices','choices'),button('advance','advance','Continue'),...utilities],readableLayout:reading(['story','choices'],['advance','save','load'],'advance')},
    {id:'ending',label:'Ending',elements:[leaf('ending','text',{binding:'ending'}),button('start','start','Begin'),...utilities],readableLayout:reading(['ending'],['save','load','start'],'save')},
    ...[false,true].map(load=>({id:load?'load-records':'records',label:'Records',modal:true,elements:[leaf('slots',load?'loadSlots':'saveSlots'),button('close','closeModal','Return')],readableLayout:reading(['slots'],['close'],'close')})),
  ]};
  await writeFile(file,JSON.stringify(p));const built=await buildSnapshot(await loadProject(root));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  const target=desktopTarget(),destination=join(out,'game-'+key),runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app');
  await exportGame({projectRoot:root,content:built.snapshot,target,destination,runtimes:{windows:runtime,macos:runtime,web:runtime}});
  return {content:built.snapshot,gameId:p.gameId,runtime,destination,target,run:async(name,steps,{profile=name,extra=[]}={})=>{
    const directory=join(out,profile);await mkdir(directory,{recursive:true});const save=join(directory,'progress.json'),input=join(out,name+'.json'),report=join(out,name+'-report.json');await writeFile(input,JSON.stringify(steps));
    const result=spawnSync(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--save',save,'--text-scale','2','--locale',locale,'--automation',input,'--report',report,'--report-checkpoints-only','--hidden',...extra],{encoding:'utf8',timeout:90000,windowsHide:true});
    await writeFile(join(out,name+'-process.txt'),(result.stdout??'')+'\n'+(result.stderr??''));assert.equal(result.status,0,result.error?.message??result.stderr);
    const data=JSON.parse(await readFile(report,'utf8'));assert.equal(data.ok,true);return data;
  }};
}
const start=(width=390,height=844)=>[{resize:{width,height}},{waitFrames:4},touch('start'),{checkpoint:'opened'}];
function unchanged(a,b){assert.deepEqual(a.state,b.state);assert.deepEqual(a.commands,b.commands);assert.deepEqual(a.scheduleDraft,b.scheduleDraft);}
function fit(ui){const r=ui.readingLayout,main=r['reading-main'],footer=r['reading-actions'];assert.equal(r.active,true);assert.ok(main.height>0);assert.ok(main.scrollWidth<=main.clientWidth+1,'No horizontal overflow');assert.ok(main.y+main.height<=footer.y+.5,'No footer overlap');assert.ok(footer.scrollHeight<=footer.clientHeight+1,'No independent footer scrolling');for(const c of r.controls)assert.ok(c.width>=43.5&&c.height>=43.5,JSON.stringify(c));}


function glyphIndex(font,codepoint){
  const count=font.readUInt16BE(4);let cmap;
  for(let i=0;i<count;i++){const at=12+i*16;if(font.toString('ascii',at,at+4)==='cmap')cmap=font.readUInt32BE(at+8);}
  assert.notEqual(cmap,undefined,'OpenType cmap exists');
  const tables=font.readUInt16BE(cmap+2);
  for(let t=0;t<tables;t++){
    const rec=cmap+4+t*8,platform=font.readUInt16BE(rec);if(platform!==0&&platform!==3)continue;
    const base=cmap+font.readUInt32BE(rec+4),format=font.readUInt16BE(base);
    if(format===12){const groups=font.readUInt32BE(base+12);for(let i=0;i<groups;i++){const g=base+16+i*12,start=font.readUInt32BE(g),end=font.readUInt32BE(g+4);if(codepoint>=start&&codepoint<=end)return font.readUInt32BE(g+8)+codepoint-start;}}
    if(format===4&&codepoint<=65535){const n=font.readUInt16BE(base+6)/2,ends=base+14,starts=ends+n*2+2,deltas=starts+n*2,offsets=deltas+n*2;for(let i=0;i<n;i++)if(codepoint>=font.readUInt16BE(starts+i*2)&&codepoint<=font.readUInt16BE(ends+i*2)){const delta=font.readInt16BE(deltas+i*2),offset=font.readUInt16BE(offsets+i*2);if(!offset)return (codepoint+delta)&65535;const index=font.readUInt16BE(offsets+i*2+offset+2*(codepoint-font.readUInt16BE(starts+i*2)));if(index)return(index+delta)&65535;}}
  }
  return 0;
}
function inkCount(image,node,scale){let count=0;for(let y=Math.ceil(node.y*scale);y<Math.floor((node.y+node.height)*scale);y++)for(let x=Math.ceil(node.x*scale);x<Math.floor((node.x+node.width)*scale);x++){const i=(y*image.width+x)*4;if(image.pixels[i]<90&&image.pixels[i+1]<110&&image.pixels[i+2]<110)count++;}return count;}

test('native first-use planning guide is local, dynamic, dismissible and remembered per game',{timeout:300000},async()=>{
  await mkdir('.cache/native-planning-guidance',{recursive:true});const out=await mkdtemp(resolve('.cache/native-planning-guidance/run-')),f=await fixture(out);
  const r=await f.run('ack', [...start(),{key:'F5'},{checkpoint:'save-shortcut'},{key:'F9'},{checkpoint:'load-shortcut'},touch('engine-planning-guide-next'),{checkpoint:'step2'},touch('engine-planning-guide-back'),{checkpoint:'back'},touch('engine-planning-guide-next'),touch('engine-planning-guide-next'),{checkpoint:'step3',screenshot:join(out,'step3-en-200.png')},touch('engine-planning-guide-done'),{checkpoint:'done'},touch('board-help'),{checkpoint:'reopen'},{key:'Escape'},{checkpoint:'escape'}],{profile:'remembered'});
  assert.equal(at(r,'opened').readingLayout.guidanceOpen,true);assert.match(at(r,'opened').texts['engine-planning-guide'],/3 periods/);assert.equal(at(r,'back').readingLayout.guidanceStep,0);
  assert.match(at(r,'step2').texts['engine-planning-guide'],/Neither action advances time/);assert.match(at(r,'step3').texts['engine-planning-guide'],/Confirm my plan/);assert.match(at(r,'step3').texts['engine-planning-guide'],/confirms the schedule while planning/);
  for(const n of ['save-shortcut','load-shortcut','step2','back','step3','done','reopen','escape']){unchanged(at(r,n),at(r,'opened'));fit(at(r,n));}
  for(const n of ['save-shortcut','load-shortcut']){assert.equal(at(r,n).readingLayout.guidanceOpen,true);assert.equal(at(r,n).uiModal,'');}
  assert.equal(at(r,'done').readingLayout.guidanceAcknowledged,true);assert.equal(at(r,'done').readingLayout.guidanceOpen,false);assert.equal(at(r,'done').focusId,'board-help');assert.equal(at(r,'reopen').readingLayout.guidanceOpen,true);
  const next=at(r,'opened').readingLayout.controls.find(c=>c.id==='engine-planning-guide-next');
  const drag=await f.run('drag-guide',[...start(),{touchDrag:{fromX:next.x+next.width/2,fromY:next.y+next.height/2,toX:next.x+next.width/2,toY:next.y+next.height/2-110}},{checkpoint:'dragged'}]);assert.equal(at(drag,'dragged').readingLayout.guidanceStep,0);unchanged(at(drag,'dragged'),at(drag,'opened'));
  const prefs=join(out,'remembered',`settings-${f.gameId}.json`),saved=JSON.parse(await readFile(prefs,'utf8'));assert.deepEqual(saved.guidance,{'compact-planning-v1':true});
  const fresh=await f.run('fresh',start(),{profile:'remembered'});assert.equal(at(fresh,'opened').readingLayout.guidanceOpen,false);assert.equal(at(fresh,'opened').readingLayout.guidanceAcknowledged,true);
  const corruptDir=join(out,'corrupt');await mkdir(corruptDir);const corrupt=join(corruptDir,`settings-${f.gameId}.json`);await writeFile(corrupt,'{broken original');
  const bad=await f.run('corrupt-prefs',[...start(),touch('engine-planning-guide-skip'),touch('board-help'),{key:'Escape'},touch('board-slot-0'),touch('board-activity-study'),touch('board-cancel'),{checkpoint:'dismissed'}],{profile:'corrupt'});
  assert.equal(at(bad,'dismissed').readingLayout.guidanceOpen,false);assert.equal(at(bad,'dismissed').readingLayout.guidanceAcknowledged,true);assert.equal(await readFile(corrupt,'utf8'),'{broken original');unchanged(at(bad,'dismissed'),at(bad,'opened'));
  const unavailable=join(out,'unavailable');await mkdir(unavailable);await mkdir(join(unavailable,`settings-${f.gameId}.json.tmp`));
  const session=await f.run('unavailable',[...start(),touch('engine-planning-guide-skip'),{checkpoint:'done'}],{profile:'unavailable'});assert.equal(at(session,'done').readingLayout.guidanceAcknowledged,true);assert.equal(await stat(join(unavailable,`settings-${f.gameId}.json`)).then(()=>true,()=>false),false);
  for(const [width,height] of [[320,568],[844,390]]){const p=await f.run('geometry-'+width,[...start(width,height),{checkpoint:'guide',screenshot:join(out,`guide-${width}.png`)},{resize:{width:1280,height:720}},{checkpoint:'resized'},touch('engine-planning-guide-skip'),{checkpoint:'canvas'}]);fit(at(p,'guide'));assert.equal(at(p,'resized').readingLayout.active,true);assert.equal(at(p,'canvas').readingLayout.active,false);unchanged(at(p,'canvas'),at(p,'opened'));}
  const five=await fixture(out,{periods:5,action:'commitPlan'}),steps=[...start(),touch('engine-planning-guide-next'),touch('engine-planning-guide-next'),{checkpoint:'launch'},touch('engine-planning-guide-done')];
  for(let i=0;i<5;i++)steps.push(touch('board-slot-'+i),touch('board-activity-rest'),touch('board-apply'));
  steps.push({checkpoint:'draft'},touch('advance'),{checkpoint:'executed'},touch('engine-reading-menu'),touch('continue'),touch('board-help'),{checkpoint:'remaining'});const launch=await five.run('five',steps);
  assert.match(at(launch,'opened').texts['engine-planning-guide'],/5 periods/);assert.match(at(launch,'opened').texts['engine-planning-guide'],/Watch \/ 見張り 1/);assert.match(at(launch,'launch').texts['engine-planning-guide'],/Launch this plan/);assert.match(at(launch,'launch').texts['engine-planning-guide'],/immediately runs/);unchanged({...at(launch,'draft'),scheduleDraft:at(launch,'opened').scheduleDraft},at(launch,'opened'));
  assert.match(at(launch,'remaining').texts['engine-planning-guide'],/4 remaining periods/);assert.match(at(launch,'remaining').texts['engine-planning-guide'],/Watch \/ 見張り 2/);
  const noCategories=await fixture(out,{categories:false}),simple=await noCategories.run('no-categories',[...start(),touch('engine-planning-guide-next'),{checkpoint:'step2'}]);assert.doesNotMatch(at(simple,'step2').texts['engine-planning-guide'],/Categor/);
  const invisible=await fixture(out,{invisiblePrimary:true}),visible=await invisible.run('invisible-primary',[...start(),touch('engine-planning-guide-next'),touch('engine-planning-guide-next'),{checkpoint:'step3'}]);
  assert.match(at(visible,'step3').texts['engine-planning-guide'],/Confirm visible plan/);assert.doesNotMatch(at(visible,'step3').texts['engine-planning-guide'],/Confirm my plan/);unchanged(at(visible,'step3'),at(visible,'opened'));
  const types=at(launch,'executed').commands.map(c=>c.type);assert.equal(types.filter(t=>t==='setSchedule').length,1);assert.equal(types.filter(t=>t==='executeActivity').length,1);
  const cjk=await fixture(out,{locale:'ja'}),ja=await cjk.run('japanese',[...start(320,568),touch('engine-planning-guide-next'),{checkpoint:'guide',screenshot:join(out,'guide-ja-200.png')},touch('engine-planning-guide-skip')]);assert.match(at(ja,'guide').texts['engine-planning-guide'],/キャンセル/);fit(at(ja,'guide'));
  const disabled=await fixture(out,{disabled:true}),blocked=await disabled.run('disabled',start());assert.equal(at(blocked,'opened').readingLayout.guidanceOpen,false);
  const preview=await f.run('preview',[{resize:{width:390,height:844}},{checkpoint:'preview'}],{profile:'preview',extra:['--preview-screen','schedule']});assert.equal(preview.final.preview.active,true);assert.equal(preview.final.commands.length,0);assert.equal(await stat(join(out,'preview',`settings-${f.gameId}.json`)).then(()=>true,()=>false),false);
  await writeFile(resolve('.cache/native-planning-guidance/latest.json'),JSON.stringify({ok:true,out},null,2));console.log('Native planning guidance evidence: '+out);
});


test('native live preview keeps automatic guidance suppressed after actions without persisting acknowledgment',{skip:!['darwin','win32'].includes(process.platform),timeout:90000},async t=>{
  await mkdir('.cache/native-guide-live-preview',{recursive:true});const out=await mkdtemp(resolve('.cache/native-guide-live-preview/run-')),f=await fixture(out);
  const preview=await f.run('baseline',[{resize:{width:390,height:844}},{checkpoint:'preview'}],{extra:['--preview-screen','schedule']});
  assert.equal(preview.final.readingLayout.guidanceOpen,false);
  const session=join(out,'live-session');await mkdir(session);const inspection=join(out,'inspection.json');
  await writeFile(inspection,JSON.stringify({version:1,contentFingerprint:f.content.contentFingerprint,screenId:'schedule',state:preview.final.state,visual:f.content.project.visual}));
  const ignored=join(out,'ordinary-save.json');await writeFile(ignored,'preserve original');
  const child=spawn(playerExecutable(f.target,f.destination),['--bundle',gameDirectory(f.target,f.destination),'--save',ignored,'--hidden','--preview-screen','schedule','--preview-inspection',inspection,'--editor-live-preview',session,'--reading-layout','on','--text-scale','2','--locale','en'],{windowsHide:true});
  t.after(()=>child.kill());let buffer='',stderr='',ready=false,serial=0;const reports=new Map();
  child.stderr.on('data',b=>stderr+=b);child.stdout.on('data',b=>{buffer+=b;while(buffer.includes('\n')){const at=buffer.indexOf('\n'),line=buffer.slice(0,at);buffer=buffer.slice(at+1);try{const r=JSON.parse(line).editorPreview;if(r?.ready)ready=true;else if(r)reports.set(r.serial,r);}catch{}}});
  async function until(predicate){for(let i=0;i<1000;i++){if(predicate())return;if(child.exitCode!==null)throw Error(stderr||'Live preview exited');await delay(20);}throw Error('Live preview timed out: '+stderr);}
  async function command(value){const id=++serial;await writeFile(join(session,'next.json'),JSON.stringify({...value,serial:id}));await rename(join(session,'next.json'),join(session,'control.json'));await until(()=>reports.has(id));return reports.get(id).ui;}
  await until(()=>ready);await command({type:'pause',paused:false});
  const before=await command({type:'inspect'});assert.equal(before.readingLayout.guidanceOpen,false);
  await command({type:'click',id:'engine-reading-menu'});const after=await command({type:'click',id:'engine-reading-menu-close'});
  assert.equal(after.preview.screenId,'','The live preview actually cleared its initial screen override');assert.equal(after.readingLayout.guidanceOpen,false);unchanged(after,before);
  const opened=await command({type:'click',id:'board-help'});assert.equal(opened.readingLayout.guidanceOpen,true);
  const dismissed=await command({type:'click',id:'engine-planning-guide-skip'});assert.equal(dismissed.readingLayout.guidanceAcknowledged,true);assert.equal(dismissed.readingLayout.guidanceOpen,false);unchanged(dismissed,before);
  assert.equal(await stat(join(session,`settings-${f.gameId}.json`)).then(()=>true,()=>false),false,'Live guide acknowledgment must not persist');assert.equal(await readFile(ignored,'utf8'),'preserve original');
  await writeFile(join(out,'report.json'),JSON.stringify({before,after,opened,dismissed},null,2));await writeFile(join(out,'process.txt'),stderr);
  await writeFile(resolve('.cache/native-guide-live-preview/latest.json'),JSON.stringify({ok:true,out},null,2));console.log('Native live guide preview evidence: '+out);
});

test('native readable record identity stays visible through confirmation and cancellation',{timeout:150000},async()=>{
  await mkdir('.cache/native-record-identity',{recursive:true});const out=await mkdtemp(resolve('.cache/native-record-identity/run-')),f=await fixture(out,{guided:false});
  await f.run('seed',[...start(),touch('engine-reading-menu'),touch('save'),{checkpoint:'empty'},touch('records__slots-request'),touch('records__close'),{checkpoint:'saved'}],{profile:'records'});
  const file=join(out,'records','progress.json'),base=JSON.parse(await readFile(file,'utf8')),slots=join(out,'records','progress.slots');await mkdir(slots,{recursive:true});
  const naive=structuredClone(base);naive.playerPresentation.saveRecord={...naive.playerPresentation.saveRecord,date:'Year 2 · Phase 8',place:'Copper roof workshop',phase:'planning',savedAt:'2026-10-24 17:08:09',sequence:12};await writeFile(file,JSON.stringify(naive));
  const utc=structuredClone(naive);utc.playerPresentation.saveRecord={...utc.playerPresentation.saveRecord,place:'Jade sail clinic',phase:'narrative',savedAt:'2026-10-25T00:08:09.123Z',sequence:13};const utcfile=join(slots,'manual-2.json');await writeFile(utcfile,JSON.stringify(utc));
  await writeFile(join(slots,'manual-3.json'),'{broken');const wrong=structuredClone(naive);wrong.contentFingerprint='sha256:other-version';await writeFile(join(slots,'manual-4.json'),JSON.stringify(wrong));
  const before=await readFile(file,'utf8'),other=await readFile(utcfile,'utf8');
  const r=await f.run('records',[...start(),touch('engine-reading-menu'),touch('load'),{checkpoint:'list',screenshot:join(out,'records-200.png')},touch('load-records__slots-slot-1'),{checkpoint:'selected'},touch('load-records__slots-request'),{checkpoint:'confirm',screenshot:join(out,'confirm-200.png')},touch('load-records__slots-cancel'),{checkpoint:'cancel'},touch('load-records__close'),{checkpoint:'returned'},touch('engine-reading-menu'),touch('save'),touch('records__slots-request'),{checkpoint:'overwrite'},touch('records__slots-cancel'),touch('records__close'),{checkpoint:'save-cancel'}],{profile:'records'});
  const text=at(r,'list').texts['load-records__slots'];assert.match(text,/Copper roof workshop/);assert.match(text,/Planning/);assert.match(text,/local time as recorded/);assert.match(text,/00:08:09 UTC/);assert.match(text,/Unreadable/);assert.match(text,/Different version/);assert.match(text,/Record 5 · Empty/);
  for(const name of ['selected','confirm','cancel']){assert.match(at(r,name).texts['load-records__slots'],/Jade sail clinic/);assert.match(at(r,name).texts['load-records__slots'],/Saved · 2026-10-25 · 00:08:09 UTC · #13/);fit(at(r,name));}
  for(const name of ['returned','save-cancel'])unchanged(at(r,name),at(r,'opened'));assert.equal(await readFile(file,'utf8'),before);assert.equal(await readFile(utcfile,'utf8'),other);
  const load=await f.run('restore',[{resize:{width:390,height:844}},touch('engine-reading-menu'),touch('load'),touch('load-records__slots-request'),touch('load-records__slots-confirm'),{checkpoint:'restored'}],{profile:'records'});assert.deepEqual(at(load,'restored').state,base.state);assert.deepEqual(at(load,'restored').commands,base.commands);
  await writeFile(resolve('.cache/native-record-identity/latest.json'),JSON.stringify({ok:true,out},null,2));console.log('Native record identity evidence: '+out);
});


test('native readable save rows expose identity before thumbnails and keep empty records compact',{timeout:150000},async()=>{
  await mkdir('.cache/native-record-density',{recursive:true});const out=await mkdtemp(resolve('.cache/native-record-density/run-')),geometry=[];
  for(const locale of ['en','ja']){
    const f=await fixture(out,{locale,guided:false}),profile='filled-'+locale;
    await f.run('seed-'+locale,[...start(),touch('engine-reading-menu'),touch('save'),touch('records__slots-request')],{profile});
    const file=join(out,profile,'progress.json'),base=JSON.parse(await readFile(file,'utf8')),slots=join(out,profile,'progress.slots');
    const date=locale==='en'?'Year 2 · Phase 8':'二年目 · 八月';
    base.playerPresentation.saveRecord={...base.playerPresentation.saveRecord,date,place:locale==='en'?'Copper roof workshop':'銅屋根の工房',phase:'planning',savedAt:'2026-10-24 17:08:09',sequence:12,thumbnail:'frame-identity-fixture.png'};
    await writeFile(join(slots,'frame-identity-fixture.png'),encodeRgbaPng({width:16,height:9,pixels:Buffer.from(Array(16*9).fill([25,91,207,255]).flat())}));await writeFile(file,JSON.stringify(base));const before=await readFile(file);
    for(const [width,height] of [[320,640],[390,844],[844,390]]){
      const name=`${locale}-${width}`;
      const empty=await f.run('empty-'+name,[...start(width,height),touch('engine-reading-menu'),touch('save'),{checkpoint:'empty',screenshot:join(out,`empty-${name}.png`)},touch('records__slots-slot-1'),{checkpoint:'selected'},touch('records__close'),{checkpoint:'returned'}]);
      const ui=at(empty,'empty'),main=ui.readingLayout['reading-main'];fit(ui);unchanged(at(empty,'returned'),at(empty,'opened'));
      for(const id of ['records__slots-manual','records__slots-auto']){const c=ui.readingLayout.controls.find(c=>c.id===id);assert.ok(c.height<=parseFloat(c.fontSize)*1.45+28,`Short group label must remain a whole word at ${name}: ${JSON.stringify(c)}`);}
      const rows=ui.nodes.filter(n=>/^records__slots-slot-\d+$/.test(n.id)),complete=rows.filter(n=>n.y>=main.y-.5&&n.y+n.height<=main.y+main.height+.5);
      if(width<height)assert.ok(complete.length>=2,`At ${name}, ${complete.length} complete empty identities were initially visible`);
      assert.ok(rows.every(n=>!n.label.startsWith('●')));const html=ui.texts.records__slots;
      assert.match(html,locale==='en'?/>Manual</:/>手動</);assert.match(html,locale==='en'?/>Auto</:/>自動</);
      const selectedTag=at(empty,'selected').texts.records__slots.match(/<button[^>]*id="records__slots-slot-1"[^>]*>/)?.[0]??'';assert.match(selectedTag,/class="record-slot record-selected"/);assert.match(selectedTag,/aria-pressed="true"/);
      const shot=join(out,`filled-${name}.png`),filled=await f.run('filled-'+name,[{resize:{width,height}},touch('engine-reading-menu'),touch('load'),{checkpoint:'filled',screenshot:shot},touch('load-records__slots-request'),{checkpoint:'confirm'},touch('load-records__slots-cancel'),touch('load-records__close'),{checkpoint:'returned'},{key:'F5'},touch('records__slots-auto'),{checkpoint:'protected'},touch('records__close')],{profile});
      const view=at(filled,'filled'),box=view.readingLayout['reading-main'],row=view.nodes.find(n=>n.id==='load-records__slots-slot-0');fit(view);assert.equal(view.readingLayout['reading-main'].scrollTop,0);
      const rml=view.texts['load-records__slots'],identityAt=rml.indexOf('class="record-identity"'),thumbnailAt=rml.indexOf('class="record-thumbnail"');assert.ok(identityAt>=0&&identityAt<thumbnailAt);assert.ok(rml.indexOf(date,identityAt)<thumbnailAt);
      const image=decodePng(await readFile(shot)),scale=image.width/width,ink=[];
      // Measured pixels prove two identity/date text lines are visible above the
      // thumbnail, rather than only finding the right text in an offscreen DOM.
      for(let y=Math.ceil((row.y+10)*scale);y<Math.floor(Math.min(row.y+125,box.y+box.height)*scale);y++){
        let count=0;for(let x=Math.ceil((row.x+18)*scale);x<Math.floor((row.x+row.width-18)*scale);x++){const i=(y*image.width+x)*4;if(image.pixels[i]<110&&image.pixels[i+1]<110&&image.pixels[i+2]<110)count++;}
        if(count>5){const last=ink.at(-1);if(last&&y-last.end<=Math.ceil(3*scale))last.end=y;else ink.push({start:y,end:y});}
      }
      assert.ok(ink.length>=2,`At ${name}, first record identity and date must render visibly before the thumbnail: ${JSON.stringify(ink)}`);
      const confirmation=at(filled,'confirm').texts['load-records__slots'];assert.ok(confirmation.includes(date));assert.match(confirmation,locale==='en'?/local time as recorded/:/記録時の現地時刻/);
      const request=at(filled,'protected').readingLayout.controls.find(c=>c.id==='records__slots-request');assert.equal(request.enabled,false,'Automatic records remain protected');
      assert.equal(at(filled,'returned').state,null);assert.deepEqual(at(filled,'returned').commands,[]);assert.deepEqual(await readFile(file),before,'Browsing and confirmation cancellation preserve saved bytes');
      geometry.push({locale,width,height,initialEmptyRows:complete.length,initialIdentityInkBands:ink.length,mainHeight:box.height});
    }
  }
  await writeFile(resolve('.cache/native-record-density/latest.json'),JSON.stringify({ok:true,out,geometry},null,2));console.log('Native save density evidence: '+out);
});

test('native Japanese fallback ships licensed glyphs and renders real ink instead of missing-character boxes',{timeout:60000},async()=>{
  await mkdir('.cache/native-japanese-fallback',{recursive:true});const out=await mkdtemp(resolve('.cache/native-japanese-fallback/run-')),f=await fixture(out,{locale:'ja',guided:false,glyphs:true});
  const r=await f.run('glyphs',[{resize:{width:390,height:844}},{checkpoint:'glyphs',screenshot:join(out,'glyphs.png')}]);
  const ui=at(r,'glyphs'),font=await readFile(join(f.runtime,'fonts/NotoSansJP.ttf'));
  for(const c of '説明戻る絞り込む活動選択予定確認適用取消')assert.notEqual(glyphIndex(font,c.codePointAt(0)),0,`Bundled Japanese glyph ${c}`);
  const exportFont=f.target==='macos'?join(f.destination,'RaisingPlayer.app/Contents/Resources/fonts/NotoSansJP.ttf'):join(f.destination,'fonts/NotoSansJP.ttf');
  assert.deepEqual(await readFile(exportFont),font,'The exported player includes the actual fallback');
  const notice=await readFile(join(f.runtime,'licenses/NotoSansJP-OFL.txt'),'utf8');assert.match(notice,/SIL OPEN FONT LICENSE/);
  assert.match(await readFile(join(out,'glyphs-process.txt'),'utf8'),/Loaded font face 'Noto Sans JP' \[regular\]/,'Native runtime registers a regular-weight fallback');
  const image=decodePng(await readFile(join(out,'glyphs.png'))),scale=image.width/ui.readingLayout.windowWidth;
  const letters=inkCount(image,ui.nodes.find(n=>n.id==='title-text'),scale),boxes=inkCount(image,ui.nodes.find(n=>n.id==='missing-glyphs'),scale);
  assert.ok(letters>boxes*1.5,`Japanese strokes (${letters}) must differ from three missing-glyph outlines (${boxes})`);
  await writeFile(resolve('.cache/native-japanese-fallback/latest.json'),JSON.stringify({ok:true,out,letters,boxes},null,2));console.log('Native Japanese fallback evidence: '+out);
});
