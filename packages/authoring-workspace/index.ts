/** Draft-level authoring index: references remain usable even when validation fails.
 * These are static dependencies, never a claim that a gameplay route is reachable. */
type Row = Record<string, any>;
export interface WorkspaceNode { key:string; kind:string; id:string; label:string; file:string; path:string; value:Row }
export interface WorkspaceEdge { from:string; to:string; file:string; path:string; relation:string; resolved:boolean }
const object=(v:unknown):v is Row=>!!v&&typeof v==='object'&&!Array.isArray(v);
// Preserve document offsets even in invalid drafts: filtering null entries would
// make an editor apply a later node to the wrong JSON array position.
const array=(v:unknown):Row[]=>{const rows:Row[]=[];if(Array.isArray(v))v.forEach((x,i)=>{if(object(x))rows[i]=x;});return rows;};
const escape=(s:string)=>s.replace(/~/g,'~0').replace(/\//g,'~1');
export function authoringWorkspace(documents:readonly {file:string;kind:string;data:unknown}[]) {
  const nodes:WorkspaceNode[]=[],edges:WorkspaceEdge[]=[];
  const people=new Map(documents.filter(d=>d.kind==='character'&&object(d.data)).map(d=>[String((d.data as Row).id),String((d.data as Row).name??(d.data as Row).id)]));
  const add=(kind:string,id:unknown,value:unknown,file:string,path:string,label?:string)=>{if(typeof id!=='string'||!object(value))return;nodes.push({key:`${kind}:${id}`,kind,id,label:label??String(value.name??value.label??id),file,path,value:structuredClone(value)});};
  for(const doc of documents){if(!object(doc.data))continue;const d=doc.data;
    if(doc.kind==='project'){
      for(const [field,kind] of [['actions','action'],['interactions','interaction'],['rules','rule'],['items','item'],['locations','location'],['policyGroups','policy'],['policyBudgets','budget'],['shops','shop']])
        array(d.gameplay?.[field!]).forEach((v:Row,i:number)=>add(kind!,v.id,v,doc.file,`/gameplay/${field}/${i}`));
      if(d.progression){add('clock','clock',{unitLabel:d.progression.unitLabel,actionPoints:d.progression.actionPoints},doc.file,'/progression');array(d.progression.actions).forEach((v:Row,i:number)=>add('timing',v.actionId,v,doc.file,`/progression/actions/${i}`));array(d.progression.appointments).forEach((v:Row,i:number)=>add('appointment',v.id,v,doc.file,`/progression/appointments/${i}`));}
      array(d.roster?.members).forEach((v:Row,i:number)=>add('actor',v.characterId,v,doc.file,`/roster/members/${i}`,people.get(v.characterId)));
      array(d.userFields).forEach((v:Row,i:number)=>add('userField',v.id,v,doc.file,`/userFields/${i}`));
      array(d.stateGroups).forEach((g:Row,i:number)=>{
        add('stateGroup',g.id,g,doc.file,`/stateGroups/${i}`);
        array(g.values).forEach((v:Row,j:number)=>add('stateValue',`${g.id}/${v.id}`,v,doc.file,`/stateGroups/${i}/values/${j}`,`${g.name} · ${v.name}`));
      });
      array(d.visual?.screens).forEach((s:Row,i:number)=>{
        add('screen',s.id,s,doc.file,`/visual/screens/${i}`);
        array(s.elements).forEach((e:Row,j:number)=>{
          const id=`${s.id}/${e.id}`,path=`/visual/screens/${i}/elements/${j}`,label=`${s.label??s.id} · ${e.text??e.label??e.kind}`;
          add('screenElement',id,e,doc.file,path,label);
          array(e.variants).forEach((v:Row,k:number)=>add('presentation',`${id}/${k}`,v,doc.file,`${path}/variants/${k}`,`${label} · ${v.label}`));
        });
      });
      if(object(d.visual?.routing)){
        add('screenRouting','routing',d.visual.routing,doc.file,'/visual/routing','기본·진행 단계 연결');
        array(d.visual.routing.conditions).forEach((r:Row,i:number)=>add('screenRoute',String(i),r,doc.file,`/visual/routing/conditions/${i}`));
      }
    }else if(['attributes','flags'].includes(doc.kind)) array(d[doc.kind]).forEach((v:Row,i:number)=>add(doc.kind==='attributes'?'attribute':'flag',v.id,v,doc.file,`/${doc.kind}/${i}`));
    else if(typeof d.id==='string'){
      add(doc.kind,d.id,d,doc.file,'');
      if(doc.kind==='character')array(d.relationships).forEach((v:Row,i:number)=>add('relationship',`${d.id}/${v.id}`,v,doc.file,`/relationships/${i}`));
      if(doc.kind==='event'||doc.kind==='ending')array(d.nodes).forEach((v:Row,i:number)=>{
        if(typeof v.id!=='string')return;
        const id=`${doc.kind}/${d.id}/${v.id}`;
        add('scriptNode',id,v,doc.file,`/nodes/${i}`,`${d.label??d.id} · ${i+1}. ${v.text??v.prompt??v.type}`);
        array(v.choices).forEach((c:Row,j:number)=>add('scriptChoice',`${id}/${c.id}`,c,doc.file,`/nodes/${i}/choices/${j}`,`${d.label??d.id} · ${c.text??c.id}`));
      });
    }
  }
  const count=new Map<string,number>();for(const n of nodes)count.set(n.key,(count.get(n.key)??0)+1);
  const known=new Set([...count].filter(([,v])=>v===1).map(([k])=>k));
  const references:Record<string,string>={interactionId:'interaction',userFieldId:'userField',speakerId:'character',stateGroupId:'stateGroup',defaultScreenId:'screen',actionId:'action',thenActionId:'action',elseActionId:'action',actionIds:'action',eventId:'event',activityId:'activity',activityIds:'activity',attributeId:'attribute',currencyAttributeId:'attribute',flagId:'flag',itemId:'item',characterId:'character',actorId:'actor',locationId:'location',locationIds:'location',shopId:'shop',groupId:'policy',groupIds:'policy',budgetId:'budget',screenId:'screen'};
  const link=(n:WorkspaceNode,to:string,path:string,relation:string)=>edges.push({from:n.key,to,file:n.file,path,relation,resolved:known.has(to)});
  const walk=(n:WorkspaceNode,v:unknown,path:string)=>{
    if(Array.isArray(v)){v.forEach((x,i)=>walk(n,x,`${path}/${i}`));return;}if(!object(v))return;
    for(const [field,child] of Object.entries(v)){
      const here=`${path}/${escape(field)}`,relation=/requirements|condition|\/when(?:\/|$)/i.test(here)?'condition':/costs/i.test(here)?'cost':/effects/i.test(here)?'effect':/flow/.test(here)?'branch':n.kind==='appointment'?'reservation':n.kind==='rule'?'trigger':n.kind==='timing'?'timing':['screen','screenElement','screenRoute','screenRouting','presentation'].includes(n.kind)?'ui':'reference';
      if(references[field])for(const [i,id] of (Array.isArray(child)?child:[child]).entries())if(typeof id==='string')link(n,`${references[field]}:${id}`,Array.isArray(child)?`${here}/${i}`:here,relation);
      if(field==='relationshipId'&&typeof child==='string'&&typeof v.characterId==='string')link(n,`relationship:${v.characterId}/${child}`,here,relation);
      if(field==='stateValueId'&&typeof child==='string'&&typeof v.stateGroupId==='string')link(n,`stateValue:${v.stateGroupId}/${child}`,here,relation);
      if(field==='initialValueId'&&n.kind==='stateGroup'&&typeof child==='string')link(n,`stateValue:${n.id}/${child}`,here,'initial');
      if(['initialInventory','initialAttributes'].includes(field)&&object(child))for(const id of Object.keys(child))link(n,`${field==='initialInventory'?'item':'attribute'}:${id}`,`${here}/${escape(id)}`,'initial');
      if(field==='initialRelationships'&&object(child))for(const [other,values] of Object.entries(child))if(object(values))for(const id of Object.keys(values))link(n,`relationship:${other}/${id}`,`${here}/${escape(other)}/${escape(id)}`,'initial');
      walk(n,child,here);
    }
  };
  const duplicates=[...count].filter(([,v])=>v>1).map(([key])=>key);
  // Duplicate definitions are shown separately; ambiguous links never silently jump.
  for(const n of nodes)if(duplicates.includes(n.key))n.key+=`@${n.file}${n.path}`;
  for(const n of nodes)walk(n,n.value,n.path);
  // Explicit script ownership and local links use qualified IDs: repeated node or
  // choice names in different events must never navigate to another document.
  for(const n of nodes){
    if(n.kind==='character'&&known.has(`actor:${n.id}`))link(n,`actor:${n.id}`,n.path,'assignment');
    if(n.kind==='actor'&&typeof n.value.activityId==='string')link(n,`activity:${n.value.activityId}`,`${n.path}/activityId`,'assignment');
    if(n.kind==='event'||n.kind==='ending'){
      array(n.value.nodes).forEach((v:Row,i:number)=>link(n,`scriptNode:${n.kind}/${n.id}/${v.id}`,`/nodes/${i}`,'script'));
    }
    if(n.kind==='scriptNode'||n.kind==='scriptChoice'){
      const parts=n.id.split('/'),prefix=parts.slice(0,2).join('/');
      for(const field of ['next','then','else'])if(typeof n.value[field]==='string')link(n,`scriptNode:${prefix}/${n.value[field]}`,`${n.path}/${field}`,'script');
      if(n.kind==='scriptNode')array(n.value.choices).forEach((v:Row,i:number)=>link(n,`scriptChoice:${n.id}/${v.id}`,`${n.path}/choices/${i}`,'choice'));
    }
  }
  // A dependency is a candidate, not proof of reachability (NOT/OR, actor scope,
  // dates and runtime conditions still apply). Keep the original condition path.
  for(const e of [...edges])if(e.from.startsWith('event:')&&e.to.startsWith('event:')&&e.relation==='condition'&&e.resolved){
    edges.push({from:e.to,to:e.from,file:e.file,path:e.path,relation:'followup',resolved:known.has(e.from)});
  }
  return {nodes,edges,duplicates,staticOnly:true};
}
export function workspaceImpact(index:ReturnType<typeof authoringWorkspace>,key:string,depth=4){
  if(!index.nodes.some(n=>n.key===key))throw new Error('선택한 제작 항목이 없습니다.');
  if(!Number.isInteger(depth)||depth<1||depth>8)throw new Error('영향 깊이는 1..8입니다.');
  const paths:(WorkspaceEdge&{direction:'incoming'|'outgoing';depth:number})[]=[];
  for(const direction of ['incoming','outgoing'] as const){let frontier=new Set([key]);const seen=new Set(frontier);
    for(let level=1;level<=depth&&frontier.size;level++){const next=new Set<string>();for(const e of index.edges){const origin=direction==='incoming'?e.to:e.from,target=direction==='incoming'?e.from:e.to;if(!frontier.has(origin))continue;paths.push({...e,direction,depth:level});if(e.resolved&&!seen.has(target)){seen.add(target);next.add(target);}}frontier=next;}
  }
  return {key,paths,staticOnly:true};
}
