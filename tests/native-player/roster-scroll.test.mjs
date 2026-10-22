import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

for(const width of [1100,360])test(`roster lists stay contained with accessible actions at width ${width}`,{timeout:90000},async()=>{
  const projectRoot=resolve('examples/genre-suite/roster');
  const built=await buildSnapshot(await loadProject(projectRoot));assert.ok(built.ok);
  const content=structuredClone(built.snapshot);content.events=[];
  const [work,rest]=content.activities;
  work.description='Personal skill and shared income. '.repeat(8);
  rest.description='Recover personal energy.';
  const first=content.project.roster.members[0];
  const character=content.characters.find(c=>c.id===first.characterId);
  content.project.roster.members=Array.from({length:6},(_,i)=>({...first,characterId:'trainee-'+i,activityId:work.id}));
  content.project.protagonistId='trainee-0';
  content.characters=content.project.roster.members.map((m,i)=>({...character,id:m.characterId,name:'Trainee '+i}));
  const widget={id:'schedule-widget',kind:'schedule',x:40,y:40,width,height:270,showActivityDescription:true};
  content.project.visual={...content.project.visual,routing:{defaultScreenId:'schedule',useLocationScreens:false,phases:[{phase:'title',screenId:'title'}]},screens:[
    {id:'title',label:'Title',elements:[{id:'start',kind:'button',x:40,y:40,width:200,height:60,action:'start'}]},
    {id:'schedule',label:'Schedule',elements:[widget,{id:'outside',kind:'button',text:'Outside',x:40,y:330,width:200,height:60,action:'navigate',screenId:'title'}]}]};
  const root=await mkdtemp(resolve('.cache/roster-scroll-')),game=resolve(root,'game');
  await exportGame({content,projectRoot,target:'windows',destination:game,runtimes:{windows:resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app')}});
  const steps=[{click:'start'},{checkpoint:'initial',screenshot:resolve(root,'initial.png')},
    {wheelWindow:{x:Math.min(580,width/2+40),y:120,deltaY:-300}},{checkpoint:'scrolled'},
    {select:'roster-trainee-5',value:rest.id},{checkpoint:'draft'},
    {click:'roster-assign'},{checkpoint:'assigned',screenshot:resolve(root,'assigned.png')},{key:'F5'},
    {click:'roster-execute'},{checkpoint:'executed'},
    {key:'F9'},{checkpoint:'restored'},{click:'outside'},{checkpoint:'outside'}];
  const script=resolve(root,'steps.json'),reportFile=resolve(root,'report.json');
  await writeFile(script,JSON.stringify(steps.flatMap(s=>s.checkpoint?[s]:[s,{waitFrames:3}])));
  const run=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',resolve(root,'save.json'),'--automation',script,'--report',reportFile,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:60000});
  const report=JSON.parse(await readFile(reportFile));assert.equal(run.status,0,JSON.stringify(report.error??run.error));assert.ok(report.ok);
  const at=id=>report.steps.find(s=>s.command.checkpoint===id).ui;
  const node=(id,key)=>at(id).nodes.find(n=>n.id===key);
  for(const checkpoint of ['initial','scrolled','draft','assigned','restored']){
    for(const id of ['roster-assign','roster-execute']){
      const button=node(checkpoint,id);assert.ok(button.y>=widget.y&&button.y+button.height<=widget.y+widget.height+1,JSON.stringify(button));
      assert.ok(button.visibleHitbox.height>=button.height-1,JSON.stringify(button));
    }
  }
  const scrolled=node('scrolled','roster-actors-schedule-widget');assert.ok(scrolled.scrollTop>0,JSON.stringify(scrolled));
  const draftScroll=node('draft','roster-actors-schedule-widget').scrollTop;assert.ok(draftScroll>0);
  assert.equal(node('assigned','roster-actors-schedule-widget').scrollTop,draftScroll);
  assert.equal(at('assigned').state.roster.assignments['trainee-5'],rest.id);
  assert.deepEqual(at('assigned').state,at('restored').state);
  assert.notDeepEqual(at('executed').state,at('assigned').state);
  assert.equal(at('outside').screenId,'title');
  console.log('Roster scroll evidence: '+root);
});
