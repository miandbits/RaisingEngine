import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { runScenario } from '../../dist/packages/runtime-service/scenario.js';
import { createRun, dispatchCommand } from '../../dist/packages/core/run.js';
import { nativeRequest, nativeTransport, closeNativeRuntimes } from '../../dist/packages/native-bridge/index.js';

export async function probeAddon() {
  process.env.RAISING_NATIVE_TRANSPORT = 'addon';
  closeNativeRuntimes();
  assert.equal(nativeTransport().transport, 'addon', JSON.stringify(nativeTransport()));
  const result = { versions: process.versions, transport: nativeTransport(), routes: 0, commands: 0 };
  for (const [sample, scenario] of [['academy-demo','certified-route'],['academy-demo','ordinary-route'],
    ...['knight','scholar','artisan','neighbor','wanderer'].map(x => ['academy-year',`${x}-year`])]) {
    const root = `examples/${sample}`;
    const built = await buildSnapshot(await loadProject(root)); assert.equal(built.ok, true);
    const content = built.snapshot;
    const definition = JSON.parse(await readFile(`${root}/scenarios/${scenario}.json`, 'utf8'));
    const route = await runScenario(content, definition); assert.equal(route.ok, true);
    const origin = route.session.origin;
    let state = createRun(content, origin.seed, origin).state;
    assert.deepEqual(nativeRequest({ operation:'create', content, seed:origin.seed, options:origin }).state, state);
    for (const command of route.session.commands) {
      const expected = dispatchCommand(content, state, command, state.stateRevision);
      assert.deepEqual(nativeRequest({ operation:'command', content, state, command, expectedRevision:state.stateRevision }), expected);
      state = expected.state; result.commands++;
    }
    assert.deepEqual(nativeRequest({ operation:'replay', content, origin, commands:route.session.commands }).state, state);
    assert.deepEqual(state, route.session.state); result.routes++;
  }
  closeNativeRuntimes();
  return result;
}
