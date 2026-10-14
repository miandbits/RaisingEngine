// Authoring macro: produces ordinary editable attributes/actions, no game-specific runtime.
export const literal=value=>({op:'literal',value});
export const read=id=>({op:'read',target:{kind:'attribute',attributeId:id}});
export const compare=(id,comparator,value)=>({op:'compare',comparator,left:read(id),right:typeof value==='number'?literal(value):value});
export const all=(...conditions)=>({op:'all',conditions});
export const effect=(id,value,op='set')=>({op,target:{kind:'attribute',attributeId:id},value:typeof value==='number'?literal(value):value});
export const action=(id,name,effects=[],extra={})=>({id,name,costs:[],effects,confirmation:{enabled:false},...extra});

/** Ordered bounded queue compiled to the engine's existing transaction/flow contract.
 * Helpers are guarded by a latch that cannot survive a successful public command.
 * Completion is explicit; it never advances the calendar implicitly.
 */
export function actionQueue({id,capacity,options,editable,onComplete=[]}) {
  if(!/^[a-z][a-z0-9-]*$/.test(id)||!Number.isInteger(capacity)||capacity<1||capacity>12||!options.length||new Set(options.map(x=>x.id)).size!==options.length)throw new Error('Invalid queue definition');
  const key=n=>`${id}-${n}`,eq=(n,v)=>compare(key(n),'eq',v);
  const attributes=[],actions=[];
  const attr=(n,max)=>attributes.push({id:key(n),label:`${id} · ${n}`,kind:'stat',initial:0,min:0,max});
  attr('count',capacity);attr('cursor',capacity);attr('running',1);attr('latch',1);attr('current',options.length);
  for(let i=1;i<=capacity;i++)attr(`slot-${i}`,options.length);
  options.forEach(o=>attr(`selected-${o.id}`,capacity));
  const guard=all(eq('latch',1),eq('running',1));
  options.forEach((o,index)=>{
    for(let i=1;i<=capacity;i++)actions.push(action(key(`put-${o.id}-${i}`),'Queue slot',[effect(key(`slot-${i}`),index+1)],{requirements:eq('latch',1)}));
    actions.push(action(key(`add-${o.id}`),`Add ${o.name}`,[effect(key('latch'),1),effect(key('count'),1,'add'),effect(key(`selected-${o.id}`),1,'add')],{
      requirements:all(editable,eq('running',0),compare(key('count'),'lt',capacity),compare(key(`selected-${o.id}`),'lt',o.limit??capacity)),
      flow:[...Array.from({length:capacity},(_,i)=>({id:`slot-${i+1}`,condition:eq('count',i+1),thenActionId:key(`put-${o.id}-${i+1}`)})),{id:'unlock',thenActionId:key('unlock')}]
    }));
    actions.push(action(key(`run-${o.id}`),o.name,[effect(key('current'),index+1),...o.effects],{requirements:guard,...o.flow?{flow:o.flow}:{}}));
  });
  const steps=[];
  for(let i=1;i<=capacity;i++)for(let o=0;o<options.length;o++)steps.push({id:`slot-${i}-${o+1}`,condition:all(eq('cursor',i),eq(`slot-${i}`,o+1)),thenActionId:key(`run-${options[o].id}`)});
  actions.push(action(key('unlock'),'Finish transaction',[effect(key('latch'),0)],{requirements:eq('latch',1)}));
  actions.push(action(key('start'),'Start queue',[effect(key('running'),1),effect(key('cursor'),1),effect(key('latch'),1)],{requirements:all(editable,eq('running',0),eq('count',capacity)),flow:[...steps,{id:'unlock',thenActionId:key('unlock')}]}));
  actions.push(action(key('next'),'Next result',[effect(key('cursor'),1,'add'),effect(key('latch'),1)],{requirements:all(eq('running',1),compare(key('cursor'),'lt',capacity)),flow:[...steps,{id:'unlock',thenActionId:key('unlock')}]}));
  actions.push(action(key('finish'),'Finish queue',[effect(key('running'),0),...onComplete],{requirements:all(eq('running',1),eq('cursor',capacity))}));
  const clear=[effect(key('count'),0),effect(key('cursor'),0),effect(key('current'),0),...Array.from({length:capacity},(_,i)=>effect(key(`slot-${i+1}`),0)),...options.map(o=>effect(key(`selected-${o.id}`),0))];
  actions.push(action(key('clear'),'Clear selection',clear,{requirements:all(editable,eq('running',0))}));
  // Undo last choice. Decrement its count before clearing its slot; count changes last.
  for(const o of options)actions.push(action(key(`undo-${o.id}`),'Remove selection',[effect(key(`selected-${o.id}`),-1,'add')],{requirements:eq('latch',1)}));
  for(let i=1;i<=capacity;i++)actions.push(action(key(`clear-slot-${i}`),'Clear slot',[effect(key(`slot-${i}`),0)],{requirements:eq('latch',1)}));
  actions.push(action(key('pop'),'Remove last',[effect(key('latch'),1)],{requirements:all(editable,eq('running',0),compare(key('count'),'gt',0)),flow:[
    ...Array.from({length:capacity},(_,i)=>options.map((o,j)=>({id:`undo-${i+1}-${j+1}`,condition:all(eq('count',i+1),eq(`slot-${i+1}`,j+1)),thenActionId:key(`undo-${o.id}`)}))).flat(),
    ...Array.from({length:capacity},(_,i)=>({id:`clear-${i+1}`,condition:eq('count',i+1),thenActionId:key(`clear-slot-${i+1}`)})),{id:'decrement',thenActionId:key('decrement')},{id:'unlock',thenActionId:key('unlock')}
  ]}));
  actions.push(action(key('decrement'),'Update queue size',[effect(key('count'),-1,'add')],{requirements:eq('latch',1)}));
  return {attributes,actions,reset:[...clear,effect(key('running'),0),effect(key('latch'),0)],key};
}
