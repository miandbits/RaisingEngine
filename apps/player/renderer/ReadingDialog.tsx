import React,{useEffect,useRef} from 'react';
import {usePlayerPreferences} from './PlayerSettings.js';

/** Native browser modality supplies focus isolation, Escape and background inertness. */
export function ReadingDialog({id,title,onClose,children,initialFocus}:{id:string;title:string;onClose:()=>void;children:React.ReactNode;initialFocus?:'heading'}) {
  const dialog=useRef<HTMLDialogElement>(null),heading=useRef<HTMLHeadingElement>(null),close=useRef(onClose),preferences=usePlayerPreferences();
  close.current=onClose;
  useEffect(()=>{
    const element=dialog.current!,previous=document.activeElement as HTMLElement|null;
    element.showModal();if(initialFocus==='heading')heading.current?.focus({preventScroll:true});preferences?.onReadingOverlay?.(id,true);
    return()=>{element.close();preferences?.onReadingOverlay?.(id,false);if(previous?.isConnected)previous.focus({preventScroll:true});};
  },[]);
  return <dialog className="reading-dialog" ref={dialog} aria-labelledby={`${id}-title`} data-reading-dialog={id} onKeyDown={event=>{
    // Reading panes remain reachable even when a review has only a close button.
    const pane=event.target instanceof HTMLElement&&event.target.classList.contains('reading-dialog-body')?event.target:null;
    if(pane){const amount=event.key==='ArrowDown'?48:event.key==='ArrowUp'?-48:event.key==='PageDown'?pane.clientHeight*.85:event.key==='PageUp'?-pane.clientHeight*.85:null;
      if(amount!==null||event.key==='Home'||event.key==='End'){event.preventDefault();pane.scrollTop=event.key==='Home'?0:event.key==='End'?pane.scrollHeight:pane.scrollTop+(amount??0);return;}}
    if(event.key!=='Tab')return;const controls=[...event.currentTarget.querySelectorAll<HTMLElement>('button:not(:disabled),input:not(:disabled),select:not(:disabled),[tabindex="0"]')].filter(element=>element.getClientRects().length);
    const first=controls[0],last=controls.at(-1);
    // Safari may exclude buttons from its default Tab order. Read-start dialogs
    // use their explicit controls so the focus loop is consistent across browsers.
    if(initialFocus==='heading'&&controls.length){const at=controls.indexOf(document.activeElement as HTMLElement);event.preventDefault();controls[at<0?(event.shiftKey?controls.length-1:0):(at+(event.shiftKey?-1:1)+controls.length)%controls.length]?.focus();return;}
    if(event.shiftKey&&(document.activeElement===first||document.activeElement===heading.current)){event.preventDefault();last?.focus();}else if(!event.shiftKey&&document.activeElement===last){event.preventDefault();first?.focus();}
  }} onCancel={event=>{event.preventDefault();close.current();}}>
    <h2 ref={heading} id={`${id}-title`} tabIndex={initialFocus==='heading'?-1:undefined}>{title}</h2>{children}
  </dialog>;
}
