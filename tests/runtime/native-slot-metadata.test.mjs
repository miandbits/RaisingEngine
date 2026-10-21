import test from 'node:test';
import assert from 'node:assert/strict';
import {content,start} from './helpers.mjs';
import {encodeSave,decodeSave} from '../../dist/packages/runtime-service/save.js';
import {previewSaveMigration} from '../../dist/packages/runtime-service/migration.js';

test('native slot metadata and schedule drafts survive strict decode and migration',async()=>{
  const source=await content(),session=start(source,{mode:'full'});
  const draft={month:0,slot:0,selected:1,activities:Array(source.project.periodsPerMonth).fill('')};draft.activities[0]=source.activities[0].id;
  const record={version:1,date:'1년 7월 · 1주',place:'별채',screenId:'home',phase:session.state.phase,thumbnail:'frame-123456-1.png',savedAt:'2026-10-21 01:00:00',sequence:1,automatic:false};
  const presentation={version:1,timeline:null,scheduleDraft:draft,saveRecord:record};
  const encoded=await encodeSave(source,session,presentation);assert.equal(encoded.ok,true,JSON.stringify(encoded));
  const decoded=await decodeSave(source,encoded.text);assert.equal(decoded.ok,true,JSON.stringify(decoded));assert.deepEqual(decoded.playerPresentation,presentation);assert.deepEqual(decoded.session.state,session.state);
  const target=await content(p=>{p.documents.find(d=>d.kind==='project').data.title+=' revised';});
  const migration=await previewSaveMigration(source,target,encoded.text,{resetPresentation:true});assert.equal(migration.ok,true,JSON.stringify(migration));
  const replayed=await decodeSave(target,migration.text);assert.equal(replayed.ok,true);assert.deepEqual(replayed.session.state,{...session.state,contentFingerprint:target.contentFingerprint});
  const retained=await encodeSave(target,replayed.session,presentation);assert.equal(retained.ok,true);assert.deepEqual((await decodeSave(target,retained.text)).playerPresentation,presentation);
  for(const invalid of [{...record,thumbnail:'../private.png'},{...record,sequence:-1},{...record,date:'x'.repeat(512)},{...record,extra:true}]){
    const result=await encodeSave(source,session,{...presentation,saveRecord:invalid});assert.equal(result.ok,false,'invalid slot metadata must fail');
  }
  for(const invalid of [{...draft,month:1},{...draft,slot:source.project.periodsPerMonth},{...draft,activities:draft.activities.map(()=> 'missing-activity')},{...draft,activities:draft.activities.map(()=> '')}]){
    const result=await encodeSave(source,session,{...presentation,scheduleDraft:invalid});assert.equal(result.ok,false,'invalid draft must fail');
  }
});
