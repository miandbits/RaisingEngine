import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {phaseForecastFixture} from '../fixtures/phase-forecast-ui.mjs';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {desktopTarget,gameDirectory,playerExecutable} from '../../dist/packages/export-service/targets.js';
import {activityResultText} from '../../dist/packages/visual/activity-result.js';

test('native committed result summaries match Web copy for EN/KO/JA success, failure and no change',{timeout:180000},async()=>{
 await mkdir('.cache/activity-result-copy',{recursive:true});const out=await mkdtemp(resolve('.cache/activity-result-copy/native-')),fixture=await phaseForecastFixture(join(out,'project'),{opted:false}),content=structuredClone(fixture.content);
 delete content.project.gameplay;for(const screen of content.project.visual.screens)for(const element of screen.elements)if(element.kind==='result')element.color='#203a3b';const cases=[['study','success'],['rest','failure'],['costly','none']];
 for(const [id,kind] of cases){const a=content.activities.find(a=>a.id===id);a.label='Authored workshop';a.costs=[];a.successEffects=[];a.failureEffects=[];a.baseEffects=kind==='none'?[]:[{op:'add',target:{kind:'relationship',characterId:'mira',relationshipId:'affection'},value:{op:'literal',value:3}}];if(kind==='none'){delete a.successCheck;delete a.successEffects;delete a.failureEffects;}else a.successCheck={chance:{op:'literal',value:kind==='success'?10000:0}};}
 const target=desktopTarget(),runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app'),destination=join(out,'game');await exportGame({content,projectRoot:fixture.root,target,destination,runtimes:{windows:runtime,macos:runtime,web:runtime}});
 const receipts=[];
 for(const locale of ['en','ko','ja'])for(const [id,kind] of cases){const name=locale+'-'+kind,input=join(out,name+'.json'),report=join(out,name+'-report.json');await writeFile(input,JSON.stringify([{clickWindow:'start'},...[0,1,2,3].map(i=>({select:'plan-'+i,value:id})),{clickWindow:'advance'},{checkpoint:'confirmed'},{clickWindow:'advance'},{checkpoint:'result',...(kind==='success'?{screenshot:join(out,name+'.png')}:{})},{waitFrames:3},{checkpoint:'stable'}]));
 const run=spawnSync(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--save',join(out,name+'-save.json'),'--automation',input,'--report',report,'--report-checkpoints-only','--hidden','--locale',locale],{encoding:'utf8',timeout:45000});assert.equal(run.status,0,run.stderr||run.error?.message);const r=JSON.parse(await readFile(report,'utf8'));assert.equal(r.ok,true,r.error);
 const ui=r.steps.find(s=>s.command.checkpoint==='result').ui,stable=r.steps.find(s=>s.command.checkpoint==='stable').ui;assert.equal(ui.state.phase,'activity-result',ui.message);assert.equal(ui.state.lastActivityResult.success?.passed??null,kind==='none'?null:kind==='success');assert.deepEqual(ui.commands,[{type:'setSchedule',activityIds:[id,id,id,id]},{type:'executeActivity'}]);assert.deepEqual(stable.state,ui.state);assert.deepEqual(stable.commands,ui.commands);
 const expected=activityResultText(ui.state.lastActivityResult,'Authored workshop',()=> 'Mira / Affection',locale),text=ui.texts.result;for(const line of expected.split(/\n| · /))assert.ok(text.includes(line),`${name}: expected ${JSON.stringify(line)} in ${JSON.stringify(text)}`);if(locale==='en')assert.doesNotMatch(text,/[가-힣]/);receipts.push({locale,kind,text,expected});
 }
 await writeFile(join(out,'proof.json'),JSON.stringify({ok:true,cases:receipts},null,2));
});
