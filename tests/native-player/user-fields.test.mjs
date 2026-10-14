import test from 'node:test';
import assert from 'node:assert/strict';
import { cp,mkdir,mkdtemp,readFile,writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { spawnSync } from 'node:child_process';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { exportGame } from '../../dist/packages/export-service/export.js';

test('native text input retains Unicode drafts without rebuilding, creates once and restores exact names', {timeout:120000},async t=>{
  await mkdir('.cache/native-user-fields-tests',{recursive:true});const root=await mkdtemp(resolve('.cache/native-user-fields-tests/run-'));
  const projectRoot=resolve(root,'project');await cp(resolve('examples/academy-journey'),projectRoot,{recursive:true});
  const file=resolve(projectRoot,'project.json'),project=JSON.parse(await readFile(file,'utf8'));
  project.userFields=[{id:'daughter-name',type:'text',label:'딸 이름',initial:'올리브',minLength:3,maxLength:8},{id:'family-name',type:'text',label:'가족 이름',initial:'<b>',minLength:0,maxLength:12}];
  const fields=[{id:'name-input',kind:'input',userFieldId:'daughter-name',x:80,y:100,width:420,height:60,fill:'#fff6df',color:'#183d35',fontSize:26},
    {id:'name-label',kind:'text',binding:'user-field',userFieldId:'daughter-name',valueFormat:'이름: {value} / {value}',x:80,y:190,width:800,height:60,fontSize:26},
    {id:'family-label',kind:'text',binding:'user-field',userFieldId:'family-name',x:80,y:260,width:800,height:60,fontSize:26},
    {id:'name-copy',kind:'input',userFieldId:'daughter-name',x:80,y:330,width:420,height:60,fill:'#fff6df',color:'#183d35',fontSize:26}];
  project.visual.screens.find(s=>s.id==='title').elements=[...structuredClone(fields),{id:'start-user-game',kind:'button',action:'start',text:'시작',x:80,y:430,width:200,height:60}];
  project.visual.screens.find(s=>s.id==='home').elements=structuredClone(fields);await writeFile(file,JSON.stringify(project,null,2));
  const built=await buildSnapshot(await loadProject(projectRoot));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  const destination=resolve(root,'game');await exportGame({content:built.snapshot,projectRoot,destination,target:'windows',runtimes:{windows:resolve(process.env.NATIVE_USER_FIELDS_RUNTIME??'dist/native-player-user-fields'),web:resolve('dist/web')}});
  const save=resolve(root,'save.json');
  async function run(name,steps){const input=resolve(root,`${name}-steps.json`),report=resolve(root,`${name}-report.json`);await writeFile(input,JSON.stringify(steps));const result=spawnSync(resolve(destination,'RaisingPlayer.exe'),['--bundle',destination,'--save',save,'--hidden','--automation',input,'--report',report],{encoding:'utf8',timeout:60000,windowsHide:true});assert.equal(result.status,0,result.error?.message??result.stderr);const data=JSON.parse(await readFile(report,'utf8'));assert.equal(data.ok,true);return data;}
  const report=await run('create',[
    {waitFrames:8},{clickWindow:'user-input-name-input'},{key:'CtrlA'},{key:'Backspace'},{checkpoint:'empty'},
    {textInput:'미'},{checkpoint:'one'},{textInput:'😀'},{checkpoint:'two'},
    {clickWindow:'start-user-game'},{checkpoint:'invalid'},
    {clickWindow:'user-input-name-input'},{key:'CtrlA'},{key:'Backspace'},{textInput:'미😀나'},{checkpoint:'valid'},
    {clickWindow:'start-user-game'},{checkpoint:'created',screenshot:resolve(root,'created.png')},{key:'F5'},{key:'F9'},{checkpoint:'loaded'},
  ]);
  const at=name=>report.steps.find(s=>s.command.checkpoint===name).ui;
  for(const name of ['one','two']){assert.equal(at(name).documentGeneration,at('empty').documentGeneration);assert.equal(at(name).focusedId,'user-input-name-input');}
  assert.equal(at('one').userInputs['name-copy'].value,'미');assert.equal(at('two').userInputs['name-copy'].value,'미😀');
  assert.equal(at('one').userDrafts['daughter-name'],'미');assert.equal(at('two').userDrafts['daughter-name'],'미😀');assert.equal(at('invalid').state,null);assert.equal(at('invalid').userInputs['name-input'].value,'미😀');assert.match(at('invalid').message,/INVALID_USER_VALUE/);
  assert.deepEqual(at('created').state.userValues,{'daughter-name':'미😀나','family-name':'<b>'});assert.equal(at('created').userInputs['name-input'].disabled,true);assert.ok(at('created').texts['name-label'].includes('미😀나'));assert.ok(at('created').texts['family-label'].includes('&lt;b&gt;'));assert.deepEqual(at('loaded').state,at('created').state);assert.deepEqual(at('loaded').userDrafts,{});
  const saved=JSON.parse(await readFile(save,'utf8'));assert.deepEqual(saved.origin.userValues,{'daughter-name':'미😀나'});assert.deepEqual(saved.commands,[]);
  const restarted=await run('restart',[{key:'F9'},{checkpoint:'restored'}]);assert.deepEqual(restarted.final.state,saved.state);assert.equal(restarted.final.userInputs['name-input'].disabled,true);
  const tampered=structuredClone(saved);tampered.origin.userValues['daughter-name']='다른이름';await writeFile(save,JSON.stringify(tampered));
  const rejected=await run('tamper',[{key:'F9'},{checkpoint:'rejected'}]);assert.equal(rejected.final.state,null);assert.match(rejected.final.message,/재현 결과/);
  for(const area of ['envelope','origin','overrides']){
    const unknown=structuredClone(saved);const container=area==='envelope'?unknown:area==='origin'?unknown.origin:unknown.origin.initialOverrides;container.unrecognized='value';await writeFile(save,JSON.stringify(unknown));
    const result=await run(`unknown-${area}`,[{clickWindow:'start-user-game'},{checkpoint:'existing'},{key:'F9'},{checkpoint:'rejected'}]);
    assert.deepEqual(result.final.state,result.steps.find(s=>s.command.checkpoint==='existing').ui.state);assert.match(result.final.message,/알 수 없는 항목/);assert.deepEqual(JSON.parse(await readFile(save,'utf8')),unknown);
  }
  t.diagnostic(`Evidence: ${root}`);
});
