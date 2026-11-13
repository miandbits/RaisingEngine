import React,{useEffect,useRef,useState} from 'react';
import type {Surface} from '../../../packages/visual/model.js';
type Dimensions={width:number;height:number};
const dimensions=new Map<string,Promise<Dimensions|null>>(),maxBytes=20*1024*1024;
export const hasSurface=(surface:Readonly<Surface>|undefined):surface is Exclude<Surface,{mode:'none'}>=>!!surface&&surface.mode!=='none';
function crc32(bytes:Uint8Array){let value=0xffffffff;for(const byte of bytes){value^=byte;for(let bit=0;bit<8;bit++)value=value&1?(value>>>1)^0xedb88320:value>>>1;}return (value^0xffffffff)>>>0;}
async function pngDimensions(src:string):Promise<Dimensions|null>{
 try{
  const response=await fetch(src);if(!response.ok||Number(response.headers.get('content-length'))>maxBytes)return null;
  const reader=response.body?.getReader();if(!reader)return null;const chunks:Uint8Array[]=[];let length=0;
  while(true){const part=await reader.read();if(part.done)break;length+=part.value.length;if(length>maxBytes){await reader.cancel();return null;}chunks.push(part.value);}
  if(length<45)return null;const bytes=new Uint8Array(length);let at=0;for(const chunk of chunks){bytes.set(chunk,at);at+=chunk.length;}
  const view=new DataView(bytes.buffer),signature=[137,80,78,71,13,10,26,10];
  if(!signature.every((b,i)=>bytes[i]===b)||view.getUint32(8)!==13||String.fromCharCode(...bytes.subarray(12,16))!=='IHDR'||crc32(bytes.subarray(12,29))!==view.getUint32(29))return null;
  const width=view.getUint32(16),height=view.getUint32(20);if(!width||!height||width>8192||height>8192||width*height>32*1024*1024)return null;
  // Decode the exact bounded bytes without a blob: URL (export CSP allows only
  // the game's own image URLs). Painting uses the immutable/versioned asset URL.
  const bitmap=await createImageBitmap(new Blob([bytes],{type:'image/png'}));
  try{return bitmap.width===width&&bitmap.height===height?{width,height}:null;}finally{bitmap.close();}
 }catch{return null;}
}
function cachedDimensions(key:string,src:string){let value=dimensions.get(key);if(!value){if(dimensions.size>=256)dimensions.delete(dimensions.keys().next().value!);value=pngDimensions(src);dimensions.set(key,value);}return value;}
/** Paint only: no padding, border, focus stop, accessible text or hit target. */
export function SurfacePaint({surface,src,identity='',opacity=1}:{surface:Readonly<Surface>|undefined;src:string;identity?:string;opacity?:number}){
 const key=identity+'|'+src,active=hasSurface(surface),layer=useRef<HTMLSpanElement>(null);
 const [loaded,setLoaded]=useState<{key:string;size:Dimensions|null}|null>(null);
 useEffect(()=>{let current=true;if(active){if(src)void cachedDimensions(key,src).then(size=>{if(current)setLoaded({key,size});});else setLoaded({key,size:null});}return()=>{current=false;};},[active,key,src]);
 // CSS backgrounds stay fixed to their owner's padding box while its content
 // scrolls. An independent nine-cell paint layer must follow the same rule.
 useEffect(()=>{const element=layer.current,owner=element?.parentElement;if(!element||!owner)return;const track=()=>{element.style.transform=`translate(${owner.scrollLeft}px,${owner.scrollTop}px)`;};track();owner.addEventListener('scroll',track,{passive:true});return()=>owner.removeEventListener('scroll',track);},[active,key]);
 if(!active)return null;
 const size=loaded?.key===key?loaded.size:null,style:React.CSSProperties={opacity};let cells:React.ReactNode=null,valid=!!size;
 if(surface.mode==='tile')style.backgroundImage=size?`url(${JSON.stringify(src)})`:undefined;
 else if(size){
  const {top,right,bottom,left}=surface.insets;
  valid=[top,right,bottom,left].every(v=>Number.isInteger(v)&&v>=0)&&left+right<size.width&&top+bottom<size.height;
  if(valid){
   const cap=(a:number,b:number)=>a?`min(${a}px, ${100*a/(a+b)}%)`:'0px';
   style.display='grid';style.gridTemplateColumns=`${cap(left,right)} minmax(0,1fr) ${cap(right,left)}`;style.gridTemplateRows=`${cap(top,bottom)} minmax(0,1fr) ${cap(bottom,top)}`;
   const xs=[0,left,size.width-right,size.width],ys=[0,top,size.height-bottom,size.height];
   cells=Array.from({length:9},(_,i)=>{const row=Math.floor(i/3),col=i%3,x=xs[col]!,y=ys[row]!,width=xs[col+1]!-x,height=ys[row+1]!-y;
    return width&&height?<svg key={i} viewBox={`${x} ${y} ${width} ${height}`} preserveAspectRatio="none" style={{gridColumn:col+1,gridRow:row+1}}><image href={src} x="0" y="0" width={size.width} height={size.height}/></svg>:null;
   });
  }
 }
 return <span ref={layer} aria-hidden="true" className="engine-surface-paint" data-surface-paint={loaded?.key!==key?'loading':valid?'ready':'unavailable'} data-contrast="unmeasured" style={style}>{valid?cells:null}</span>;
}
