import React, { useState } from 'react';
import type { Element } from '../../../packages/visual/model.js';
import { widgetCommand, type WidgetCommand } from '../../../packages/visual/commands.js';
export { widgetCommand, type WidgetCommand } from '../../../packages/visual/commands.js';
import type { Confirmation, FacilityView } from '../../../packages/gameplay/model.js';

type Availability = { enabled: boolean; reason?: string | null };
export interface GameWidgetsView {
  location: { id: string; name: string } | null;
  locations: readonly ({ id: string; name: string } & Availability)[];
  shops: readonly { id: string; name: string; currencyAttributeId?: string; offers: readonly ({ itemId: string; name: string; price: number; stock: number | null; owned: number; maxQuantity?: number; confirmation?: Confirmation } & Availability)[];
    sellOffers?: readonly ({ itemId: string; name: string; price: number; owned: number; maxQuantity: number; confirmation?: Confirmation } & Availability)[] }[];
  diets?: readonly (FacilityView & { intervalPeriods: number; selected: boolean })[];
  services?: readonly (FacilityView & { maxQuantity: number })[];
  actions?: readonly (FacilityView & { maxQuantity: number })[];
  policyGroups?: readonly ({ id: string; name: string; trigger: { type: string; intervalPeriods?: number }; options: readonly (FacilityView & { actionId: string; selected: boolean; budgetUses?: readonly {budgetId:string;amount:number}[] })[] } & Availability)[];
  policyBudgets?: readonly {id:string;name:string;capacity:number;unit?:string;used:number}[];
  interactions: readonly ({ id: string; name: string; characterId?: string; itemId?: string } & Availability)[];
  inventory: readonly { id: string; name: string; quantity: number }[];
}
export interface RelationshipRow { id: string; characterId?: string; relationshipId?: string; name: string; label: string; value: number; min: number; max: number }

export function JourneyWidget({ element, gameplay, relationships, busy, editing, onCommand, items, assetUrl, stats=[], activeDietId }: {
  element: Element; gameplay?: GameWidgetsView | null | undefined; relationships?: readonly RelationshipRow[] | undefined;
  busy: boolean; editing: boolean; onCommand?: ((command: WidgetCommand) => void) | undefined;
  items?: readonly { id: string; name?: string; imageAssetId?: string; description?: string; maxStack?: number }[] | undefined;
  activeDietId?: string | null | undefined;
  stats?: readonly { id: string; label: string; value: number; min: number; max: number }[];
  assetUrl?: ((id: string) => string) | undefined;
}) {
  const [quantities,setQuantities]=useState<Record<string,number>>({});
  const quantity=(key:string,max=99)=>Math.min(max,Math.max(1,quantities[key]??1));
  const stepper=(key:string,max=99)=><div className="journey-quantity"><button aria-label={`${key} 수량 줄이기`} disabled={busy||quantity(key,max)<=1} onClick={()=>setQuantities(q=>({...q,[key]:quantity(key,max)-1}))}>−</button><span>수량 {quantity(key,max)}</span><button aria-label={`${key} 수량 늘리기`} disabled={busy||quantity(key,max)>=max} onClick={()=>setQuantities(q=>({...q,[key]:quantity(key,max)+1}))}>+</button></div>;
  const affordable=(costs:readonly {attributeId:string;amount:number}[],q:number)=>{const totals=new Map<string,number>();for(const c of costs)totals.set(c.attributeId,(totals.get(c.attributeId)??0)+c.amount*q);return [...totals].every(([id,amount])=>{const stat=stats.find(s=>s.id===id);return !stat||stat.value-amount>=stat.min;});};
  const costs=(rows:readonly {attributeId:string;amount:number}[],q=1)=>rows.map(c=>`${stats.find(s=>s.id===c.attributeId)?.label??c.attributeId} ${c.amount*q}`).join(' · ')||'무료';
  const effects=(rows:FacilityView['effects'],q=1)=>rows.map(e=>{
    if(e.op==='setFlag')return `${e.flagId} ${e.value?'켜짐':'꺼짐'}`;
    const target=e.target;
    const label=target.kind==='attribute'?stats.find(s=>s.id===target.attributeId)?.label??target.attributeId:target.kind==='inventory'?items?.find(i=>i.id===target.itemId)?.name??target.itemId:target.relationshipId;
    return `${label} ${e.op==='set'?'= ':''}${e.value.op==='literal'?`${e.op==='add'&&e.value.value>=0?'+':''}${e.value.value*(e.op==='add'?q:1)}`:'현재 상태 기준 계산'}`;
  }).join(' · ');
  const run = (command: WidgetCommand) => { if (!editing && !busy) onCommand?.(command); };
  const icon = (id: string) => { const item = items?.find(i => i.id === id); return item?.imageAssetId && assetUrl
    ? <img className="journey-item-icon" src={assetUrl(item.imageAssetId)} alt=""/> : null; };
  if (element.kind === 'relationships') return <div className="journey-widget" data-testid="relationships-widget">
    <h3>인물 관계</h3>{relationships?.length ? relationships.map(r => <div className="journey-relation" key={r.id}>
      <span>{r.name} · {r.label}</span><strong data-testid={`relationship-${r.id}`}>{r.value}</strong><progress aria-label={`${r.name} ${r.label}`} value={r.value - r.min} max={r.max - r.min || 1}/>
    </div>) : <p className="journey-empty">등록된 관계가 없습니다.</p>}
  </div>;
  if (!gameplay) return <div className="journey-widget"><p className="journey-empty">육성 작업실에서 장소와 행동을 설정하세요.</p></div>;
  if(element.kind==='actions')return <div className="journey-widget" data-testid="actions-widget">{gameplay.actions?.filter(row=>!element.actionIds?.length||element.actionIds.includes(row.id)).map(row=>{const key=`action__${element.id}__${row.id}`,q=quantity(key,row.maxQuantity),can=row.enabled&&affordable(row.costs,q);return <div className="journey-card" key={row.id}><strong>{row.name}</strong><small>비용 {costs(row.costs,q)}</small><small>{effects(row.effects,q)}</small>{stepper(key,row.maxQuantity)}<button data-testid={key} disabled={busy||!can} onClick={()=>run({type:'executeAction',actionId:row.id,quantity:q})}>{row.name}</button>{!can&&<small>{row.reason??'선택 수량의 비용이 부족합니다.'}</small>}</div>;})}</div>;
  if(element.kind==='policies')return <div className="journey-widget" data-testid="policies-widget">{gameplay.policyBudgets?.map(b=><small key={b.id}>{b.name}: {b.used} / {b.capacity} {b.unit??''}</small>)}{gameplay.policyGroups?.filter(group=>!element.groupIds?.length||element.groupIds.includes(group.id)).map(group=><section key={group.id}><h3>{group.name}</h3><small>{group.trigger.type==='afterActivity'?'활동 종료 후':'활동 시작 전'} · {group.trigger.intervalPeriods??1}기간마다</small>{group.options.map(option=><div className="journey-card" key={option.id}><strong>{option.name}{option.selected?' · 선택 중':''}</strong><small>{costs(option.costs)} · {effects(option.effects)}</small>{!!option.budgetUses?.length&&<small>{option.budgetUses.map(use=>{const b=gameplay.policyBudgets?.find(b=>b.id===use.budgetId);return `${b?.name??use.budgetId} ${use.amount} ${b?.unit??''}`;}).join(' · ')}</small>}<button data-testid={`policy__${element.id}__${group.id}__${option.id}`} disabled={busy||!option.enabled||option.selected} onClick={()=>run({type:'selectPolicy',groupId:group.id,optionId:option.id})}>{option.name}</button>{!option.enabled&&!option.selected&&<small>{option.reason}</small>}</div>)}<button data-testid={`policy_clear__${element.id}__${group.id}`} disabled={busy||!group.enabled} onClick={()=>run({type:'selectPolicy',groupId:group.id,optionId:null})}>{group.name} 해제</button><small>선택 시 비용 없이, 설정한 시점에 비용과 효과가 적용됩니다.</small></section>)}</div>;
  if(element.kind==='diets')return <div className="journey-widget" data-testid="diets-widget"><h3>식단</h3>{gameplay.diets?.map(row=><div className="journey-card" key={row.id}><strong>{row.name}{row.selected?' · 선택 중':''}</strong><small>{costs(row.costs)} · {row.intervalPeriods}기간마다</small><small>{effects(row.effects)}</small><button disabled={busy||!row.enabled||row.selected} onClick={()=>run({type:'setDiet',dietId:row.id})}>식단 선택</button>{!row.enabled&&<small>{row.reason}</small>}</div>)}<button disabled={busy||!activeDietId} onClick={()=>run({type:'setDiet',dietId:null})}>식단 해제</button><small>선택 시 비용 없이, 활동 시작 시 주기에 따라 비용과 효과가 적용됩니다.</small></div>;
  if(element.kind==='services')return <div className="journey-widget" data-testid="services-widget"><h3>서비스</h3>{gameplay.services?.map(row=>{const q=quantity(row.id,row.maxQuantity),can=row.enabled&&affordable(row.costs,q);return <div className="journey-card" key={row.id}><strong>{row.name}</strong><small>비용 {costs(row.costs,q)}</small><small>{effects(row.effects,q)}</small>{stepper(row.id,row.maxQuantity)}<button disabled={busy||!can} onClick={()=>run({type:'useService',serviceId:row.id,quantity:q})}>이용</button>{!can&&<small>{row.reason??'선택 수량의 비용이 부족합니다.'}</small>}</div>;})}</div>;
  if(element.kind==='sell')return <div className="journey-widget" data-testid="sell-widget"><h3>판매</h3>{gameplay.shops.filter(shop=>!element.shopId||shop.id===element.shopId).map(shop=><section key={shop.id}>{shop.sellOffers?.map(row=>{const key=`${shop.id}-${row.itemId}`,q=quantity(key,row.maxQuantity),currency=stats.find(s=>s.id===(shop.currencyAttributeId??'money')),can=row.enabled&&row.owned>=q&&(!currency||currency.value+row.price*q<=currency.max);return <div className="journey-card" key={row.itemId}><strong>{row.name}</strong><small>받을 금액 {row.price*q} · 소지 {row.owned}</small>{stepper(key,row.maxQuantity)}<button disabled={busy||!can} onClick={()=>run({type:'sell',shopId:shop.id,itemId:row.itemId,quantity:q})}>판매</button>{!can&&<small>{row.reason??'소지 수량과 받을 금액의 한도를 확인해 주세요.'}</small>}</div>;})}</section>)}</div>;
  if (element.kind === 'locations') return <div className="journey-widget" data-testid="locations-widget"><h3>어디로 갈까요?</h3>
    {gameplay.locations.map(location => <div className="journey-card" key={location.id}>
      <button disabled={busy || !location.enabled} onClick={() => run({ type: 'visit', locationId: location.id })}>{location.name}{location.id === gameplay.location?.id ? ' · 현재 장소' : ' 방문'}</button>
      {!location.enabled && location.reason && <small>{location.reason}</small>}
    </div>)}{!gameplay.locations.length && <p className="journey-empty">연결된 장소가 없습니다.</p>}
  </div>;
  if (element.kind === 'shop') {
    const shops = gameplay.shops.filter(shop => !element.shopId || shop.id === element.shopId);
    return <div className="journey-widget" data-testid="shop-widget">{shops.map(shop => <section key={shop.id}><h3>{shop.name}</h3>
      {shop.offers.map(offer => {const key=`${shop.id}-${offer.itemId}`,max=offer.maxQuantity??99,q=quantity(key,max),can=offer.enabled&&(offer.stock===null||offer.stock>=q)&&affordable([{attributeId:shop.currencyAttributeId??'money',amount:offer.price}],q)&&offer.owned+q<=(items?.find(i=>i.id===offer.itemId)?.maxStack??Number.MAX_SAFE_INTEGER);return <div className="journey-card journey-offer" key={offer.itemId}>
        <div className="journey-item-info">{icon(offer.itemId)}<div><strong>{offer.name}</strong><small>가격 {offer.price} · 재고 {offer.stock ?? '무제한'} · 소지 {offer.owned}</small></div></div>
        {stepper(key,max)}<small>합계 {offer.price*q}</small><button aria-label={`${offer.name} 구매`} disabled={busy || !can} onClick={() => run({ type: 'buy', shopId: shop.id, itemId: offer.itemId, quantity: q })}>구매</button>
        {!can && <small className="journey-reason">{offer.reason??'선택 수량의 비용·재고·소지 한도를 확인해 주세요.'}</small>}
      </div>;})}
    </section>)}{!shops.length && <p className="journey-empty">이 장소에서 이용할 상점이 없습니다.</p>}</div>;
  }
  if (element.kind === 'inventory') return <div className="journey-widget" data-testid="inventory-widget"><h3>소지품</h3>
    {gameplay.inventory.length ? gameplay.inventory.map(item => <div className="journey-inventory-row" key={item.id}><span className="journey-item-info">{icon(item.id)}{item.name}</span><strong data-testid={`inventory-${item.id}`}>{item.quantity}개</strong></div>) : <p className="journey-empty">아직 소지품이 없습니다.</p>}
  </div>;
  if (element.kind === 'interactions') {
    const rows = gameplay.interactions.filter(row => !element.interactionId || row.id === element.interactionId);
    return <div className="journey-widget" data-testid="interactions-widget"><h3>대화와 선물</h3>{rows.map(row => <div className="journey-card" key={row.id}>
      <button disabled={busy || !row.enabled} onClick={() => run({ type: 'interact', interactionId: row.id })}>{row.name}</button>
      {!row.enabled && row.reason && <small>{row.reason}</small>}
    </div>)}{!rows.length && <p className="journey-empty">이 장소에서 할 수 있는 교류가 없습니다.</p>}</div>;
  }
  return null;
}

