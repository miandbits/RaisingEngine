import test from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {createRun,dispatchCommand} from '../../dist/packages/core/run.js';
import {nativeCreateRun,nativeDispatchCommand,nativeReplay} from '../../dist/packages/native-bridge/index.js';
import {encodeSave,decodeSave} from '../../dist/packages/runtime-service/save.js';
import {fixture} from '../systems/fixture.mjs';

const base=await buildSnapshot(await loadProject(path.resolve('tests/fixtures/projects/free-expedition')));
assert.equal(base.ok,true,JSON.stringify(base.diagnostics));
function setup(locked=false){
 const c=structuredClone(base.snapshot),d=c.project.systems,g=c.project.gameplay;
 const trip=d.expeditions.find(e=>e.id===g.actions.find(a=>a.id==='depart-forest').expeditionId), enemy=d.enemies[0];
 Object.assign(enemy,{maxHp:18,attack:8,defense:1,attackPattern:[0,8,5],damageVariance:0,guardRoundDown:true,rewards:{items:[],effects:[]}});
 Object.assign(trip,{entryNodeId:'rock',nodes:[{id:'rock',name:'Rock',connections:['exit'],enemyId:enemy.id,manualBattle:true,battleRequirements:locked?{op:'not',condition:{op:'always'}}:{op:'always'}},{id:'exit',name:'Exit',connections:[],exit:true}]});
 const overrides={'day-stage':1,[d.hero.maxHpAttributeId]:32,[d.hero.attackAttributeId]:7,[d.hero.defenseAttributeId]:0};
 const origin={seed:42,mode:'full',initialOverrides:{attributes:overrides,flags:{},relationships:{}}},commands=[];
 let state=nativeCreateRun(c,42,origin).state;assert.deepEqual(state,createRun(c,42,origin).state);
 return {c,origin,commands,get state(){return state;},send(command,error){const before=structuredClone(state),n=nativeDispatchCommand(c,state,command,state.stateRevision),ts=dispatchCommand(c,state,command,state.stateRevision);assert.deepEqual(n,ts);if(error){assert.equal(n.ok,false,JSON.stringify(n));assert.equal(n.error.code,error);assert.deepEqual(n.state,before);}else{assert.equal(n.ok,true,JSON.stringify(n));state=n.state;commands.push(command);}return n;},async save(){const e=await encodeSave(c,{origin,state,commands});assert.equal(e.ok,true,JSON.stringify(e));const d=await decodeSave(c,e.text);assert.equal(d.ok,true,JSON.stringify(d));assert.deepEqual(d.session.state,state);assert.deepEqual(nativeReplay(c,origin,commands),{ok:true,state});}};
}
test('optional battle, deterministic telegraph pattern, guard rounding, victory and save replay',async()=>{
 const r=setup();r.send({type:'executeAction',actionId:'depart-forest',quantity:1});assert.equal(r.state.phase,'expedition');await r.save();
 r.send({type:'startNodeBattle'});assert.equal(r.state.phase,'battle');const rng=structuredClone(r.state.rng);
 r.send({type:'battleAction',action:'attack'});assert.equal(r.state.systems.battle.enemyHp,12);assert.equal(r.state.systems.expedition.heroHp,32);await r.save();
 r.send({type:'battleAction',action:'guard'});assert.equal(r.state.systems.expedition.heroHp,28);
 r.send({type:'battleAction',action:'guard'});assert.equal(r.state.systems.expedition.heroHp,26,'odd damage rounds down when configured');
 r.send({type:'battleAction',action:'attack'});r.send({type:'battleAction',action:'attack'});assert.equal(r.state.phase,'expedition');assert.equal(r.state.systems.lastBattle.outcome,'victory');assert.equal(r.state.systems.expedition.heroHp,26,'killing blow has no reply');assert.deepEqual(r.state.rng,rng);await r.save();
 r.send({type:'startNodeBattle'},'NO_MANUAL_BATTLE');
 r.send({type:'moveExpedition',nodeId:'exit'});r.send({type:'returnFromExpedition'});await r.save();
});
test('battle condition prevents starting but permits leaving; manual node needs a single enemy',async()=>{
 const r=setup(true);r.send({type:'executeAction',actionId:'depart-forest',quantity:1});r.send({type:'startNodeBattle'},'BATTLE_LOCKED');r.send({type:'moveExpedition',nodeId:'exit'});r.send({type:'returnFromExpedition'});await r.save();
 const f=await fixture(p=>{const n=p.systems.expeditions[0].nodes[0];n.manualBattle=true;delete n.enemyId;});assert.equal(f.built.ok,false);assert.ok(f.built.diagnostics.some(d=>d.code==='MANUAL_BATTLE_ENEMY'));
});
