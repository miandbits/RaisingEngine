import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile,stat} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {desktopTarget,gameDirectory,playerExecutable} from '../../dist/packages/export-service/targets.js';

test('rendered layout diagnostics measure clipping, point targets and solid contrast without treating scroll reachability as failure',{timeout:120000},async()=>{
  const project=resolve('examples/my-first-raising-game'),built=await buildSnapshot(await loadProject(project));assert.equal(built.ok,true);
  const content=structuredClone(built.snapshot),visual=content.project.visual,title=visual.screens.find(s=>s.id==='title');
  const text=(id,x,y,width,height,copy,extra={})=>({id,kind:'text',x,y,width,height,text:copy,fontSize:18,fill:'#ffffff',color:'#000000',...extra});
  title.elements=[
    {id:'tiny',kind:'button',x:20,y:20,width:30,height:30,text:'Go',action:'start',fill:'#ffffff',color:'#000000'},
    text('clipped',20,100,120,24,'A long wrapped sentence that cannot fit into its deliberately short box. '.repeat(5)),
    text('low',20,150,400,60,'Low contrast text',{color:'#dddddd'}),
    text('good',20,230,400,60,'Readable solid contrast'),
    text('translucent',20,310,400,60,'Unknown translucent contrast',{opacity:.5}),
    {id:'painted-sibling',kind:'panel',x:20,y:400,width:400,height:60,fill:'#000000'},
    {id:'layered',kind:'text',x:20,y:400,width:400,height:60,fontSize:18,text:'Text over a separate painted panel',color:'#ffffff'},
    {id:'outside',kind:'button',x:1260,y:400,width:100,height:60,text:'Offscreen',action:'advance'},
    {id:'fixed',kind:'group',x:850,y:80,width:200,height:40,clipChildren:true},
    text('ancestor-clipped',0,0,180,150,'Text that fits its own box but is clipped by its fixed ancestor. '.repeat(2),{parentId:'fixed'}),
    {id:'fixed-button',kind:'button',parentId:'fixed',x:0,y:30,width:150,height:60,text:'Cut off',action:'advance'},
    {id:'fixed-scroll',kind:'group',x:850,y:300,width:200,height:40,clipChildren:true},
    {id:'nested-scroll',kind:'group',parentId:'fixed-scroll',x:0,y:0,width:180,height:140,overflow:'scroll'},
    text('nested-copy',0,0,160,800,'Nested scrolling content. '.repeat(10),{parentId:'nested-scroll'}),
    {id:'scroll',kind:'group',x:500,y:80,width:300,height:140,overflow:'scroll'},
    text('scroll-copy',0,0,260,1000,'Reachable content. '.repeat(20),{parentId:'scroll'}),
    {id:'scroll-action',kind:'button',parentId:'scroll',x:0,y:1050,width:250,height:60,text:'Reachable button',action:'advance'},
  ];
  visual.readableLayout={fontSize:20};title.readableLayout={items:[{elementId:'clipped'},{elementId:'low'},{elementId:'good'}],actions:['tiny']};
  const output=resolve('.cache/native-layout-diagnostics');await mkdir(output,{recursive:true});const out=await mkdtemp(join(output,'run-'));
  const runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app'),target=desktopTarget(),destination=join(out,'game');
  await exportGame({content,projectRoot:project,destination,target,runtimes:{windows:runtime,macos:runtime,web:runtime}});
  async function run(name,width,height,scale){const report=join(out,name+'.json'),steps=join(out,name+'-steps.json'),save=join(out,name+'-save.json');
    await writeFile(steps,JSON.stringify([{resize:{width,height}},{waitFrames:8},{checkpoint:'measured',screenshot:join(out,name+'.png')}]));
    const child=spawnSync(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--save',save,'--hidden','--preview-screen','title','--preview-reading-layout','auto','--preview-text-scale',String(scale),'--locale','en','--automation',steps,'--report',report,'--report-checkpoints-only'],{encoding:'utf8',timeout:60000});
    assert.equal(child.status,0,child.stderr||child.error?.message);const result=JSON.parse(await readFile(report,'utf8'));assert.equal(result.ok,true,result.error);assert.equal(await stat(save).then(()=>true,()=>false),false,'Preview does not write a game save');return result.steps.find(s=>s.command.checkpoint==='measured').ui;
  }
  const desktop=await run('desktop',1280,720,1),d=desktop.layoutDiagnostics;assert.equal(d.measurement,'rmlui-rendered-layout');assert.equal(d.scope,'current-screen-and-state');
  const has=(code,elementId)=>d.issues.some(i=>i.code===code&&i.elementId===elementId);
  assert.ok(has('CLIPPED_TEXT','clipped'));assert.ok(has('CLIPPED_TEXT','ancestor-clipped'));assert.ok(has('CLIPPED_SCROLL_REGION','nested-scroll'));assert.ok(has('SMALL_TOUCH_TARGET','tiny'));assert.ok(has('CLIPPED_CONTROL','fixed-button'));assert.ok(has('OUTSIDE_VIEWPORT','outside'));assert.ok(has('LOW_CONTRAST','low'));
  assert.ok(!has('LOW_CONTRAST','good'));assert.ok(!has('LOW_CONTRAST','translucent'));
  assert.ok(d.contrastSamples.some(s=>s.elementId==='translucent'&&!s.measured&&s.reason==='transparency'));
  assert.ok(d.contrastSamples.some(s=>s.elementId==='layered'&&!s.measured&&s.reason==='image-decorator-or-layered-surface'));
  assert.ok(d.contrastSamples.some(s=>s.elementId==='good'&&s.measured&&s.ratio>20));
  assert.ok(d.coverage.scrollRegions>0);assert.equal(d.coverage.truncated,false);
  assert.ok(!d.issues.some(i=>['scroll-copy','scroll-action'].includes(i.elementId)&&['OUTSIDE_VIEWPORT','CLIPPED_CONTROL','CLIPPED_TEXT'].includes(i.code)),JSON.stringify(d.issues));
  for(const id of ['nested-scroll','nested-copy']){const node=desktop.nodes.find(n=>n.id===id);assert.ok(node);assert.equal(node.x,850,'Clipped scroll hierarchy keeps world x');assert.equal(node.y,300,'Clipped scroll hierarchy keeps world y');}
  assert.equal(d.configuration.windowWidth,1280);assert.equal(d.configuration.locale,'en');assert.equal(d.configuration.readableActive,false);
  const phone=await run('phone-200',390,844,2),p=phone.layoutDiagnostics;assert.equal(p.configuration.windowWidth,390);assert.equal(p.configuration.windowHeight,844);assert.equal(p.configuration.textScale,2);assert.equal(p.configuration.readableActive,true);
  const narrow=await run('phone-300-200',300,568,2);assert.equal(narrow.layoutDiagnostics.configuration.windowWidth,300);assert.equal(narrow.layoutDiagnostics.configuration.textScale,2);assert.equal(narrow.layoutDiagnostics.configuration.readableActive,true);
  assert.deepEqual(phone.commands,[]);assert.deepEqual(desktop.commands,[]);assert.deepEqual(phone.state,desktop.state);
  await writeFile(join(output,'latest.json'),JSON.stringify({ok:true,out,desktopIssues:d.issues.length,phoneIssues:p.issues.length},null,2));console.log('Layout diagnostics evidence: '+out);
});
