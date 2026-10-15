import {commandAttempts,runAnalysis} from './runner.js';
import {createSession,dispatchSession,encodeSave,decodeSave} from '../runtime-service/index.js';
import {dispatchCommand,type ContentSnapshot,type RunState,type Command} from '../core/index.js';
import type {AnalysisOptions,Trace} from './model.js';

export function inspectCommands(content:ContentSnapshot,state:RunState,limit=512){
 const attempts=commandAttempts(content,state),rows=attempts.slice(0,limit).map(commands=>{let next=state;for(const command of commands){const r=dispatchCommand(content,next,command,next.stateRevision);if(!r.ok)return {commands,available:false,code:r.error.code,reason:r.error.message};next=r.state;}return {commands,available:true,code:'OK',reason:''};});
 return {rows,total:attempts.length,truncated:attempts.length>limit,sampledRoster:!!content.project.roster,scope:'단위 수량·현재 연결·최대 256개 다인 활동 조합. 전체 명령 공간의 증명이 아닙니다.'};
}
export function stateInvariants(content:ContentSnapshot,state:RunState):string[]{
 const errors:string[]=[];const finite=(v:any,p:string)=>{if(typeof v==='number'&&!Number.isFinite(v))errors.push(p+': 비유한 수치');else if(v&&typeof v==='object')for(const [k,x] of Object.entries(v))finite(x,p+'/'+k);};finite(state,'state');
 const attributes=(values:Record<string,number>,p:string)=>{for(const a of content.attributes){const n=values[a.id];if(n!==undefined&&(n<a.min||n>a.max))errors.push(p+'/'+a.id+': 능력치 범위');}};attributes(state.attributes,'attributes');
 const walk=(v:any,p:string)=>{if(!v||typeof v!=='object')return;for(const [k,x] of Object.entries(v)){if(k==='inventory'&&x&&typeof x==='object')for(const [id,n] of Object.entries(x))if(typeof n==='number'&&(!Number.isInteger(n)||n<0)||n===null||typeof n!=='number'&&typeof n!=='object')errors.push(p+'/'+k+'/'+id+': 소지품 수량');if(k==='attributes'&&x&&typeof x==='object')attributes(x as any,p);walk(x,p+'/'+k);}};walk(state,'state');for(const [id,values] of Object.entries(state.roster?.attributes??{}))attributes(values as Record<string,number>,'roster/'+id);for(const [id,values] of Object.entries(state.roster?.inventory??{}))for(const [item,n] of Object.entries(values))if(!Number.isInteger(n)||n<0)errors.push('roster/'+id+'/inventory/'+item+': 소지품 수량');
 for(const u of state.systems?.tactics?.active?.units??[])if(u.hp<0||u.hp>u.stats.maxHp)errors.push('tactics/'+u.id+': HP 범위');return errors;
}
export async function runAssurance(content:ContentSnapshot,input:Partial<AnalysisOptions>={},cancelled?:()=>boolean){
 const options:AnalysisOptions={runs:32,seed:42,strategy:'random',maxCommands:150,timeoutMs:60000,...input};
 const failures:{run:number;step:number;kind:string;message:string}[]=[],coverage:Record<string,{available:number;blocked:number;reasons:Record<string,number>} >={};
 let transitions=0,saves=0,rejections=0,cancellations=0;const witnesses:Trace[]=[];const blockedExamples:{run:number;step:number;commands:Command[];code:string;reason:string}[]=[];
 const fail=(run:number,step:number,kind:string,message:string)=>{if(failures.length<100)failures.push({run,step,kind,message});};
 const result=await runAnalysis(content,options,{...(cancelled?{cancelled}:{}),trace:async(trace)=>{
  const started=createSession(content,trace.seed,{mode:'full'});if(!started.ok){fail(trace.run,0,'create',started.error.message);return;}let session=started.session;
  const midpoint=Math.floor(trace.commands.length/2);
  for(let i=0;i<=trace.commands.length;i++){
   if(i===midpoint){
    const before=JSON.stringify(session.state),probe=inspectCommands(content,session.state,128);
    for(const row of probe.rows){const key=row.commands.map(c=>c.type).join(' → ');const c=coverage[key]??={available:0,blocked:0,reasons:{}};c[row.available?'available':'blocked']++;if(!row.available){c.reasons[row.code]=(c.reasons[row.code]??0)+1;if(blockedExamples.length<30&&!blockedExamples.some(e=>e.code===row.code&&JSON.stringify(e.commands)===JSON.stringify(row.commands)))blockedExamples.push({run:trace.run,step:i,commands:row.commands,code:row.code,reason:row.reason});}}
    const cmd=trace.commands[i]??({type:'acknowledgeResult'} as Command);const stale=dispatchCommand(content,session.state,cmd,session.state.stateRevision+1);if(stale.ok||JSON.stringify(session.state)!==before)fail(trace.run,i,'rejection','버전 충돌에서 상태가 변했거나 성공했습니다.');else rejections++;
    if(session.state.progression?.pending){const c=dispatchCommand(content,session.state,{type:'cancelSession'},session.state.stateRevision);if(!c.ok||c.state.progression?.pending)fail(trace.run,i,'cancel','예약 행동 취소가 실패했습니다.');else cancellations++;}
    const encoded=await encodeSave(content,session);if(!encoded.ok)fail(trace.run,i,'save',encoded.error.message);else{const restored=await decodeSave(content,encoded.text);if(!restored.ok)fail(trace.run,i,'load',restored.error.message);else if(JSON.stringify(restored.session.state)!==before)fail(trace.run,i,'load','저장 전후 상태 불일치');else{session=restored.session;saves++;}}
   }
   for(const message of stateInvariants(content,session.state))fail(trace.run,i,'invariant',message);
   if(i===trace.commands.length)break;const old=session.state.stateRevision,before=JSON.stringify(session.state),next=dispatchSession(content,session,trace.commands[i]!,old);if(!next.ok){fail(trace.run,i,'replay',next.error.message);break;}if(JSON.stringify(session.state)!==before||next.session.state.stateRevision!==old+1)fail(trace.run,i,'revision','원본 상태 변경 또는 버전 증가 오류');session=next.session;transitions++;
  }
  if(JSON.stringify(session.state)!==JSON.stringify(trace.state))fail(trace.run,trace.commands.length,'determinism','저장 재생 후 최종 결과 불일치');
  if(witnesses.length<12&&!witnesses.some(w=>w.state.endingId===trace.state.endingId&&w.outcome===trace.outcome))witnesses.push(trace);
 }});
 return {version:1,contentFingerprint:content.contentFingerprint,options,ok:failures.length===0&&result.completed===options.runs,checks:{transitions,saves,rejections,cancellations},coverage,blockedExamples,failures,witnesses,result,limitations:['미관측 경로는 미검증입니다.','명령 가능/거부 집계는 각 회차의 중간 상태에서 조사한 후보입니다.','명령 후보는 단위 수량과 제한된 다인 조합입니다.','취소 검사는 관측한 진행 세션에서만 수행합니다.']};
}
