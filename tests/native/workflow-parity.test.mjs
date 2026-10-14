import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { StudioEditor } from '../../dist/packages/studio-service/editor.js';
import { McpWorkspace } from '../../dist/packages/mcp-service/workspace.js';
import { PlayerController } from '../../dist/packages/player-service/controller.js';
import { encodeSave, decodeSave } from '../../dist/packages/runtime-service/save.js';
import { nativeRequest, nativeTransport, closeNativeRuntimes } from '../../dist/packages/native-bridge/index.js';
import { freeze } from '../../dist/packages/core/util.js';
import { fixture } from '../gameplay/fixture.mjs';

const version = c => ({ generation:c.generation, revision:c.session.state.stateRevision });
const backends = ['typescript','process','addon'];
function backend(value) {
  closeNativeRuntimes();
  process.env.RAISING_RUNTIME = value === 'typescript' ? value : 'native';
  process.env.RAISING_NATIVE_TRANSPORT = value === 'typescript' ? 'auto' : value;
  if (value !== 'typescript') assert.equal(nativeTransport().transport,value);
}
function cleanup(t) {
  const original = [process.env.RAISING_RUNTIME,process.env.RAISING_NATIVE_TRANSPORT];
  t.after(() => { closeNativeRuntimes(); for(const [i,name] of ['RAISING_RUNTIME','RAISING_NATIVE_TRANSPORT'].entries()) {
    if(original[i]===undefined) delete process.env[name]; else process.env[name]=original[i];
  } });
}
function preview(content,root) {
  // Same handoff as Studio's serialized studio:preview IPC handler.
  const player=new PlayerController(); player.content=content; player.root=root; player.generation++;
  assert.equal(player.newRun(null).error,null); return player;
}

test('editor draft -> player gift -> active-event save/load preserves exact journal and conflict behavior across all transports', async t => {
  cleanup(t); const {root}=await fixture(t), editor=new StudioEditor(); await editor.open(root);
  const event=structuredClone(editor.documents.find(d=>d.file==='events/gift-friendship.json').data);
  event.nodes[0].text='편집한 관계 사건 🌷 선물 고마워요';
  editor.edit({type:'batch',edits:[{type:'replace',file:'events/gift-friendship.json',data:event}]},editor.revision);
  const built=await editor.snapshot(editor.revision); assert.equal(built.ok,true);
  let expected;
  for(const transport of backends) {
    backend(transport); const player=preview(built.snapshot,root), replies=[];
    for(const action of [{type:'visit',locationId:'city'},{type:'visit',locationId:'market'},
      {type:'buy',shopId:'florist',itemId:'flower',quantity:1},{type:'visit',locationId:'city'},
      {type:'interact',interactionId:'gift-mentor'}]) {
      const token=version(player), reply=player.act(action,token); assert.equal(reply.error,null);
      const current=player.session;
      const rejected=player.act(action,token); assert.ok(rejected.error); assert.equal(player.session,current);
      replies.push({state:reply.view.state,narrative:reply.view.narrative,gameplay:reply.view.gameplay,error:rejected.error});
    }
    assert.equal(player.view().narrative.node.text,event.nodes[0].text);
    const checkpoint=player.session, token=version(player), file=resolve(root,`save-${transport}.json`);
    assert.equal((await player.save(file,token)).error,null);
    assert.equal(player.act({type:'continue'},version(player)).error,null);
    assert.equal(player.session.state.phase,'planning');
    assert.equal((await player.load(file,version(player))).error,null);
    assert.deepEqual(player.session,checkpoint);
    assert.ok(player.act({type:'continue'},token).error,'loading invalidates old generation tokens');
    assert.equal(player.act({type:'continue'},version(player)).error,null);
    const encoded=await encodeSave(built.snapshot,player.session); assert.equal(encoded.ok,true);
    const roundtrip=await decodeSave(built.snapshot,encoded.text); assert.equal(roundtrip.ok,true);
    assert.deepEqual(roundtrip.session,player.session);
    const actual={replies,session:player.session,encoded:encoded.text};
    if(expected) assert.deepEqual(actual,expected); else expected=actual;
  }
});

test('MCP edits cannot change an existing native preview or overwrite a human draft, and changed-content saves remain rejected', async t => {
  cleanup(t); backend('addon'); const {root}=await fixture(t), editor=new StudioEditor(); await editor.open(root);
  const built=await editor.snapshot(editor.revision); assert.equal(built.ok,true);
  const player=preview(built.snapshot,root), mcp=new McpWorkspace(root), token=await mcp.readProject();
  const doc=editor.documents.find(d=>d.file==='events/gift-friendship.json');
  const human=structuredClone(doc.data); human.label='저장하지 않은 인간 초안';
  editor.edit({type:'replace',file:doc.file,data:human},editor.revision);
  const draft=editor.view();
  const remote=structuredClone(doc.data); remote.nodes[0].text='LLM이 저장한 새 대사';
  const updated=await mcp.apply(token.version,[{type:'replace',file:doc.file,data:remote}],'test shared contract');
  await assert.rejects(()=>mcp.apply(token.version,[{type:'replace',file:doc.file,data:human}],'stale'),e=>e.code==='VERSION_CONFLICT');
  await assert.rejects(()=>editor.save(editor.revision));
  assert.deepEqual(editor.view().documents,draft.documents);
  assert.equal(player.content.events.find(e=>e.id==='gift-friendship').nodes[0].text,built.snapshot.events.find(e=>e.id==='gift-friendship').nodes[0].text);
  const save=await encodeSave(player.content,player.session); assert.equal(save.ok,true);
  const fresh=new StudioEditor(); await fresh.open(root); const changed=await fresh.snapshot(fresh.revision); assert.equal(changed.ok,true);
  let expected;
  for(const transport of backends) {
    backend(transport);
    assert.equal((await decodeSave(built.snapshot,save.text)).ok,true);
    const mismatch=await decodeSave(changed.snapshot,save.text);
    assert.equal(mismatch.ok,false); assert.equal(mismatch.error.code,'CONTENT_MISMATCH');
    const scenario=await mcp.runScenario(updated.version,'scholar-year'); assert.equal(scenario.ok,true);
    if(expected) assert.deepEqual(scenario,expected); else expected=scenario;
  }
});

test('repeated editor snapshot revisions evict native cache registrations and close releases the survivors', async t => {
  cleanup(t); backend('addon'); const {built}=await fixture(t); assert.equal(built.ok,true);
  for(let revision=1;revision<=40;revision++) {
    const data=structuredClone(built.snapshot); data.attributes.find(a=>a.id==='money').initial=revision;
    const content=freeze(data);
    const result=nativeRequest({operation:'create',content,seed:1,options:{mode:'full'}});
    assert.equal(result.ok,true); assert.equal(result.state.attributes.money,revision);
  }
  const cached=id=>nativeRequest({operation:'create',contentId:`content-${id}`,seed:1,options:{mode:'full'}});
  for(let id=1;id<=24;id++) assert.equal(cached(id).error.code,'UNKNOWN_CONTENT');
  for(let id=25;id<=40;id++) assert.equal(cached(id).state.attributes.money,id);
  closeNativeRuntimes(); assert.equal(cached(40).error.code,'UNKNOWN_CONTENT');
});

test('pre-migration save passes through current player normalization and resave identically in every backend', async t => {
  cleanup(t); const text=await readFile(new URL('../journey/fixtures/pre-migration-save.json',import.meta.url),'utf8');
  let expected;
  for(const transport of backends) {
    backend(transport); const player=new PlayerController(); assert.equal((await player.openProject(resolve('examples/academy-demo'))).error,null);
    assert.equal((await player.load(resolve('tests/journey/fixtures/pre-migration-save.json'),null)).error,null);
    assert.deepEqual(player.session.state,JSON.parse(text).state);
    assert.equal(player.act({type:'continue'},version(player)).error,null);
    const encoded=await encodeSave(player.content,player.session); assert.equal(encoded.ok,true);
    const roundtrip=await decodeSave(player.content,encoded.text); assert.equal(roundtrip.ok,true);
    assert.deepEqual(roundtrip.session,player.session);
    const actual={session:player.session,text:encoded.text};
    if(expected) assert.deepEqual(actual,expected); else expected=actual;
  }
});

test('save schema/version/tampered-state and failed-journal diagnostics are transport independent', async t => {
  cleanup(t); const {built}=await fixture(t); assert.equal(built.ok,true);
  backend('typescript'); const player=preview(built.snapshot,'unused');
  const encoded=await encodeSave(built.snapshot,player.session); assert.equal(encoded.ok,true);
  const original=JSON.parse(encoded.text);
  const mutations=[
    save=>{save.saveVersion='future';}, save=>{save.coreVersion='future';},
    save=>{save.state.attributes.money++;},
    save=>{save.commands=[{type:'buy',shopId:'missing',itemId:'flower',quantity:1}];},
    save=>{save.commands=[{type:'visit',locationId:'city'},{type:'visit',locationId:'missing'}];},
  ];
  for(const mutate of mutations) {
    const save=structuredClone(original); mutate(save); let expected;
    for(const transport of backends) {
      backend(transport); const result=await decodeSave(built.snapshot,JSON.stringify(save));
      assert.equal(result.ok,false);
      if(expected) assert.deepEqual(result,expected); else expected=result;
    }
  }
});
