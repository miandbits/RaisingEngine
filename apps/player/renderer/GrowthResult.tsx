import React from 'react';
import {activityGrowthRows,growthOwnerId,growthWords} from '../../../packages/visual/growth-summary.js';
import {activityResultWords} from '../../../packages/visual/activity-result.js';
import type {VisualContext} from './VisualSurface.js';
import './growth-result.css';

export function GrowthResult({context}:{context:VisualContext}) {
 const result=context.dataState?.lastActivityResult,words=growthWords(context.locale);if(!result)return null;
 const records=result.actorResults?.map(row=>({result:row.result,actor:context.characters?.find(c=>c.id===row.characterId)?.name??row.characterId}))??[{result,actor:''}];
 return <div className="growth-summary" data-growth-summary="activity">{records.map(({result:r,actor},index)=><section key={index}>
  <div className="growth-caption"><strong className="growth-heading">{actor&&actor+' · '}{context.activities.find(a=>a.id===r.activityId)?.label??r.activityId}</strong>
  <p className="growth-scope">{words.scope}{r.success&&' · '+(r.success.passed?activityResultWords(context.locale).success:activityResultWords(context.locale).failure)}</p></div>
  {activityGrowthRows(r).length>0&&<div className="growth-columns"><span>{words.attribute}</span><span>{words.before} → {words.after}</span><span>{words.change}</span></div>}
  {activityGrowthRows(r).length===0?<p>{words.none}</p>:activityGrowthRows(r).map(row=>{
   const t=row.target,owner=growthOwnerId(t),character=t.kind==='relationship'?context.characters?.find(c=>c.id===t.characterId):null;
   const label=t.kind==='attribute'?context.stats.find(s=>s.id===t.attributeId)?.label??t.attributeId:t.kind==='inventory'?context.items?.find(i=>i.id===t.itemId)?.name??t.itemId:(character?.name??t.characterId)+' / '+(context.relationships?.find(r=>r.characterId===t.characterId&&r.relationshipId===t.relationshipId)?.label??t.relationshipId);
   return <div className="growth-row" key={row.key} data-growth-key={row.key}><strong>{owner&&!actor?(context.characters?.find(c=>c.id===owner)?.name??owner)+' / ':''}{label}</strong><span><span className="growth-inline-label">{words.before} </span>{row.before} → <span className="growth-inline-label">{words.after} </span>{row.after}</span><span className="growth-delta"><span className="growth-inline-label">{words.change} </span>{row.delta>0?'+':''}{row.delta}</span></div>;
  })}
 </section>)}</div>;
}
