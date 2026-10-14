import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';
import {fixture} from '../gameplay/fixture.mjs';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/session.js';
import {encodeSave} from '../../dist/packages/runtime-service/save.js';

test('debug bridge runs validated unsaved draft through C++ and preserves editor/disk/history with stale guards',async t=>{
  const {root}=await fixture(t,p=>{p.initialState.attributes.money=100;p.gameplay.actions=[{id:'recover',name:'Recovery',costs:[{attributeId:'money',amount:4}],effects:[{op:'add',target:{kind:'attribute',attributeId:'stamina'},value:{op:'literal',value:5}}]}];});
  const service=new QtStudioService();await service.handle({method:'open',params:{root}});const editor=service.editor,doc=editor.documents.find(d=>d.kind==='project'),disk=await readFile(path.join(root,doc.file));
  const data=structuredClone(doc.data);data.gameplay.actions[0].costs[0].amount=9;editor.edit({type:'replace',file:doc.file,data},editor.revision);
  const before=editor.view();const started=await service.handle({method:'debugStart',params:{revision:before.revision,seed:7}});const debug=started.debug;assert.equal(debug.backend,'cpp');
  const params={revision:before.revision,sessionId:debug.sessionId};
  const inspected=await service.handle({method:'debugInspect',params:{...params,subject:{kind:'action',id:'recover'}}});assert.equal(inspected.debug.inspection.eligible,true);assert.equal(inspected.debug.inspection.preview.diff.find(d=>d.path==='/attributes/money').after,91);assert.deepEqual(inspected.debug.state,debug.state);
  const executed=await service.handle({method:'debugCommand',params:{...params,expectedStateRevision:debug.stateRevision,command:{type:'executeAction',actionId:'recover',quantity:1}}});assert.equal(executed.debug.state.attributes.money,91);
  const stale=await service.handle({method:'debugCommand',params:{...params,expectedStateRevision:debug.stateRevision,command:{type:'executeAction',actionId:'recover',quantity:1}}});assert.equal(stale.debug.ok,false);assert.deepEqual(stale.debug.state,executed.debug.state);
  const impact=await service.handle({method:'debugImpact',params:{revision:before.revision,subject:{kind:'action',id:'recover'}}});assert.ok(impact.debug.impact.edges.some(e=>e.to==='attribute:money'));
  const snapshot=await editor.snapshot(before.revision);assert.equal(snapshot.ok,true);const created=createSession(snapshot.snapshot,99);assert.equal(created.ok,true);let session=created.session;
  for(let i=0;i<2;i++){const result=dispatchSession(snapshot.snapshot,session,{type:'executeAction',actionId:'recover',quantity:1},session.state.stateRevision);assert.equal(result.ok,true);session=result.session;}
  const encoded=await encodeSave(snapshot.snapshot,session);assert.equal(encoded.ok,true);
  const imported=await service.handle({method:'debugLoad',params:{revision:before.revision,text:encoded.text}});assert.equal(imported.debug.ok,true);assert.deepEqual(imported.debug.state,session.state);assert.equal(imported.debug.seed,99);assert.notEqual(imported.debug.sessionId,debug.sessionId);
  const tampered=JSON.parse(encoded.text);tampered.state.attributes.money+=1;
  const rejected=await service.handle({method:'debugLoad',params:{revision:before.revision,text:JSON.stringify(tampered)}});assert.equal(rejected.debug.ok,false);assert.equal(rejected.debug.error.code,'SAVE_STATE_MISMATCH');assert.equal(rejected.debug.sessionId,imported.debug.sessionId);assert.deepEqual(rejected.debug.state,imported.debug.state);
  const malformed=await service.handle({method:'debugLoad',params:{revision:before.revision,text:'{"broken":'}});assert.equal(malformed.debug.ok,false);assert.deepEqual(malformed.debug.state,imported.debug.state);
  assert.deepEqual(editor.view(),before);assert.deepEqual(await readFile(path.join(root,doc.file)),disk);
  data.title+=' changed';editor.edit({type:'replace',file:doc.file,data},editor.revision);
  await assert.rejects(service.handle({method:'debugInspect',params}),/갱신/);
  await assert.rejects(service.handle({method:'debugCommand',params:{...params,revision:editor.revision,expectedStateRevision:executed.debug.stateRevision,command:{type:'executeAction',actionId:'recover',quantity:1}}}),/프로젝트가 변경/);
  const fresh=await service.handle({method:'debugStart',params:{revision:editor.revision,seed:7}});assert.notEqual(fresh.debug.sessionId,debug.sessionId);
  await writeFile(path.join(root,doc.file),JSON.stringify({...doc.data,title:'external'}));
  await assert.rejects(service.handle({method:'debugInspect',params:{revision:editor.revision,sessionId:fresh.debug.sessionId,subject:{kind:'action',id:'recover'}}}));
});
