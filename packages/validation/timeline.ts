import type { JsonObject, ProjectDocument } from './model.js';
import { objectItems, type Index, type SemanticContext } from './context.js';

export function checkTimelines(ctx:SemanticContext,doc:ProjectDocument,visual:JsonObject):void {
  const timelines:Index=new Map();ctx.addArray(timelines,doc,'/visual/timelines',visual.timelines,'활동 연출');
  for(const [i,timeline]of objectItems(visual.timelines).entries()){
    const path=`/visual/timelines/${i}`,clips:Index=new Map();ctx.addArray(clips,doc,path+'/clips',timeline.clips,'연출 클립');
    for(const [j,clip]of objectItems(timeline.clips).entries()){
      const cp=`${path}/clips/${j}`;
      if(clip.imageAssetId){const asset=ctx.resolve(ctx.assets,String(clip.imageAssetId),doc,cp+'/imageAssetId','이미지');if(asset&&asset.data.kind!=='image')ctx.report(doc,cp+'/imageAssetId','TIMELINE_IMAGE_KIND','연출에는 이미지 에셋을 선택하세요.');}
      if(clip.stage && typeof clip.stage==='object' && !Array.isArray(clip.stage)) {
        const stage=clip.stage as JsonObject,sp=cp+'/stage',layers:Index=new Map();
        ctx.addArray(layers,doc,sp+'/layers',stage.layers,'연출 레이어');
        for(const [k,layer] of objectItems(stage.layers).entries()) {
          for(const [f,frame] of objectItems(layer.frames).entries()) {
            const fp=`${sp}/layers/${k}/frames/${f}/imageAssetId`;
            const asset=ctx.resolve(ctx.assets,String(frame.imageAssetId),doc,fp,'연출 프레임 이미지');
            if(asset&&asset.data.kind!=='image')ctx.report(doc,fp,'TIMELINE_IMAGE_KIND','연출 프레임에는 이미지 에셋을 선택하세요.');
          }
        }
      }
      if(!clip.imageAssetId&&!clip.stage&&!String(clip.text??'').trim())ctx.report(doc,cp,'TIMELINE_EMPTY_CLIP','클립에 이미지, 합성 장면 또는 문구를 입력하세요.');
      if(/\{(?!(?:activity|outcome|day|changes)\})[^}]*\}/.test(String(clip.text??'')))ctx.report(doc,cp+'/text','TIMELINE_TEXT_TOKEN','연출 문구에는 {activity}, {outcome}, {day}, {changes}만 사용할 수 있습니다.');
    }
  }
  const activities=new Set<string>(),project=doc.data as JsonObject,gameplay=(project.gameplay??{})as JsonObject,calendar=(gameplay.ruleCalendar??{})as JsonObject;
  for(const [i,binding]of objectItems(visual.activityTimelines).entries()){
    const path=`/visual/activityTimelines/${i}`,activityId=String(binding.activityId);
    ctx.resolve(ctx.activities,activityId,doc,path+'/activityId','활동');
    if(activities.has(activityId))ctx.report(doc,path+'/activityId','TIMELINE_DUPLICATE_ACTIVITY','한 활동에 연출 연결은 하나만 만들 수 있습니다.');activities.add(activityId);
    for(const field of ['timelineId','successTimelineId','failureTimelineId'])if(binding[field])ctx.resolve(timelines,String(binding[field]),doc,path+'/'+field,'활동 연출');
    if(binding.repeat==='logicalDays'&&(!Number.isInteger(calendar.daysPerMonth)||Number(calendar.daysPerMonth)<3||Number(calendar.daysPerMonth)>366))ctx.report(doc,path+'/repeat','TIMELINE_DAY_CALENDAR','일별 화면 반복에는 규칙 달력의 월별 일수가 필요합니다. 실제 활동 계산은 기간당 한 번만 수행됩니다.');
  }
}
