import React,{useEffect} from 'react';
import type {TimelineView,TimelineAction} from '../../../packages/player-service/presentation.js';
import {assetUrl} from './assets.js';
import {timelineLayerFrameIndex} from '../../../packages/visual/timeline.js';
import './timeline.css';

export function TimelineOverlay({timeline,generation,busy,act,save,load}:{timeline:TimelineView|null;generation:number;busy:boolean;act:(action:TimelineAction|{type:'continue'})=>void;save:()=>void;load:()=>void}){
  const cursor=timeline?.cursor;
  useEffect(()=>{
    if(!cursor||cursor.completed||cursor.paused||busy)return;
    const timer=setTimeout(()=>act({type:'timelineTick',deltaMs:100}),100);
    return()=>clearTimeout(timer);
  },[cursor,busy,act]);
  if(!timeline||timeline.plan.resultPlayback)return null;
  const {plan}=timeline,clip=plan.clips[Math.min(timeline.cursor.clipIndex,plan.clips.length-1)];
  if(!clip)return null;
  const elapsedMs=cursor!.completed?clip.durationMs:cursor!.elapsedMs;
  const opacity=clip.transition==='fade'&&!cursor!.completed?Math.min(1,elapsedMs/Math.min(300,clip.durationMs)):1;
  return <div className="timeline-overlay" data-testid="timeline-overlay"><section className="timeline-dialog" role="dialog" aria-modal="true" aria-labelledby="timeline-title">
    <header><h2 id="timeline-title">{cursor!.completed?'활동 연출 완료':'활동 기록'}</h2><span id="timeline-status" data-testid="timeline-status">{clip.day!==null?`${clip.day}일 · `:''}{Math.min(cursor!.clipIndex+1,plan.clips.length)} / {plan.clips.length} · {cursor!.speed}배속{cursor!.paused?' · 일시 정지':''}</span></header>
    {clip.stage?<div id="timeline-stage" data-testid="timeline-stage" className="timeline-stage" style={{opacity,aspectRatio:`${clip.stage.width} / ${clip.stage.height}`,width:`min(100%, ${38*clip.stage.width/clip.stage.height}vh)`}}>
      {clip.stage.layers.map(layer=>{
        const index=timelineLayerFrameIndex(layer,elapsedMs),frame=layer.frames[index];
        return frame&&<img key={layer.id} data-layer-id={layer.id} data-frame-index={index} className="timeline-layer" style={{left:`${100*layer.x/clip.stage!.width}%`,top:`${100*layer.y/clip.stage!.height}%`,width:`${100*layer.width/clip.stage!.width}%`,height:`${100*layer.height/clip.stage!.height}%`}} src={assetUrl(frame.imageAssetId,generation)} alt=""/>;
      })}
    </div>:clip.imageAssetId&&<img key={clip.id} id="timeline-image" className="timeline-image" style={{opacity}} src={assetUrl(clip.imageAssetId,generation)} alt=""/>}
    <p id="timeline-text" data-testid="timeline-text">{clip.text}</p>
    <progress aria-label="현재 연출 진행" max={clip.durationMs} value={cursor!.completed?clip.durationMs:cursor!.elapsedMs}/>
    <footer>
      <button id="timeline-pause" data-testid="timeline-pause" disabled={busy||cursor!.completed} onClick={()=>act({type:'timelinePause'})}>{cursor!.paused?'재생':'일시 정지'}</button>
      <button id="timeline-speed" data-testid="timeline-speed" disabled={busy||cursor!.completed} onClick={()=>act({type:'timelineSpeed'})}>{cursor!.speed===1?'빠르게 ×4':'보통 ×1'}</button>
      <button id="timeline-skip" data-testid="timeline-skip" disabled={busy||cursor!.completed} onClick={()=>act({type:'timelineSkip'})}>건너뛰기</button>
      <button id="timeline-save" data-testid="timeline-save" disabled={busy} onClick={save}>저장</button>
      <button id="timeline-load" data-testid="timeline-load" disabled={busy} onClick={load}>불러오기</button>
      <button id="timeline-continue" data-testid="timeline-continue" disabled={busy||!cursor!.completed} onClick={()=>act({type:'continue'})}>다음으로</button>
    </footer>
  </section></div>;
}
