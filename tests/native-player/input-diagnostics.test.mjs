import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {scrollGroupFixture} from '../fixtures/scroll-groups.mjs';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {desktopTarget,gameDirectory,playerExecutable} from '../../dist/packages/export-service/targets.js';

test('native live diagnostics follow touch, wheel and keyboard scrolling without game commands',{timeout:120000},async()=>{
  await mkdir('.cache/input-diagnostics',{recursive:true});
  const out=await mkdtemp(resolve('.cache/input-diagnostics/native-'));
  const fixture=await scrollGroupFixture(join(out,'project')),target=desktopTarget(),destination=join(out,'game');
  const runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app');
  await exportGame({content:fixture.content,projectRoot:fixture.root,target,destination,runtimes:{windows:runtime,macos:runtime,web:runtime}});
  const cases=[
    {name:'touch',size:{width:390,height:844},args:['--reading-layout','on','--text-scale','2'],input:[{touchDrag:{fromX:190,fromY:500,toX:190,toY:100}}],pane:u=>u.readingLayout['reading-main']},
    {name:'wheel',size:{width:1280,height:800},args:[],input:[{wheelWindow:{x:600,y:350,deltaY:-12}}],pane:u=>u.nodes.find(n=>n.id==='credits')},
    {name:'keyboard',size:{width:1280,height:800},args:[],input:[{key:'Tab'},{key:'PageDown'}],pane:u=>u.nodes.find(n=>n.id==='credits')},
  ];
  for(const c of cases){
    const input=join(out,c.name+'.json'),report=join(out,c.name+'-report.json');
    await writeFile(input,JSON.stringify([{resize:c.size},{waitFrames:4},{checkpoint:'before'},...c.input,{waitFrames:4},{checkpoint:'after'}]));
    const child=spawnSync(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--save',join(out,'save.json'),'--automation',input,'--report',report,'--editor-events','--hidden',...c.args],{encoding:'utf8',timeout:30000,windowsHide:true});
    await writeFile(join(out,c.name+'-events.jsonl'),child.stdout??'');
    assert.equal(child.status,0,child.stderr||child.error?.message);
    const result=JSON.parse(await readFile(report,'utf8'));
    const before=result.steps.find(s=>s.command.checkpoint==='before').ui,after=result.steps.find(s=>s.command.checkpoint==='after').ui;
    assert.ok(c.pane(after).scrollTop>c.pane(before).scrollTop,c.name+' genuinely scrolled');
    assert.deepEqual(after.state,before.state);assert.deepEqual(after.commands,before.commands);
    // Read stdout emitted by the ordinary frame loop, not the automation report.
    // The consumer receives no action/rebuild after this input to refresh its view.
    const events=child.stdout.split('\n').flatMap(line=>{try{return [JSON.parse(line)]}catch{return []}}).filter(e=>e.event==='editor-view');
    const live=events.at(-1)?.view;
    assert.ok(live,c.name+' publishes a live view');
    assert.equal(c.pane(live).scrollTop,c.pane(after).scrollTop,c.name+' diagnostic geometry must reflect the visible scroll');
    assert.deepEqual(live.state,before.state);assert.deepEqual(live.commands,before.commands);
  }
  console.log('Native input diagnostic evidence: '+out);
});
