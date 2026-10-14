import test from 'node:test';
import assert from 'node:assert/strict';
import { content as fixtureContent } from '../runtime/helpers.mjs';
import { createRun, dispatchCommand } from '../../dist/packages/core/run.js';
import { nativeRequest, closeNativeRuntimes } from '../../dist/packages/native-bridge/index.js';
import createModule from '../../dist/wasm/raising-wasm.mjs';
import { WasmRuntime } from '../../dist/packages/native-bridge/wasm.js';

test('damaged stored schedules return errors without mutating state or breaking subsequent commands', async () => {
  const content = await fixtureContent();
  const initial = createRun(content, 42, { mode: 'full' }).state;
  const ready = dispatchCommand(content, initial, { type: 'setSchedule', activityIds: ['rest','rest','rest'] }, initial.stateRevision).state;
  const expected = dispatchCommand(content, ready, { type: 'executeActivity' }, ready.stateRevision);
  const wasm = new WasmRuntime(await createModule());
  const previous = process.env.RAISING_NATIVE_TRANSPORT;
  try {
    for (const transport of ['process','addon','wasm']) {
      closeNativeRuntimes();
      process.env.RAISING_NATIVE_TRANSPORT = transport === 'wasm' ? 'addon' : transport;
      const request = payload => transport === 'wasm' ? wasm.request(payload) : nativeRequest(payload);
      for (const schedule of [null, [], ['rest'], ['rest','rest'], ['rest','rest','rest','rest'], [null,'rest','rest'], {}, 'rest']) {
        const state = { ...ready, schedule };
        const before = JSON.stringify(state);
        const result = request({ operation:'command', content, state, command:{type:'executeActivity'}, expectedRevision:state.stateRevision });
        assert.equal(result.ok, false, `${transport}: ${JSON.stringify(schedule)}`);
        assert.equal(result.error.code, schedule === null ? 'WRONG_PHASE' : 'INCOMPATIBLE_STATE');
        assert.deepEqual(result.state, state);
        assert.equal(JSON.stringify(state), before);
        assert.deepEqual(request({ operation:'command', content, state:ready, command:{type:'executeActivity'}, expectedRevision:ready.stateRevision }), expected);
      }
      for (const periodIndex of [1,2]) {
        const state = { ...ready, periodIndex, schedule:[] };
        const result = request({ operation:'command', content, state, command:{type:'setSchedule',activityIds:['rest','rest','rest']}, expectedRevision:state.stateRevision });
        assert.equal(result.ok, false);
        assert.equal(result.error.code, 'COMPLETED_SLOT');
        assert.deepEqual(result.state, state);
      }
    }
  } finally {
    closeNativeRuntimes();
    if (previous === undefined) delete process.env.RAISING_NATIVE_TRANSPORT;
    else process.env.RAISING_NATIVE_TRANSPORT = previous;
  }
});
