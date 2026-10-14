import { loadProject, buildSnapshot } from '../../../packages/project-service/index.js';
import { runScenario, encodeSave, decodeSave } from '../../../packages/runtime-service/index.js';
import type { ScenarioSpecification } from '../../../packages/schema/generated/scenario.js';

const args = process.argv.slice(2);
const positional = args.filter(a => !a.startsWith('--'));
const json = args.includes('--json');
const roundtrip = args.includes('--roundtrip');
if (positional.length < 1 || positional.length > 2 || args.some(a => a.startsWith('--') && a !== '--json' && a !== '--roundtrip')) {
  console.error('Usage: npm run scenarios -- <project> [scenarioId] [--json] [--roundtrip]');
  process.exitCode = 2;
} else {
  try {
    const loaded = await loadProject(positional[0]!);
    const built = await buildSnapshot(loaded);
    if (!built.ok) {
      console.log(JSON.stringify({ ok: false, diagnostics: built.diagnostics }, null, 2)); process.exitCode = 1;
    } else {
      const scenarios = loaded.documents.filter(d => d.kind === 'scenario').map(d => d.data as unknown as ScenarioSpecification)
        .filter(s => positional[1] === undefined || s.id === positional[1]);
      if (!scenarios.length) {
        console.log(JSON.stringify({ ok: false, error: { code: 'SCENARIO_NOT_FOUND', message: '실행할 시나리오가 없습니다.' } })); process.exitCode = 1;
      } else {
        const results = [];
        for (const scenario of scenarios) {
          const result = await runScenario(built.snapshot, scenario, roundtrip ? {
            checkpoint: async session => {
              const encoded = await encodeSave(built.snapshot, session);
              return encoded.ok ? decodeSave(built.snapshot, encoded.text) : encoded;
            },
          } : {});
          results.push({ scenarioId: result.scenarioId, ok: result.ok, steps: result.steps, assertions: result.assertions,
            state: result.session?.state ?? null, ...(!result.ok ? { error: result.error } : {}) });
        }
        const ok = results.every(r => r.ok);
        if (json) console.log(JSON.stringify({ reportVersion: '1.0.0', ok, roundtrip, results }, null, 2));
        else for (const result of results) console.log(`${result.ok ? 'PASS' : 'FAIL'} ${result.scenarioId}: ending=${result.state?.endingId ?? '-'}, steps=${result.steps}, assertions=${result.assertions.filter(a => a.passed).length}/${result.assertions.length}${'error' in result ? `, ${result.error?.code}: ${result.error?.message}` : ''}`);
        process.exitCode = ok ? 0 : 1;
      }
    }
  } catch (error) { console.error(error instanceof Error ? error.message : String(error)); process.exitCode = 1; }
}
