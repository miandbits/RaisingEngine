import {useLayoutEffect,useRef,type RefObject} from 'react';

type Position={elementId:string;offset:number;inset:number;fallback:number;focus?:string};
const textNodes=(element:Element)=>{const nodes:Text[]=[],walk=document.createTreeWalker(element,NodeFilter.SHOW_TEXT);while(walk.nextNode())if(!(walk.currentNode.parentElement?.closest('dialog')))nodes.push(walk.currentNode as Text);return nodes;};
function caret(element:Element,offset:number):Range|null {
  for(const node of textNodes(element)){if(offset<node.length){const range=document.createRange();range.setStart(node,offset);range.setEnd(node,Math.min(node.length,offset+1));return range;}offset-=node.length;}return null;
}
function snapshot(root:HTMLElement):Position|null {
  const top=root.getBoundingClientRect().top;
  const elements=[...root.querySelectorAll<HTMLElement>('[data-element-id]')];
  const element=elements.find(e=>e.getBoundingClientRect().bottom>top+2);if(!element)return null;
  const id=element.dataset.elementId!,length=textNodes(element).reduce((sum,node)=>sum+node.length,0);
  let low=0,high=length;
  // Find the first rendered character on the visible line, rather than a pixel ratio.
  while(low<high){const mid=(low+high)>>>1,rect=caret(element,mid)?.getBoundingClientRect();if(rect&&rect.bottom<=top+2)low=mid+1;else high=mid;}
  const rect=caret(element,low)?.getBoundingClientRect()??element.getBoundingClientRect();
  const focused=document.activeElement as HTMLElement|null;
  const focus=focused&&root.contains(focused)?focused.dataset.readingId??focused.closest<HTMLElement>('[data-element-id]')?.dataset.elementId:undefined;
  return {elementId:id,offset:low,inset:rect.top-top,fallback:root.scrollTop,...(focus?{focus}:{})};
}

/** Session-only logical positions; gameplay state and portable saves are untouched. */
export function useReadingPosition(root:RefObject<HTMLDivElement|null>,key:string,geometry:string,enabled:boolean) {
  const positions=useRef(new Map<string,Position>()),activeKey=useRef(key),restoring=useRef(false),dimensions=useRef({width:0,height:0});
  const capture=()=>{const node=root.current;if(node&&!restoring.current&&node.clientWidth===dimensions.current.width&&node.clientHeight===dimensions.current.height){const position=snapshot(node);if(position)positions.current.set(activeKey.current,position);}};
  useLayoutEffect(()=>{
    if(!enabled||!root.current)return;const node=root.current;activeKey.current=key;restoring.current=true;
    const position=positions.current.get(key);
    if(position){const element=[...node.querySelectorAll<HTMLElement>('[data-element-id]')].find(e=>e.dataset.elementId===position.elementId);const rect=element?(caret(element,position.offset)?.getBoundingClientRect()??element.getBoundingClientRect()):null;
      node.scrollTop=rect?node.scrollTop+rect.top-node.getBoundingClientRect().top-position.inset:position.fallback;
      if(position.focus&&!document.querySelector('dialog[open],.authored-slot-modal')){const control=[...node.querySelectorAll<HTMLElement>('[data-reading-id],[data-element-id]')].find(e=>(e.dataset.readingId??e.dataset.elementId)===position.focus);(control?.matches('button,input,select')?control:control?.querySelector<HTMLElement>('button,input,select'))?.focus({preventScroll:true});}
    }else node.scrollTop=0;
    dimensions.current={width:node.clientWidth,height:node.clientHeight};
    const frame=requestAnimationFrame(()=>{restoring.current=false;capture();});
    if(positions.current.size>64)positions.current.delete(positions.current.keys().next().value!);
    return()=>cancelAnimationFrame(frame);
  },[key,geometry,enabled]);
  return {onScroll:capture,onFocusCapture:capture,onPointerDownCapture:capture};
}
