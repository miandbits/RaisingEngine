import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {join,resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {desktopTarget,gameDirectory,playerExecutable} from '../../dist/packages/export-service/targets.js';
import {firstGameRoute} from '../qt-studio/first-game-route.mjs';

const button=(id,action,text=id,extra={})=>({id,kind:'button',action,text,autoLabel:false,x:24,y:620,width:230,height:48,fill:'#315f58',color:'#fff8e9',...extra});
const leaf=(id,kind,extra={})=>({id,kind,x:24,y:40,width:1200,height:530,...extra});
const reading=(items,actions,primaryAction)=>({items:items.map(item=>typeof item==='string'?{elementId:item}:item),actions,actionLayout:'grouped',...(primaryAction?{primaryAction}:{})});
const prose=Array.from({length:20},(_,i)=>`Paragraph ${i+1}. A careful reader can keep their place while inspecting a plan, changing text size, or saving. 한글 문장과日本語の文章を最後まで読めます。`).join('\n\n');
const touch=id=>({scrollIntoView:id,touchWindow:id,waitFrames:2});
const at=(r,name)=>r.steps.find(s=>s.command.checkpoint===name)?.ui;

async function fixture(out,periods=3,disabled=false){
  const root=join(out,'project-'+periods+(disabled?'-disabled':''));await cp(resolve('examples/my-first-raising-game'),root,{recursive:true});
  const file=join(root,'project.json'),p=JSON.parse(await readFile(file,'utf8'));p.periodsPerMonth=periods;
  const utilities=[button('save','save','Save'),button('load','load','Load'),button('help','openModal','Help',{screenId:'help'}),button('sound','sound','Sound')];
  p.visual={width:1280,height:720,readableLayout:{background:'#f5efdf'},saveSystem:{manualSlots:6,autoSlots:3},screens:[
    {id:'title',label:'Title',elements:[leaf('title-text','text',{text:prose}),button('start','start','Begin'),...utilities],readableLayout:reading(['title-text'],['start','load','help','sound'],'start')},
    {id:'schedule',label:'Schedule',elements:[leaf('planner','schedule',{scheduleLayout:'board',visibleWhen:'planning',scheduleGroups:[{id:'learning',label:'Learn / 学ぶ',activityIds:['study','walk']},{id:'recovery',label:'Rest and work',activityIds:['rest','work']},{id:'all',label:'Authored all category',activityIds:['work']}]}),leaf('result','result',{visibleWhen:'result'}),button('advance','advance','Continue'),...utilities],readableLayout:reading([{elementId:'planner',schedulePresentation:'compact'},'result'],['advance','save','load','help','sound'],'advance')},
    {id:'story',label:'Story',elements:[leaf('story','dialogue'),leaf('choices','choices'),button('advance','advance','Continue'),...utilities],readableLayout:reading(['story','choices'],['advance','save','load','help','sound'],'advance')},
    {id:'ending',label:'Ending',elements:[leaf('ending','text',{binding:'ending'}),button('start','start','Begin'),...utilities],readableLayout:reading(['ending'],['save','start','load','help'],'save')},
    {id:'help',label:'Help',modal:true,elements:[leaf('instructions','text',{text:prose}),button('back','closeModal','Return')],readableLayout:reading(['instructions'],['back'],'back')},
    ...[false,true].map(load=>({id:load?'load-records':'records',label:'Records',modal:true,elements:[leaf('slots',load?'loadSlots':'saveSlots'),button('close','closeModal','Return')],readableLayout:reading(['slots'],['close'],'close')})),
  ]};
  if(disabled){const screen=p.visual.screens.find(s=>s.id==='schedule');screen.elements.find(e=>e.id==='planner').parentId='planner-parent';screen.elements.unshift({id:'planner-parent',kind:'group',x:0,y:0,width:1280,height:720,enabledCondition:{op:'not',condition:{op:'always'}}});}
  await writeFile(file,JSON.stringify(p));
  const eventFile=join(root,'events/garden-invitation.json'),event=JSON.parse(await readFile(eventFile,'utf8'));event.nodes[0].text=prose;await writeFile(eventFile,JSON.stringify(event));
  const built=await buildSnapshot(await loadProject(root));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  const target=desktopTarget(),destination=join(out,'game-'+periods+(disabled?'-disabled':'')),runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app');
  await exportGame({projectRoot:root,content:built.snapshot,target,destination,runtimes:{windows:runtime,macos:runtime,web:runtime}});
  return {content:built.snapshot,target,destination,run:async(name,steps,scale=2)=>{
    const input=join(out,name+'.json'),report=join(out,name+'-report.json');await writeFile(input,JSON.stringify(steps));
    const r=spawnSync(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--save',join(out,name+'-save.json'),'--text-scale',String(scale),'--automation',input,'--report',report,'--report-checkpoints-only','--hidden'],{encoding:'utf8',timeout:90000,windowsHide:true});
    await writeFile(join(out,name+'-process.txt'),(r.stdout??'')+'\n'+(r.stderr??''));assert.equal(r.status,0,r.error?.message??r.stderr);
    const data=JSON.parse(await readFile(report,'utf8'));assert.equal(data.ok,true);return data;
  }};
}

function fit(ui){
  const r=ui.readingLayout,main=r['reading-main'],footer=r['reading-actions'];assert.equal(r.active,true);
  assert.ok(main.height>=r.safeArea.height*.6,`Reading pane budget: ${JSON.stringify(r)}`);
  assert.ok(main.y+main.height<=footer.y+.5,'Footer does not overlap content');
  assert.ok(footer.scrollHeight<=footer.clientHeight+1,'Grouped footer needs no second persistent scroll');
  assert.ok(main.scrollWidth<=main.clientWidth+1,'No horizontal text scroll');
  for(const c of r.controls){assert.ok(c.width>=43.5&&c.height>=43.5,JSON.stringify(c));assert.ok(parseFloat(c.fontSize)>=18*r.textScale-.1);}
}

test('native grouped reading flow preserves context and compact plans use explicit local apply/cancel',{timeout:240000},async t=>{
  await mkdir('.cache/native-reading-workflow',{recursive:true});const out=await mkdtemp(resolve('.cache/native-reading-workflow/run-')),f=await fixture(out);
  for(const [width,height] of [[320,568],[390,844],[844,390]])for(const scale of [1,2])await t.test(`${width}x${height} ${scale*100}%`,async()=>{
    const r=await f.run(`size-${width}-${scale}`,[{resize:{width,height}},{waitFrames:4},{checkpoint:'title'},touch('start'),{checkpoint:'plan',screenshot:join(out,`plan-${width}-${scale}.png`)},touch('engine-reading-menu'),{checkpoint:'menu'},touch('engine-reading-menu-close'),{checkpoint:'returned'}],scale);
    fit(at(r,'title'));fit(at(r,'plan'));assert.equal(at(r,'menu').readingLayout.menuOpen,true);assert.ok(at(r,'menu').elements.some(e=>e.id==='save'));
    assert.deepEqual(at(r,'returned').state,at(r,'plan').state);assert.deepEqual(at(r,'returned').commands,at(r,'plan').commands);
    assert.equal(at(r,'plan').readingLayout.controls.filter(c=>c.id.startsWith('board-slot-')).length,3);
    assert.ok(!at(r,'plan').elements.some(e=>e.id.startsWith('board-activity-')),'Summary has no catalogue');
    if(width===390&&scale===2){const menu=at(r,'title').readingLayout.controls.find(c=>c.id==='engine-reading-menu');const x=menu.x+menu.width/2,y=menu.y+menu.height/2;
      const drag=await f.run('swipe-menu',[{resize:{width,height}},{waitFrames:4},{checkpoint:'before'},{touchDrag:{fromX:x,fromY:y,toX:x,toY:y-100}},{checkpoint:'after'}]);
      assert.equal(at(drag,'after').readingLayout.menuOpen,false);assert.deepEqual(at(drag,'after').commands,at(drag,'before').commands);assert.equal(at(drag,'after').state,null);
    }
  });
  const overlay=await f.run('overlay-resize',[
    {resize:{width:390,height:844}},{waitFrames:4},touch('start'),{checkpoint:'start'},touch('engine-reading-menu'),
    {resize:{width:1280,height:720}},{waitFrames:4},{checkpoint:'menu-desktop'},touch('engine-reading-menu-close'),{checkpoint:'canvas'},
    {resize:{width:390,height:844}},{waitFrames:4},touch('board-slot-0'),touch('board-activity-study'),
    {resize:{width:1280,height:720}},{waitFrames:4},{checkpoint:'picker-desktop'},touch('board-cancel'),{checkpoint:'cancelled-canvas'},
  ]);
  assert.equal(at(overlay,'menu-desktop').readingLayout.active,true);assert.equal(at(overlay,'menu-desktop').readingLayout.menuOpen,true);
  assert.equal(at(overlay,'picker-desktop').readingLayout.active,true);assert.equal(at(overlay,'picker-desktop').readingLayout.pickerOpen,true);
  for(const name of ['canvas','cancelled-canvas']){assert.equal(at(overlay,name).readingLayout.active,false);assert.deepEqual(at(overlay,name).state,at(overlay,'start').state);assert.deepEqual(at(overlay,name).commands,at(overlay,'start').commands);assert.deepEqual(at(overlay,name).scheduleDraft,at(overlay,'start').scheduleDraft);}
  const r=await f.run('context',[
    {resize:{width:390,height:844}},{waitFrames:4},touch('start'),{checkpoint:'before-picker'},
    touch('board-slot-0'),touch('board-group-option-all'),{checkpoint:'authored-all'},touch('board-group-all'),{checkpoint:'all'},touch('board-group-option-learning'),{checkpoint:'category'},touch('board-activity-study'),{checkpoint:'pending',screenshot:join(out,'picker-details.png')},touch('board-cancel'),{checkpoint:'cancelled'},
    touch('board-slot-0'),touch('board-activity-walk'),touch('board-apply'),{checkpoint:'applied'},
    touch('board-slot-1'),touch('board-activity-study'),touch('board-apply'),touch('board-slot-2'),touch('board-activity-rest'),touch('board-apply'),touch('advance'),touch('advance'),touch('advance'),{checkpoint:'story'},
    {touchDrag:{fromX:180,fromY:500,toX:180,toY:140}},{checkpoint:'scrolled'},
    touch('engine-reading-menu'),{checkpoint:'menu'},touch('engine-reading-menu-close'),{checkpoint:'menu-return'},
    touch('engine-reading-menu'),touch('help'),{checkpoint:'help'},touch('help__back'),{checkpoint:'help-return'},
    touch('engine-reading-menu'),touch('save'),touch('records__slots-request'),touch('records__close'),{checkpoint:'save-return'},
    touch('engine-reading-menu'),touch('load'),touch('load-records__close'),{checkpoint:'load-cancel-return'},
    touch('engine-reading-menu'),touch('engine-reading-options'),touch('engine-reading-settings-text-100'),touch('engine-reading-close'),{checkpoint:'scaled'},
    {resize:{width:844,height:390}},{waitFrames:4},{checkpoint:'rotated',screenshot:join(out,'restored-landscape.png')},
    touch('advance'),{checkpoint:'new-node'},touch('engine-reading-menu'),{checkpoint:'choice-menu'},{key:'Escape'},{checkpoint:'choice-return'},
  ]);
  const before=at(r,'before-picker');
  for(const name of ['pending','cancelled']){assert.deepEqual(at(r,name).state,before.state);assert.deepEqual(at(r,name).commands,before.commands);assert.deepEqual(at(r,name).scheduleDraft,before.scheduleDraft);}
  assert.deepEqual(at(r,'authored-all').elements.filter(e=>e.id.startsWith('board-activity-')).map(e=>e.id),['board-activity-work']);assert.equal(at(r,'all').elements.filter(e=>e.id.startsWith('board-activity-')).length,4);
  assert.deepEqual(at(r,'category').elements.filter(e=>e.id.startsWith('board-activity-')).map(e=>e.id).sort(),['board-activity-study','board-activity-walk']);
  assert.equal(at(r,'pending').readingLayout.pendingActivity,'study');
  assert.ok(at(r,'pending').texts.planner.includes('Costs')&&at(r,'pending').texts.planner.includes('Base effects'),'Rules are localized and visible');
  assert.equal(at(r,'applied').scheduleDraft.activities[0],'walk');assert.deepEqual(at(r,'applied').commands,before.commands);
  const scrolled=at(r,'scrolled');assert.equal(scrolled.state.activeNarrative.nodeId,'hello');assert.ok(scrolled.readingLayout['reading-main'].scrollTop>100);
  for(const name of ['menu-return','help-return','save-return','load-cancel-return']){
    const returned=at(r,name);assert.deepEqual(returned.state,scrolled.state);assert.deepEqual(returned.commands,scrolled.commands);
    assert.ok(Math.abs(returned.readingLayout['reading-main'].scrollTop-scrolled.readingLayout['reading-main'].scrollTop)<2,name+' retains paragraph');
    assert.equal(returned.focusId,'engine-reading-menu',name+' returns focus');
  }
  for(const name of ['scaled','rotated']){const view=at(r,name);assert.deepEqual(view.state,scrolled.state);assert.deepEqual(view.commands,scrolled.commands);assert.equal(view.readingLayout.position.anchor,scrolled.readingLayout.position.anchor);assert.equal(view.readingLayout.position.text,scrolled.readingLayout.position.text,'Logical paragraph survives reflow');assert.match(view.readingLayout.position.text,/Paragraph/);assert.ok(Math.abs(view.readingLayout.position.fraction-scrolled.readingLayout.position.fraction)<.025);}
  assert.equal(at(r,'new-node').state.activeNarrative.nodeId,'invitation');assert.equal(at(r,'new-node').readingLayout['reading-main'].scrollTop,0);
  assert.ok(!at(r,'new-node').elements.some(e=>e.id==='advance'),'Unavailable primary is omitted from footer');
  assert.equal(at(r,'choice-menu').elements.find(e=>e.id==='advance').enabled,false,'Its original disabled semantics remain in menu');
  assert.deepEqual(at(r,'choice-return').commands,at(r,'new-node').commands,'Escape never bypasses the choice');
  const route=firstGameRoute(f.content,true),steps=[{resize:{width:320,height:568}},{waitFrames:4}];let saves=0,finished=false;
  for(const step of route.steps){
    if(step.checkpoint==='ending')finished=true;
    if(step.select)steps.push(touch('board-slot-'+step.select.slice(5)),touch('board-activity-'+step.value),touch('board-apply'));
    else if(step.key==='F5'){if(!finished)steps.push(touch('engine-reading-menu'));steps.push(touch('save'),touch('records__slots-request'));if(saves++)steps.push(touch('records__slots-confirm'));steps.push(touch('records__close'));}
    else if(step.key==='F9')steps.push(touch('engine-reading-menu'),touch('load'),touch('load-records__slots-request'),touch('load-records__slots-confirm'));
    else if(step.click)steps.push(touch(step.click));else steps.push(step);
  }
  const played=await f.run('complete-touch-route',steps);for(const [name,state] of Object.entries(route.expected))assert.deepEqual(at(played,name).state,state,name);
  assert.equal(played.final.state.phase,'finished');assert.equal(played.final.commands.filter(c=>c.type==='executeActivity').length,9);
  const five=await fixture(out,5),dynamic=[{resize:{width:320,height:568}},{waitFrames:4},touch('start'),{checkpoint:'slots'}];
  for(let i=0;i<5;i++)dynamic.push(touch('board-slot-'+i),touch('board-activity-rest'),touch('board-apply'));
  dynamic.push(touch('advance'),{checkpoint:'committed'});const expanded=await five.run('five-periods',dynamic);
  assert.equal(at(expanded,'slots').elements.filter(e=>/^board-slot-/.test(e.id)).length,5);assert.deepEqual(at(expanded,'committed').state.schedule,Array(5).fill('rest'));
  const disabled=await fixture(out,3,true),blocked=await disabled.run('disabled-planner',[{resize:{width:390,height:844}},{waitFrames:4},touch('start'),{checkpoint:'before'},touch('board-slot-0'),{checkpoint:'after'}]);
  assert.equal(at(blocked,'before').elements.find(e=>e.id==='board-slot-0').enabled,false);assert.equal(at(blocked,'after').readingLayout.pickerOpen,false);assert.deepEqual(at(blocked,'after').scheduleDraft,at(blocked,'before').scheduleDraft);assert.deepEqual(at(blocked,'after').commands,at(blocked,'before').commands);
  await writeFile(resolve('.cache/native-reading-workflow/latest.json'),JSON.stringify({ok:true,out},null,2));console.log('Native grouped reading evidence: '+out);
});
