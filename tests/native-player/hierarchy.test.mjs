import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('native hierarchy resolves parent anchors, inherited conditions, component scopes and clipped pointer input', {timeout:120000},async()=>{
 await mkdir(resolve('.cache/native-hierarchy'),{recursive:true});const root=await mkdtemp(resolve('.cache/native-hierarchy/run-'));
 const projectRoot=resolve(root,'project');await cp(resolve('examples/academy-journey'),projectRoot,{recursive:true});
 const file=resolve(projectRoot,'project.json'),p=JSON.parse(await readFile(file,'utf8'));
 p.visual.viewportMode='responsive';
 p.visual.components??=[];p.visual.components.push({id:'nested-card',label:'Nested card',width:260,height:120,elements:[
  {id:'card-group',kind:'group',x:10,y:10,width:240,height:100,anchorX:'stretch',clipChildren:true,opacity:0.8},
  {id:'card-text',parentId:'card-group',kind:'text',x:10,y:10,width:150,height:30,text:'Nested template'},
 ]});
 p.visual.dataTables??=[];p.visual.dataTables.push({id:'nested-rows',name:'Nested rows',columns:[{id:'name',name:'Name',type:'text'}],rows:[{id:'one',values:{name:'Bound nested row'}}]});
 p.visual.screens.find(s=>s.id==='home').elements=[
  {id:'outer',kind:'group',x:100,y:100,width:400,height:400,anchorX:'stretch',fill:'#204040',clipChildren:true,opacity:0.5},
  {id:'inner',parentId:'outer',kind:'group',x:40,y:30,width:300,height:160,anchorX:'stretch',clipChildren:true,opacity:0.5},
  {id:'clipped-button',parentId:'inner',kind:'button',x:-100,y:10,width:160,height:40,text:'Outside center',action:'navigate',screenId:'schedule'},
  {id:'nested-button',parentId:'inner',kind:'button',x:200,y:70,width:80,height:40,anchorX:'right',text:'Schedule',action:'navigate',screenId:'schedule'},
  {id:'instance',parentId:'outer',kind:'component',componentId:'nested-card',x:40,y:220,width:260,height:120},
  {id:'hidden-group',kind:'group',x:800,y:50,width:200,height:100,hidden:true},
  {id:'hidden-button',parentId:'hidden-group',kind:'button',x:0,y:0,width:120,height:40,text:'Hidden',action:'sound'},
  {id:'disabled-group',kind:'group',x:800,y:180,width:200,height:100,enabledCondition:{op:'not',condition:{op:'always'}}},
  {id:'disabled-button',parentId:'disabled-group',kind:'button',x:0,y:0,width:120,height:40,text:'Disabled',action:'navigate',screenId:'schedule'},
  {id:'disabled-widget',parentId:'disabled-group',kind:'locations',x:0,y:50,width:200,height:160},
  {id:'nested-list',kind:'collection',componentId:'nested-card',source:{type:'table',tableId:'nested-rows'},x:650,y:400,width:300,height:120,rowHeight:120,
   bindings:[{elementId:'card-text',property:'text',field:'name'}]},
 ];await writeFile(file,JSON.stringify(p));
 const built=await buildSnapshot(await loadProject(projectRoot));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
 const destination=resolve(root,'game');await exportGame({content:built.snapshot,projectRoot,destination,target:'windows',runtimes:{windows:resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app'),web:resolve('dist/web')}});
 const steps=[{click:'start'},{checkpoint:'initial',screenshot:resolve(root,'initial.png')},
  {clickWindow:'clipped-button'},{checkpoint:'clipped-click'},{resize:{width:1600,height:720}},{waitFrames:20},{checkpoint:'wide',screenshot:resolve(root,'wide.png')},
  {clickWindow:'nested-button'},{checkpoint:'navigated'},{key:'F5'},{key:'F9'},{checkpoint:'restored'}];
 const input=resolve(root,'steps.json'),output=resolve(root,'report.json');await writeFile(input,JSON.stringify(steps));
 const child=spawnSync(resolve(destination,'RaisingPlayer.exe'),['--bundle',destination,'--save',resolve(root,'save.json'),'--hidden','--automation',input,'--report',output],{encoding:'utf8',timeout:90000,windowsHide:true});
 assert.equal(child.status,0,`${child.error?.message??child.stderr}\n${output}`);
 const report=JSON.parse(await readFile(output,'utf8')),ui=tag=>report.steps.find(s=>s.command.checkpoint===tag).ui;
 assert.deepEqual(ui('clipped-click').state,ui('initial').state);
 assert.equal(ui('initial').elements.find(e=>e.id==='hidden-button'),undefined);
 assert.equal(ui('initial').elements.find(e=>e.id==='disabled-button').enabled,false);
 assert.equal(ui('initial').elements.find(e=>e.id==='visit-market').enabled,false);
 assert.equal(ui('initial').texts['instance__card-text'],'Nested template');
 assert.equal(ui('initial').texts['nested-list__one__card-text'],'Bound nested row');
 const node=(tag,id)=>ui(tag).nodes.find(n=>n.id===id);
 assert.equal(node('initial','clipped-button').clickableAtCenter,false);
 assert.equal(node('initial','nested-button').x,340);assert.equal(node('wide','nested-button').x,660);
 assert.equal(node('initial','instance__card-text').x,160);
 assert.equal(ui('navigated').screenId,'schedule');assert.deepEqual(ui('restored').state,ui('navigated').state);
 console.log(`Native hierarchy evidence: ${root}`);
});
