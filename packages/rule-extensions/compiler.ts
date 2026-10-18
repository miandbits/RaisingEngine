import { childPointer, diagnostic, isObject, type Diagnostic, type Json, type JsonObject, type LoadedProject, type ProjectDocument } from '../validation/model.js';

/** Portable rule packs compile to the existing deterministic runtime vocabulary.
 * No evaluation, file access or host callbacks occur here. Originals are never mutated. */
export const RULE_API = 1;
export const RULE_COMPILER = 'raising-rules-1';
const idPattern = /^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/;
const versionPattern = /^\d+\.\d+\.\d+$/;
const forbidden = new Set(['__proto__', 'constructor', 'prototype']);
type Origin = { doc: ProjectDocument; pointer: string; pinned?: boolean };
type Definition = { pack: JsonObject; value: JsonObject; origin: Origin; kind: string };
export interface RuleCompilation {
  documents: ProjectDocument[];
  diagnostics: Diagnostic[];
  lock?: JsonObject;
  remap: (item: Diagnostic) => Diagnostic;
}
class RuleError extends Error {
  constructor(readonly origin: Origin, readonly code: string, message: string) { super(message); }
}
export function compileRules(project: LoadedProject): RuleCompilation {
  const diagnostics: Diagnostic[] = [];
  const origins = new Map<string, Origin>();
  const generatedOrigins = new Map<string, Origin>();
  const source = project.documents.find(d => d.kind === 'project');
  const raw = source && isObject(source.data) ? source.data.ruleExtensions : undefined;
  const result: RuleCompilation = { documents: project.documents, diagnostics, remap(item) {
    let pointer = item.pointer;
    while (true) {
      const origin = origins.get(`${item.file}:${pointer}`);
      if (origin) return { ...diagnostic(origin.doc, origin.pointer, item.code, item.message, item.relatedIds, item.severity),
        relatedLocations: [{ file: item.file, pointer: item.pointer, line: item.line, column: item.column }] };
      if (!pointer) return item;
      pointer = pointer.slice(0, pointer.lastIndexOf('/'));
    }
  } };
  // Existing projects retain their exact validation and snapshot path.
  if (raw === undefined && !project.documents.some(d => d.kind !== 'extension' && /"op":"rule(?:Condition|Effect)"/.test(JSON.stringify(d.data)))) return result;
  const fail = (origin: Origin, code: string, message: string): never => { throw new RuleError(origin, code, message); };
  const object = (value: Json | undefined, origin: Origin): JsonObject => isObject(value) ? value : fail(origin, 'RULE_SHAPE', '객체가 필요합니다.');
  const array = (value: Json | undefined, origin: Origin, max = 128): Json[] => Array.isArray(value) && value.length <= max ? value : fail(origin, 'RULE_SHAPE', `목록은 ${max}개 이하여야 합니다.`);
  const at = (origin: Origin, key: string | number): Origin => origin.pinned ? origin : ({ doc: origin.doc, pointer: childPointer(origin.pointer, key) });
  const keys = (value: JsonObject, allowed: string[], origin: Origin) => {
    for (const key of Object.keys(value)) if (!allowed.includes(key) || forbidden.has(key)) fail(at(origin,key), 'RULE_FIELD', `지원하지 않는 필드: ${key}`);
  };
  const identifier = (value: Json | undefined, origin: Origin): string => typeof value === 'string' && value.length <= 48 && idPattern.test(value) ? value : fail(origin, 'RULE_ID', 'ID는 소문자·숫자·하이픈으로 된 48자 이하 이름이어야 합니다.');
  const text = (value: Json | undefined, origin: Origin): string => typeof value === 'string' && value.length > 0 && value.length <= 200 ? value : fail(origin, 'RULE_NAME', '이름이 필요합니다 (최대 200자).');
  const definitions = new Map<string, Definition>();
  const packs = new Map<string, JsonObject>();
  let nodes = 0;
  const budget = (origin: Origin, depth: number) => { if (++nodes > 100000 || depth > 32) fail(origin,'RULE_LIMIT','규칙 확장 한도(100000 노드 / 깊이 32)를 초과했습니다.'); };
  const parameters = (definition: Definition, supplied: Json | undefined, origin: Origin): JsonObject => {
    const args = supplied === undefined ? {} : object(supplied, at(origin,'args'));
    const output: JsonObject = {};
    const params = definition.value.parameters as JsonObject ?? {};
    for (const key of Object.keys(args)) if (!Object.hasOwn(params,key)) fail(at(at(origin,'args'),key),'RULE_ARGUMENT',`알 수 없는 매개변수: ${key}`);
    for (const [key, rawParam] of Object.entries(params)) {
      const param = rawParam as JsonObject;
      const value = Object.hasOwn(args,key) ? args[key] : param.default;
      if (value === undefined) fail(at(at(origin,'args'),key),'RULE_ARGUMENT',`필수 매개변수: ${param.name ?? key}`);
      const valid = param.type === 'integer' ? typeof value === 'number' && Number.isSafeInteger(value) && value >= Number(param.min ?? -2147483648) && value <= Number(param.max ?? 2147483647)
        : param.type === 'boolean' ? typeof value === 'boolean'
        : typeof value === 'string' && value.length > 0 && value.length <= 200 && (param.type === 'text' || idPattern.test(value));
      if (!valid) fail(at(at(origin,'args'),key),'RULE_ARGUMENT',`매개변수 형식·범위 오류: ${param.name ?? key} (${param.type})`);
      output[key] = value!;
    }
    return output;
  };
  const substitute = (value: Json, args: JsonObject, origin: Origin, depth = 0): Json => {
    budget(origin,depth);
    if (Array.isArray(value)) return value.map(v => substitute(v,args,origin,depth+1));
    if (!isObject(value)) return value;
    if (Object.hasOwn(value,'$param')) {
      keys(value,['$param'],origin); const key = String(value.$param);
      if (!Object.hasOwn(args,key)) fail(origin,'RULE_PARAMETER',`정의되지 않은 매개변수: ${key}`);
      return structuredClone(args[key]!);
    }
    return Object.fromEntries(Object.entries(value).map(([k,v]) => {
      if (forbidden.has(k)) fail(origin,'RULE_FIELD',`허용되지 않는 필드: ${k}`);
      return [k,substitute(v,args,origin,depth+1)];
    }));
  };
  const resolve = (call: JsonObject, kind: string, origin: Origin): Definition => {
    const pack = packs.get(String(call.packageId)) ?? fail(origin,'RULE_PACKAGE_MISSING',`규칙 패키지가 없습니다: ${call.packageId ?? ''}`);
    if (call.version !== pack.version) fail(origin,'RULE_VERSION',`요청 버전 ${call.version}과 설치된 ${pack.version}이 다릅니다.`);
    const d = definitions.get(`${call.packageId}/${kind}/${call.ruleId}`);
    return d ?? fail(origin,'RULE_DEFINITION_MISSING',`규칙을 찾을 수 없습니다: ${call.packageId}/${call.ruleId} (${kind})`);
  };
  const sourceOrigin = (origin: Origin): Origin => {
    let pointer=origin.pointer;
    while(pointer){const mapped=generatedOrigins.get(`${origin.doc.file}:${pointer}`);if(mapped)return mapped;pointer=pointer.slice(0,pointer.lastIndexOf('/'));}
    return origin;
  };
  const expand = (value: Json, origin: Origin, stack: string[] = [], depth = 0, outputPath=origin.pointer): Json => {
    if(stack.length)budget(origin,depth);
    if (Array.isArray(value)) {
      const output:Json[]=[];
      for(const [i,v] of value.entries()) {
        const input=at(origin,i), position=childPointer(outputPath,output.length);
        // Preserve the author location even when an earlier effect expands into several.
        const author=sourceOrigin(input);
        const expanded=expand(v,input,stack,depth+1,position);
        const values=isObject(v)&&v.op==='ruleEffect'&&Array.isArray(expanded)?expanded:[expanded];
        for(const item of values){origins.set(`${origin.doc.file}:${childPointer(outputPath,output.length)}`,author);output.push(item);}
      }
      return output;
    }
    if (!isObject(value)) return value;
    if (value.op === 'ruleCondition' || value.op === 'ruleEffect') {
      keys(value,['op','packageId','version','ruleId','args'],origin);
      const kind = value.op === 'ruleCondition' ? 'conditions' : 'effects';
      const d = resolve(value,kind,origin), key = `${d.pack.id}/${kind}/${d.value.id}`;
      if (stack.includes(key)) fail(origin,'RULE_CYCLE',`재귀 규칙 호출: ${[...stack,key].join(' → ')}`);
      origins.set(`${origin.doc.file}:${outputPath}`,sourceOrigin(origin));
      const body = substitute(d.value.body!,parameters(d,value.args,origin),origin);
      return expand(body,{...sourceOrigin(origin),pinned:true},[...stack,key],depth+1,outputPath);
    }
    return Object.fromEntries(Object.entries(value).map(([key,v]) => [key,expand(v,at(origin,key),stack,depth+1,childPointer(outputPath,key))]));
  };
  try {
    let library: JsonObject | undefined;
    if (raw !== undefined && source) {
      const root: Origin = {doc:source,pointer:'/ruleExtensions'};
      library = object(raw,root); keys(library,['apiVersion','packages','progressions'],root);
      if (library.apiVersion !== RULE_API) fail(root,'RULE_API','지원하는 규칙 API 버전은 1입니다.');
      for (const [pi,p] of array(library.packages,at(root,'packages'),64).entries()) {
        const origin=at(at(root,'packages'),pi), pack=object(p,origin);
        keys(pack,['id','version','name','apiVersion','conditions','effects','progressions'],origin);
        const id=identifier(pack.id,at(origin,'id')); text(pack.name,at(origin,'name'));
        if (pack.apiVersion !== RULE_API || typeof pack.version !== 'string' || !versionPattern.test(pack.version)) fail(origin,'RULE_API','패키지 API 1과 정확한 x.y.z 버전이 필요합니다.');
        if (packs.has(id)) fail(origin,'RULE_DUPLICATE',`중복 패키지: ${id}`); packs.set(id,pack);
        for (const kind of ['conditions','effects','progressions']) for (const [di,v] of array(pack[kind] ?? [],at(origin,kind)).entries()) {
          const pos=at(at(origin,kind),di), def=object(v,pos), rid=identifier(def.id,at(pos,'id'));
          keys(def,kind==='progressions'?['id','name','parameters','initial','states','transitions']:['id','name','parameters','body'],pos);
          text(def.name,at(pos,'name'));
          const key=`${id}/${kind}/${rid}`; if(definitions.has(key))fail(pos,'RULE_DUPLICATE',`중복 규칙: ${rid}`);
          const params=object(def.parameters ?? {},at(pos,'parameters'));
          if(Object.keys(params).length>32)fail(pos,'RULE_LIMIT','매개변수는 32개 이하여야 합니다.');
          for(const [key,value] of Object.entries(params)) {
            identifier(key,at(pos,'parameters')); const param=object(value,at(pos,'parameters'));
            keys(param,['type','name','default','min','max'],pos);text(param.name,pos);
            if(!['integer','boolean','text','attribute','character','item','event','flag','stateGroup','stateValue'].includes(String(param.type)))fail(pos,'RULE_PARAMETER',`지원하지 않는 매개변수 형식: ${param.type}`);
            for(const bound of ['min','max'])if(param[bound]!==undefined&&(!Number.isSafeInteger(param[bound])||param.type!=='integer'))fail(pos,'RULE_PARAMETER','수치 범위는 정수 매개변수에만 지정합니다.');
            if(Number(param.min??-2147483648)>Number(param.max??2147483647))fail(pos,'RULE_PARAMETER','매개변수 최솟값이 최댓값보다 큽니다.');
          }
          if(kind==='conditions'&&!isObject(def.body))fail(pos,'RULE_BODY','조건 본문은 조건 객체여야 합니다.');
          if(kind==='effects')array(def.body,pos,100);
          definitions.set(key,{pack,value:def,origin:pos,kind});
        }
      }
    }
    // Check dependencies and parameter declarations even for definitions not used yet.
    const dependencies=new Map<string,string[]>();
    for(const [key,d] of definitions) {
      const params=d.value.parameters as JsonObject ?? {}, defaults:JsonObject={};
      for(const [name,p] of Object.entries(params)) {const param=p as JsonObject;if(param.default===undefined)defaults[name]=param.type==='integer'?Number(param.min??0):param.type==='boolean'?false:'reference';}
      parameters(d,defaults,d.origin);
      const links:string[]=[];
      const scan=(value:Json,origin:Origin,depth=0):void=>{
        budget(origin,depth);
        if(Array.isArray(value)){value.forEach((v,i)=>scan(v,at(origin,i),depth+1));return;}
        if(!isObject(value))return;
        if(Object.hasOwn(value,'$param')){keys(value,['$param'],origin);if(!Object.hasOwn(params,String(value.$param)))fail(origin,'RULE_PARAMETER',`정의되지 않은 매개변수: ${value.$param}`);return;}
        if(value.op==='ruleCondition'||value.op==='ruleEffect') {const kind=value.op==='ruleCondition'?'conditions':'effects';const target=resolve(value,kind,origin);links.push(`${target.pack.id}/${kind}/${target.value.id}`);}
        for(const [k,v] of Object.entries(value))scan(v,at(origin,k),depth+1);
      };
      scan(d.value,d.origin);dependencies.set(key,links);
    }
    const done=new Set<string>();
    const visit=(key:string,path:string[])=>{if(path.includes(key))fail(definitions.get(key)!.origin,'RULE_CYCLE',`재귀 규칙 호출: ${[...path,key].join(' → ')}`);if(done.has(key))return;if(path.length>32)fail(definitions.get(key)!.origin,'RULE_LIMIT','규칙 호출 깊이가 32를 넘습니다.');for(const next of dependencies.get(key)??[])visit(next,[...path,key]);done.add(key);};
    for(const key of definitions.keys())visit(key,[]);
    const docs=project.documents.map(d => d.kind==='extension'||d.kind==='pluginLock'?d:{...d,data:structuredClone(d.data)});
    const projectDoc=docs.find(d=>d.kind==='project');
    if(projectDoc&&isObject(projectDoc.data))delete projectDoc.data.ruleExtensions;
    if(library&&source&&projectDoc&&isObject(projectDoc.data)) {
      const data=projectDoc.data, stateGroups=array(data.stateGroups??[],{doc:source,pointer:'/stateGroups'});
      const gameplay=object(data.gameplay??{locations:[],items:[],shops:[],interactions:[]},{doc:source,pointer:'/gameplay'});
      const actions=array(gameplay.actions??[],{doc:source,pointer:'/gameplay/actions'},512);
      const instanceIds=new Set<string>();
      for(const [i,v] of array(library.progressions??[],{doc:source,pointer:'/ruleExtensions/progressions'},64).entries()) {
        const origin:Origin={doc:source,pointer:`/ruleExtensions/progressions/${i}`}, instance=object(v,origin);
        keys(instance,['id','name','packageId','version','ruleId','args','scope'],origin);
        const id=identifier(instance.id,origin);text(instance.name,origin);
        if(instanceIds.has(id))fail(origin,'RULE_DUPLICATE',`중복 진행 인스턴스: ${id}`);instanceIds.add(id);
        if(instance.scope!=='shared'&&instance.scope!=='character')fail(origin,'RULE_SCOPE','진행 범위는 shared 또는 character입니다.');
        const d=resolve(instance,'progressions',origin), definition=object(substitute(d.value,parameters(d,instance.args,origin),origin),origin);
        const states=array(definition.states,origin), ids=new Set<string>();
        for(const s of states){const state=object(s,origin);keys(state,['id','name'],origin);const sid=identifier(state.id,origin);text(state.name,origin);if(ids.has(sid))fail(origin,'RULE_DUPLICATE',`중복 상태: ${sid}`);ids.add(sid);}
        if(!ids.has(String(definition.initial)))fail(origin,'RULE_STATE','초기 상태가 없습니다.');
        const group=`rx-${id}`;
        if(stateGroups.some(s=>isObject(s)&&s.id===group))fail(origin,'RULE_COLLISION',`상태 ID 충돌: ${group}`);
        origins.set(`${source.file}:/stateGroups/${stateGroups.length}`,origin);
        generatedOrigins.set(`${source.file}:/stateGroups/${stateGroups.length}`,origin);
        stateGroups.push({id:group,name:instance.name!,scope:instance.scope!,initialValueId:definition.initial!,values:states});
        const transitionIds=new Set<string>();
        for(const value of array(definition.transitions,origin)) {
          const t=object(value,origin);keys(t,['id','name','from','to','requirements','costs','effects','eventId'],origin);
          const tid=identifier(t.id,origin);text(t.name,origin);
          if(transitionIds.has(tid))fail(origin,'RULE_DUPLICATE',`중복 전이: ${tid}`);transitionIds.add(tid);
          const from=array(t.from,origin);if(!from.length||from.some(s=>typeof s!=='string'||!ids.has(s))||!ids.has(String(t.to)))fail(origin,'RULE_STATE','전이의 시작·도착 상태가 없습니다.');
          const actionId=`rx-${id}-${tid}`;if(actions.some(a=>isObject(a)&&a.id===actionId))fail(origin,'RULE_COLLISION',`행동 ID 충돌: ${actionId}`);
          const owner=instance.scope==='character'?{owner:{kind:'actor'}}:{};
          const conditions:Json[]=[{op:'any',conditions:from.map(state=>({op:'stateIs',stateGroupId:group,stateValueId:state,...owner}))}];
          if(t.requirements)conditions.push(t.requirements);
          origins.set(`${source.file}:/gameplay/actions/${actions.length}`,origin);
          generatedOrigins.set(`${source.file}:/gameplay/actions/${actions.length}`,origin);
          actions.push({id:actionId,name:t.name!,costs:t.costs??[],maxQuantity:1,requirements:{op:'all',conditions},effects:[...(array(t.effects??[],origin,100)),{op:'setState',stateGroupId:group,stateValueId:t.to!,...owner}],...(t.eventId?{eventId:t.eventId}:{})});
        }
      }
      if((library.progressions as Json[]|undefined)?.length){data.stateGroups=stateGroups;gameplay.actions=actions;data.gameplay=gameplay;}
      result.lock={compiler:RULE_COMPILER,library:structuredClone(library)};
    }
    result.documents=docs.map(d => d.kind==='extension'||d.kind==='pluginLock'?d:{...d,data:expand(d.data,{doc:d,pointer:''})});
  } catch(error) {
    if(!(error instanceof RuleError))throw error;
    diagnostics.push(diagnostic(error.origin.doc,error.origin.pointer,error.code,error.message));
    result.documents=[];
  }
  return result;
}
