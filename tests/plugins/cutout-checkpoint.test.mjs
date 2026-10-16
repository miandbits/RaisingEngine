import test from 'node:test';
import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';

test('Wasm exact checkpoint resumes loop and physics pose in a fresh module',async()=>{
  const fixture=spawnSync(resolve('dist/cutout/build-win/cutout-core-tests.exe'),['--physics-parity'],{encoding:'utf8',maxBuffer:64*1024*1024});
  assert.equal(fixture.status,0,fixture.stderr);
  const {script}=JSON.parse(fixture.stdout);
  const setup=script.slice(0,script.findIndex(op=>op.op==='update'));
  const factory=(await import(pathToFileURL(resolve('dist/cutout/build-wasm/cutout-runtime.mjs')).href)).default;
  async function runtime(){
    const module=await factory(),raw=module.cwrap('raising_extension_request','string',['string']);
    const call=op=>{const r=JSON.parse(raw(JSON.stringify(op)));assert.equal(r.ok,true,JSON.stringify(r));return r;};
    setup.forEach(call);return call;
  }
  const a=await runtime();
  for(let i=0;i<37;i++)a({op:'update',dtMs:17});
  const saved=JSON.parse(JSON.stringify(a({op:'saveAll',exact:true}).state));
  assert.ok(Object.keys(saved.instances).length>0);
  assert.ok(Object.values(saved.instances).every(i=>i.state.version===2));
  assert.ok(Object.values(saved.instances).some(i=>i.state.tracks.some(t=>t.current?.timeMs>0)));
  const b=await runtime();b({op:'restoreAll',state:saved});
  assert.deepEqual(b({op:'saveAll',exact:true}).state,saved);
  for(const dt of [0,7,16,83,250,500]){
    if(dt){a({op:'update',dtMs:dt});b({op:'update',dtMs:dt});}
    for(const instance of Object.keys(saved.instances))
      assert.deepEqual(a({op:'frame',instance}),b({op:'frame',instance}),'rendered pose at delta '+dt);
    assert.deepEqual(a({op:'saveAll',exact:true}),b({op:'saveAll',exact:true}),'continued integrator at delta '+dt);
  }
  assert.ok(Object.values(a({op:'saveAll'}).state.instances).every(i=>i.state.version===1),'ordinary saves retain legacy policy');
});
