import test from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import {readFile,writeFile} from 'node:fs/promises';
import {fixture} from '../systems/fixture.mjs';
import createModule from '../../dist/wasm/raising-wasm.mjs';
import {WasmRuntime} from '../../dist/packages/native-bridge/wasm.js';
import {WebPlayerController} from '../../dist/packages/web-player/controller.js';
import {PlayerController} from '../../dist/packages/player-service/controller.js';

for(const backend of ['native','wasm'])test(`${backend} battle HUD selection, targets and midbattle save restore cannot duplicate rewards`,async()=>{
  const {root,built}=await fixture(p=>{p.systems.battleHistoryLimit=20;});assert.ok(built.ok,JSON.stringify(built.diagnostics));
  const runtime=backend==='wasm'?new WasmRuntime(await createModule()):null;let saved='';const file=path.join(root,'battle-save.json');
  const c=runtime?new WebPlayerController(built.snapshot,runtime,{read:async()=>saved,write:async text=>{saved=text;}},()=>true):new PlayerController();
  if(!runtime)assert.equal((await c.openProject(root)).error,null);assert.equal((await c.newRun(null)).error,null);
  const version=()=>({generation:c.generation,revision:c.session.state.stateRevision});
  const act=async action=>{const reply=await c.act(action,version());assert.equal(reply.error,null);return reply.view;};
  const save=async()=>{assert.equal((await(runtime?c.save(version()):c.save(file,version()))).error,null);if(!runtime)saved=await readFile(file,'utf8');return saved;};
  const load=async text=>{saved=text;if(!runtime)await writeFile(file,text);return runtime?c.load(version()):c.load(file,version());};
  await act({type:'equip',equipmentId:'iron-sword'});await act({type:'schedule',activityIds:['forest-trip','rest','rest']});await act({type:'execute'});await act({type:'moveExpedition',nodeId:'clearing'});
  await act({type:'selectBattleAction',battleAction:'guard'});await act({type:'selectBattleTarget',battleTarget:'self'});await act({type:'confirmBattleAction'});
  await act({type:'selectBattleAction',battleAction:'useItem',itemId:'potion'});assert.ok((await c.act({type:'selectBattleTarget',battleTarget:'enemy'},version())).error);await act({type:'selectBattleTarget',battleTarget:'self'});await act({type:'confirmBattleAction'});assert.equal(c.session.state.gameplay.inventory.potion,1);
  const initial=structuredClone(c.session);assert.equal(initial.state.phase,'battle');
  assert.ok((await c.act({type:'confirmBattleAction'},version())).error);
  await act({type:'selectBattleAction',battleAction:'attack'});assert.ok((await c.act({type:'selectBattleTarget',battleTarget:'self'},version())).error);assert.ok((await c.act({type:'confirmBattleAction'},version())).error);assert.deepEqual(c.session,initial);
  await act({type:'selectBattleTarget',battleTarget:'enemy'});const pending=structuredClone(c.presentation.battleSelection);const checkpoint=await save();assert.deepEqual(JSON.parse(checkpoint).playerPresentation.battleSelection,pending);
  await act({type:'cancelBattleAction'});assert.equal(c.presentation.battleSelection,null);assert.deepEqual(c.session,initial);
  assert.equal((await load(checkpoint)).error,null);assert.deepEqual(c.session,initial);assert.deepEqual(c.presentation.battleSelection,pending);
  const stale=version();await act({type:'confirmBattleAction'});const afterFirst=structuredClone(c.session);assert.equal(c.presentation.battleSelection,null);assert.equal(c.session.commands.length,initial.commands.length+1);assert.ok((await c.act({type:'confirmBattleAction'},stale)).error);assert.deepEqual(c.session,afterFirst);
  assert.equal((await load(checkpoint)).error,null);await act({type:'confirmBattleAction'});assert.deepEqual(c.session,afterFirst);
  for(let i=0;c.session.state.phase==='battle'&&i<20;i++){await act({type:'selectBattleAction',battleAction:'attack'});await act({type:'selectBattleTarget',battleTarget:'enemy'});await act({type:'confirmBattleAction'});}
  assert.equal(c.session.state.systems.lastBattle.outcome,'victory');assert.equal(c.session.state.gameplay.inventory.gem,1);assert.ok(c.session.state.systems.lastBattle.log.length>0);assert.ok(c.session.state.systems.lastBattle.rewardChanges.length>0);
  const completed=structuredClone(c.session),finished=await save();assert.equal((await load(finished)).error,null);assert.deepEqual(c.session,completed);assert.ok((await c.act({type:'confirmBattleAction'},version())).error);assert.deepEqual(c.session,completed);
  const bad=JSON.parse(checkpoint);bad.playerPresentation.battleSelection.target='self';assert.ok((await load(JSON.stringify(bad))).error);assert.deepEqual(c.session,completed);
});
