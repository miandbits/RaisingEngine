import React from 'react';
import type { Element, VisualDesign } from '../../../packages/visual/model.js';
import type { GameplayDefinition } from '../../../packages/gameplay/model.js';

export function ActionFields({element,design,gameplay,onChange}:{element:Element;design:VisualDesign;gameplay?:GameplayDefinition;onChange:(value:Partial<Element>)=>void}) {
  const list=(field:'screenId'|'locationId'|'shopId'|'itemId'|'interactionId',label:string,rows:readonly {id:string;name?:string;label?:string}[],optional=false)=><label>{label}<select aria-label={label} value={element[field]??''} onChange={e=>onChange({[field]:e.target.value||undefined})}><option value="">{optional?'현재 장소의 전체 목록':'연결 대상 선택'}</option>{rows.map(row=><option key={row.id} value={row.id}>{row.name??row.label??row.id} · {row.id}</option>)}</select></label>;
  return <>
    {element.action==='navigate'&&list('screenId','이동할 화면',design.screens.filter(s=>!['title','story','ending'].includes(s.id)))}
    {element.action==='visit'&&list('locationId','방문할 장소',gameplay?.locations??[])}
    {(element.action==='buy'||element.kind==='shop')&&list('shopId','연결할 상점',gameplay?.shops??[],element.kind==='shop')}
    {element.action==='buy'&&list('itemId','구매할 아이템',(gameplay?.items??[]).filter(item=>gameplay?.shops.find(shop=>shop.id===element.shopId)?.offers.some(offer=>offer.itemId===item.id)))}
    {element.action==='buy'&&<label>구매 수량<input aria-label="구매 수량" type="number" min={1} max={999} value={element.quantity??1} onChange={e=>onChange({quantity:Math.max(1,Math.min(999,Number(e.target.value)||1))})}/></label>}
    {(element.action==='interact'||element.kind==='interactions')&&list('interactionId','연결할 교류',gameplay?.interactions??[],element.kind==='interactions')}
    {['navigate','visit','buy','interact'].includes(element.action??'')&&<p>실제 플레이에서 장소·비용·소지품·조건을 확인한 뒤 실행합니다. 화면 이동은 시간이나 장소를 바꾸지 않습니다.</p>}
  </>;
}
