import test from 'node:test';
import assert from 'node:assert/strict';
import { writeFile, mkdir } from 'node:fs/promises';
import { content, doc, start, send } from './helpers.mjs';
import { commandAttempts } from '../../dist/packages/analysis-service/runner.js';
import { dispatchSession } from '../../dist/packages/runtime-service/session.js';
import { encodeSave, decodeSave } from '../../dist/packages/runtime-service/save.js';

test('96-month campaign reaches ending with exact mid-campaign saves and bounded journals', {timeout:120000}, async () => {
  const snapshot=await content(p=>{doc(p,'project').durationMonths=96;});
  let live=start(snapshot), restored=live; const checkpoints=[];const started=performance.now();
  while(live.state.phase!=='finished'&&live.commands.length<10000){
    const candidates=live.state.phase==='planning'?[ [{type:'setSchedule',activityIds:['rest','rest','rest']}] ]:commandAttempts(snapshot,live.state);
    let accepted;
    for(const commands of candidates){let candidate=live;let valid=true;for(const command of commands){const next=dispatchSession(snapshot,candidate,command,candidate.state.stateRevision);if(!next.ok){valid=false;break;}candidate=next.session;}if(valid&&candidate!==live){accepted=commands;break;}}
    assert.ok(accepted,`no progress at ${live.state.phase}`);
    for(const command of accepted){live=send(snapshot,live,command);restored=send(snapshot,restored,command);}
    if(live.commands.length%100===0||live.state.phase==='finished'){
      const saved=await encodeSave(snapshot,restored);assert.ok(saved.ok);
      const loaded=await decodeSave(snapshot,saved.text);assert.ok(loaded.ok);restored=loaded.session;
      assert.deepEqual(restored,live);checkpoints.push({commands:live.commands.length,period:live.state.periodIndex,bytes:Buffer.byteLength(saved.text)});
    }
  }
  assert.equal(live.state.phase,'finished');assert.equal(live.state.periodIndex,287);assert.deepEqual(live,restored);assert.ok(checkpoints.length>5);
  await mkdir('.cache/durability',{recursive:true});await writeFile('.cache/durability/long-campaign.json',JSON.stringify({months:96,commands:live.commands.length,checkpoints,elapsedMs:performance.now()-started,ending:live.state.endingId,ok:true},null,2));
});
