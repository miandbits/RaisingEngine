import test from 'node:test';
import assert from 'node:assert/strict';
import {galleryBranchRows} from '../../dist/packages/visual/gallery-branches.js';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {mkdtemp,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';

test('branch projection hides unvisited text, scopes identity, handles merged/nested choices without simulating',()=>{
  const content={project:{gallery:{entries:[{id:'memory',kind:'event',sourceId:'event'}]}},events:[{id:'event',entryNodeId:'first',nodes:[
    {id:'first',type:'choice',choices:[{id:'yes',text:'Help',next:'nested'},{id:'no',text:'SECRET OPTION',next:'end'}]},
    {id:'nested',type:'choice',choices:[{id:'yes',text:'SECRET NESTED',next:'end'}]},
    {id:'dead',type:'choice',choices:[{id:'yes',text:'UNREACHABLE',next:'end'}]},{id:'end',type:'end'},
  ]}],endings:[]};
  const record={kind:'event',sourceId:'event',frames:[{nodeId:'first',choiceId:'yes',text:'Will you help?'}],replayFrames:[{}]};
  const ui={selectedId:'memory',entries:{memory:{records:[record]}}},before=structuredClone({content,ui});
  const rows=galleryBranchRows(content,ui);assert.equal(rows.length,3);
  assert.equal(rows[0].values.unlocked,true);assert.equal(rows[0].values.canReplay,true);assert.equal(rows[0].values.recordIndex,0);
  assert.equal(rows[1].values.locked,true);assert.equal(rows[2].values.locked,true);
  assert.ok(!JSON.stringify(rows).includes('SECRET'));assert.ok(!JSON.stringify(rows).includes('UNREACHABLE'));
  assert.equal(rows[2].values.prompt,'아직 보지 못한 선택 지점');
  assert.deepEqual({content,ui},before);
  const wrong={...record,sourceId:'other'};assert.equal(galleryBranchRows(content,{...ui,entries:{memory:{records:[wrong]}}}).length,0);
  const second={...record,frames:[{nodeId:'first',choiceId:'no',text:'Will you help?'}]};
  const both=galleryBranchRows(content,{...ui,entries:{memory:{records:[record,second]}}});
  assert.equal(both[1].values.unlocked,true);assert.equal(both[1].values.recordIndex,1);
  const legacy=galleryBranchRows(content,{...ui,entries:{memory:{records:[{...record,replayFrames:[]}]}}});
  assert.equal(legacy[0].values.unlocked,true);assert.equal(legacy[0].values.canReplay,false);
});

test('public project remains valid with optional branch UI contracts',async()=>{
  const result=await buildSnapshot(await loadProject('examples/last-grandson-polished'));
  assert.equal(result.ok,true,JSON.stringify(result.diagnostics));
});

test('older renderers reject branch lists and component override actions before export',async()=>{
  const projectRoot=resolve('examples/last-grandson-polished');
  const built=await buildSnapshot(await loadProject(projectRoot));assert.equal(built.ok,true);
  const runtime=await mkdtemp(resolve('.cache/gallery-branches-old-runtime-'));
  await writeFile(resolve(runtime,'RaisingPlayer.exe'),'fixture, never executed');
  await writeFile(resolve(runtime,'capabilities.json'),JSON.stringify({formatVersion:'1',imageExtensions:[],audioExtensions:[],visualFeatures:[]}));
  for(const element of [{id:'test',kind:'collection',source:{type:'galleryBranches'}},{id:'test',kind:'component',overrides:[{elementId:'button',values:{action:'galleryBranch'}}]}]){
    const content=structuredClone(built.snapshot);content.project.visual.screens[0].elements.push(element);
    await assert.rejects(exportGame({content,projectRoot,destination:resolve(runtime,'rejected'),target:'windows',runtimes:{windows:runtime,web:runtime}}),/gallery-branches-v1/);
  }
});
