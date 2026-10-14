import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('native schedule columns and menu show escaped current attributes; zero results retain saved history', {timeout:120000},async t=>{
  await mkdir('.cache/native-activity-summary',{recursive:true});const root=await mkdtemp(resolve('.cache/native-activity-summary/run-'));
  const projectRoot=resolve(root,'project');await cp(resolve('examples/academy-journey'),projectRoot,{recursive:true});
  const project=JSON.parse(await readFile(resolve(projectRoot,'project.json'),'utf8'));
  project.visual.widgetTheme='ornate';
  const rest=JSON.parse(await readFile(resolve(projectRoot,'activities/rest.json'),'utf8'));
  rest.previewAttributes=[{attributeId:'money',label:'비용',valueFormat:'<b>{value}</b> $& {value}'},{attributeId:'skill',label:'단계'}];
  rest.baseEffects=[{op:'add',target:{kind:'attribute',attributeId:'money'},value:{op:'literal',value:0}}];delete rest.presentation;
  await writeFile(resolve(projectRoot,'activities/rest.json'),JSON.stringify(rest));
  project.initialState.attributes.money=17;
  const columns={id:'columns',kind:'schedule',x:10,y:10,width:800,height:160,scheduleLayout:'columns',visibleWhen:'planning'};
  const menu={id:'menu',kind:'schedule',x:10,y:190,width:528,height:392,frameAssetId:'visual-courtyard',scheduleLayout:'menu',scheduleGroups:[{id:'courses',label:'Courses',activityIds:['rest']},...Array.from({length:3},(_,i)=>({id:`group-${i}`,label:`분류 ${i}`,activityIds:['rest']}))],visibleWhen:'planning'};
  const advance={id:'next',kind:'button',action:'advance',text:'Next',x:840,y:600,width:200,height:60};
  const result={id:'result',kind:'result',x:10,y:10,width:800,height:400,visibleWhen:'result'};
  const calendar={id:'calendar-test',kind:'schedule',scheduleLayout:'calendar',x:840,y:10,width:420,height:460,visibleWhen:'planning'};
  const elements=[columns,menu,result,advance,calendar];
  for(const screen of project.visual.screens)if(['home','schedule'].includes(screen.id))screen.elements=structuredClone(elements);
  project.visual.screens.find(s=>s.id==='title').elements=[{id:'start-test',kind:'button',action:'start',text:'Start',x:20,y:20,width:200,height:60}];
  await writeFile(resolve(projectRoot,'project.json'),JSON.stringify(project));
  const built=await buildSnapshot(await loadProject(projectRoot));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  const runtime=resolve(process.env.RAISING_ACTIVITY_SUMMARY_RUNTIME??'dist/native-player-activity-summary');
  const destination=resolve(root,'game');await exportGame({content:built.snapshot,projectRoot,destination,target:'windows',runtimes:{windows:runtime,web:resolve('dist/web')}});
  const steps=[{clickWindow:'start-test'},{select:'plan-0',value:'rest'},{select:'plan-1',value:'rest'},{select:'plan-2',value:'rest'},
    {checkpoint:'columns'},{clickWindow:'plan-group-courses'},{checkpoint:'menu',screenshot:resolve(root,'menu.png')},
    {clickWindow:'plan-activity-rest'},{checkpoint:'calendar-name'},{clickWindow:'next'},{clickWindow:'next'},{checkpoint:'result',screenshot:resolve(root,'result.png')},{key:'F5'},{key:'F9'},{checkpoint:'restored'}];
  const file=resolve(root,'steps.json'),reportPath=resolve(root,'report.json'),save=resolve(root,'save.json');await writeFile(file,JSON.stringify(steps));
  const run=spawnSync(resolve(destination,'RaisingPlayer.exe'),['--bundle',destination,'--save',save,'--hidden','--automation',file,'--report',reportPath],{encoding:'utf8',timeout:60000,windowsHide:true});
  assert.equal(run.status,0,run.error?.message??run.stderr);
  const report=JSON.parse(await readFile(reportPath,'utf8'));assert.equal(report.ok,true);const at=id=>report.steps.find(s=>s.command.checkpoint===id).ui;
  const lastCategory=at('columns').nodes.find(n=>n.id==='plan-group-group-2');assert.ok(lastCategory.y+lastCategory.height<=menu.y+menu.height,lastCategory.y+lastCategory.height);
  assert.ok(Object.values(at('columns').texts).join('').includes('현재 비용 &lt;b&gt;17&lt;/b&gt; $&amp; 17'));
  assert.ok(Object.values(at('menu').texts).join('').includes('현재 비용 &lt;b&gt;17&lt;/b&gt; $&amp; 17'));
  assert.ok(at('calendar-name').texts['calendar-test'].includes('calendar-activity-name'));
  assert.ok(at('calendar-name').texts['calendar-test'].includes('휴식'));
  const output=at('result').texts.result;assert.ok(output.includes('능력 변화 없음'),output);assert.ok(!output.includes('소지금 0'),output);
  assert.equal(at('result').state.lastActivityResult.changes.some(c=>c.kind==='numeric'&&c.delta===0),true);
  assert.deepEqual(at('result').state,at('restored').state);
  assert.deepEqual(JSON.parse(await readFile(save,'utf8')).state,at('result').state);
  // A bounded scrolling menu preserves two columns and reachable buttons with
  // all eight long metadata entries. This is a disposable fixture, not production.
  const ids=[];
  for(let i=0;i<11;i++){
    const activity={...rest,id:`long-${i}`,label:`Long ${i}`,previewAttributes:Array.from({length:8},(_,n)=>({attributeId:'money',label:`정보${n}`,valueFormat:'긴정보'.repeat(20)+' {value}'}))};
    ids.push(activity.id);await writeFile(resolve(projectRoot,`activities/${activity.id}.json`),JSON.stringify(activity));
  }
  for(const screen of project.visual.screens)if(['home','schedule'].includes(screen.id)){
    const entry=screen.elements.find(e=>e.id==='menu');entry.width=528;entry.scheduleGroups[0].activityIds=ids;
  }
  await writeFile(resolve(projectRoot,'project.json'),JSON.stringify(project));
  const longBuilt=await buildSnapshot(await loadProject(projectRoot));assert.equal(longBuilt.ok,true,JSON.stringify(longBuilt.diagnostics));
  const longDestination=resolve(root,'long-game');await exportGame({content:longBuilt.snapshot,projectRoot,destination:longDestination,target:'windows',runtimes:{windows:runtime,web:resolve('dist/web')}});
  await writeFile(file,JSON.stringify([{clickWindow:'start-test'},{clickWindow:'plan-group-courses'},{checkpoint:'long',screenshot:resolve(root,'long.png')},{wheelWindow:{x:532,y:430,deltaY:-200}},{waitFrames:40},{checkpoint:'scrolled'},{clickWindow:'plan-activity-long-10'},{checkpoint:'selected',screenshot:resolve(root,'calendar-name.png')} ]));
  const longRun=spawnSync(resolve(longDestination,'RaisingPlayer.exe'),['--bundle',longDestination,'--save',resolve(root,'long-save.json'),'--hidden','--automation',file,'--report',resolve(root,'long-report.json')],{encoding:'utf8',timeout:60000,windowsHide:true});
  assert.equal(longRun.status,0,longRun.error?.message??longRun.stderr);
  const longReport=JSON.parse(await readFile(resolve(root,'long-report.json'),'utf8'));assert.equal(longReport.ok,true);
  const longUI=longReport.steps.find(s=>s.command.checkpoint==='long').ui;
  const first=longUI.elements.find(e=>e.id==='plan-activity-long-0'),second=longUI.elements.find(e=>e.id==='plan-activity-long-1');
  assert.equal(first.y,second.y);assert.ok(second.x>first.x);assert.ok(first.height<210);
  const scrolled=longReport.steps.find(s=>s.command.checkpoint==='scrolled').ui;
  assert.ok(scrolled.elements.find(e=>e.id==='plan-activity-long-0').y<first.y);
  assert.equal(longReport.final.controls.find(c=>c.id==='plan-0').value,'long-10');
  t.diagnostic(`Evidence: ${root}`);
});
