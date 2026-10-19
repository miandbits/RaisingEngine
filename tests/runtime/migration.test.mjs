import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile, writeFile, readdir } from 'node:fs/promises';
import { join } from 'node:path';
import { content, start, send, doc, temporary } from './helpers.mjs';
import { encodeSave, decodeSave, loadRun } from '../../dist/packages/runtime-service/save.js';
import { previewSaveMigration, publishMigratedSave } from '../../dist/packages/runtime-service/migration.js';

test('content migration validates old save, reports changed state, reloads new save and preserves recovery source', async t => {
  const source = await content();
  let session = start(source);
  session = send(source, session, { type: 'setSchedule', activityIds: ['rest','rest','rest'] });
  session = send(source, session, { type: 'executeActivity' });
  const encoded = await encodeSave(source, session); assert.ok(encoded.ok);
  const target = await content(p => { doc(p,'attributes').attributes.find(a => a.id === 'money').initial += 10; });
  assert.equal((await decodeSave(target, encoded.text)).error.code, 'CONTENT_MISMATCH');
  const preview = await previewSaveMigration(source, target, encoded.text); assert.ok(preview.ok, JSON.stringify(preview));
  assert.ok(preview.report.changes.some(c => c.path === '/attributes/money' && c.after - c.before === 10));
  const root = await temporary(t), old = join(root,'old.json'), next = join(root,'new.json');
  await writeFile(old, encoded.text);
  const published = await publishMigratedSave(next, source, target, encoded.text, preview.report.approval); assert.ok(published.ok,JSON.stringify(published));
  const loaded = await loadRun(next,target); assert.ok(loaded.ok);
  assert.deepEqual(loaded.session.state.rng, session.state.rng);
  const continued = send(target, loaded.session, {type:'acknowledgeResult'});
  const roundtrip = await encodeSave(target, continued); assert.ok(roundtrip.ok);
  assert.deepEqual((await decodeSave(target, roundtrip.text)).session, continued);
  assert.equal(await readFile(old,'utf8'), encoded.text);
  assert.deepEqual((await loadRun(old,source)).session, session);
  assert.equal((await publishMigratedSave(old,source,target,encoded.text,preview.report.approval)).error.code,'MIGRATION_IO');
  assert.equal(await readFile(old,'utf8'), encoded.text);
  assert.equal((await publishMigratedSave(next,source,target,encoded.text,'stale')).error.code,'MIGRATION_STALE');
  assert.deepEqual((await readdir(root)).sort(), ['new.json','old.json']);
});

test('migration fails closed for tampering, deleted activity, different game, cancellation and changed approval', async () => {
  const source=await content(), session=send(source,start(source),{type:'setSchedule',activityIds:['rest','rest','rest']});
  const encoded=await encodeSave(source,session); assert.ok(encoded.ok);
  const corrupt=JSON.parse(encoded.text); corrupt.state.attributes.money++;
  assert.equal((await previewSaveMigration(source,source,JSON.stringify(corrupt))).error.code,'SAVE_STATE_MISMATCH');
  const removed=await content(p=>{p.documents=p.documents.filter(d=>!(d.kind==='activity'&&d.data.id==='rest'));});
  const failed=await previewSaveMigration(source,removed,encoded.text);
  assert.equal(failed.error.code,'MIGRATION_REPLAY'); assert.equal(failed.error.path,'/commands/0');
  const other=await content(p=>{doc(p,'project').gameId='different-game';});
  assert.equal((await previewSaveMigration(source,other,encoded.text)).error.code,'MIGRATION_GAME');
  assert.equal((await previewSaveMigration(source,source,encoded.text,{cancelled:()=>true})).error.code,'MIGRATION_CANCELLED');
  assert.equal((await previewSaveMigration(source,source,'not even parsed',{cancelled:()=>true})).error.code,'MIGRATION_CANCELLED','cancel before parsing or replaying a long journal');
});

test('presentation reset is explicit and reported, legacy saves without presentation still work', async () => {
  const source=await content(), target=await content(p=>{doc(p,'project').title+=' updated';});
  const encoded=await encodeSave(source,start(source),{version:1,timeline:null,extensions:{test:{phase:0.5}}}); assert.ok(encoded.ok);
  assert.equal((await previewSaveMigration(source,target,encoded.text)).error.code,'MIGRATION_PRESENTATION');
  const preview=await previewSaveMigration(source,target,encoded.text,{resetPresentation:true}); assert.ok(preview.ok);
  assert.equal(preview.report.presentationReset,true); assert.equal(JSON.parse(preview.text).playerPresentation,undefined);
  assert.ok((await decodeSave(target,preview.text)).ok);
});
