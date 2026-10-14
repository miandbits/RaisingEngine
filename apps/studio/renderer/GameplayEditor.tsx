import { GameplayList, ConnectionPicker } from './GameplayList.js';
import React, { useEffect, useState } from 'react';
import type { DraftDocument, StudioView } from '../../../packages/studio-service/editor.js';
import type { Json } from '../../../packages/validation/model.js';
import type { GameplayDefinition as GameplayContract } from '../../../packages/gameplay/model.js';
import { createVisual, type VisualDesign } from '../../../packages/visual/model.js';
import { createScreen, type ScreenTemplate } from '../../../packages/visual/screens.js';
import './gameplay-editor.css';

type GameplayDefinition = Omit<GameplayContract,'locations'> & {locations:GameplayContract['locations'][number][]};
type Category = 'locations' | 'items' | 'shops' | 'interactions';
export interface GameplayFocus { category:Category; id:string; filter?:string }
type Row = Record<string, any>;
export interface RelationEventDraft { interactionId:string; id:string; label:string; text:string; characterId:string; relationshipId:string; threshold:number }
const names: Record<Category,string> = { locations:'장소', items:'아이템', shops:'상점', interactions:'교류·선물' };
const object = (value:unknown):Row => value && typeof value==='object' && !Array.isArray(value) ? value as Row : {};
function Field({label,value,onCommit,multiline=false}:{label:string;value:string;onCommit:(value:string)=>void|boolean;multiline?:boolean}) {
  const [draft,setDraft]=useState(value);useEffect(()=>setDraft(value),[value]);
  const commit=()=>{if(draft!==value&&onCommit(draft)===false)setDraft(value);};
  return <label>{label}{multiline?<textarea aria-label={label} value={draft} onChange={e=>setDraft(e.target.value)} onBlur={commit}/>:<input aria-label={label} value={draft} onChange={e=>setDraft(e.target.value)} onBlur={commit} onKeyDown={e=>{if(e.key==='Enter')e.currentTarget.blur();}}/>}</label>;
}
function NumberField({label,value,onCommit,min=0,max=2147483647}:{label:string;value:number;onCommit:(value:number)=>void;min?:number;max?:number}) {
  return <Field label={label} value={String(value)} onCommit={text=>{const n=Number(text);if(text.trim()&&Number.isSafeInteger(n)&&n>=min&&n<=max){onCommit(n);return true;}return false;}}/>;
}
function Pick({label,value,rows,onChange,empty}:{label:string;value:string;rows:readonly Row[];onChange:(value:string)=>void;empty?:string}) {
  return <label>{label}<select aria-label={label} value={value} onChange={e=>onChange(e.target.value)}>{empty!==undefined&&<option value="">{empty}</option>}{rows.map(row=><option key={row.id} value={row.id}>{row.name??row.label??row.id}</option>)}</select></label>;
}

export function GameplayEditor({view,onChange,onJump,onScreen,onCreateEvent,onPreview,focus,onFocus}:{
  focus?:GameplayFocus|undefined;onFocus:(focus:GameplayFocus)=>void;view:StudioView;onChange:(document:DraftDocument,next:Json)=>void;onJump:(file:string,pointer?:string)=>void;
  onScreen:(id:string)=>void;onCreateEvent:(draft:RelationEventDraft)=>void;onPreview:()=>void;
}) {
  const project=view.documents.find(d=>d.kind==='project')!;const root=object(project.data);
  const definition=root.gameplay as GameplayDefinition|undefined;
  const category=focus?.category??'locations';const selected=focus?.id??'';const filter=focus?.filter??'';
  const [adding,setAdding]=useState(false);const [newId,setNewId]=useState('');const [newName,setNewName]=useState('');const [error,setError]=useState('');
  const [screenTemplate,setScreenTemplate]=useState<ScreenTemplate>('city');
  const [eventDialog,setEventDialog]=useState(false);const [eventId,setEventId]=useState('');const [eventLabel,setEventLabel]=useState('');const [eventText,setEventText]=useState('');const [threshold,setThreshold]=useState(10);const [relationship,setRelationship]=useState('');
  const characters:Row[]=view.documents.filter(d=>d.kind==='character').map(d=>({...object(d.data),file:d.file}));
  const attributes=view.documents.filter(d=>d.kind==='attributes').flatMap(d=>object(d.data).attributes??[]);
  const images=view.documents.filter(d=>d.kind==='assets').flatMap(d=>object(d.data).assets??[]).filter((a:Row)=>a.kind==='image');
  const events:Row[]=view.documents.filter(d=>d.kind==='event').map(d=>({...object(d.data),file:d.file}));
  const screens=(root.visual as VisualDesign|undefined)?.screens??[];
  const rows=(definition?.[category]??[]) as Row[];const row=rows.find(r=>r.id===selected)??rows[0];
  const commit=(next:GameplayDefinition,visual?:VisualDesign)=>onChange(project,{...root,gameplay:next,...(visual?{visual}:{})} as unknown as Json);
  const update=(patch:Row)=>{if(!definition||!row)return;const next={...row,...patch};Object.keys(next).forEach(k=>{if(next[k]===undefined)delete next[k];});commit({...definition,[category]:rows.map(r=>r.id===row.id?next:r)} as GameplayDefinition);};
  const gotoCategory=(value:Category)=>onFocus({category:value,id:''});
  const targets=[...attributes.map((a:Row)=>({id:`attribute/${a.id}`,name:`능력·자원 · ${a.label}`})),...characters.flatMap(c=>(c.relationships??[]).map((r:Row)=>({id:`relationship/${c.id}/${r.id}`,name:`${c.name} · ${r.label}`})))];
  const numericTarget=(key:string)=>{const [kind,id,relationshipId]=key.split('/');return kind==='attribute'?{kind,attributeId:id}:{kind,characterId:id,relationshipId};};
  const targetKey=(target:Row)=>target.kind==='attribute'?`attribute/${target.attributeId}`:`relationship/${target.characterId}/${target.relationshipId}`;
  const conditionEditor=(value:Row|undefined,onCommit:(value:Row)=>void)=>{
    const simple=!value||value.op==='always'||value.op==='compare'&&value.left?.op==='read'&&value.right?.op==='literal';
    return <fieldset><legend>사용 조건</legend>{!simple?<><p>복합 조건이 설정되어 있습니다. 콘텐츠 문서에서 상세 편집할 수 있습니다.</p><button onClick={()=>onJump(project.file,'/gameplay')}>복합 조건 편집</button></>:<><label>조건 방식<select aria-label="조건 방식" value={value?.op==='compare'?'compare':'always'} onChange={e=>onCommit(e.target.value==='always'?{op:'always'}:{op:'compare',comparator:'gte',left:{op:'read',target:numericTarget(targets[0]?.id??'attribute/money')},right:{op:'literal',value:0}})}><option value="always">항상 가능</option><option value="compare">수치 비교</option></select></label>{value?.op==='compare'&&<><Pick label="조건 수치" value={targetKey(value.left.target)} rows={targets} onChange={key=>onCommit({...value,left:{op:'read',target:numericTarget(key)}})}/><Pick label="조건 비교" value={value.comparator} rows={[{id:"gte",name:"이상"},{id:"gt",name:"초과"},{id:"lte",name:"이하"},{id:"lt",name:"미만"},{id:"eq",name:"같음"},{id:"ne",name:"다름"}]} onChange={comparator=>onCommit({...value,comparator})}/><NumberField label="조건 기준값" value={value.right.value} min={-2147483648} onCommit={v=>onCommit({...value,right:{op:'literal',value:v}})}/></>}</>}</fieldset>;
  };
  if(!definition)return <section className="gameplay-intro"><p className="eyebrow">RAISING WORKSHOP</p><h1>장소와 사람을 연결하는 육성 작업실</h1><p>집에서 일정을 정하고, 도시에서 물건을 사서 인물에게 선물하세요. 조건에 맞으면 관계 사건이 이어집니다. 장소·상점·선물의 규칙과 화면을 직접 만들 수 있습니다.</p><button className="studio-primary" onClick={()=>{const visual=structuredClone((root.visual as VisualDesign|undefined)??createVisual());let id='home';for(let i=2;visual.screens.some(s=>s.id===id);i++)id=`home-${i}`;visual.screens.push(createScreen(id,'우리 집','home',visual.width,visual.height));commit({startLocationId:'home',locations:[{id:'home',name:'우리 집',screenId:id,connections:[]}],items:[],shops:[],interactions:[]},visual);}}>육성 작업실 시작</button><p>시작 장소와 집 화면을 추가합니다. 기존 일정·사건·엔딩은 계속 사용할 수 있습니다.</p></section>;
  const createRow=()=>{
    if(!/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(newId)||newId.length>64){setError('ID는 소문자 영문으로 시작하고 숫자·하이픈을 사용하세요.');return;}
    if(rows.some(r=>r.id===newId)){setError('이미 사용 중인 ID입니다.');return;}
    if(!newName.trim()){setError('이름을 입력하세요.');return;}
    let item:Row={id:newId,name:newName.trim()};let next=structuredClone(definition);
    if(category==='locations'){item.connections=[definition.startLocationId];next.locations=next.locations.map(l=>l.id===definition.startLocationId?{...l,connections:[...new Set([...l.connections,newId])]}:l);}
    if(category==='items')item={...item,maxStack:99,initialQuantity:0};
    if(category==='shops')item={...item,locationId:definition.startLocationId,currencyAttributeId:attributes.find((a:Row)=>a.id==='money'&&a.kind==='resource')?.id??attributes.find((a:Row)=>a.kind==='resource')?.id??attributes[0]?.id,offers:[]};
    if(category==='interactions')item={...item,locationId:definition.startLocationId,effects:[],oncePerPeriod:true};
    next={...next,[category]:[...next[category],item]} as GameplayDefinition;commit(next);onFocus({category,id:newId});setAdding(false);
  };
  const removeRow=()=>{if(!row)return;const references:string[]=[];
    if(category==='locations'){if(definition.startLocationId===row.id)references.push('시작 장소');references.push(...definition.shops.filter(s=>s.locationId===row.id).map(s=>`상점 ${s.name}`),...definition.interactions.filter(i=>i.locationId===row.id).map(i=>`교류 ${i.name}`));}
    if(category==='items')references.push(...definition.shops.filter(s=>s.offers.some(o=>o.itemId===row.id)).map(s=>`상점 ${s.name}`),...definition.interactions.filter(i=>i.itemId===row.id).map(i=>`교류 ${i.name}`));
    for(const screen of screens)for(const e of screen.elements)if(e[({locations:'locationId',items:'itemId',shops:'shopId',interactions:'interactionId'} as const)[category]]===row.id)references.push(`화면 ${screen.label}`);
    if(references.length){setError(`사용 중이므로 삭제할 수 없습니다: ${references.join(', ')}`);return;}
    const next={...definition,[category]:rows.filter(r=>r.id!==row.id)} as GameplayDefinition;
    if(category==='locations')next.locations=next.locations.map(l=>({...l,connections:l.connections.filter(id=>id!==row.id)}));commit(next);onFocus({category,id:''});setError('');
  };
  return <section className="gameplay-editor">
    <header className="gameplay-heading"><div><p className="eyebrow">RAISING WORKSHOP</p><h1>육성 작업실</h1><p>장소 → 상점 → 선물 → 관계 사건을 연결합니다.</p></div><button className="studio-primary" onClick={onPreview}>이 흐름 플레이</button></header>
    <div className="journey-steps" aria-label="제작 흐름">{(['locations','items','shops','interactions'] as Category[]).map((c,i)=><button key={c} aria-pressed={category===c} onClick={()=>gotoCategory(c)}><span>{i+1}</span>{names[c]}<strong>{definition[c].length}</strong></button>)}</div>
    {error&&<div className="studio-alert" role="alert">{error}<button onClick={()=>setError('')}>닫기</button></div>}
    <div className="gameplay-grid"><aside className="gameplay-browser"><input aria-label="육성 콘텐츠 검색" placeholder="이름 또는 ID 검색" value={filter} onChange={e=>onFocus({category,id:selected,filter:e.target.value})}/><button className="studio-primary" onClick={()=>{let id=`${({locations:'location',items:'item',shops:'shop',interactions:'interaction'} as const)[category]}-${rows.length+1}`;for(let n=rows.length+2;rows.some(r=>r.id===id);n++)id=`${category}-${n}`;setNewId(id);setNewName('');setError('');setAdding(true);}}>{names[category]} 추가</button>
      <GameplayList items={rows as {id:string;name:string}[]} selected={row?.id??''} query={filter} onSelect={id=>{onFocus({category,id,filter});setError('');}}/>{!rows.length&&<p className="gameplay-empty">첫 {names[category]}을 추가하세요.</p>}
    </aside><main className="gameplay-form">{row?<>
      <div className="gameplay-row-heading"><div><span>{names[category]} / {row.id}</span><h2>{row.name}</h2></div><button onClick={removeRow}>{names[category]} 삭제</button></div>
      <Field label={`${names[category]} 이름`} value={row.name} onCommit={name=>update({name})}/>
      {category==='locations'&&<>
        <label className="gameplay-check"><input type="checkbox" checked={definition.startLocationId===row.id} onChange={()=>commit({...definition,startLocationId:row.id})}/>새 게임 시작 장소</label>
        <Pick label="장소 화면" value={row.screenId??''} rows={screens.filter(s=>!['title','story','ending'].includes(s.id))} empty="일정 화면 사용" onChange={screenId=>update({screenId:screenId||undefined})}/>
        <div className="gameplay-actions"><Pick label="장소 화면 템플릿" value={screenTemplate} rows={[{id:'home',name:'집'},{id:'city',name:'도시·이동'},{id:'shop',name:'상점'},{id:'social',name:'교류'}]} onChange={v=>setScreenTemplate(v as ScreenTemplate)}/><button disabled={screens.length>=100} onClick={()=>{const visual=structuredClone((root.visual as VisualDesign|undefined)??createVisual());let id=row.id;for(let i=2;visual.screens.some(s=>s.id===id);i++)id=`${row.id.slice(0,58)}-${i}`;visual.screens.push(createScreen(id,row.name,screenTemplate,visual.width,visual.height));commit({...definition,locations:definition.locations.map(l=>l.id===row.id?{...l,screenId:id}:l)},visual);}}>장소 화면 만들기</button>{row.screenId&&<button onClick={()=>onScreen(row.screenId)}>이 화면 배치 편집</button>}</div>
        <Pick label="장소 배경" value={row.presentation?.backgroundAssetId??''} rows={images} empty="이전 배경 유지" onChange={id=>update({presentation:id?{...row.presentation,backgroundAssetId:id}:undefined})}/>
        <ConnectionPicker locations={definition.locations} currentId={row.id} connected={row.connections} onToggle={(id,checked)=>commit({...definition,locations:definition.locations.map(x=>x.id===row.id?{...x,connections:checked?[...new Set([...x.connections,id])]:x.connections.filter(other=>other!==id)}:x.id===id?{...x,connections:checked?[...new Set([...x.connections,row.id])]:x.connections.filter(other=>other!==row.id)}:x)})}/>
        {conditionEditor(row.requirements,requirements=>update({requirements}))}
      </>}
      {category==='items'&&<><Field label="아이템 설명" value={row.description??''} multiline onCommit={description=>update({description})}/><div className="gameplay-two"><NumberField label="최대 소지 개수" value={row.maxStack} min={1} max={999999} onCommit={maxStack=>update({maxStack})}/><NumberField label="시작 소지 개수" value={row.initialQuantity??0} max={row.maxStack} onCommit={initialQuantity=>update({initialQuantity})}/></div><Pick label="아이템 이미지" value={row.imageAssetId??''} rows={images} empty="없음" onChange={id=>update({imageAssetId:id||undefined})}/><p>판매 가격과 재고는 상점에서, 선물 효과는 교류·선물에서 설정합니다.</p></>}
      {category==='shops'&&<>
        <div className="gameplay-two"><Pick label="상점 장소" value={row.locationId} rows={definition.locations} onChange={locationId=>update({locationId})}/><Pick label="거래 자원" value={row.currencyAttributeId} rows={attributes} onChange={currencyAttributeId=>update({currencyAttributeId})}/></div>
        <h3>판매 상품</h3>{(row.offers as Row[]).map((offer,index)=><fieldset key={index} className="gameplay-offer"><legend>상품 {index+1}</legend><Pick label={`상품 ${index+1} 아이템`} value={offer.itemId} rows={definition.items} onChange={itemId=>update({offers:row.offers.map((o:Row,i:number)=>i===index?{...o,itemId}:o)})}/><div className="gameplay-two"><NumberField label={`상품 ${index+1} 가격`} value={offer.price} onCommit={price=>update({offers:row.offers.map((o:Row,i:number)=>i===index?{...o,price}:o)})}/><NumberField label={`상품 ${index+1} 재고`} value={offer.stock??0} onCommit={stock=>update({offers:row.offers.map((o:Row,i:number)=>i===index?{...o,stock}:o)})}/></div><label className="gameplay-check"><input aria-label={`상품 ${index+1} 무제한 재고`} type="checkbox" checked={offer.stock===null} onChange={e=>update({offers:row.offers.map((o:Row,i:number)=>i===index?{...o,stock:e.target.checked?null:10}:o)})}/>무제한 재고</label><button onClick={()=>update({offers:row.offers.filter((_:Row,i:number)=>i!==index)})}>상품 {index+1} 삭제</button></fieldset>)}
        <button disabled={!definition.items.some(item=>!row.offers.some((o:Row)=>o.itemId===item.id))} onClick={()=>{const item=definition.items.find(item=>!row.offers.some((o:Row)=>o.itemId===item.id));if(item)update({offers:[...row.offers,{itemId:item.id,price:10,stock:10}]});}}>판매 상품 추가</button>{!definition.items.length&&<p>먼저 아이템을 추가하세요.</p>}
      </>}
      {category==='interactions'&&<>
        <div className="gameplay-two"><Pick label="교류 장소" value={row.locationId} rows={definition.locations} onChange={locationId=>update({locationId})}/><Pick label="교류 인물" value={row.characterId??''} rows={characters} empty="특정 인물 없음" onChange={characterId=>update({characterId:characterId||undefined})}/></div>
        <div className="gameplay-two"><Pick label="선물할 아이템" value={row.itemId??''} rows={definition.items} empty="아이템 소비 없음 · 대화" onChange={itemId=>update(itemId?{itemId,quantity:1}:{itemId:undefined,quantity:undefined})}/>{row.itemId&&<NumberField label="선물 개수" value={row.quantity??1} min={1} max={999999} onCommit={quantity=>update({quantity})}/>}</div>
        <label className="gameplay-check"><input aria-label="기간당 한 번" type="checkbox" checked={row.oncePerPeriod??false} onChange={e=>update({oncePerPeriod:e.target.checked})}/>한 기간에 한 번만 실행</label>
        <h3>실행 비용</h3>{(row.costs??[]).map((cost:Row,index:number)=><div className="gameplay-effect" key={index}><Pick label={`비용 ${index+1} 자원`} value={cost.attributeId} rows={attributes} onChange={attributeId=>update({costs:row.costs.map((c:Row,i:number)=>i===index?{...c,attributeId}:c)})}/><NumberField label={`비용 ${index+1} 수량`} value={cost.amount} onCommit={amount=>update({costs:row.costs.map((c:Row,i:number)=>i===index?{...c,amount}:c)})}/><button onClick={()=>update({costs:row.costs.filter((_:Row,i:number)=>i!==index)})}>비용 삭제</button></div>)}<button disabled={!attributes.length} onClick={()=>update({costs:[...(row.costs??[]),{attributeId:attributes[0].id,amount:1}]})}>비용 추가</button>
        <h3>성장·관계 효과</h3>{(row.effects as Row[]).map((effect,index)=>effect.target&&effect.value?.op==='literal'?<div className="gameplay-effect" key={index}><Pick label={`효과 ${index+1} 대상`} value={targetKey(effect.target)} rows={targets} onChange={key=>update({effects:row.effects.map((e:Row,i:number)=>i===index?{...e,target:numericTarget(key)}:e)})}/><Pick label={`효과 ${index+1} 방식`} value={effect.op} rows={[{id:'add',name:'더하기 / 빼기'},{id:'set',name:'값 지정'}]} onChange={op=>update({effects:row.effects.map((e:Row,i:number)=>i===index?{...e,op}:e)})}/><NumberField label={`효과 ${index+1} 수치`} value={effect.value.value} min={-2147483648} onCommit={value=>update({effects:row.effects.map((e:Row,i:number)=>i===index?{...e,value:{op:'literal',value}}:e)})}/><button onClick={()=>update({effects:row.effects.filter((_:Row,i:number)=>i!==index)})}>효과 삭제</button></div>:<p key={index}>복합 효과 {index+1} · <button onClick={()=>onJump(project.file,'/gameplay')}>콘텐츠에서 편집</button></p>)}
        <button disabled={!targets.length} onClick={()=>{const target=targets.find(t=>t.id.startsWith(`relationship/${row.characterId}/`))??targets[0];if(target)update({effects:[...row.effects,{op:'add',target:numericTarget(target.id),value:{op:'literal',value:10}}]});}}>효과 추가</button>
        {conditionEditor(row.requirements,requirements=>update({requirements}))}
        <fieldset><legend>관계 사건 연결</legend><Pick label="교류 후 사건" value={row.eventId??''} rows={events} empty="사건 없음" onChange={eventId=>update({eventId:eventId||undefined})}/><p>선물과 효과를 적용한 뒤 사건 조건을 검사합니다. 조건이 맞으면 대본을 진행하고 같은 기간으로 돌아옵니다.</p><div className="gameplay-actions"><button disabled={!row.characterId||!characters.find(c=>c.id===row.characterId)?.relationships.length} onClick={()=>{setEventId(`${row.id.slice(0,55)}-event`);setEventLabel(`${row.name} 이후`);setEventText('선물 고마워요. 당신과 조금 더 가까워진 것 같아요.');setThreshold(10);setRelationship(characters.find(c=>c.id===row.characterId)?.relationships[0]?.id??'');setEventDialog(true);}}>관계 사건 만들기</button>{row.eventId&&<button onClick={()=>{const event=events.find(e=>e.id===row.eventId);if(event)onJump(event.file);}}>연결된 대본 편집</button>}</div></fieldset>
      </>}
    </>:<div className="gameplay-empty"><h2>{names[category]}을 추가하면 여기에 설정이 표시됩니다.</h2><p>왼쪽 추가 버튼으로 시작하세요. 변경 사항은 초안에 반영되며 실행 취소할 수 있습니다.</p></div>}</main>
    <aside className="gameplay-guide"><h3>제작 체크</h3><ul><li>장소에 화면을 연결합니다.</li><li>장소끼리 오갈 길을 정합니다.</li><li>상점에 아이템·가격·재고를 넣습니다.</li><li>선물에 아이템과 관계 효과를 연결합니다.</li><li>조건에 맞는 관계 사건을 작성합니다.</li></ul><h3>현재 프로젝트</h3><p>시작: {definition.locations.find(l=>l.id===definition.startLocationId)?.name}</p><p>사용자 화면 {screens.filter(s=>!['title','schedule','story','ending'].includes(s.id)).length}개 · 사건 {events.length}개</p><p>자동 검증 결과는 콘텐츠 탭에서도 확인할 수 있습니다. 모든 비용과 조건은 실제 플레이에서 다시 검사합니다.</p></aside></div>
    {adding&&<div className="modal-backdrop"><section className="studio-modal" role="dialog" aria-modal="true" aria-label="육성 콘텐츠 추가"><h2>{names[category]} 추가</h2><label>ID<input aria-label="육성 콘텐츠 ID" value={newId} onChange={e=>setNewId(e.target.value)}/></label><label>이름<input aria-label="육성 콘텐츠 이름" value={newName} onChange={e=>setNewName(e.target.value)}/></label>{category==='locations'&&<p>새 장소는 시작 장소와 양방향으로 연결됩니다. 생성 후 연결과 화면을 변경할 수 있습니다.</p>}{error&&<p role="alert">{error}</p>}<footer><button onClick={()=>setAdding(false)}>취소</button><button className="studio-primary" onClick={createRow}>육성 콘텐츠 만들기</button></footer></section></div>}
    {eventDialog&&row&&<div className="modal-backdrop"><section className="studio-modal" role="dialog" aria-modal="true" aria-label="관계 사건 만들기"><h2>선물에서 이어지는 관계 사건</h2><label>사건 ID<input aria-label="관계 사건 ID" value={eventId} onChange={e=>setEventId(e.target.value)}/></label><label>사건 이름<input aria-label="관계 사건 이름" value={eventLabel} onChange={e=>setEventLabel(e.target.value)}/></label><Pick label="사건 관계 수치" value={relationship} rows={characters.find(c=>c.id===row.characterId)?.relationships??[]} onChange={setRelationship}/><NumberField label="사건 관계 기준" value={threshold} min={-2147483648} onCommit={setThreshold}/><label>첫 대사<textarea aria-label="관계 사건 대사" value={eventText} onChange={e=>setEventText(e.target.value)}/></label>{error&&<p role="alert">{error}</p>}<footer><button onClick={()=>setEventDialog(false)}>취소</button><button className="studio-primary" onClick={()=>{if(!/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(eventId)||eventId.length>64||events.some(e=>e.id===eventId)||!eventLabel.trim()||!eventText.trim()){setError('사용하지 않은 사건 ID, 이름과 대사를 입력하세요.');return;}onCreateEvent({interactionId:row.id,id:eventId,label:eventLabel.trim(),text:eventText,characterId:row.characterId,relationshipId:relationship,threshold});setEventDialog(false);setError('');}}>사건 생성·연결</button></footer></section></div>}
  </section>;
}
