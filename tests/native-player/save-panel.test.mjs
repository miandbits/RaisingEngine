import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,readFile,writeFile,mkdir,copyFile} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('authored save panel distinguishes empty, saved, live, and damaged records',{timeout:120000},async()=>{
 const source=resolve('tests/fixtures/projects/last-grandson-polished');
 const result=await buildSnapshot(await loadProject(source));assert.equal(result.ok,true);
 const content=structuredClone(result.snapshot),root=await mkdtemp(resolve('.cache/save-panel-'));
 const runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??'dist/native-player-app');
 const el=(id,kind,x,y,width,height,extra={})=>({id,kind,x,y,width,height,...extra});
 const button=(id,action,x,y,extra={})=>el(id,'button',x,y,250,48,{text:id,action,autoLabel:false,...extra});
 content.project.visual.screens=[
  {id:'title',label:'Title',elements:[button('start','start',50,40),button('open','openModal',50,110,{screenId:'records'})]},
  {id:'home',label:'Home',elements:[button('change','executeAction',50,40,{actionId:'raise-skill'}),button('open','openModal',50,110,{screenId:'records'})]},
  {id:'schedule',label:'Schedule',elements:[]},{id:'story',label:'Story',elements:[]},{id:'ending',label:'Ending',elements:[]},
  {id:'records',label:'Records',modal:true,elements:[
   el('status','text',400,100,700,60,{binding:'save-summary',valueFormat:'status'}),
   el('values','text',400,180,700,120,{binding:'save-summary',valueFormat:'attributes',attributeIds:['skill']}),
   el('notice','text',400,320,700,60,{binding:'player-notice'}),
   button('save','save',400,400),button('load','load',700,400),button('close','closeModal',400,470,{color:'#263C36',fill:'#E4DDCD',hoverFill:'#D2DBCD'})
  ]}
 ];
 content.project.visual.routing={defaultScreenId:'home',useLocationScreens:false,phases:[{phase:'title',screenId:'title'},{phase:'planning',screenId:'home'}]};
 content.project.gameplay??={startLocationId:'home',locations:[{id:'home',name:'Home',connections:[]}],items:[],shops:[],interactions:[]};
 content.project.gameplay.actions??=[];
 content.project.gameplay.actions.push({id:'raise-skill',name:'Raise skill',costs:[],effects:[{op:'add',target:{kind:'attribute',attributeId:'skill'},value:{op:'literal',value:1}}],confirmation:{enabled:false}});
 const bundle=join(root,'game');
 const exported=await exportGame({projectRoot:source,content,target:'windows',destination:bundle,runtimes:{windows:runtime,web:resolve('dist/web-player')}});
 assert.equal(exported.directory,bundle);
 const save=join(root,'slot.json');
 const run=async(name,steps)=>{
  const script=join(root,name+'.json'),report=join(root,name+'-report.json');await writeFile(script,JSON.stringify(steps));
  const r=spawnSync(join(bundle,'RaisingPlayer.exe'),['--bundle',bundle,'--save',save,'--automation',script,'--report',report,'--hidden'],{windowsHide:true,encoding:'utf8',timeout:30000});
  assert.equal(r.status,0,r.stderr);const data=JSON.parse(await readFile(report,'utf8'));assert.equal(data.ok,true);return data;
 };
 const steps=[],click=id=>steps.push({click:id},{waitFrames:2}),mark=id=>steps.push({checkpoint:id});
 click('open');mark('empty');click('records__close');click('start');click('open');click('records__save');mark('saved');click('records__close');click('change');click('open');mark('changed');click('records__load');mark('restored');
 const r=await run('main',steps),at=id=>r.steps.find(s=>s.command.checkpoint===id).ui;
 assert.equal(at('empty').elements.find(b=>b.id==='records__load').enabled,false);
 assert.equal(at('empty').elements.find(b=>b.id==='records__save').enabled,false);
 assert.equal(at('empty').elements.find(b=>b.id==='records__close').color.toLowerCase(),'#263c36','Authored modal button color must use the same style rules as the base screen');
 assert.equal(at('saved').elements.find(b=>b.id==='records__load').enabled,true);
 assert.equal(at('saved').elements.find(b=>b.id==='records__save').enabled,false,'A completed modal save cannot be accidentally repeated');
 assert.equal(at('changed').elements.find(b=>b.id==='records__save').enabled,true,'Reopening the panel permits a new save');
 assert.equal(at('changed').state.attributes.skill,at('saved').state.attributes.skill+1);
 assert.deepEqual(at('restored').state,at('saved').state);
 const values=ui=>ui.texts['records__values'];
 assert.ok(values(at('saved')));assert.equal(values(at('changed')),values(at('saved')),'Saved values must not follow live changes');
 await writeFile(save,'{broken');const broken=await run('damaged',[{click:'open'},{waitFrames:2},{checkpoint:'bad'}]);
 assert.equal(broken.final.elements.find(b=>b.id==='records__load').enabled,false);
 assert.equal(await readFile(save,'utf8'),'{broken','Preview must not rewrite damaged files');
 const oldRuntime=join(root,'old-runtime');await mkdir(oldRuntime);
 const capabilities=JSON.parse(await readFile(join(runtime,'capabilities.json'),'utf8'));
 capabilities.visualFeatures=capabilities.visualFeatures.filter(x=>x!=='save-panel-v1');
 await writeFile(join(oldRuntime,'capabilities.json'),JSON.stringify(capabilities));
 await copyFile(join(runtime,'RaisingPlayer.exe'),join(oldRuntime,'RaisingPlayer.exe'));
 await assert.rejects(exportGame({projectRoot:source,content,target:'windows',destination:join(root,'old'),runtimes:{windows:oldRuntime,web:resolve('dist/web-player')}}),/save-panel-v1/);
});
