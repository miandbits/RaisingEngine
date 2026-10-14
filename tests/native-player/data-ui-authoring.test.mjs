import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdir,mkdtemp,readFile,writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { spawnSync } from 'node:child_process';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { exportGame } from '../../dist/packages/export-service/export.js';

// This verifier consumes the project saved through Qt authoring. It never changes content.
test('Qt-authored data cards play through native purchase, policies, relationships and custom rows',
  {skip:!process.env.RAISING_DATA_UI_PROJECT,timeout:120000},async()=>{
  const projectRoot=resolve(process.env.RAISING_DATA_UI_PROJECT),loaded=await loadProject(projectRoot),built=await buildSnapshot(loaded);
  assert.equal(built.ok,true,JSON.stringify(built.diagnostics));const project=built.snapshot.project;
  await mkdir(resolve('.cache/native-data-ui-authoring'),{recursive:true});const root=await mkdtemp(resolve('.cache/native-data-ui-authoring/run-'));
  const destination=resolve(root,'game');await exportGame({content:built.snapshot,projectRoot,destination,target:'windows',runtimes:{windows:resolve('dist/native-player-app'),web:resolve('dist/web')}});
  const route=(screenId,target)=>{const found=project.visual.screens.find(s=>s.id===screenId)?.elements.find(e=>e.kind==='button'&&e.action==='navigate'&&e.screenId===target);assert.ok(found,`${screenId} needs an authored route to ${target}`);return found.id;};
  const row=(id,child='button')=>`collection-1__${id}__${child}`;
  const steps=[{click:'start'},{key:'F5'},{checkpoint:'initial'},
    {click:route('home','screen-3')},{checkpoint:'outside-shop'},{key:'F9'},
    {click:'visit-market'},{click:route('market','screen-3')},{checkpoint:'shop'},
    {click:row('florist--flower')},{click:'cancel-purchase'},{checkpoint:'cancel'},
    {click:row('florist--flower')},{click:'confirm-purchase'},{checkpoint:'bought'},
    {key:'F9'},{click:route('home','screen-4')},{click:row('policy-2--option-1')},{click:'confirm-purchase'},
    {click:row('policy-3--option-2')},{click:'confirm-purchase'},{checkpoint:'policies',screenshot:resolve(root,'policies.png')},
    {key:'F9'},{click:route('home','screen-5')},{checkpoint:'relationships'},
    {click:row('mentor--trust')},{checkpoint:'custom'},
    {click:row('row-1')},{click:'cancel-purchase'},{checkpoint:'custom-cancel'},
    {click:row('row-1')},{click:'confirm-purchase'},{checkpoint:'rest'},{key:'F5'},
    {click:row('row-2')},{click:'confirm-purchase'},{checkpoint:'prayer'},
    {key:'F9'},{checkpoint:'restored',screenshot:resolve(root,'restored.png')}];
  const input=resolve(root,'steps.json'),output=resolve(root,'report.json');await writeFile(input,JSON.stringify(steps));
  const child=spawnSync(resolve(destination,'RaisingPlayer.exe'),['--bundle',destination,'--save',resolve(root,'save.json'),'--hidden','--automation',input,'--report',output],{encoding:'utf8',timeout:90000,windowsHide:true});
  assert.equal(child.status,0,child.error?.message??child.stderr);const report=JSON.parse(await readFile(output,'utf8'));assert.equal(report.ok,true);
  const ui=tag=>report.steps.find(s=>s.command.checkpoint===tag).ui;
  const button=(tag,id)=>ui(tag).elements.find(e=>e.id===id);
  const initial=ui('initial').state;
  assert.equal(ui('outside-shop').nodes.find(e=>e.id==='collection-1').rowCount,0);
  assert.deepEqual(ui('cancel').state,ui('shop').state);
  assert.equal(ui('bought').state.attributes.money,initial.attributes.money-40);
  assert.equal(ui('bought').state.gameplay.inventory.flower,initial.gameplay.inventory.flower+1);
  assert.equal(ui('policies').state.gameplay.policySelections['policy-2'],'option-1');
  assert.equal(ui('policies').state.gameplay.policySelections['policy-3'],'option-2');
  assert.equal(button('policies',row('policy-2--option-2')).enabled,false);
  assert.equal(button('policies',row('policy-3--option-2')).enabled,false);
  assert.ok(ui('policies').nodes.find(e=>e.id==='collection-1').scrollTop>0);
  assert.ok(ui('relationships').texts[row('mentor--trust','value')].includes(String(initial.relationships.mentor.trust)));
  assert.equal(ui('custom').nodes.find(e=>e.id==='collection-1').rowCount,2);
  assert.equal(button('custom',row('row-3')),undefined);
  assert.deepEqual(ui('custom-cancel').state,ui('custom').state);
  assert.equal(ui('rest').state.attributes.money,initial.attributes.money-8);
  assert.equal(ui('rest').state.attributes.stamina,Math.min(100,initial.attributes.stamina+20));
  assert.equal(ui('prayer').state.attributes.money,ui('rest').state.attributes.money-1);
  assert.equal(ui('prayer').state.attributes.reputation,ui('rest').state.attributes.reputation+2);
  assert.deepEqual(ui('restored').state,ui('rest').state);
});
