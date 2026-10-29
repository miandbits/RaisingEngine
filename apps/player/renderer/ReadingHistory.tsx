import React,{createContext,useContext,useEffect,useRef} from 'react';
import type {ReadingHistoryReceipt,ReadingHistorySnapshot} from '../../../packages/visual/reading-history.js';
import {slotWords} from '../../../packages/player-service/save-slots.js';
import {readableDefaults} from '../../../packages/visual/readable-layout.js';
import {ReadingDialog} from './ReadingDialog.js';
import {calendarText,type Calendar,type CalendarDate} from '../../../packages/core/calendar.js';

/** Date labels only: legacy calendar arithmetic and authored HUD text stay unchanged. */
export function readingHistoryDate(date:CalendarDate|null|undefined,calendar:Calendar|undefined,locale:string|undefined):string {
  return date?calendarText(date,calendar??{monthsPerYear:12,daysPerMonth:28},locale):'';
}

export interface ReadingHistoryHost {enabled:boolean;epoch:string;blocked:boolean;record:(receipt:ReadingHistoryReceipt)=>void;open:()=>void}
export const ReadingHistoryContext=createContext<ReadingHistoryHost|null>(null);
export const useReadingHistory=()=>useContext(ReadingHistoryContext);

/** A receipt proves presentation of a revealed block, not that every line was read. */
export function historyTextVisible(element:Element):boolean {
  if(document.hidden||!document.hasFocus()||!element.isConnected||element.closest('[inert]'))return false;
  if([...document.querySelectorAll('dialog[open],[aria-modal="true"],.timeline-overlay')].some(overlay=>!overlay.contains(element)))return false;
  const rect=element.getBoundingClientRect();let left=Math.max(0,rect.left),top=Math.max(0,rect.top),right=Math.min(innerWidth,rect.right),bottom=Math.min(innerHeight,rect.bottom);
  for(let node:Element|null=element;node;node=node.parentElement){
    const style=getComputedStyle(node);if(style.display==='none'||style.visibility!=='visible'||Number(style.opacity)<=0)return false;
    const r=node.getBoundingClientRect();
    if(/hidden|clip|scroll|auto/.test(style.overflowX)){left=Math.max(left,r.left);right=Math.min(right,r.right);}
    if(/hidden|clip|scroll|auto/.test(style.overflowY)){top=Math.max(top,r.top);bottom=Math.min(bottom,r.bottom);}
    const clip=/^inset\(([\d.]+)px ([\d.]+)px ([\d.]+)px ([\d.]+)px\)$/.exec(style.clipPath);
    if(clip){top=Math.max(top,r.top+Number(clip[1]));right=Math.min(right,r.right-Number(clip[2]));bottom=Math.min(bottom,r.bottom-Number(clip[3]));left=Math.max(left,r.left+Number(clip[4]));}
  }
  if(right-left<1||bottom-top<1)return false;
  // Reject boxes covered by unrelated authored elements as well as clipped boxes.
  return [[.5,.5],[.15,.15],[.85,.15],[.15,.85],[.85,.85]].some(([x,y])=>{const hit=document.elementFromPoint(left+(right-left)*x!,top+(bottom-top)*y!);return !!hit&&(element.contains(hit));});
}
export function PresentedHistoryText({text,receipt}:{text:string;receipt?:Omit<ReadingHistoryReceipt,'text'|'speaker'>|undefined}) {
  const host=useReadingHistory(),element=useRef<HTMLSpanElement>(null);
  useEffect(()=>{
    if(!receipt||!host?.enabled||host.blocked)return;const node=element.current;if(!node)return;let first=0,second=0;
    const capture=()=>{cancelAnimationFrame(first);cancelAnimationFrame(second);first=requestAnimationFrame(()=>{second=requestAnimationFrame(()=>{if(!historyTextVisible(node))return;
      const speaker=[...node.closest('.visual-viewport')!.querySelectorAll<HTMLElement>('[data-history-speaker]')].find(historyTextVisible)?.textContent??'';
      // This component renders only its explicit revealed text, never hidden suffixes.
      host.record({...receipt,text:node.textContent??'',speaker});
    });});};
    const intersection=new IntersectionObserver(capture),resize=new ResizeObserver(capture);intersection.observe(node);resize.observe(node);
    document.addEventListener('scroll',capture,true);document.addEventListener('visibilitychange',capture);window.addEventListener('focus',capture);node.closest('.visual-viewport')?.addEventListener('animationend',capture);capture();
    return()=>{cancelAnimationFrame(first);cancelAnimationFrame(second);intersection.disconnect();resize.disconnect();document.removeEventListener('scroll',capture,true);document.removeEventListener('visibilitychange',capture);window.removeEventListener('focus',capture);node.closest('.visual-viewport')?.removeEventListener('animationend',capture);};
  },[host?.enabled,host?.blocked,host?.epoch,receipt?.key,receipt?.kind,receipt?.locale,receipt?.date,text]);
  return <span ref={element} data-history-presented={receipt?.kind}>{text}</span>;
}
export function RecentReading({snapshot,onClose,locale,date,phase,textScale=1,background=readableDefaults.background}:{snapshot:ReadingHistorySnapshot;onClose:()=>void;locale?:string|undefined;date:string;phase:string;textScale?:number;background?:string|undefined}) {
  const words=(ko:string,en:string,ja:string)=>slotWords(locale,ko,en,ja),list=useRef<HTMLDivElement>(null),rgb=parseInt(background.slice(1),16),ink=.2126*((rgb>>16)&255)+.7152*((rgb>>8)&255)+.0722*(rgb&255)>145?'#263c36':'#f5efdf';
  useEffect(()=>{const element=list.current;if(element)element.scrollTop=element.scrollHeight;},[]);
  return <div className="reading-history-host" style={{fontSize:18*textScale,'--readable-background':background,'--readable-ink':ink} as React.CSSProperties}>
    <ReadingDialog id="engine-reading-history-panel" title={words('최근 읽은 내용 · 이번 실행','Recent reading · this session','最近の文章 · このセッション')} onClose={onClose}>
      <div className="reading-dialog-body reading-history-body" ref={list}>
        <p data-reading-id="engine-reading-history-current">{[date,phase].filter(Boolean).join(' · ')}</p>
        <p className="reading-history-notice">{snapshot.reason==='restored'?words('불러온 지점부터 기록합니다. 이전 진행의 기록은 포함되지 않습니다.','Reading starts from the restored point. Earlier session history is not included.','読み込んだ地点から記録します。それ以前の履歴は含まれません。'):snapshot.reason==='new-run'?words('새 게임을 시작한 뒤 표시된 내용입니다.','Text presented since starting this new game.','この新しいゲームで表示された文章です。'):words('이번 실행에서 표시된 내용입니다. 저장 파일에 포함되지 않습니다.','Text presented during this session. It is not included in saves.','このセッションで表示された文章です。セーブには含まれません。')}</p>
        {!snapshot.entries.length&&<p data-reading-id="engine-reading-history-empty">{words('아직 표시된 이야기나 선택한 응답이 없습니다.','No story text or chosen response has been presented yet.','まだ物語の文章や選んだ応答が表示されていません。')}</p>}
        {snapshot.omittedEntries>0&&<p data-reading-id="engine-reading-history-trimmed">{words(`오래된 항목 ${snapshot.omittedEntries}개가 용량 제한으로 생략되었습니다.`,`${snapshot.omittedEntries} older entries were omitted to keep this history bounded.`,`容量の上限により古い${snapshot.omittedEntries}件を省略しました。`)}</p>}
        {snapshot.entries.map(entry=><article key={entry.sequence} data-reading-id={`engine-reading-history-entry-${entry.sequence}`} data-history-kind={entry.kind}>
          <p className="reading-history-meta">{[entry.kind==='chosen-response'?words('선택한 응답','Chosen response','選んだ応答'):entry.speaker,entry.date,entry.locale].filter(Boolean).join(' · ')}</p><p className="reading-history-text">{entry.text}</p>
          {entry.truncated&&<p>{words('이 항목은 길이 제한으로 일부만 보관됩니다.','This entry was shortened to fit the history limit.','長さの上限により、この項目の一部だけを保持しています。')}</p>}
        </article>)}
      </div>
      <div className="reading-dialog-actions"><button data-reading-id="engine-reading-history-close" onClick={onClose}>{words('닫기','Close','閉じる')}</button></div>
    </ReadingDialog>
  </div>;
}
