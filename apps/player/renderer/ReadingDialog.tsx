import React,{useEffect,useRef} from 'react';
import {usePlayerPreferences} from './PlayerSettings.js';

/** Native browser modality supplies focus isolation, Escape and background inertness. */
export function ReadingDialog({id,title,onClose,children}:{id:string;title:string;onClose:()=>void;children:React.ReactNode}) {
  const dialog=useRef<HTMLDialogElement>(null),close=useRef(onClose),preferences=usePlayerPreferences();
  close.current=onClose;
  useEffect(()=>{
    const element=dialog.current!,previous=document.activeElement as HTMLElement|null;
    element.showModal();preferences?.onReadingOverlay?.(id,true);
    return()=>{element.close();preferences?.onReadingOverlay?.(id,false);if(previous?.isConnected)previous.focus({preventScroll:true});};
  },[]);
  return <dialog className="reading-dialog" ref={dialog} aria-labelledby={`${id}-title`} data-reading-dialog={id} onKeyDown={event=>{
    if(event.key!=='Tab')return;const controls=[...event.currentTarget.querySelectorAll<HTMLElement>('button:not(:disabled),input:not(:disabled),select:not(:disabled),[tabindex="0"]')].filter(element=>element.getClientRects().length);
    const first=controls[0],last=controls.at(-1);if(event.shiftKey&&document.activeElement===first){event.preventDefault();last?.focus();}else if(!event.shiftKey&&document.activeElement===last){event.preventDefault();first?.focus();}
  }} onCancel={event=>{event.preventDefault();close.current();}}>
    <h2 id={`${id}-title`}>{title}</h2>{children}
  </dialog>;
}
