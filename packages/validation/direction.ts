import type {ProjectDocument,JsonObject} from './model.js';
import type {SemanticContext} from './context.js';
export function checkDirection(ctx:SemanticContext,doc:ProjectDocument,visual:JsonObject){
 const d=visual.direction as any;const root='/visual/direction';
 const report=(p:string,m:string)=>ctx.report(doc,p,'DIRECTION_REFERENCE',m);
 const ids=(items:any[],p:string)=>{const seen=new Set<string>();for(const [i,x] of items.entries()){if(seen.has(x.id))report(`${p}/${i}/id`,'중복 ID입니다.');seen.add(x.id);}return seen;};
 if(d){ids(d.actors,root+'/actors');ids(d.sequences,root+'/sequences');
  for(const [i,a] of d.actors.entries()){const p=`${root}/actors/${i}`;const asset=(id:string,q:string)=>{const x=ctx.resolve(ctx.assets,id,doc,q,'이미지');if(x&&x.data.kind!=='image')report(q,'이미지 에셋이어야 합니다.');};asset(a.assetId,p+'/assetId');
   for(const group of ['expressions','outfits','motions']){ids(a[group]??[],p+'/'+group);for(const [j,v] of (a[group]??[]).entries()){if(group==='motions')v.frames.forEach((f:string,k:number)=>asset(f,`${p}/${group}/${j}/frames/${k}`));else asset(v.assetId,`${p}/${group}/${j}/assetId`);}}
  }
  for(const [i,s] of d.sequences.entries())for(const [j,stage] of s.stages.entries()){const seen=new Set();for(const [k,c] of stage.cues.entries()){const p=`${root}/sequences/${i}/stages/${j}/cues/${k}`,a=d.actors.find((a:any)=>a.id===c.actorId);if(seen.has(c.actorId))report(p+'/actorId','동시 구간에 같은 배우를 두 번 지정할 수 없습니다.');seen.add(c.actorId);if(!a){report(p+'/actorId','배우가 없습니다.');continue;}for(const [field,list] of [['expressionId','expressions'],['outfitId','outfits'],['motionId','motions']] as const)if(c[field]&&!(a[list]??[]).some((v:any)=>v.id===c[field]))report(p+'/'+field,'배우의 표정·의상·동작 ID를 확인하세요.');}}
 }
 for(const [i,s] of (visual.screens as any[]??[]).entries())for(const [j,e] of (s.elements??[]).entries())if(e.action==='playSequence'&&!d?.sequences.some((s:any)=>s.id===e.sequenceId))report(`/visual/screens/${i}/elements/${j}/sequenceId`,'연결할 연출이 없습니다.');
 for(const [i,s] of (visual.components as any[]??[]).entries())for(const [j,e] of (s.elements??[]).entries())if(e.action==='playSequence'&&!d?.sequences.some((s:any)=>s.id===e.sequenceId))report(`/visual/components/${i}/elements/${j}/sequenceId`,'연결할 연출이 없습니다.');
}
