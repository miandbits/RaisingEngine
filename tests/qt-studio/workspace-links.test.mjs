import test from 'node:test';
import assert from 'node:assert/strict';
import {authoringWorkspace} from '../../dist/packages/authoring-workspace/index.js';

test('selected UI traverses interaction, choice effects and qualified follow-up dependencies',()=>{
  const docs=[{file:'project.json',kind:'project',data:{gameplay:{interactions:[{id:'talk',eventId:'first',costs:[{attributeId:'gold',amount:2}]}]},visual:{screens:[{id:'home',elements:[{id:'talk',kind:'button',interactionId:'talk'},{id:'trust',kind:'meter',dataBinding:{kind:'relationship',characterId:'mentor',relationshipId:'trust'}}]}]}}},
    {file:'mentor.json',kind:'character',data:{id:'mentor',relationships:[{id:'trust',label:'Trust'}]}},
    {file:'first.json',kind:'event',data:{id:'first',label:'Meeting',nodes:[{id:'answer',type:'choice',prompt:'Answer?',choices:[{id:'yes',text:'Promise',next:'end',effects:[{op:'setFlag',flagId:'promise',value:true}]}]},{id:'end',type:'end'}]}},
    {file:'second.json',kind:'event',data:{id:'second',label:'Meeting',requirements:{op:'all',conditions:[{op:'eventCompleted',eventId:'first',owner:{kind:'actor'}},{op:'flag',flagId:'promise',value:true}]},nodes:[{id:'answer',type:'dialogue',text:'Again',next:'end'},{id:'end',type:'end'}]}},
    {file:'flags.json',kind:'flags',data:{flags:[{id:'promise'}]}}];
  const graph=authoringWorkspace(docs),has=(from,to,relation)=>graph.edges.some(e=>e.from===from&&e.to===to&&(!relation||e.relation===relation));
  assert.ok(has('screenElement:home/talk','interaction:talk'));
  assert.ok(has('interaction:talk','event:first'));
  assert.ok(has('interaction:talk','attribute:gold','cost'));
  assert.ok(has('screenElement:home/trust','relationship:mentor/trust'));
  assert.ok(has('event:first','scriptNode:event/first/answer','script'));
  assert.ok(has('scriptNode:event/first/answer','scriptChoice:event/first/answer/yes','choice'));
  assert.ok(has('scriptChoice:event/first/answer/yes','flag:promise','effect'));
  assert.ok(has('scriptChoice:event/first/answer/yes','scriptNode:event/first/end','script'));
  assert.ok(!has('scriptChoice:event/first/answer/yes','scriptNode:event/second/end'));
  assert.ok(has('event:first','event:second','followup'));
  const follow=graph.edges.find(e=>e.relation==='followup');assert.equal(follow.file,'second.json');assert.match(follow.path,/requirements/);
  assert.equal(graph.staticOnly,true);
  assert.equal(graph.nodes.find(n=>n.key==='scriptChoice:event/first/answer/yes').path,'/nodes/0/choices/0');
});

test('invalid sparse drafts preserve exact edit offsets and ambiguous script targets stay unresolved',()=>{
  const graph=authoringWorkspace([{file:'e.json',kind:'event',data:{id:'e',nodes:[null,{id:'same',type:'choice',choices:[null,{id:'x',next:'same'}]},{id:'same',type:'end'}]}}]);
  assert.equal(graph.nodes.find(n=>n.kind==='scriptChoice').path,'/nodes/1/choices/1');
  assert.ok(graph.duplicates.includes('scriptNode:event/e/same'));
  assert.ok(graph.edges.filter(e=>e.to==='scriptNode:event/e/same').every(e=>!e.resolved));
});
