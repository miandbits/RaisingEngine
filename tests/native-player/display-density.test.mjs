import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('display density preserves authored layout, mouse/touch input and saved state across resizing',{timeout:180000},async()=>{
  const source=resolve('tests/fixtures/projects/last-grandson-polished');
  const snapshot=await buildSnapshot(await loadProject(source));assert.equal(snapshot.ok,true);
  const content=structuredClone(snapshot.snapshot),root=await mkdtemp(resolve('.cache/display-density-'));
  const runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app');
  const button=(id,action,x,y)=>({id,kind:'button',x,y,width:180,height:48,text:id,action,autoLabel:false});
  Object.assign(content.project.visual,{width:640,height:360,viewportMode:'fit',screens:[
    {id:'title',label:'Title',elements:[button('start','start',20,250)]},
    {id:'home',label:'Home',elements:[
      {id:'sample',kind:'text',x:20,y:20,width:580,height:180,fontSize:20,text:'눈을 건너는 아이 / 日本語 / AVATAR\n둘째 줄도 같은 자리입니다. 0123456789'},
      button('save','save',20,250),button('load','load',230,250)]},
    {id:'schedule',label:'Schedule',elements:[]},{id:'story',label:'Story',elements:[]},{id:'ending',label:'Ending',elements:[]}
  ],routing:{defaultScreenId:'home',useLocationScreens:false,phases:[{phase:'title',screenId:'title'},{phase:'planning',screenId:'home'}]}});
  const bundle=join(root,'game');await exportGame({projectRoot:source,content,target:'windows',destination:bundle,runtimes:{windows:runtime,web:runtime}});
  const steps=[{resize:{width:640,height:360}},{waitFrames:3},{clickWindow:'start'},{waitFrames:3},{checkpoint:'base'}];
  for(const [name,width,height] of [['fractional',960,540],['double',1280,720],['triple',1920,1080],['pillarbox',1920,720],['letterbox',1280,1000],['back',640,360]]){
    steps.push({resize:{width,height}},{waitFrames:3},{checkpoint:name},{screenshot:join(root,name+'.png')});
    steps.push({clickWindow:'save'},{waitFrames:2},{touchWindow:'load'},{waitFrames:2},{checkpoint:name+'-loaded'});
  }
  const script=join(root,'steps.json'),report=join(root,'report.json');await writeFile(script,JSON.stringify(steps));
  const run=spawnSync(join(bundle,'RaisingPlayer.exe'),['--bundle',bundle,'--save',join(root,'save.json'),'--automation',script,'--report',report,'--hidden'],{windowsHide:true,encoding:'utf8',timeout:90000});
  assert.equal(run.status,0,run.stderr||String(run.error));
  const result=JSON.parse(await readFile(report,'utf8')),at=name=>result.steps.find(s=>s.command.checkpoint===name).ui;
  const base=at('base'),geometry=ui=>ui.nodes.filter(n=>['sample','save','load'].includes(n.id)).map(n=>({id:n.id,x:n.x,y:n.y,width:n.width,height:n.height}));
  assert.equal(base.presentation.fontRasterDensity,1);
  for(const name of ['fractional','double','triple','pillarbox','letterbox','back']){
    const view=at(name),scale=view.presentation.height/view.viewport.height;
    assert.equal(view.presentation.fontRasterDensity,Math.min(4,Math.max(1,Math.ceil(scale-.01))));
    assert.deepEqual(geometry(view),geometry(base),name+' must not reflow authored content');
    assert.deepEqual(at(name+'-loaded').state,base.state,name+' must preserve state and input mapping');
    const png=await readFile(join(root,name+'.png'));
    // SDL captures the game viewport, excluding the surrounding letterbox.
    assert.equal(png.readUInt32BE(16),Math.floor(view.presentation.width));assert.equal(png.readUInt32BE(20),Math.floor(view.presentation.height));
  }
  assert.equal(at('double').presentation.fontRasterDensity,2);
  assert.equal(at('triple').presentation.fontRasterDensity,3);
  assert.equal(at('back').presentation.fontRasterDensity,1);
});
