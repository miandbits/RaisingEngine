import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {analyzeInputIdleMemory} from '../../scripts/lib/ios-input-memory.mjs';

const fixtures=JSON.parse(readFileSync(new URL('./fixtures/ios-input-memory.json',import.meta.url),'utf8'));
for(const fixture of fixtures.cases) test('recorded native input memory: '+fixture.name,()=>{
  const report=analyzeInputIdleMemory(fixture.samples,fixture.start,fixture.end);
  assert.equal(report.diagnosticPass,fixture.expectedPass);
  if(fixture.name==='growth-with-page-reclamation') {
    assert.ok(report.growthKiB<0,'The observed net RSS fell despite accumulating temporaries');
    assert.ok(report.medianGrowthKiBPerMinute>report.rateLimitKiBPerMinute);
  }
});
test('incomplete or unordered observations cannot pass',()=>{
  const f=fixtures.cases[0];
  assert.throws(()=>analyzeInputIdleMemory(f.samples,0,60000),/eight actual minutes/);
  assert.throws(()=>analyzeInputIdleMemory(f.samples.slice(0,3),0,f.end),/25 live app/);
  assert.throws(()=>analyzeInputIdleMemory([...f.samples].reverse(),0,f.end),/timestamps must increase/);
});
