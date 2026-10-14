import test from 'node:test';
import assert from 'node:assert/strict';
import {fixture} from './fixture.mjs';
import {driver,start,win} from './driver.mjs';
import {getSystemsView} from '../../dist/packages/systems/service.js';

test('battle history and exact clamped rewards match TypeScript/native/Wasm and save replay',{timeout:90000},async()=>{
  const f=await fixture(p=>{p.systems.battleHistoryLimit=2;p.gameplay.items.find(i=>i.id==='gem').initialQuantity=2;
    p.systems.enemies[0].rewards.effects=[{op:'add',target:{kind:'attribute',attributeId:'money'},value:{op:'literal',value:1000000}},
      {op:'add',target:{kind:'relationship',characterId:'mentor',relationshipId:'trust'},value:{op:'literal',value:1000}}];});
  assert.equal(f.built.ok,true,JSON.stringify(f.built.diagnostics));const c=f.built.snapshot,d=await driver(c,true);await start(d);await d.send({type:'moveExpedition',nodeId:'clearing'});
  assert.deepEqual(d.state.systems.battle.log,[]);const initial=structuredClone(d.state);
  const view=getSystemsView(c,d.state);assert.deepEqual(view.battleActions.map(x=>x.id),['attack','guard','flee','use-item--potion']);
  assert.equal(view.battleActions[3].enabled,false);assert.match(view.battleActions[3].reason,/가득/);
  await d.send({type:'battleAction',action:'useItem',itemId:'potion'},'HP_FULL');assert.deepEqual(d.state,initial);
  await d.send({type:'battleAction',action:'guard'});await d.send({type:'battleAction',action:'useItem',itemId:'potion'});
  assert.equal(d.state.systems.battle.log[1].action,'useItem');assert.ok(d.state.systems.battle.log[1].heroHeal>0);
  const beforeRewards={money:d.state.attributes.money,trust:d.state.relationships.mentor.trust};await win(d);
  const result=d.state.systems.lastBattle;assert.equal(result.log.length,2);assert.equal(result.log.at(-1).turn,result.turn);
  assert.ok(result.log[0].turn>1);assert.deepEqual(result.discardedItems,[{itemId:'gem',quantity:1}]);
  const item=result.rewardChanges.find(x=>x.kind==='numeric'&&x.target.kind==='inventory');assert.equal(item.delta,0);assert.equal(item.clamped,'max');
  const money=result.rewardChanges.find(x=>x.kind==='numeric'&&x.target.kind==='attribute');assert.equal(money.delta,d.state.attributes.money-beforeRewards.money);assert.equal(money.clamped,'max');
  const relation=result.rewardChanges.find(x=>x.kind==='numeric'&&x.target.kind==='relationship');assert.equal(relation.delta,d.state.relationships.mentor.trust-beforeRewards.trust);
  const after=structuredClone(d.state);getSystemsView(c,d.state);assert.deepEqual(d.state,after);
  await d.send({type:'moveExpedition',nodeId:'gate'});await d.send({type:'moveExpedition',nodeId:'clearing'});
  assert.equal(d.state.phase,'expedition');assert.deepEqual(d.state.systems.lastBattle.rewardChanges,result.rewardChanges);
  assert.equal(d.state.attributes.money,after.attributes.money);assert.equal(d.state.gameplay.inventory.gem,2);
});

test('defeat and flee retain committed bounded logs without victory rewards',{timeout:90000},async()=>{
  for(const action of ['flee','guard']){
    const f=await fixture(p=>{p.systems.battleHistoryLimit=1;if(action==='guard')p.systems.enemies[0].attack=1000;});
    assert.equal(f.built.ok,true);const d=await driver(f.built.snapshot,true);await start(d);await d.send({type:'moveExpedition',nodeId:'clearing'});await d.send({type:'battleAction',action});
    assert.equal(d.state.systems.lastBattle.log.length,1);assert.equal(d.state.systems.lastBattle.log[0].action,action);
    assert.deepEqual(d.state.systems.lastBattle.rewardChanges,[]);assert.equal(d.state.gameplay.inventory.gem,0);
  }
});

test('legacy projects create no history or reward fields and preserve old replay shape',async()=>{
  const f=await fixture();assert.equal(f.built.ok,true);const d=await driver(f.built.snapshot);await start(d);await d.send({type:'moveExpedition',nodeId:'clearing'});
  assert.equal(Object.hasOwn(d.state.systems.battle,'log'),false);await win(d);
  assert.equal(Object.hasOwn(d.state.systems.lastBattle,'log'),false);assert.equal(Object.hasOwn(d.state.systems.lastBattle,'rewardChanges'),false);
});
