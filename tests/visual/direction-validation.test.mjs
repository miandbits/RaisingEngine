import test from 'node:test';import assert from 'node:assert/strict';
import {loadProject} from '../../dist/packages/project-service/loader.js';import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
const loaded=await loadProject('projects/party-tactics-lab');
const definition={actors:[{id:'hero',name:'Hero',assetId:'visual-haru',expressions:[{id:'happy',assetId:'visual-haru'}]}],sequences:[{id:'hello',name:'Hello',stages:[{name:'Enter',durationMs:1000,cues:[{actorId:'hero',x:0,y:0,width:100,height:200}]}]}]};
async function build(change){const copy=structuredClone(loaded),p=copy.documents.find(d=>d.kind==='project').data;p.visual.direction=structuredClone(definition);change?.(p);return buildSnapshot(copy);}
test('direction uses the common schema and semantic diagnostics',async()=>{
 assert.equal((await build()).ok,true);
 for(const change of [p=>p.visual.direction.actors.push(structuredClone(p.visual.direction.actors[0])),p=>p.visual.direction.actors[0].assetId='missing',p=>p.visual.direction.sequences[0].stages[0].cues[0].actorId='missing',p=>p.visual.direction.sequences[0].stages[0].cues[0].expressionId='missing',p=>p.visual.direction.sequences[0].stages[0].cues.push(structuredClone(p.visual.direction.sequences[0].stages[0].cues[0])),p=>p.visual.direction.sequences[0].stages[0].durationMs=0]){const result=await build(change);assert.equal(result.ok,false);assert.ok(result.diagnostics.length);}
});
test('screen and component sequence references cannot silently become broken links',async()=>{
 const r=await build(p=>p.visual.screens[0].elements.push({id:'bad-sequence',kind:'button',x:0,y:0,width:100,height:40,action:'playSequence',sequenceId:'missing'}));assert.equal(r.ok,false);assert.ok(r.diagnostics.some(d=>d.code==='DIRECTION_REFERENCE'));
});
