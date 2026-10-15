import {compareGoalBaseline} from '../../dist/packages/analysis-service/goal-history.js';

// Portable approval contexts are usable only after reproducing the original
// immutable trace. Both CI and the editor graph use this exact gate.
export async function compareReviewedBaseline(content, baseline, runtime, route, timeoutMs = 60000, policy = 'strict') {
  const started = Date.now();
  let comparisonRuntime = runtime, runtimeVerification;
  if (policy === 'verify-replay' && runtime !== baseline.runtime) {
    const replay = await compareGoalBaseline(baseline.content, baseline, runtime, undefined, timeoutMs);
    const verified = replay.firstStateDifference === null && replay.firstCheckDifference === null && replay.firstCommandDifference === null && replay.right.reached === baseline.trace.reached && !replay.right.failure && !replay.right.unexecuted;
    runtimeVerification = {status: verified ? 'verified' : 'mismatch', baseline: baseline.runtime, current: runtime, firstStateDifference: replay.firstStateDifference, firstCheckDifference: replay.firstCheckDifference};
    if (!verified) return {runtimeVerification};
    comparisonRuntime = baseline.runtime;
  }
  const pair = await compareGoalBaseline(content, baseline, comparisonRuntime, route, Math.max(1, timeoutMs - (Date.now() - started)));
  return {pair, comparisonRuntime, runtimeVerification};
}
