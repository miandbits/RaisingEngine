import { isObject, type JsonObject, type ProjectDocument } from './model.js';
import { objectItems, type Index, type SemanticContext } from './context.js';
import { sourceFields } from '../visual/data-ui.js';

export function checkDataUi(ctx:SemanticContext,doc:ProjectDocument,visual:JsonObject):void {
  const project=doc.data as JsonObject,gameplay=(project.gameplay??{}) as JsonObject;
  const tables:Index=new Map();ctx.addArray(tables,doc,'/visual/dataTables',visual.dataTables,'데이터 표');
  const typeOf=(value:unknown)=>typeof value==='string'?'text':typeof value;
  for(const [i,table] of objectItems(visual.dataTables).entries()){
    const p=`/visual/dataTables/${i}`,columns:Index=new Map(),rows:Index=new Map();
    ctx.addArray(columns,doc,p+'/columns',table.columns,'데이터 열');ctx.addArray(rows,doc,p+'/rows',table.rows,'데이터 행');
    for(const [j,column] of objectItems(table.columns).entries())if(column.id==='id')ctx.report(doc,`${p}/columns/${j}/id`,'UI_DATA_RESERVED','id는 행의 고정 ID입니다. 다른 열 ID를 사용하세요.');
    for(const [j,row] of objectItems(table.rows).entries()){
      const values=(row.values??{}) as JsonObject;
      for(const column of objectItems(table.columns))if(typeOf(values[String(column.id)])!==column.type)ctx.report(doc,`${p}/rows/${j}/values/${column.id}`,'UI_DATA_TYPE','열의 자료형에 맞는 값을 입력하세요.');
      for(const key of Object.keys(values))if(!columns.has(key))ctx.report(doc,`${p}/rows/${j}/values/${key}`,'UI_DATA_COLUMN','정의되지 않은 열입니다.');
    }
  }
  const refs:Record<string,Set<unknown>>={screenId:new Set(objectItems(visual.screens).map(x=>x.id)),actionId:new Set(objectItems(gameplay.actions).map(x=>x.id)),shopId:new Set(objectItems(gameplay.shops).map(x=>x.id)),itemId:new Set(objectItems(gameplay.items).map(x=>x.id)),groupId:new Set(objectItems(gameplay.policyGroups).map(x=>x.id)),locationId:new Set(objectItems(gameplay.locations).map(x=>x.id)),interactionId:new Set(objectItems(gameplay.interactions).map(x=>x.id))};
  const required:Record<string,string[]>={navigate:['screenId'],visit:['locationId'],buy:['shopId','itemId'],sell:['shopId','itemId'],executeAction:['actionId'],selectPolicy:['groupId'],interact:['interactionId'],useService:['serviceId'],equip:['equipmentId'],unequip:['slot'],acceptQuest:['questId'],claimQuest:['questId'],moveExpedition:['nodeId'],battleAction:['battleAction'],selectBattleAction:['battleAction'],selectBattleTarget:['battleTarget']};
  const supported=['start','advance','save','load','sound','navigate','visit','buy','sell','executeAction','selectPolicy','interact','useService','setDiet','equip','unequip','acceptQuest','claimQuest','moveExpedition','returnFromExpedition','battleAction','selectBattleAction','selectBattleTarget','confirmBattleAction','cancelBattleAction'];
  for(const [i,screen] of objectItems(visual.screens).entries())for(const [j,element] of objectItems(screen.elements).entries()){
    if(element.kind!=='collection')continue;
    const p=`/visual/screens/${i}/elements/${j}`,source=(element.source??{}) as JsonObject,kind=String(source.type);
    const component=objectItems(visual.components).find(x=>x.id===element.componentId);
    if(!component){ctx.report(doc,p+'/componentId','UI_COMPONENT_REFERENCE','연결된 재사용 UI가 없습니다.');continue;}
    const children=objectItems(component.elements),bindings=objectItems(element.bindings),fields:Record<string,string>={...(sourceFields[kind]??{})};
    let table:JsonObject|undefined;
    if(kind==='table'){
      table=ctx.resolve(tables,String(source.tableId),doc,p+'/source/tableId','데이터 표')?.data;
      fields.id='text';for(const column of objectItems(table?.columns))fields[String(column.id)]=String(column.type);
    }
    const allowed:Record<string,string[]>={shopOffers:['shopId'],policyOptions:['groupId'],relationships:['characterId'],table:['tableId'],battleActors:[],battleActions:[],battleTargets:[],battleLog:[],battleRewards:[]};
    for(const key of Object.keys(source))if(key!=='type'&&!(allowed[kind]??[]).includes(key))ctx.report(doc,`${p}/source/${key}`,'UI_DATA_SOURCE_FILTER','이 데이터 원본에서 사용할 수 없는 필터입니다.');
    for(const field of ['shopId','groupId'])if(source[field]&&!refs[field]!.has(source[field]))ctx.report(doc,`${p}/source/${field}`,'UI_ACTION_REFERENCE','데이터 원본의 연결 대상을 찾을 수 없습니다.');
    if(source.characterId)ctx.resolve(ctx.characters,String(source.characterId),doc,p+'/source/characterId','인물');
    if(kind.startsWith('battle')&&!project.systems)ctx.report(doc,p+'/source','UI_SYSTEMS_REQUIRED','전투 데이터 원본에는 탐험·전투 시스템이 필요합니다.');
    if(['battleLog','battleRewards'].includes(kind)&&!(project.systems as JsonObject|undefined)?.battleHistoryLimit)ctx.report(doc,p+'/source','UI_BATTLE_HISTORY_REQUIRED','전투 기록·실제 보상을 표시하려면 전투 기록 한도를 설정하세요.');
    if(kind!=='table'&&!kind.startsWith('battle')&&!project.gameplay&&kind!=='relationships')ctx.report(doc,p+'/source','UI_GAMEPLAY_REQUIRED','게임 규칙 데이터가 필요합니다.');
    const seen=new Set<string>();
    for(const [k,binding] of bindings.entries()){
      const bp=`${p}/bindings/${k}`,field=String(binding.field),property=String(binding.property),child=children.find(x=>x.id===binding.elementId),key=`${binding.elementId}:${property==='label'?'text':property}`;
      if(!child)ctx.report(doc,bp+'/elementId','UI_BINDING_CHILD','재사용 UI 안에 연결할 요소가 없습니다.');
      if(seen.has(key))ctx.report(doc,bp+'/property','UI_BINDING_DUPLICATE','같은 요소 속성에 중복 연결할 수 없습니다.');seen.add(key);
      if(!fields[field]){ctx.report(doc,bp+'/field','UI_DATA_FIELD','데이터 원본에 해당 필드가 없습니다.');continue;}
      const expected=['visible','enabled'].includes(property)?'boolean':['quantity','value','maxValue'].includes(property)?'number':['text','label'].includes(property)?null:'text';
      if(expected&&fields[field]!==expected)ctx.report(doc,bp+'/field','UI_BINDING_TYPE','연결할 속성과 필드 자료형이 다릅니다.');
      if(binding.format!==undefined&&(!['text','label'].includes(property)||/\{(?!value\})[^}]*\}/.test(String(binding.format))))ctx.report(doc,bp+'/format','UI_BINDING_FORMAT','텍스트 표시 형식에는 {value}만 사용할 수 있습니다.');
      if(child&&property==='imageAssetId'&&child.kind!=='image')ctx.report(doc,bp+'/property','UI_BINDING_KIND','이미지 요소에 연결하세요.');
      if(child&&['value','maxValue'].includes(property)&&child.kind!=='meter')ctx.report(doc,bp+'/property','UI_BINDING_KIND','수치 게이지 요소에 연결하세요.');
      if(child&&!['text','label','imageAssetId','visible','value','maxValue'].includes(property)&&child.kind!=='button'&&!(property==='enabled'&&child.kind==='group'))ctx.report(doc,bp+'/property','UI_BINDING_KIND','행동은 버튼, 활성화는 버튼이나 그룹에 연결하세요.');
      if(kind!=='table'&&(Object.hasOwn(refs,property)||property==='optionId'||property==='imageAssetId'||property==='action'||property==='battleAction'||property==='battleTarget')&&field!==property)ctx.report(doc,bp+'/field','UI_BINDING_REFERENCE_FIELD','실행 대상에는 같은 종류의 ID 필드를 연결하세요.');
      if(kind!=='table'&&property==='quantity'&&!['quantity','maxQuantity'].includes(field))ctx.report(doc,bp+'/field','UI_BINDING_QUANTITY','수량에는 양의 정수 수량 필드를 연결하세요.');
    }
    if(isObject(element.rowFilter)){
      const filter=element.rowFilter,field=String(filter.field),type=fields[field];
      if(!type)ctx.report(doc,p+'/rowFilter/field','UI_DATA_FIELD','필터 필드가 없습니다.');
      else if(typeOf(filter.value)!==type||(!['eq','ne'].includes(String(filter.operator))&&type!=='number'))ctx.report(doc,p+'/rowFilter','UI_FILTER_TYPE','비교값과 필드 자료형을 맞추세요. 수치 비교는 숫자 필드만 지원합니다.');
    }
    for(const child of children.filter(x=>x.kind==='button')){
      const map=new Map(bindings.filter(x=>x.elementId===child.id).map(x=>[String(x.property),x]));
      const validate=(row:JsonObject|undefined,rp:string)=>{
        const resolved={...child};for(const [property,binding] of map)if(row)resolved[property]=row[String(binding.field)]!;
        const action=String(map.has('action')&&!row?(kind==='shopOffers'?'buy':kind==='policyOptions'?'selectPolicy':kind==='battleActions'?'selectBattleAction':kind==='battleTargets'?'selectBattleTarget':child.action):resolved.action);
        if(!supported.includes(action))ctx.report(doc,rp,'UI_BINDING_ACTION','지원하지 않는 버튼 행동입니다.');
        for(const field of required[action]??[])if(!resolved[field]&&(!map.has(field)||row))ctx.report(doc,rp,'UI_BINDING_ARGUMENT',`${action} 행동에 ${field} 연결이 필요합니다.`);
        if(resolved.quantity!==undefined&&(!Number.isInteger(resolved.quantity)||Number(resolved.quantity)<1||Number(resolved.quantity)>999))ctx.report(doc,rp,'UI_BINDING_QUANTITY','수량은 1..999 정수여야 합니다.');
        if(['battleAction','selectBattleAction'].includes(action)){
          if(resolved.battleAction&&!['attack','guard','flee','useItem'].includes(String(resolved.battleAction)))ctx.report(doc,rp,'UI_BATTLE_ACTION','지원하지 않는 전투 행동입니다.');
          if(resolved.battleAction==='useItem'&&(!map.has('itemId')||row)&&!objectItems((project.systems as JsonObject|undefined)?.consumables).some(x=>x.itemId===resolved.itemId))ctx.report(doc,rp,'UI_BATTLE_ITEM','전투에 사용할 수 있는 회복 아이템을 연결하세요.');
        }
        if(action==='selectBattleTarget'&&resolved.battleTarget&&!['self','enemy'].includes(String(resolved.battleTarget)))ctx.report(doc,rp,'UI_BATTLE_TARGET','전투 대상은 자신 또는 적이어야 합니다.');
        if(row){
          for(const [field,ids] of Object.entries(refs))if(resolved[field]!==undefined&&!ids.has(resolved[field]))ctx.report(doc,rp,'UI_BINDING_REFERENCE',`${field} 연결 대상 '${resolved[field]}'가 없습니다.`);
          if(action==='buy'||action==='sell'){const shop=objectItems(gameplay.shops).find(x=>x.id===resolved.shopId);if(shop&&!objectItems(action==='buy'?shop.offers:shop.sellOffers).some(x=>x.itemId===resolved.itemId))ctx.report(doc,rp,'UI_BINDING_OFFER','상품이 선택한 상점의 거래 목록에 없습니다.');}
          if(action==='selectPolicy'&&resolved.optionId!==undefined&&resolved.optionId!==null){const group=objectItems(gameplay.policyGroups).find(x=>x.id===resolved.groupId);if(group&&!objectItems(group.options).some(x=>x.id===resolved.optionId))ctx.report(doc,rp,'UI_BINDING_OPTION','정책 그룹에 선택지가 없습니다.');}
        }
      };
      if(table)for(const [r,row] of objectItems(table.rows).entries())validate({...row.values as JsonObject,id:row.id!},`/visual/dataTables/${objectItems(visual.dataTables).findIndex(x=>x.id===table!.id)}/rows/${r}/values`);else validate(undefined,p+'/bindings');
    }
    if(table)for(const [r,row]of objectItems(table.rows).entries())for(const child of children.filter(x=>x.kind==='meter')){
      const resolved={...child};for(const binding of bindings.filter(x=>x.elementId===child.id))resolved[String(binding.property)]=(row.values as JsonObject)[String(binding.field)]!;
      if((resolved.value!==undefined&&(typeof resolved.value!=='number'||!Number.isFinite(resolved.value)))||(resolved.maxValue!==undefined&&(typeof resolved.maxValue!=='number'||!Number.isFinite(resolved.maxValue)||resolved.maxValue<=0)))ctx.report(doc,`/visual/dataTables/${objectItems(visual.dataTables).findIndex(x=>x.id===table!.id)}/rows/${r}/values`,'UI_METER_RANGE','게이지 값은 유한한 수이고 최댓값은 0보다 커야 합니다.');
    }
    if(table)for(const [r,row] of objectItems(table.rows).entries())for(const binding of bindings.filter(x=>x.property==='imageAssetId')){
      const id=(row.values as JsonObject)[String(binding.field)];if(id==='')continue;
      const asset=ctx.resolve(ctx.assets,String(id),doc,`/visual/dataTables/${objectItems(visual.dataTables).findIndex(x=>x.id===table!.id)}/rows/${r}/values/${binding.field}`,'데이터 이미지');if(asset&&asset.data.kind!=='image')ctx.report(doc,p+'/bindings','UI_IMAGE_KIND','이미지 에셋을 연결하세요.');
    }
  }
}
