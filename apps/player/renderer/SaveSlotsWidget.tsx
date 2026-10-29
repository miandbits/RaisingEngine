import React from 'react';
import type {Element} from '../../../packages/visual/model.js';
import {slotWords,type SaveSlotsView,type SaveSlotAction} from '../../../packages/player-service/save-slots.js';
import {savedTime,savedPhase} from './save-display.js';

export function SaveSlotsWidget({element,view,locale,busy,onAction}:{element:Element;view?:SaveSlotsView;locale?:string;busy:boolean;onAction?:(action:SaveSlotAction)=>void}){
  const words=(ko:string,en:string,ja:string)=>slotWords(locale,ko,en,ja),saving=element.kind==='saveSlots';
  if(!view)return <p>{words('플레이어에서 저장 기록을 확인하세요.','Records are available in the player.','プレイヤーで記録を確認してください。')}</p>;
  const rows=view.rows.filter(row=>row.automatic===view.automatic).slice().sort((a,b)=>view.automatic?(b.record?.sequence??0)-(a.record?.sequence??0):a.index-b.index),pages=Math.max(1,Math.ceil(rows.length/6));
  const selected=rows.find(row=>row.index===view.selected),label=(index:number)=>`${view.automatic?words('자동 기록','Auto','自動記録'):words('기록','Record','記録')} ${view.automatic?rows.findIndex(r=>r.index===index)+1:index+1}`;
  const date=(row:typeof selected)=>row?.date??row?.record?.date??(row?.periodIndex!==undefined?`${words('진행','Progress','進行')} ${row.periodIndex+1}`:'');
  const timestamp=(row:typeof selected)=>{const stamp=savedTime(row?.record?.savedAt,locale);return stamp.text?<small className="slot-time" title={stamp.original}>{words('저장: ','Saved: ','保存: ')}{stamp.text}</small>:null;};
  const stage=(row:typeof selected)=>savedPhase(row?.record?.phase,locale);
  const status=(row:typeof selected)=>row?.status==='empty'?words('비어 있음','Empty','空き'):row?.status==='invalid'?words('손상된 기록 · 원본 보존','Unreadable · original kept','読めない記録 · 原本を保持'):row?.status==='incompatible'?words('다른 버전 · 불러오기 불가','Different version · cannot load','異なる版 · 読み込み不可'):date(row);
  const latest=Math.max(0,...rows.map(row=>row.record?.sequence??0));
  const act=(action:SaveSlotAction)=>{if(!busy)onAction?.(action);};
  const download=<button data-slot-action="export" disabled={busy||!view.current} onClick={()=>act({type:'export'})}>{words('백업 내려받기','Download backup','バックアップ保存')}</button>;
  const backup=<div className="slot-backups">{view.available&&download}<button data-slot-action="import" disabled={busy} onClick={()=>act({type:'import'})}>{words('백업 가져오기','Import backup','バックアップ読込')}</button></div>;
  if(view.confirmation)return <section className="web-save-slots slot-confirmation" aria-label={words('기록 확인','Confirm record','記録の確認')}>
    <h2>{view.confirmation==='save'?`${label(view.selected)} · ${words('덮어쓸까요?','Overwrite?','上書きしますか？')}`:view.confirmation==='import'?words('백업에서 이어갈까요?','Load a backup file?','バックアップを読み込みますか？'):words('이날의 이야기로 돌아갈까요?','Return to this moment?','この日の物語に戻りますか？')}</h2>
    {view.confirmation!=='import'&&selected&&<div className="slot-confirmation-identity" data-testid="selected-checkpoint"><strong>{label(selected.index)}</strong><p>{[status(selected),selected.record?.place,stage(selected)].filter(Boolean).join(' · ')}</p>{timestamp(selected)}</div>}
    <p>{view.confirmation==='save'?words('선택한 기록이 현재 진행으로 바뀝니다.','The selected record will be replaced by your current progress.','選んだ記録を現在の進行で上書きします。'):words('저장하지 않은 진행은 사라집니다.','Unsaved progress will be lost.','保存していない進行は失われます。')}</p>
    {view.current&&<p>{words('현재 진행','Current progress','現在の進行')} · {[view.current.date,view.current.place,savedPhase(view.current.phase,locale)].filter(Boolean).join(' · ')}</p>}
    <div className="slot-confirmation-actions"><button data-slot-action="cancel" autoFocus disabled={busy} onClick={()=>act({type:'cancel'})}>{words('취소 · 기록 목록으로','Cancel · back to records','キャンセル · 記録へ')}</button><button data-slot-action="confirm" disabled={busy} onClick={()=>act({type:'confirm'})}>{view.confirmation==='save'?words('덮어쓰기','Overwrite record','上書き'):words('불러오기','Load record','読み込む')}</button></div>
  </section>;
  return <section className="web-save-slots" aria-label={saving?words('저장 기록','Save records','記録を保存'):words('불러올 기록','Load records','記録を読み込む')} style={{'--slot-accent':element.hoverFill??'#345c50','--slot-paper':element.fill??'#f5efdf','--slot-border':element.borderColor??'#c7b99a'} as React.CSSProperties}>
    {!view.available&&<div className="slot-storage-recovery"><p role="status">{words('브라우저 저장소를 열지 못했습니다. 백업 파일을 내려받아 주세요.','Browser storage is unavailable. Download a backup file to keep your progress.','ブラウザーの保存領域を開けません。バックアップを保存してください。')}</p>{download}</div>}
    <div className="slot-toolbar"><button data-slot-action="manual" aria-label={words('수동 기록','Manual records','手動の記録')} aria-pressed={!view.automatic} disabled={busy} onClick={()=>act({type:'group',automatic:false})}>{words('수동','Manual','手動')}</button><button data-slot-action="auto" aria-label={words('자동 기록','Automatic records','自動の記録')} aria-pressed={view.automatic} disabled={busy} onClick={()=>act({type:'group',automatic:true})}>{words('자동','Auto','自動')}</button></div>
    <div className="slot-grid">{rows.slice(view.page*6,view.page*6+6).map(row=><button className={`slot-card${row.status==='empty'?' slot-empty':''}`} key={row.index} data-slot-index={row.index} data-slot-automatic={row.automatic} data-slot-status={row.status} aria-pressed={view.selected===row.index} disabled={busy||!view.available} onClick={()=>act({type:'select',index:row.index})}>
      {row.status==='empty'?<span className="slot-empty-identity" data-slot-part="identity"><strong>{label(row.index)}</strong>{' · '+status(row)}</span>:<span className="slot-details"><strong data-slot-part="identity">{label(row.index)}{row.record?.sequence===latest&&latest>0&&<span className="slot-latest"> · {words('최근 기록','Latest in this group','このグループの最新')}</span>}</strong><span data-slot-part="date">{status(row)}</span>{row.record?.place&&<span>{row.record.place}</span>}{stage(row)&&<span>{stage(row)}</span>}{timestamp(row)}</span>}
    </button>)}</div>
    {rows.some(row=>row.record)&&<p className="slot-image-note">{words('이 플레이어에는 저장 화면 미리보기가 없습니다.','Preview images are not available in this player.','このプレイヤーでは保存画面の画像を表示できません。')}</p>}
    {backup}
    <div className="slot-footer"><div><button aria-label={words('이전 기록 페이지','Previous records page','記録の前のページ')} disabled={busy||view.page===0} onClick={()=>act({type:'page',delta:-1})}>‹</button><span>{view.page+1} / {pages}</span><button aria-label={words('다음 기록 페이지','Next records page','記録の次のページ')} disabled={busy||view.page+1>=pages} onClick={()=>act({type:'page',delta:1})}>›</button></div><button disabled={busy||!view.available||!selected||(saving?view.automatic||!view.current:selected.status!=='ready')} onClick={()=>act({type:saving?'save':'load'})}>{saving?(view.automatic?words('자동 기록은 보호됩니다','Automatic records are protected','自動記録は保護されています'):`${label(view.selected)} · ${selected?.status==='empty'?words('새로 남기기','save new','新しく保存'):words('덮어쓰기 확인','confirm overwrite','上書きを確認')}`):words('선택한 기록 이어 읽기','Continue selected record','選んだ記録から続ける')}</button></div>
  </section>;
}
