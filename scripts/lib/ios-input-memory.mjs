import assert from 'node:assert/strict';

const median=values=>values.sort((a,b)=>a-b)[Math.floor(values.length/2)];
export function analyzeInputIdleMemory(samples,start,end) {
  assert.ok(Number.isFinite(start)&&Number.isFinite(end)&&end-start>=480000,'The native input must remain idle for eight actual minutes');
  const idle=samples.filter(row=>row.at>=start+15000&&row.at<=end&&Number.isFinite(row.rssKiB)&&row.rssKiB>0);
  assert.ok(idle.length>=25,'At least 25 live app memory samples are required');
  const rates=[];
  for(let i=1;i<idle.length;i++) {
    const previous=idle[i-1],current=idle[i],elapsed=current.at-previous.at;
    assert.ok(elapsed>0,'Memory sample timestamps must increase');
    rates.push((current.rssKiB-previous.rssKiB)*60000/elapsed);
  }
  const early=median(idle.slice(0,5).map(x=>x.rssKiB)),late=median(idle.slice(-5).map(x=>x.rssKiB)),rate=median(rates);
  // Page reclamation can hide steady allocation in the overall RSS change.
  // Keep both measures and retain the raw series for interpretation.
  return {seconds:(end-start)/1000,samples:idle.length,firstMedianKiB:early,lastMedianKiB:late,growthKiB:late-early,medianGrowthKiBPerMinute:rate,peakRssKiB:Math.max(...idle.map(x=>x.rssKiB)),growthLimitKiB:8192,rateLimitKiBPerMinute:1024,diagnosticPass:late<=early+8192&&rate<=1024,limit:'Finite simulator regression for retained per-frame temporaries; not a shipping-device memory budget or proof of no leaks.'};
}
