import type { ProjectDocument, JsonObject } from './model.js';
import { objectItems, type SemanticContext } from './context.js';
import { checkDataUi } from './data-ui.js';
import { checkHierarchy } from './hierarchy.js';
import { resolveScreenElements } from '../visual/components.js';
import type { ReadonlyDesign } from '../visual/model.js';
import { checkUiComposition } from './ui-composition.js';
import { checkDirection } from './direction.js';
import { checkTimelines } from './timeline.js';
export function checkVisual(ctx:SemanticContext,doc:ProjectDocument,visual:JsonObject):void {
  checkDirection(ctx,doc,visual);
  checkUiComposition(ctx,doc,visual);
  checkDataUi(ctx,doc,visual);
  checkTimelines(ctx,doc,visual);
  const styleIds=new Set<string>();
  const checkStyle=(style:JsonObject,p:string)=>{if(style.fontAssetId){const asset=ctx.resolve(ctx.assets,String(style.fontAssetId),doc,p+'/fontAssetId','폰트');if(asset&&asset.data.kind!=='font')ctx.report(doc,p+'/fontAssetId','UI_FONT_KIND','폰트 에셋을 선택하세요.');}};
  checkStyle((visual.defaultStyle??{}) as JsonObject,'/visual/defaultStyle');
  for(const [i,s] of objectItems(visual.styles).entries()){if(styleIds.has(String(s.id)))ctx.report(doc,`/visual/styles/${i}/id`,'UI_STYLE_DUPLICATE','공통 스타일 ID가 중복되었습니다.');styleIds.add(String(s.id));checkStyle(s,`/visual/styles/${i}`);}
  const allScreens=new Set(objectItems(visual.screens).map(s=>s.id));
  const root=doc.data as JsonObject;
  const gameplay=(root.gameplay??{}) as JsonObject;
  const systems=(root.systems??{}) as JsonObject;
  const refs:Record<string,Set<unknown>>={dietId:new Set(objectItems(gameplay.diets).map(x=>x.id)),serviceId:new Set(objectItems(gameplay.services).map(x=>x.id)),screenId:allScreens,locationId:new Set(objectItems(gameplay.locations).map(x=>x.id)),shopId:new Set(objectItems(gameplay.shops).map(x=>x.id)),itemId:new Set(objectItems(gameplay.items).map(x=>x.id)),interactionId:new Set(objectItems(gameplay.interactions).map(x=>x.id))};
  refs.questId=new Set(objectItems(systems.quests).map(x=>x.id));
  refs.actionId=new Set(objectItems(gameplay.actions).map(x=>x.id));
  refs.groupId=new Set(objectItems(gameplay.policyGroups).map(x=>x.id));
  refs.equipmentId=new Set(objectItems(systems.equipment).map(x=>x.id));
  refs.nodeId=new Set(objectItems(systems.expeditions).flatMap(x=>objectItems(x.nodes).map(node=>node.id)));
  const checkElement=(e:JsonObject,ep:string,width:number,height:number)=>{
      checkStyle(e,ep);
      if (e.kind === 'input' || e.binding === 'user-field' || e.userFieldId !== undefined) {
        if (!objectItems(root.userFields).some(field => field.id === e.userFieldId)) ctx.report(doc, ep+'/userFieldId', 'UI_USER_FIELD_REFERENCE', `사용자 필드 '${e.userFieldId ?? ''}'가 없습니다.`);
      }
      if (e.binding === 'user-field' && e.kind !== 'text') ctx.report(doc, ep+'/binding', 'UI_USER_FIELD_BINDING_KIND', '사용자 필드 표시 바인딩은 텍스트 요소에 설정하세요.');
      if(e.clipChildren!==undefined&&e.kind!=='group')ctx.report(doc,ep+'/clipChildren','UI_GROUP_REQUIRED','자식 자르기는 그룹에만 설정할 수 있습니다.');
      for(const [field,ref] of [['actionIds','actionId'],['groupIds','groupId']])for(const [i,id] of ((e[field!]??[]) as string[]).entries())if(!refs[ref!]!.has(id))ctx.report(doc,`${ep}/${field}/${i}`,'UI_ACTION_REFERENCE',`연결 대상 '${id}'가 없습니다.`);
      if(e.action==='executeAction'&&!e.actionId&&!objectItems(e.selectionBindings).some(b=>b.property==='actionId'))ctx.report(doc,ep+'/actionId','UI_ACTION_REQUIRED','범용 행동을 선택하세요.');
      if(e.action==='selectPolicy'){
        if(!e.groupId)ctx.report(doc,ep+'/groupId','UI_ACTION_REQUIRED','정책 그룹을 선택하세요.');
        const group=objectItems(gameplay.policyGroups).find(x=>x.id===e.groupId);
        if(e.optionId!==null&&e.optionId!==undefined&&group&&!objectItems(group.options).some(x=>x.id===e.optionId))ctx.report(doc,ep+'/optionId','UI_ACTION_REFERENCE','정책 그룹에 선택지가 없습니다.');
      }
      if(!root.gameplay&&(['actions','policies'].includes(String(e.kind))||['executeAction','selectPolicy'].includes(String(e.action))))ctx.report(doc,ep,'UI_GAMEPLAY_REQUIRED','범용 규칙 작업실에서 행동과 정책을 설정하세요.');
      if(['selectBattleAction','selectBattleTarget','confirmBattleAction','cancelBattleAction'].includes(String(e.action))&&!root.systems)ctx.report(doc,ep,'UI_SYSTEMS_REQUIRED','전투 행동에는 탐험·전투 시스템이 필요합니다.');
      if(['battleAction','selectBattleAction'].includes(String(e.action))&&e.battleAction==='useItem'&&!objectItems((root.systems as JsonObject|undefined)?.consumables).some(x=>x.itemId===e.itemId))ctx.report(doc,ep+'/itemId','UI_BATTLE_ITEM','전투 회복 아이템을 선택하세요.');
      if(e.kind==='button'&&!e.action)ctx.report(doc,ep+'/action','UI_ACTION_REQUIRED','버튼에는 행동 연결이 필요합니다.');
      if(e.imageAssetId){const asset=ctx.resolve(ctx.assets,String(e.imageAssetId),doc,ep+'/imageAssetId','이미지');if(asset&&asset.data.kind!=='image')ctx.report(doc,ep+'/imageAssetId','UI_IMAGE_KIND','이미지 에셋을 선택하세요.');}
      if(e.styleId&&!styleIds.has(String(e.styleId)))ctx.report(doc,ep+'/styleId','UI_STYLE_REFERENCE','연결된 공통 스타일이 없습니다.');
      for(const [a,id] of ((e.attributeIds??[]) as string[]).entries())ctx.resolve(ctx.attributes,id,doc,`${ep}/attributeIds/${a}`,'능력·자원');
      for(const field of ['frameAssetId','entryFrameAssetId'])if(e[field])ctx.resolve(ctx.assets,String(e[field]),doc,`${ep}/${field}`,'이미지');
      const groupIds=new Set<string>();
      for(const [g,group] of objectItems(e.scheduleGroups).entries()) {
        const gp=`${ep}/scheduleGroups/${g}`;
        if(groupIds.has(String(group.id)))ctx.report(doc,`${gp}/id`,'UI_SCHEDULE_GROUP_DUPLICATE','일정 분류 ID가 중복되었습니다.');
        groupIds.add(String(group.id));
        for(const [a,id] of ((group.activityIds??[]) as string[]).entries())ctx.resolve(ctx.activities,id,doc,`${gp}/activityIds/${a}`,'활동');
        if(group.iconAssetId)ctx.resolve(ctx.assets,String(group.iconAssetId),doc,`${gp}/iconAssetId`,'이미지');
      }
      if(e.scheduleLayout==='menu'&&!objectItems(e.scheduleGroups).length)ctx.report(doc,`${ep}/scheduleGroups`,'UI_SCHEDULE_GROUP_REQUIRED','분류 메뉴에는 활동 분류를 추가하세요.');
      const systemActions=['acceptQuest','claimQuest','equip','unequip','moveExpedition','returnFromExpedition','battleAction'];
      if(!root.systems&&(['equipment','quests','expedition','battle','relationship-stages'].includes(String(e.kind))||systemActions.includes(String(e.action))))ctx.report(doc,ep,'UI_SYSTEMS_REQUIRED','탐험·성장 작업실에서 시스템을 설정하세요.');
      const required:Record<string,string[]>={executeActorAction:['actionId','actorId'],openModal:['screenId'],navigate:['screenId'],visit:['locationId'],buy:['shopId','itemId'],sell:['shopId','itemId'],useService:['serviceId'],interact:['interactionId'],acceptQuest:['questId'],claimQuest:['questId'],equip:['equipmentId'],unequip:['slot'],moveExpedition:['nodeId'],battleAction:e.battleAction==='useItem'?['battleAction','itemId']:['battleAction'],selectBattleAction:e.battleAction==='useItem'?['battleAction','itemId']:['battleAction'],selectBattleTarget:['battleTarget']};
      for(const field of required[String(e.action)]??[])if(!e[field]&&!objectItems(e.selectionBindings).some(b=>b.property===field))ctx.report(doc,`${ep}/${field}`,'UI_ACTION_REQUIRED',`이 행동에는 ${field} 연결이 필요합니다.`);
      if(!e.parentId&&((e.x as number)<0||(e.y as number)<0||(e.x as number)+(e.width as number)>width||(e.y as number)+(e.height as number)>height))ctx.report(doc,ep,'UI_BOUNDS','요소가 기준 화면 영역을 벗어났습니다.');
      if(e.kind==='image'&&(e.imageSource??'asset')==='asset'&&!e.imageAssetId)ctx.report(doc,`${ep}/imageAssetId`,'UI_IMAGE_REQUIRED','고정 이미지는 이미지 에셋을 선택해야 합니다.');
      if(e.binding==='attribute-value'&&!ctx.attributes.has(String(e.attributeId)))ctx.report(doc,`${ep}/attributeId`,'UI_BINDING_REFERENCE',`능력·자원 '${e.attributeId}'가 없습니다.`);
      if(e.binding==='relationship-value'){
        const character=ctx.characters.get(String(e.characterId))?.[0];
        if(!character)ctx.report(doc,`${ep}/characterId`,'UI_BINDING_REFERENCE',`인물 '${e.characterId}'가 없습니다.`);
        else if(!objectItems(character.data.relationships).some(r=>r.id===e.relationshipId))ctx.report(doc,`${ep}/relationshipId`,'UI_BINDING_REFERENCE',`해당 인물에 관계 수치 '${e.relationshipId}'가 없습니다.`);
      }
      if(typeof e.valueFormat==='string'&&/\{(?!value\}|label\})[^}]*\}/.test(e.valueFormat))ctx.report(doc,`${ep}/valueFormat`,'UI_BINDING_FORMAT','표시 형식에는 {value}, {label}만 사용할 수 있습니다.');
      for(const [field,ids] of Object.entries(refs))if(e[field]!==undefined&&!ids.has(e[field]))ctx.report(doc,`${ep}/${field}`,'UI_ACTION_REFERENCE',`연결 대상 '${e[field]}'가 없습니다. 화면 또는 육성 작업실에서 대상을 확인하세요.`);
      if(!root.gameplay&&(['locations','shop','inventory','interactions','relationships','diets','services','sell'].includes(String(e.kind))||['navigate','visit','buy','interact','sell','setDiet','useService'].includes(String(e.action))))ctx.report(doc,ep,'UI_GAMEPLAY_REQUIRED','이 요소는 육성 작업실의 장소·행동 설정이 필요합니다.');
      if(e.action==='buy'||e.action==='sell'){
        const shop=objectItems(gameplay.shops).find(x=>x.id===e.shopId);
        if(shop&&!objectItems(e.action==='sell'?shop.sellOffers:shop.offers).some(x=>x.itemId===e.itemId))ctx.report(doc,`${ep}/itemId`,'UI_SHOP_OFFER','선택한 상점의 거래 목록에 없는 아이템입니다.');
      }
  };
  const components=new Map<string,{data:JsonObject,path:string}>();
  for(const [i,component] of objectItems(visual.components).entries()) {
    const cp=`/visual/components/${i}`,id=String(component.id);
    if(components.has(id))ctx.report(doc,cp+'/id','UI_COMPONENT_DUPLICATE',`재사용 UI '${id}'가 중복되었습니다.`);
    else components.set(id,{data:component,path:cp});
    const ids=new Set<string>();
    checkHierarchy(ctx,doc,objectItems(component.elements),cp+'/elements');
    for(const [j,child] of objectItems(component.elements).entries()) {
      const ep=`${cp}/elements/${j}`;
      if(!['panel','text','image','button','meter','group','input','component'].includes(String(child.kind)))ctx.report(doc,ep+'/kind','UI_COMPONENT_CHILD_KIND','재사용 UI 안에는 패널·텍스트·이미지·버튼·게이지·그룹만 배치할 수 있습니다. 중첩 구성은 지원하지 않습니다.');
      if(ids.has(String(child.id)))ctx.report(doc,ep+'/id','UI_ELEMENT_DUPLICATE','재사용 UI 안의 요소 ID가 중복되었습니다.');
      ids.add(String(child.id));checkElement(child,ep,Number(component.width),Number(component.height));
    }
  }
  const axis=(position:number,size:number,delta:number,anchor:unknown):[number,number]=>anchor==='stretch'?[position,Math.max(16,size+delta)]:[position+(anchor==='center'?delta/2:anchor==='right'||anchor==='bottom'?delta:0),size];
  const screens=new Set<string>();
  for(const [i,screen] of objectItems(visual.screens).entries()) {
    const p=`/visual/screens/${i}`,id=String(screen.id);
    if(screens.has(id))ctx.report(doc,p+'/id','UI_SCREEN_DUPLICATE',`화면 '${id}'가 중복되었습니다.`);
    screens.add(id);const ids=new Set<string>(),elements:JsonObject[]=[];
    checkHierarchy(ctx,doc,objectItems(screen.elements),p+'/elements');
    const register=(elementId:string,ep:string)=>{
      if(ids.has(elementId))ctx.report(doc,ep+'/id','UI_ELEMENT_DUPLICATE',`화면 안의 요소 ID '${elementId}'가 중복되었습니다. 구성 인스턴스와 자식 ID도 확인하세요.`);
      ids.add(elementId);
    };
    for(const [j,e] of objectItems(screen.elements).entries()) {
      const ep=`${p}/elements/${j}`;
      register(String(e.id),ep);checkElement(e,ep,Number(visual.width),Number(visual.height));
      if(e.kind!=='component'){elements.push(e);continue;}
      const component=components.get(String(e.componentId));
      if(!component){ctx.report(doc,ep+'/componentId','UI_COMPONENT_REFERENCE',`재사용 UI '${e.componentId}'가 없습니다.`);continue;}
      const children=objectItems(component.data.elements),overrides=new Map<string,{values:JsonObject,path:string}>();
      for(const [k,override] of objectItems(e.overrides).entries()) {
        const op=`${ep}/overrides/${k}`,childId=String(override.elementId);
        if(overrides.has(childId))ctx.report(doc,op+'/elementId','UI_COMPONENT_OVERRIDE_DUPLICATE','같은 자식의 재정의가 중복되었습니다.');
        if(!children.some(child=>child.id===childId))ctx.report(doc,op+'/elementId','UI_COMPONENT_OVERRIDE_REFERENCE',`재정의할 자식 '${childId}'가 없습니다.`);
        overrides.set(childId,{values:(override.values??{}) as JsonObject,path:op+'/values'});
      }
      for(const [k,child] of children.entries()) {
        const override=overrides.get(String(child.id)),merged={...child,...override?.values};
        if(override)checkElement(merged,override.path,Number(component.data.width),Number(component.data.height));
        const [x,width]=axis(Number(child.x),Number(child.width),Number(e.width)-Number(component.data.width),child.anchorX);
        const [y,height]=axis(Number(child.y),Number(child.height),Number(e.height)-Number(component.data.height),child.anchorY);
        const expanded={...merged,id:`${e.id}__${child.id}`,x:Number(e.x)+x,y:Number(e.y)+y,width,height,
          hidden:Boolean(e.hidden||child.hidden)||Boolean(e.visibleWhen&&e.visibleWhen!=='always'&&child.visibleWhen&&child.visibleWhen!=='always'&&e.visibleWhen!==child.visibleWhen),
          opacity:Number(e.opacity??1)*Number(child.opacity??1),visibleWhen:child.visibleWhen&&child.visibleWhen!=='always'?child.visibleWhen:e.visibleWhen??'always'};
        register(expanded.id,ep);
        if(!e.parentId&&!child.parentId&&(expanded.x<0||expanded.y<0||expanded.x+width>Number(visual.width)||expanded.y+height>Number(visual.height)))ctx.report(doc,ep,'UI_BOUNDS',`재사용 UI의 자식 '${child.id}'가 기준 화면 영역을 벗어났습니다.`);
        elements.push(expanded);
      }
    }
    try {elements.splice(0,elements.length,...resolveScreenElements(visual as unknown as ReadonlyDesign,id).map(x=>x as unknown as JsonObject));} catch {ctx.report(doc,p,"UI_EXPANSION_LIMIT","부품 순환·깊이 또는 펼친 요소 수 한도를 확인하세요.");continue;}
    const active=elements.filter(e=>!e.hidden&&(e.opacity??1)!==0&&(e.visibleWhen??'always')==='always');
    const has=(kind:string,action?:string)=>active.some(e=>e.kind===kind&&(!action||e.action===action));
    if((id==='title'||id==='ending')&&!has('button','start'))ctx.report(doc,p,'UI_NAVIGATION','이 화면에는 항상 표시되는 새 게임 버튼이 필요합니다.');
    const autoPlanButton=id==='schedule'&&root.schedulePlayback==='continuous'&&elements.some(e=>e.kind==='button'&&e.action==='advance'&&!e.hidden&&Number(e.opacity??1)>0&&e.visibleWhen==='planning');
    if((id==='schedule'||id==='story')&&!has('button','advance')&&!autoPlanButton)ctx.report(doc,p,'UI_NAVIGATION','이 화면에는 진행 버튼이 필요합니다. 자동 일정은 계획 중 표시되는 버튼을 사용할 수 있습니다.');
    if(id==='schedule'&&!elements.some(e=>e.kind==='schedule'&&!e.hidden&&Number(e.opacity??1)>0&&['always','planning'].includes((e.visibleWhen??'always') as string)))ctx.report(doc,p,'UI_NAVIGATION','일정 화면에는 일정을 선택할 요소가 필요합니다.');
    if(id==='story')for(const kind of ['dialogue','choices'])if(!elements.some(e=>e.kind===kind&&!e.hidden&&Number(e.opacity??1)>0&&['always',kind==='choices'?'choice':'dialogue'].includes((e.visibleWhen??'always') as string)))ctx.report(doc,p,'UI_NAVIGATION',`대화 화면에는 ${kind==='choices'?'선택지':'대사'} 요소가 필요합니다.`);
  }
  for(const id of ['title','schedule','story','ending'])if(!screens.has(id))ctx.report(doc,'/visual/screens','UI_SCREEN_MISSING',`필수 화면 '${id}'가 없습니다.`);
}
