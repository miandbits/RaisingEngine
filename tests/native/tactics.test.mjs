import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {createRun,dispatchCommand} from '../../dist/packages/core/run.js';
import {nativeCreateRun,nativeDispatchCommand,nativeReplay} from '../../dist/packages/native-bridge/index.js';
import {encodeSave,decodeSave} from '../../dist/packages/runtime-service/save.js';
import {fixture} from '../systems/fixture.mjs';

const proof=JSON.parse(await readFile('.cache/tactics-proof.json','utf8'));
const built=await buildSnapshot(await loadProject(proof.project));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));const content=built.snapshot;
function session(attributes={}){
 const origin={seed:42,mode:'full',initialOverrides:{attributes,flags:{},relationships:{}}},a=createRun(content,42,origin),b=nativeCreateRun(content,42,origin);assert.deepEqual(b,a);assert.ok(a.ok);let state=a.state;const commands=[];
 const send=(cmd,ok=true)=>{const ts=dispatchCommand(content,state,cmd,state.stateRevision),cpp=nativeDispatchCommand(content,state,cmd,state.stateRevision);assert.equal(ts.ok,ok,JSON.stringify(ts));assert.equal(cpp.ok,ok,JSON.stringify(cpp));if(ok){assert.deepEqual(cpp,ts);state=cpp.state;commands.push(cmd);}else{assert.equal(cpp.error.code,ts.error.code);assert.deepEqual(cpp.state,state);}return cpp;};
 return {origin,commands,send,get state(){return state;},async save(){const r=await encodeSave(content,{origin,commands,state});assert.ok(r.ok,JSON.stringify(r));const loaded=await decodeSave(content,r.text);assert.ok(loaded.ok,JSON.stringify(loaded));assert.deepEqual(loaded.session.state,state);assert.deepEqual(nativeReplay(content,origin,commands),{ok:true,state});return r.text;}};
}
const depart=s=>s.send({type:'executeAction',actionId:'tactics-depart',quantity:1});
function choose(s){const a=s.state.systems.tactics.active,actor=a.units.find(u=>u.id===a.order[a.cursor]);for(const skillId of content.project.systems.tactics.units.find(u=>u.id===actor.unitId).skillIds)for(const target of a.units){const cmd={type:'tacticsAction',unitId:actor.id,skillId,targetId:target.id};if(dispatchCommand(content,s.state,cmd,s.state.stateRevision).ok)return cmd;}throw Error('No available action');}

test('GUI authored party combat: TS/C++ parity, cast save, waves, equipment, growth, quest and replay',{timeout:120000},async()=>{
 const s=session();s.send({type:'visit',locationId:'market'});s.send({type:'buy',shopId:'florist',itemId:'training-sword',quantity:1});s.send({type:'visit',locationId:'home'});s.send({type:'tacticsEquip',unitId:'vanguard',equipmentId:'wooden-sword'});s.send({type:'tacticsEquip',unitId:'mage',equipmentId:'wooden-sword'},false);
 s.send({type:'tacticsUnequip',unitId:'vanguard',slot:'weapon'});s.send({type:'tacticsEquip',unitId:'mage',equipmentId:'wooden-sword'});s.send({type:'tacticsUnequip',unitId:'mage',slot:'weapon'});s.send({type:'tacticsEquip',unitId:'vanguard',equipmentId:'wooden-sword'});
 s.send({type:'acceptQuest',questId:'first-expedition'});depart(s);s.send({type:'moveExpedition',nodeId:'sealed-path'},false);s.send({type:'moveExpedition',nodeId:'clearing'});
 assert.equal(s.state.systems.tactics.active.units.length,4);assert.equal(s.state.systems.tactics.active.units.find(u=>u.id==='vanguard').stats.attack,22);
 s.send({type:'tacticsAction',unitId:'vanguard',skillId:'strike',targetId:'wave-0-wolf'},false);
 s.send({type:'tacticsAction',unitId:'mage',skillId:'spark',targetId:'mage'},false);
 s.send({type:'tacticsAction',unitId:'mage',skillId:'spark',targetId:'wave-0-wolf'});
 assert.ok(s.state.systems.tactics.active.units.find(u=>u.id==='mage').casting);const mid=await s.save();const saved=structuredClone(s.state),remaining=[];
 s.send({type:'tacticsAction',unitId:'vanguard',skillId:'strike',targetId:'wave-0-hexer'},false);
 let poison=false,wave2=false;for(let i=0;s.state.systems.tactics.active&&i<80;i++){const cmd=choose(s);remaining.push(cmd);s.send(cmd);const a=s.state.systems.tactics.active;poison||=a?.units.some(u=>u.statuses.some(x=>x.id==='poison'))??false;wave2||=a?.wave===1;}
 assert.equal(s.state.systems.tactics.active,null);assert.equal(s.state.systems.tactics.last.outcome,'victory');assert.ok(poison);assert.ok(wave2);assert.equal(s.state.systems.tactics.growth.mage.level,2);assert.equal(s.state.systems.tactics.growth.vanguard.experience,12);assert.equal(s.state.systems.quests['first-expedition'].status,'completed');assert.equal(s.state.gameplay.inventory.flower,1);
 let resumed=saved;for(const cmd of remaining){const r=nativeDispatchCommand(content,resumed,cmd,resumed.stateRevision);assert.ok(r.ok);resumed=r.state;}assert.deepEqual(resumed,s.state);
 const badVersion=nativeDispatchCommand(content,s.state,{type:'returnFromExpedition'},s.state.stateRevision-1);assert.equal(badVersion.ok,false);assert.deepEqual(badVersion.state,s.state);
 const bad=JSON.parse(mid);bad.state.systems.tactics.active.units[0].hp+=1;assert.equal((await decodeSave(content,JSON.stringify(bad))).ok,false);
 s.send({type:'moveExpedition',nodeId:'entrance'});s.send({type:'returnFromExpedition'});s.send({type:'claimQuest',questId:'first-expedition'});await s.save();
 await mkdir('docs/evidence/tactics',{recursive:true});await writeFile('docs/evidence/tactics/core-verification.json',JSON.stringify({ok:true,commands:s.commands,castSave:true,parity:true,poison,wave2,final:s.state},null,2));
});

test('defeat and malformed definitions',{timeout:120000},async()=>{
 const f=await fixture(p=>{
  p.systems.tactics=structuredClone(content.project.systems.tactics);const d=p.systems.tactics;
  for(const u of d.units){delete u.characterId;delete u.statAttributes;if(u.control==='player'){u.stats.maxHp=1;u.stats.attack=0;u.skillIds=['wait'];}else{u.stats.maxHp=999;u.stats.attack=100;}}
  d.skills.push({id:'wait',name:'Wait',target:'self',kind:'status',power:0,accuracy:100,castTurns:0,cooldown:0,statusId:'shield'});
  const node=p.systems.expeditions[0].nodes.find(n=>n.id==='clearing');delete node.enemyId;node.encounterId='forest-party';
  p.gameplay.actions=[{id:'depart',name:'Depart',requirements:{op:'always'},costs:[],effects:[],expeditionId:'forest'}];
 });assert.equal(f.built.ok,true,JSON.stringify(f.built.diagnostics));const c=f.built.snapshot;let state=createRun(c,42,{mode:'full'}).state;
 for(const cmd of [{type:'executeAction',actionId:'depart',quantity:1},{type:'moveExpedition',nodeId:'clearing'},{type:'tacticsAction',unitId:'mage',skillId:'wait',targetId:'mage'},{type:'tacticsAction',unitId:'vanguard',skillId:'wait',targetId:'vanguard'}]){const ts=dispatchCommand(c,state,cmd,state.stateRevision),cpp=nativeDispatchCommand(c,state,cmd,state.stateRevision);assert.equal(ts.ok,true,JSON.stringify(ts));assert.deepEqual(cpp,ts);state=cpp.state;}
 assert.equal(state.systems.tactics.last.outcome,'defeat');assert.equal(state.systems.expedition,null);assert.deepEqual(state.systems.tactics.growth,{});assert.equal(state.systems.tactics.last.experience,0);
 const altered=await loadProject(proof.project);const doc=altered.documents.find(d=>d.kind==='project');doc.data.systems.tactics.units[0].skillIds=['absent'];doc.data.systems.tactics.encounters[0].waves[0].unitIds=['vanguard','vanguard'];doc.data.systems.tactics.skills[0].statusId='absent';doc.data.systems.expeditions[0].nodes[0].encounterId='missing';const bad=await buildSnapshot(altered);assert.equal(bad.ok,false);for(const code of ['REFERENCE_MISSING','TACTICS_CONTROL','TACTICS_DUPLICATE'])assert.ok(bad.diagnostics.some(d=>d.code===code),code);assert.ok(bad.diagnostics.every(d=>d.pointer.startsWith('/systems/')));
});

test('attribute-driven HP, poison ticks, guard and ally healing remain deterministic',async()=>{
 const s=session({stamina:41});depart(s);s.send({type:'moveExpedition',nodeId:'clearing'});assert.equal(s.state.systems.tactics.active.units.find(u=>u.id==='mage').stats.maxHp,41);
 s.send({type:'tacticsAction',unitId:'mage',skillId:'venom',targetId:'wave-0-wolf'});s.send({type:'tacticsAction',unitId:'vanguard',skillId:'guard',targetId:'vanguard'});
 const hp=s.state.systems.tactics.active.units.find(u=>u.id==='vanguard').hp;assert.ok(hp<80);s.send({type:'tacticsAction',unitId:'mage',skillId:'mend',targetId:'vanguard'});const hero=s.state.systems.tactics.active.units.find(u=>u.id==='vanguard');assert.equal(hero.hp,Math.min(80,hp+15)-2);assert.ok(hero.statuses.some(x=>x.id==='shield'));assert.ok(hero.statuses.some(x=>x.id==='poison'));await s.save();
});

test('stun skips a turn without executing an attack; zero accuracy misses',async()=>{
 const loaded=await loadProject(proof.project),doc=loaded.documents.find(d=>d.kind==='project');doc.data.systems.tactics.statuses.find(s=>s.id==='poison').stun=true;doc.data.systems.tactics.skills.find(s=>s.id==='strike').accuracy=0;const result=await buildSnapshot(loaded);assert.ok(result.ok,JSON.stringify(result.diagnostics));const c=result.snapshot;let state=createRun(c,42,{mode:'full'}).state;
 for(const cmd of [{type:'executeAction',actionId:'tactics-depart',quantity:1},{type:'moveExpedition',nodeId:'clearing'},{type:'tacticsAction',unitId:'mage',skillId:'venom',targetId:'wave-0-wolf'},{type:'tacticsAction',unitId:'vanguard',skillId:'strike',targetId:'wave-0-wolf'}]){const a=dispatchCommand(c,state,cmd,state.stateRevision),b=nativeDispatchCommand(c,state,cmd,state.stateRevision);assert.ok(a.ok,JSON.stringify(a));assert.deepEqual(b,a);state=b.state;}
 const log=state.systems.tactics.active.log;assert.ok(log.some(x=>x.includes('wave-0-wolf stunned')));assert.ok(log.some(x=>x.includes('vanguard strike wave-0-wolf miss')));assert.ok(!log.some(x=>x.includes('wave-0-wolf bite')));
});

test('retreat cleans active encounter and grants no victory rewards; save replay stays compatible',async()=>{
 const s=session();depart(s);s.send({type:'moveExpedition',nodeId:'clearing'});s.send({type:'tacticsFlee'});assert.equal(s.state.phase,'planning');assert.equal(s.state.systems.tactics.active,null);assert.equal(s.state.systems.expedition,null);assert.equal(s.state.systems.tactics.last.outcome,'fled');assert.equal(s.state.gameplay.inventory.flower,0);assert.deepEqual(s.state.systems.tactics.growth,{});await s.save();
});
