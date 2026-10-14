import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

// Consume the Qt-authored project without changing its content or the user's save.
test('Qt-authored modular battle HUD selects targets, restores pending actions and shows committed rewards',
 {skip:!process.env.RAISING_BATTLE_HUD_PROJECT,timeout:180000},async()=>{
  const projectRoot=resolve(process.env.RAISING_BATTLE_HUD_PROJECT);
  const built=await buildSnapshot(await loadProject(projectRoot));
  assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  await mkdir(resolve('.cache/native-battle-hud'),{recursive:true});
  const root=await mkdtemp(resolve('.cache/native-battle-hud/run-')),destination=resolve(root,'game');
  await exportGame({content:built.snapshot,projectRoot,destination,target:'windows',runtimes:{windows:resolve('dist/native-player-app'),web:resolve('dist/web')}});
  const action=id=>`hud-actions__${id}__button`,target=id=>`hud-targets__${id}__button`;
  const steps=[{click:'start'},{click:'schedule-link'},
   {select:'plan-0',value:'forest-trip'},{select:'plan-1',value:'rest'},{select:'plan-2',value:'rest'},
   {click:'advance'},{click:'advance'},{click:'move-expedition-clearing'},{checkpoint:'battle'},
   {click:action('guard')},{checkpoint:'guard-selected'},{click:target('self')},{checkpoint:'guard-targeted'},
   {click:'hud-confirm'},{checkpoint:'guarded'},
   {click:action('attack')},{click:target('enemy')},{key:'F5'},{checkpoint:'pending',screenshot:resolve(root,'pending.png')},
   {click:'hud-cancel'},{checkpoint:'cancelled'},{key:'F9'},{checkpoint:'loaded'},
   {click:'hud-confirm'},{checkpoint:'first-hit'}];
  // The authored hero begins with attack zero; canonical minimum damage is one.
  const enemy=built.snapshot.project.systems.enemies.find(e=>e.id==='forest-slime');
  for(let turn=1;turn<enemy.maxHp;turn++)steps.push({click:action('attack')},{click:target('enemy')},{click:'hud-confirm'});
  steps.push({checkpoint:'victory',screenshot:resolve(root,'victory.png')},{key:'F5'},
   {click:'move-expedition-entrance'},{click:'move-expedition-clearing'},{checkpoint:'revisited'},
   {key:'F9'},{checkpoint:'victory-loaded'});
  const input=resolve(root,'steps.json'),output=resolve(root,'report.json'),save=resolve(root,'save.json');
  await writeFile(input,JSON.stringify(steps));
  const child=spawnSync(resolve(destination,'RaisingPlayer.exe'),['--bundle',destination,'--save',save,'--hidden','--automation',input,'--report',output],{encoding:'utf8',timeout:150000,windowsHide:true});
  assert.equal(child.status,0,`${child.error?.message??child.stderr}\n${output}`);
  const report=JSON.parse(await readFile(output,'utf8'));assert.equal(report.ok,true);
  const ui=tag=>report.steps.find(s=>s.command.checkpoint===tag).ui;
  const enabled=(tag,id)=>ui(tag).elements.find(e=>e.id===id)?.enabled;
  const initial=ui('battle').state;
  assert.deepEqual(ui('guard-selected').state,initial);assert.deepEqual(ui('guard-targeted').state,initial);
  assert.equal(enabled('guard-selected','hud-confirm'),false);assert.equal(enabled('guard-targeted','hud-confirm'),true);
  assert.equal(ui('guarded').state.systems.battle.turn,1);
  assert.equal(ui('guarded').state.systems.battle.log[0].target,'self');
  assert.ok(ui('guarded').texts['hud-log__turn-1__title'].includes('방어'));
  assert.deepEqual(ui('cancelled').state,ui('pending').state);assert.equal(ui('cancelled').battleSelection,null);
  assert.deepEqual(ui('loaded').state,ui('pending').state);assert.deepEqual(ui('loaded').battleSelection,ui('pending').battleSelection);
  assert.equal(ui('first-hit').state.stateRevision,ui('pending').state.stateRevision+1);
  assert.equal(ui('first-hit').battleSelection,null);assert.equal(enabled('first-hit','hud-confirm'),false);
  const victory=ui('victory').state;
  assert.equal(victory.phase,'expedition');assert.equal(victory.systems.lastBattle.outcome,'victory');
  assert.equal(victory.systems.lastBattle.log.length,enemy.maxHp+1);
  assert.equal(victory.gameplay.inventory.flower,initial.gameplay.inventory.flower+1);
  assert.equal(victory.systems.lastBattle.rewardChanges[0].delta,1);
  assert.ok(ui('victory').texts['hud-list2__reward-1__title'].includes('+1'));
  assert.equal(ui('revisited').state.gameplay.inventory.flower,victory.gameplay.inventory.flower);
  assert.deepEqual(ui('revisited').state.systems.lastBattle,victory.systems.lastBattle);
  assert.deepEqual(ui('victory-loaded').state,victory);
  console.log(`Native authored battle evidence: ${root}`);
 });
