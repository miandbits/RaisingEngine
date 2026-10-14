import type {Transition,ContentSnapshot} from '../core/model.js';
import type {Trace} from './model.js';
export interface RoutePoint {nodeId:string;label:string;kind:string;step:number;periodIndex:number;chance:number|null;roll:number|null;passed:boolean|null}
export interface RouteNode {id:string;label:string;kind:string;periodIndex:number;depth:number;chanceMin:number|null;chanceMax:number|null;chanceSum:number;chanceCount:number;visits:number;runs:number;sampleRuns:number[]}
export interface RouteEdge {id:string;from:string;to:string;visits:number;runs:number;sampleRuns:number[]}
export interface RouteGraph {nodes:RouteNode[];edges:RouteEdge[];runCount:number}
export interface RunSummary {traceHash?:string;attributes:Record<string,number>;run:number;seed:number;outcome:string;reason:string;endingId:string|null;commandCount:number;periodIndex:number;nodeIds:string[];edgeIds:string[]}
export const routeEdgeId=(from:string,to:string)=>JSON.stringify([from,to]);
const graphIndexes=new WeakMap<RouteGraph,{nodes:Map<string,RouteNode>;edges:Map<string,RouteEdge>}>();
export function transitionRoute(t:Readonly<Transition>,step:number,content?:ContentSnapshot):RoutePoint[]{
  const points:RoutePoint[]=[];
  const add=(kind:string,id:string,label=id,success:Transition['activity']=null)=>points.push({nodeId:JSON.stringify([kind,t.time.periodIndex,id,success?.success?.passed??null]),label,kind,step,periodIndex:t.time.periodIndex,chance:success?.success?.chance??null,roll:success?.success?.roll??null,passed:success?.success?.passed??null});
  if(t.activity)add('activity',t.activity.activityId,content?.activities.find(a=>a.id===t.activity!.activityId)?.label??t.activity.activityId,t.activity);
  const command=t.command;
  if(command.type==='visit')add('location',command.locationId,content?.project.gameplay?.locations.find(x=>x.id===command.locationId)?.name??command.locationId);
  if(command.type==='moveExpedition')add('expedition',command.nodeId);
  if(command.type==='battleAction')add('battle',command.action==='useItem'?`useItem/${command.itemId}`:command.action);
  if(command.type==='executeAction')add('action',command.actionId);
  if(command.type==='selectPolicy')add('policy',`${command.groupId}/${command.optionId??'none'}`);
  if(command.type==='useService')add('service',command.serviceId);
  if(command.type==='setDiet')add('diet',command.dietId??'none');
  if(command.type==='acceptQuest'||command.type==='claimQuest')add('quest',`${command.type}/${command.questId}`);
  if(command.type==='equip')add('equipment',command.equipmentId);
  if(command.type==='interact')add('interaction',command.interactionId);
  if(command.type==='buy'||command.type==='sell')add(command.type,`${command.shopId}/${command.itemId}`);
  for(const event of t.eventDecisions)if(event.reason==='selected')add('event',event.eventId,content?.events.find(x=>x.id===event.eventId)?.label??event.eventId);
  if(t.narrative){const n=t.narrative;add(n.choiceId?'choice':'script',`${n.cursor.kind}/${n.cursor.id}/${n.cursor.nodeId}${n.choiceId?`/${n.choiceId}`:''}`);}
  if(t.endingSelected)add('ending',t.endingSelected,content?.endings.find(x=>x.id===t.endingSelected)?.label??t.endingSelected);
  return points;
}
export function summarizeRun(trace:Trace):RunSummary {
  const route=trace.route??[],nodeIds=[...new Set(route.map(x=>x.nodeId))],edgeIds=[...new Set(route.slice(1).map((x,i)=>routeEdgeId(route[i]!.nodeId,x.nodeId)))];
  return {attributes:{...trace.state.attributes},run:trace.run,seed:trace.seed,outcome:trace.outcome,reason:trace.reason,endingId:trace.state.endingId,commandCount:trace.commands.length,periodIndex:trace.state.periodIndex,nodeIds,edgeIds};
}
export function addRoute(graph:RouteGraph,trace:Trace):void {
  let indexes=graphIndexes.get(graph);if(!indexes){indexes={nodes:new Map(graph.nodes.map(x=>[x.id,x])),edges:new Map(graph.edges.map(x=>[x.id,x]))};graphIndexes.set(graph,indexes);}
  const {nodes,edges}=indexes;const seenNodes=new Set<string>(),seenEdges=new Set<string>(),route=trace.route??[];
  for(let i=0;i<route.length;i++){
    const point=route[i]!;let node=nodes.get(point.nodeId);
    if(!node){node={id:point.nodeId,label:point.label,kind:point.kind,periodIndex:point.periodIndex,depth:point.step,chanceMin:null,chanceMax:null,chanceSum:0,chanceCount:0,visits:0,runs:0,sampleRuns:[]};nodes.set(node.id,node);graph.nodes.push(node);}
    node.depth=Math.min(node.depth,point.step);if(point.chance!==null){node.chanceMin=Math.min(node.chanceMin??point.chance,point.chance);node.chanceMax=Math.max(node.chanceMax??point.chance,point.chance);node.chanceSum+=point.chance;node.chanceCount++;}
    node.visits++;if(!seenNodes.has(node.id)){node.runs++;if(node.sampleRuns.length<20)node.sampleRuns.push(trace.run);seenNodes.add(node.id);}
    if(i){const from=route[i-1]!.nodeId,id=routeEdgeId(from,point.nodeId);let edge=edges.get(id);if(!edge){edge={id,from,to:point.nodeId,visits:0,runs:0,sampleRuns:[]};edges.set(id,edge);graph.edges.push(edge);}edge.visits++;if(!seenEdges.has(id)){edge.runs++;if(edge.sampleRuns.length<20)edge.sampleRuns.push(trace.run);seenEdges.add(id);}}
  }
  graph.runCount++;
}
