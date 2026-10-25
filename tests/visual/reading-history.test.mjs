import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,readdir} from 'node:fs/promises';
import Ajv2020 from 'ajv/dist/2020.js';
import {ReadingHistory,readingHistoryLimits as limits,readingHistoryPrefix} from '../../dist/packages/visual/reading-history.js';
import {requiredPlayerFeatures,missingPlayerFeatures} from '../../dist/packages/export-service/compatibility.js';
import {checkVisual} from '../../dist/packages/validation/visual.js';
import {SemanticContext} from '../../dist/packages/validation/context.js';
const line=(text,extra={})=>({epoch:'run-1',key:'event-a:node-1:actor-a:period-0',kind:'dialogue',text,locale:'en',speaker:'Mira',date:'Week 1',...extra});
test('repaints and growing revealed prefixes coalesce; smaller prefixes never erase shown text',()=>{
 const h=new ReadingHistory('run-1');assert.equal(h.record(line('A')),true);assert.equal(h.record(line('A')),false);assert.equal(h.record(line('A new horizon')),true);assert.equal(h.record(line('A new')),false);
 assert.equal(h.snapshot().entries.length,1);assert.equal(h.snapshot().entries[0].text,'A new horizon');assert.equal(h.snapshot().entries[0].sequence,1);
 assert.equal(h.record(line('Different shown words')),true);assert.equal(h.snapshot().entries.length,2);
});
test('locale, speaker, actor/node and accepted response snapshots remain distinct',()=>{
 const h=new ReadingHistory('run-1');h.record(line('Hello'));h.record(line('こんにちは',{locale:'ja'}));h.record(line('Hello',{speaker:'Sora'}));h.record(line('Hello',{key:'event-a:node-1:actor-b:period-0'}));
 h.record(line('Keep the promise',{kind:'chosen-response',key:'choice-1:keep'}));assert.equal(h.record(line('Keep the promise',{kind:'chosen-response',key:'choice-1:keep'})),false);
 h.record(line('A new line',{key:'node-2'}));h.record(line('Hello'));assert.equal(h.snapshot().entries.length,7);assert.deepEqual(h.snapshot().entries.slice(0,2).map(e=>e.text),['Hello','こんにちは']);
});
test('successful restore/new-run reset is explicit and rejects stale asynchronous receipts',()=>{
 const h=new ReadingHistory('run-1');h.record(line('Future branch'));h.reset('load-2','restored');assert.equal(h.record(line('Late callback')),false);assert.deepEqual(h.snapshot().entries,[]);assert.equal(h.snapshot().reason,'restored');
 h.record(line('Restored line',{epoch:'load-2'}));assert.equal(h.snapshot().entries[0].sequence,1);
 // A canceled or failed restore never calls reset, so the current history is retained.
 const before=h.snapshot();assert.deepEqual(h.snapshot(),before);h.reset('run-3','new-run');assert.equal(h.snapshot().utf8Bytes,0);assert.equal(h.snapshot().omittedEntries,0);
});
test('UTF-8 bounds preserve code points, mark truncation, and account for metadata',()=>{
 assert.deepEqual(readingHistoryPrefix('A🌅한字',8),{text:'A🌅한',bytes:8,truncated:true});assert.deepEqual(readingHistoryPrefix('A\ud800B',5),{text:'A�B',bytes:5,truncated:false});
 const h=new ReadingHistory('run-1');h.record(line('🌅'.repeat(10000),{speaker:'人'.repeat(1000),date:'D'.repeat(2000)}));const s=h.snapshot(),e=s.entries[0];assert.ok(e.truncated);assert.ok(s.utf8Bytes<=limits.entryUtf8Bytes);assert.ok(!e.text.includes('�'));assert.ok(Buffer.byteLength(e.speaker)<=limits.metadataUtf8Bytes);assert.equal(h.record(line('Invalid overlong identity',{key:'x'.repeat(1025)})),false);
});
test('entry and total byte caps evict oldest records, with stable sequence and an omission count',()=>{
 const short=new ReadingHistory('run-1');for(let i=0;i<205;i++)short.record(line('Line '+i,{key:'node-'+i}));const s=short.snapshot();assert.equal(s.entries.length,200);assert.equal(s.omittedEntries,5);assert.equal(s.entries[0].sequence,6);
 const large=new ReadingHistory('run-1');for(let i=0;i<50;i++)large.record(line('界'.repeat(10000),{key:'node-'+i}));const l=large.snapshot();assert.ok(l.utf8Bytes<=limits.totalUtf8Bytes);assert.ok(l.entries.length<50);assert.equal(l.omittedEntries+l.entries.length,50);assert.equal(l.entries.at(-1).sequence,50);
 const untouched=large.snapshot();l.entries[0].text='External mutation';l.entries.pop();assert.deepEqual(large.snapshot(),untouched);
});
test('empty or stale receipts have no effect and snapshots cannot mutate the host',()=>{
 const h=new ReadingHistory('run-1');for(const receipt of [line(''),line(' \n '),line('secret',{epoch:'old'}),line('missing identity',{key:''})])assert.equal(h.record(receipt),false);assert.deepEqual(h.snapshot().entries,[]);assert.equal(h.snapshot().utf8Bytes,0);
});
function fixture(){return {project:{gameId:'history-fixture',visual:{width:1280,height:720,routing:{defaultScreenId:'story',phases:[]},screens:[{id:'story',label:'Story',elements:[{id:'recent',kind:'button',x:0,y:0,width:200,height:60,text:'Recent reading',action:'readingHistory'}]}]}},activities:[],events:[],endings:[],characters:[],attributes:[],assets:[]};}
test('authored history action requires the opt-in; capability remains narrow and old projects require none',()=>{
 const content=fixture(),ctx=new SemanticContext(),doc={file:'project.json',kind:'project',data:content.project,version:'test',locations:new Map()};checkVisual(ctx,doc,content.project.visual);assert.ok(ctx.diagnostics.some(d=>d.code==='UI_READING_HISTORY_REQUIRED'&&d.severity==='error'));
 content.project.visual.readingHistory={};const valid=new SemanticContext();checkVisual(valid,doc,content.project.visual);assert.equal(valid.diagnostics.length,0);const required=requiredPlayerFeatures(content);assert.ok(required.includes('reading-history-v1'));assert.ok(missingPlayerFeatures(required,['readable-layout-v1','narrative-gallery-v1']).includes('reading-history-v1'));
 delete content.project.visual.readingHistory;content.project.visual.screens[0].elements=[];assert.ok(!requiredPlayerFeatures(content).includes('reading-history-v1'));
});

test('history is an explicit empty opt-in, never an implicit storage or budget setting',async()=>{
 const ajv=new Ajv2020({strict:true,allErrors:true});for(const file of (await readdir('packages/schema')).filter(f=>f.endsWith('.schema.json')))ajv.addSchema(JSON.parse(await readFile('packages/schema/'+file,'utf8')));
 const validate=ajv.getSchema('https://raising-engine.example/schemas/0.1.0/visual.schema.json'),visual=fixture().project.visual;
 visual.readingHistory={};const before=structuredClone(visual);assert.equal(validate(visual),true,ajv.errorsText(validate.errors));assert.deepEqual(visual,before);
 for(const invalid of [true,[],null,{persistent:true},{maxEntries:1000000}])assert.equal(validate({...visual,readingHistory:invalid}),false);
 delete visual.readingHistory;assert.equal(validate(visual),true,'absence is schema-compatible; semantic action dependency is checked separately');
});
