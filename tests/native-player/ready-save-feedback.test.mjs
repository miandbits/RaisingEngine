import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('successful load notice keeps continuous schedule UI; a blocked activity still offers planning', {timeout:180000}, async()=>{
 const projectRoot=resolve('tests/fixtures/projects/last-grandson-polished');
 const built=await buildSnapshot(await loadProject(projectRoot));assert.equal(built.ok,true);
 await mkdir(resolve('.cache'),{recursive:true});const root=await mkdtemp(resolve('.cache/ready-save-feedback-'));
 for(const blocked of [false,true]){
  const content=structuredClone(built.snapshot),tag=blocked?'blocked':'available';
  content.project.visual.screens=[
   {id:'title',label:'Title',elements:[{id:'start',kind:'button',x:10,y:10,width:200,height:48,action:'start'}]},
   {id:'home',label:'Home',elements:[
    {id:'plan',kind:'schedule',x:10,y:10,width:600,height:300},
    {id:'advance',kind:'button',action:'advance',x:10,y:330,width:200,height:48},
    {id:'planning-only',kind:'text',visibleWhen:'planning',text:'Choose a schedule',x:10,y:400,width:500,height:40}
   ]},
   {id:'schedule',label:'Result',elements:[{id:'result',kind:'result',x:10,y:10,width:600,height:400}]},
   {id:'story',label:'Story',elements:[]},{id:'ending',label:'Ending',elements:[]}
  ];
  content.project.visual.routing={defaultScreenId:'home',useLocationScreens:false,phases:[{phase:'title',screenId:'title'},{phase:'planning',screenId:'home'},{phase:'ready',screenId:'home'},{phase:'activity-result',screenId:'schedule'}]};
  delete content.project.visual.saveSystem;
  if(blocked){content.project.initialState.attributes.money=0;content.activities.find(a=>a.id==='rest').costs=[{attributeId:'money',amount:1}];}
  const game=resolve(root,tag);await exportGame({content,projectRoot,target:'windows',destination:game,runtimes:{windows:resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-month-two')}});
  const steps=[{click:'start'},...[0,1,2].map(i=>({select:'plan-'+i,value:'rest'})),{click:'advance'},{checkpoint:'ready'},
   {key:'F5'},{checkpoint:'saved'},{key:'F9'},{checkpoint:'loaded'},{scheduleAdvanceMs:750},{checkpoint:'advanced'}];
  const script=resolve(root,tag+'-steps.json'),reportFile=resolve(root,tag+'-report.json');await writeFile(script,JSON.stringify(steps.flatMap(s=>s.click||s.key?[s,{waitFrames:2}]:[s])));
  const run=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',resolve(root,tag+'-save.json'),'--automation',script,'--report',reportFile,'--hidden'],{windowsHide:true,encoding:'utf8',timeout:90000});
  const r=JSON.parse(await readFile(reportFile));assert.equal(run.status,0,JSON.stringify(r.error??run.error));assert.equal(r.ok,true);
  const at=id=>r.steps.find(s=>s.command.checkpoint===id).ui;
  assert.equal(at('ready').state.phase,'ready');assert.deepEqual(at('saved').state,at('loaded').state);
  if(blocked){assert.equal(at('advanced').state.phase,'ready');assert.equal(at('advanced').nodes.find(n=>n.id==='planning-only').visible,true);assert.ok(at('advanced').message);}
  else{for(const id of ['ready','saved','loaded'])assert.equal(at(id).nodes.find(n=>n.id==='planning-only').visible,false,id);assert.equal(at('advanced').state.phase,'activity-result');}
 }
 console.log('Ready save UI evidence: '+root);
});
