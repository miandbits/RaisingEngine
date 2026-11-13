import assert from 'node:assert/strict';

const median=values=>values.sort((a,b)=>a-b)[Math.floor(values.length/2)];
export function analyzeInputIdleMemory(samples,start,end,{metric='footprintKiB'}={}) {
  assert.ok(['footprintKiB','rssKiB'].includes(metric),'Known memory metric required');
  assert.ok(Number.isFinite(start)&&Number.isFinite(end)&&end-start>=480000,'The native input must remain idle for eight actual minutes');
  const idle=samples.filter(row=>row.at>=start+15000&&row.at<=end&&Number.isFinite(row[metric])&&row[metric]>0);
  assert.ok(idle.length>=25,'At least 25 live app memory samples for '+metric+' are required');
  const rates=[];
  for(let i=1;i<idle.length;i++) {
    const previous=idle[i-1],current=idle[i],elapsed=current.at-previous.at;
    assert.ok(elapsed>0,'Memory sample timestamps must increase');
    rates.push((current[metric]-previous[metric])*60000/elapsed);
  }
  const early=median(idle.slice(0,5).map(x=>x[metric])),late=median(idle.slice(-5).map(x=>x[metric])),rate=median(rates);
  // Physical footprint includes compressed pages; RSS alone can conceal
  // accumulation under memory pressure. Keep net and interval diagnostics.
  return {metric,seconds:(end-start)/1000,samples:idle.length,firstMedianKiB:early,lastMedianKiB:late,growthKiB:late-early,medianGrowthKiBPerMinute:rate,peakKiB:Math.max(...idle.map(x=>x[metric])),growthLimitKiB:8192,rateLimitKiBPerMinute:1024,diagnosticPass:late<=early+8192&&rate<=1024,limit:'Finite simulator regression for retained per-frame temporaries; not a shipping-device memory budget or proof of no leaks.'};
}
