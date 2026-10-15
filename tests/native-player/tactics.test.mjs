import test from 'node:test';import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';import {resolve} from 'node:path';import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';import {exportGame} from '../../dist/packages/export-service/export.js';
test('Qt authored party expedition: real player skills, targets, cast save, waves, growth, quest and fresh load',{timeout:180000},async()=>{
 const proof=JSON.parse(await readFile('.cache/tactics-proof.json','utf8'));assert.ok(proof.guiEdited&&proof.saveReopen&&proof.undoRedo);const b=await buildSnapshot(await loadProject(proof.project));assert.equal(b.ok,true,JSON.stringify(b.diagnostics));
 const root=await mkdtemp(resolve('.cache/tactics-play-')),game=resolve(root,'game'),evidence=resolve('docs/evidence/tactics');await mkdir(evidence,{recursive:true});
 const runtimes={windows:resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-tactics'),web:resolve('dist/web')};await assert.rejects(exportGame({content:b.snapshot,projectRoot:proof.project,destination:resolve(root,'old'),target:'windows',runtimes:{...runtimes,windows:resolve('dist/native-player-ui5')}}),/party-tactics-v1/);await exportGame({content:b.snapshot,projectRoot:proof.project,destination:game,target:'windows',runtimes});
 const save=resolve(evidence,'player-save.json');async function play(steps,name){const script=resolve(evidence,name+'-steps.json'),report=resolve(evidence,name+'-report.json');await writeFile(script,JSON.stringify(steps,null,2));const r=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',save,'--automation',script,'--report',report,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:90000});const data=JSON.parse(await readFile(report,'utf8'));assert.equal(r.status,0,JSON.stringify(data.error??r.stderr.slice(-1200)));assert.equal(data.ok,true);return data;}
 const cast=[{click:'party-skill-spark'},{click:'party-target-wave-0-wolf'}];
 const strike=[{click:'party-skill-strike'},{click:'party-target-wave-0-wolf'}];
 const commands=JSON.parse(await readFile(resolve(evidence,'core-verification.json'),'utf8')).commands.filter(c=>c.type==='tacticsAction').slice(2);
 const rest=commands.flatMap(c=>[{click:'party-skill-'+c.skillId},{click:'party-target-'+c.targetId},{checkpoint:'after-'+c.skillId}]);
 const steps=[{click:'start'},{click:'visit-market'},{click:'buy-florist-training-sword'},{click:'confirm-purchase'},{click:'visit-home'},{click:'tactics-entry'},{checkpoint:'camp',screenshot:resolve(evidence,'camp.png')},
  {click:'party-equip-vanguard-wooden-sword'},{click:'accept-quest-first-expedition'},{click:'party-depart'},{click:'confirm-purchase'},{checkpoint:'branch',screenshot:resolve(evidence,'branch.png')},
  {click:'move-expedition-clearing'},{checkpoint:'battle',screenshot:resolve(evidence,'battle.png')},...cast,{checkpoint:'casting',screenshot:resolve(evidence,'casting.png')},{key:'F5'},{checkpoint:'saved'},...strike,{checkpoint:'wave-two',screenshot:resolve(evidence,'wave-two.png')},
  {key:'F9'},{checkpoint:'restored'},...strike,...rest,{checkpoint:'victory',screenshot:resolve(evidence,'victory.png')},{click:'move-expedition-entrance'},{click:'return-expedition'},{click:'claim-quest-first-expedition'},{checkpoint:'quest'},{key:'F5'},{checkpoint:'final-saved'}];
 const r=await play(steps,'play'),ui=id=>r.steps.find(s=>s.command.checkpoint===id).ui;
 assert.equal(ui('branch').elements.find(e=>e.id==='move-expedition-sealed-path').enabled,false);
 assert.equal(ui('battle').state.systems.tactics.active.units.length,4);assert.ok(ui('casting').state.systems.tactics.active.units.find(u=>u.id==='mage').casting);
 assert.deepEqual(ui('saved').state,ui('restored').state);assert.equal(ui('wave-two').state.systems.tactics.active.wave,1);
 assert.equal(ui('victory').state.systems.tactics.last.outcome,'victory');assert.equal(ui('victory').state.systems.tactics.growth.mage.level,2);assert.equal(ui('victory').state.systems.quests['first-expedition'].status,'completed');assert.equal(ui('quest').state.systems.quests['first-expedition'].status,'claimed');
 const loaded=await play([{key:'F9'},{checkpoint:'fresh'}],'fresh');assert.deepEqual(loaded.steps.find(s=>s.command.checkpoint==='fresh').ui.state,ui('final-saved').state);
 await writeFile(resolve(evidence,'verification.json'),JSON.stringify({ok:true,project:proof.project,game,guiAuthoring:true,actualPlayer:true,castSave:true,freshLoad:true},null,2));
});
