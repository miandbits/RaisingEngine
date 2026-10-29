import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {join,resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {firstGameRoute} from '../qt-studio/first-game-route.mjs';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {desktopTarget,gameDirectory,playerExecutable} from '../../dist/packages/export-service/targets.js';

const button=(id,action,text=id)=>({id,kind:'button',action,text,autoLabel:false,x:30,y:600,width:240,height:48,fill:'#315f58',color:'#fffaef'});
const leaf=(id,kind,extra={})=>({id,kind,x:30,y:80,width:1150,height:300,...extra});
const reading=(items,actions=[])=>({items:items.map(elementId=>({elementId})),actions});

test('native reading view reflows in points, respects safe areas, persists text settings and cancels swiped taps', {timeout:180000},async t=>{
  await mkdir('.cache/native-readable',{recursive:true});const out=await mkdtemp(resolve('.cache/native-readable/run-')),projectRoot=join(out,'project');
  await cp(resolve('examples/my-first-raising-game'),projectRoot,{recursive:true});
  const file=join(projectRoot,'project.json'),project=JSON.parse(await readFile(file,'utf8'));
  const prose=Array(16).fill('A long English sentence wraps between words. 한글 문장과 日本語の文章も最後まで読めます。').join(' ');
  project.visual={width:1280,height:720,readableLayout:{background:'#f5efdf'},saveSystem:{manualSlots:6,autoSlots:3},screens:[
    {id:'title',label:'Title',elements:[button('start','start','Start a new life with a deliberately long translated label'),leaf('prose','text',{text:prose}),{...button('desktop-start','start','Start from the canvas'),x:12,y:664,height:44},button('load','load')],readableLayout:reading(['start','prose'],['load'])},
    {id:'schedule',label:'Schedule',elements:[leaf('calendar','text',{binding:'calendar'}),leaf('planner','schedule',{scheduleLayout:'board',showActivityDescription:true,visibleWhen:'planning'}),leaf('result','result',{visibleWhen:'result'}),button('advance','advance','Start these activities'),button('save','save'),button('load','load')],readableLayout:reading(['calendar','planner','result'],['advance','save','load'])},
    {id:'story',label:'Story',elements:[leaf('story','dialogue'),leaf('choices','choices',{choiceLayout:'cards'}),button('advance','advance'),button('save','save'),button('load','load')],readableLayout:reading(['story','choices'],['advance','save','load'])},
    {id:'ending',label:'Ending',elements:[leaf('ending','text',{binding:'ending'}),button('start','start')],readableLayout:reading(['ending'],['start'])},
    ...[false,true].map(load=>({id:load?'load-records':'records',label:'Records',modal:true,elements:[leaf('slots',load?'loadSlots':'saveSlots'),button('close','closeModal')],readableLayout:reading(['slots'],['close'])})),
  ]};
  await writeFile(file,JSON.stringify(project));const built=await buildSnapshot(await loadProject(projectRoot));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  const target=desktopTarget(),destination=join(out,'game'),runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app');
  await exportGame({projectRoot,content:built.snapshot,target,destination,runtimes:{windows:runtime,macos:runtime,web:runtime}});
  const save=join(out,'save.json');
  const run=async(name,steps,args=[])=>{
    const input=join(out,name+'.json'),report=join(out,name+'-report.json');await writeFile(input,JSON.stringify(steps.map(step=>['records','scaled','rotated','story-1','ending'].includes(step.checkpoint)?{...step,screenshot:join(out,name+'-'+step.checkpoint+'.png')}:step)));

    const child=spawnSync(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--save',save,'--automation',input,'--report',report,'--hidden',...args],{encoding:'utf8',windowsHide:true,timeout:60000});
    await writeFile(join(out,name+'-process.txt'),(child.stdout??'')+'\n'+(child.stderr??''));assert.equal(child.status,0,child.error?.message??child.stderr);
    const result=JSON.parse(await readFile(report,'utf8'));assert.equal(result.ok,true);return result;
  };
  const at=(report,name)=>report.steps.find(step=>step.command.checkpoint===name).ui;
  const fit=ui=>{
    const r=ui.readingLayout;assert.equal(r.active,true);assert.equal(ui.viewport.width,r.windowWidth);assert.equal(ui.viewport.height,r.windowHeight);
    const main=r['reading-main'],actions=r['reading-actions'];assert.ok(main.height>40,JSON.stringify(r));assert.ok(main.y+main.height<=actions.y+.6,'Actions must not cover reading content');
    assert.ok(main.scrollWidth<=main.clientWidth+1,'No horizontal text scrolling');
    for(const control of r.controls){assert.ok(control.width>=43.5,JSON.stringify(control));assert.ok(control.height>=43.5,JSON.stringify(control));}
    assert.ok(r.controls.every(c=>Number.parseFloat(c.fontSize)>=18*r.textScale-.1),'Interactive labels scale with the text setting');
    assert.ok(actions.y+actions.height<=r.safeArea.y+r.safeArea.height+.5,'Footer respects safe area');
    if(r.textScale===2&&r.windowWidth<=390)for(const control of r.controls.filter(c=>['advance','save','load','engine-reading-options'].includes(c.id)))assert.ok(control.width>=r.safeArea.width-64,'200% footer labels use a full-width column instead of splitting ordinary words');
  };
  const desktop=await run('desktop-toolbar',[
    {resize:{width:1280,height:720}},{waitFrames:4},{checkpoint:'canvas',screenshot:join(out,'desktop-toolbar.png')},
    {touchWindow:'desktop-start'},{checkpoint:'started'},{click:'engine-reading-options'},{checkpoint:'options'},
    {click:'engine-reading-close'},{checkpoint:'closed'},
  ]);
  const canvas=at(desktop,'canvas');assert.equal(canvas.readingLayout.active,false);
  const toolbar=canvas.readingLayout.controls.find(c=>c.id==='engine-reading-options');
  const authored=canvas.readingLayout.controls.find(c=>c.id==='desktop-start');
  assert.ok(toolbar&&authored);assert.ok(toolbar.y>=authored.y+authored.height-.5,'Reading launcher cannot cover authored bottom-left buttons');
  assert.ok(toolbar.height>=43.5,'Desktop launcher remains at least 44 window points');
  assert.equal(at(desktop,'started').state.phase,'planning','A real touch must reach the authored button');
  assert.equal(at(desktop,'options').readingLayout.active,true);
  assert.deepEqual(at(desktop,'closed').state,at(desktop,'started').state);
  assert.deepEqual(at(desktop,'closed').commands,at(desktop,'started').commands);
  for(const [w,h] of [[320,640],[390,844],[844,390]])for(const scale of [1,1.25,1.5,2])await t.test(`${w}x${h} at ${scale*100}%`,async()=>{
    const safe={x:w>h?28:0,y:w>h?0:24,width:w-(w>h?56:0),height:h-(w>h?20:44)};
    const report=await run(`size-${w}-${scale}`,[{resize:{width:w,height:h}},{safeArea:safe},{waitFrames:4},{checkpoint:'view',screenshot:join(out,`size-${w}-${scale}.png`)}],['--text-scale',String(scale)]);
    fit(at(report,'view'));assert.equal(at(report,'view').readingLayout.textScale,scale);assert.equal(at(report,'view').state,null);
  });
  const swiped=await run('swipe',[{resize:{width:390,height:844}},{waitFrames:4},{checkpoint:'before'},{touchDrag:{fromX:100,fromY:65,toX:100,toY:25}},{checkpoint:'after'}]);
  assert.deepEqual(at(swiped,'after').state,at(swiped,'before').state);assert.deepEqual(at(swiped,'after').commands,at(swiped,'before').commands);assert.ok(at(swiped,'after').readingLayout['reading-main'].scrollTop>0);
  const report=await run('settings-and-records',[
    {resize:{width:390,height:844}},{waitFrames:4},{click:'engine-reading-options'},{click:'engine-reading-settings-text-200'},{checkpoint:'scaled'},
    {click:'engine-reading-settings-reading-on'},{click:'engine-reading-close'},{click:'start'},{checkpoint:'started'},
    {resize:{width:844,height:390}},{waitFrames:4},{checkpoint:'rotated'},{click:'save'},{checkpoint:'records'},
    {click:'records__slots-request'},{click:'records__close'},{checkpoint:'saved'},{click:'load'},
    {click:'load-records__slots-request'},{click:'load-records__slots-confirm'},{checkpoint:'restored'},
  ]);
  assert.equal(at(report,'scaled').settings.values.textScale,2);assert.deepEqual(at(report,'rotated').state,at(report,'started').state);assert.deepEqual(at(report,'rotated').commands,at(report,'started').commands);
  fit(at(report,'rotated'));fit(at(report,'records'));assert.equal(at(report,'records').elements.filter(e=>/^records__slots-slot-\d+$/.test(e.id)).length,6);
  assert.deepEqual(at(report,'restored').state,at(report,'saved').state);
  const fresh=await run('fresh',[{resize:{width:320,height:640}},{waitFrames:4},{checkpoint:'prefs'},{click:'load'},{click:'load-records__slots-request'},{click:'load-records__slots-confirm'},{checkpoint:'restored'}]);
  assert.equal(at(fresh,'prefs').settings.values.textScale,2);assert.equal(at(fresh,'prefs').settings.values.readingLayout,'on');assert.deepEqual(at(fresh,'restored').state,at(report,'saved').state);fit(at(fresh,'restored'));
  const route=firstGameRoute(built.snapshot,true),steps=[{resize:{width:320,height:640}},{waitFrames:4}];let saves=0;
  const touch=id=>steps.push({scrollIntoView:id},{touchWindow:id});
  for(const step of route.steps){
    if(step.select){touch('board-slot-'+step.select.slice(5));touch('board-activity-'+step.value);}
    else if(step.key==='F5'){steps.push(step);touch('records__slots-request');if(saves++)touch('records__slots-confirm');touch('records__close');}
    else if(step.key==='F9'){steps.push(step);touch('load-records__slots-request');touch('load-records__slots-confirm');}
    else if(step.click)touch(step.click);else steps.push(step);
  }
  const played=await run('touch-campaign',steps,['--text-scale','2','--save',join(out,'route-save.json')]);
  for(const [label,state] of Object.entries(route.expected))assert.deepEqual(at(played,label).state,state,'Touch campaign '+label);
  assert.ok(played.final.commands.some(c=>c.type==='choose'));assert.equal(played.final.commands.filter(c=>c.type==='executeActivity').length,9);
  const prefFile=join(out,'settings-'+project.gameId+'.json'),beforePreview=await readFile(prefFile,'utf8');
  const preview=await run('preview',[{resize:{width:390,height:844}},{waitFrames:4},{checkpoint:'view'}],['--preview-screen','title','--preview-reading-layout','on','--preview-text-scale','1.25']);
  assert.equal(preview.final.readingLayout.textScale,1.25);assert.equal(await readFile(prefFile,'utf8'),beforePreview,'Preview overrides never persist settings');
  const oldPrefs=JSON.parse(beforePreview);delete oldPrefs.readingLayout;delete oldPrefs.textScale;await writeFile(prefFile,JSON.stringify(oldPrefs));
  const migrated=await run('old-preferences',[{resize:{width:390,height:844}},{waitFrames:4}]);assert.equal(migrated.final.settings.values.textScale,1);assert.equal(migrated.final.settings.values.readingLayout,'auto');
  await writeFile(prefFile,JSON.stringify({...oldPrefs,readingLayout:'unknown',textScale:99}));const invalid=await run('invalid-preferences',[{resize:{width:390,height:844}},{waitFrames:4}]);assert.equal(invalid.final.settings.values.textScale,1);assert.equal(invalid.final.settings.values.readingLayout,'auto');
  for(const args of [['--text-scale','1.1'],['--reading-layout','off']]){const invalid=spawnSync(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--frames','1','--hidden',...args],{encoding:'utf8',timeout:10000});assert.notEqual(invalid.status,0);}
  await writeFile(resolve('.cache/native-readable/latest.json'),JSON.stringify({out,target,ok:true},null,2));console.log(`Native readable evidence: ${out}`);
});
