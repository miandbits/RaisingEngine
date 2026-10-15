import {goalChangeItems,reviewGoalItems} from './goal-partial.mjs';
import {canonicalJson} from '../../dist/packages/project-service/snapshot.js';

const key=i=>i.kind+':'+i.path;
function indexed(observations=[]){const counts=new Map();return new Map(observations.map(o=>{const n=counts.get(o.step)??0;counts.set(o.step,n+1);return [o.step+':'+n,o];}));}
export function goalReviewGraph(pair,accepted=[]){
  const raw=new Map(goalChangeItems(pair).map(i=>[key(i),i])),reviewed=reviewGoalItems(pair,accepted),steps={};
  for(const item of reviewed){
    const original=indexed(raw.get(key(item))?.observations),approval=accepted.find(a=>a.context===item.context&&key(a)===key(item));
    const expected=indexed(approval?.expectation??approval?.observations),last=indexed(approval?.observations),diffs=indexed(item.observations);
    for(const k of new Set([...original.keys(),...expected.keys(),...last.keys(),...diffs.keys()])){
      const a=original.get(k),b=expected.get(k),c=last.get(k),d=diffs.get(k),step=(a??b??c??d).step;
      const base=a?a.before:b?b.before:c?c.after:d.before,current=a?a.after:base,wanted=b?b.after:base;
      const pending=canonicalJson(wanted)!==canonicalJson(current),approved=!!approval&&!pending;
      if(!pending&&!approved)continue;
      const row={id:item.id,kind:item.kind,path:item.path,label:item.label,original:base,expected:wanted,current,status:pending?'regression':'approved',approvable:pending&&item.approvable,occurrences:item.observations.length};
      (steps[step]??=[]).push(row);
    }
  }
  return {steps,markers:Object.entries(steps).map(([step,rows])=>({step:Number(step),approved:rows.filter(r=>r.status==='approved').length,pending:rows.filter(r=>r.status==='regression').length}))};
}
