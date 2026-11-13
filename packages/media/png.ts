import { createHash } from 'node:crypto';
import { deflateSync, inflateSync } from 'node:zlib';
export interface RgbaImage {width:number;height:number;pixels:Buffer}
export const PNG_LIMITS = Object.freeze({maxBytes:20*1024*1024,maxSide:8192,maxPixels:32*1024*1024});
const signature=Buffer.from([137,80,78,71,13,10,26,10]);
const table=Uint32Array.from({length:256},(_,n)=>{for(let k=0;k<8;k++)n=n&1?0xedb88320^(n>>>1):n>>>1;return n>>>0;});
function crc(bytes:Buffer){let value=0xffffffff;for(const b of bytes)value=table[(value^b)&255]!^(value>>>8);return (value^0xffffffff)>>>0;}
function fail():never{throw new Error('손상되었거나 지원하지 않는 PNG 파일입니다.');}
function bounds(width:number,height:number){if(!width||!height||width>PNG_LIMITS.maxSide||height>PNG_LIMITS.maxSide||width*height>PNG_LIMITS.maxPixels)throw new Error('PNG 크기는 8192px 및 32메가픽셀 이하여야 합니다.');}
const passes=[[0,0,8,8],[4,0,8,8],[0,4,4,8],[2,0,4,4],[0,2,2,4],[1,0,2,2],[0,1,1,2]];
/** Bounded PNG decoder: standard grayscale/RGB/indexed/alpha, 1–16bit, Adam7. */
export function decodePng(bytes:Buffer):RgbaImage{
  if(bytes.length<45||bytes.length>PNG_LIMITS.maxBytes||!bytes.subarray(0,8).equals(signature))fail();
  let offset=8,width=0,height=0,depth=0,type=-1,interlace=0,ended=false,seenData=false,dataEnded=false;
  let palette:Buffer|undefined,alpha:Buffer|undefined;const compressed:Buffer[]=[];
  while(offset<bytes.length){
    if(offset+12>bytes.length)fail();const length=bytes.readUInt32BE(offset),end=offset+12+length;if(end>bytes.length)fail();
    const kind=bytes.toString('ascii',offset+4,offset+8),data=bytes.subarray(offset+8,end-4);
    if(!/^[A-Za-z]{4}$/.test(kind)||crc(bytes.subarray(offset+4,end-4))!==bytes.readUInt32BE(end-4))fail();
    if(!width&&kind!=='IHDR')fail();
    if(kind==='IHDR'){
      if(offset!==8||length!==13)fail();width=data.readUInt32BE(0);height=data.readUInt32BE(4);bounds(width,height);
      depth=data[8]!;type=data[9]!;interlace=data[12]!;
      const legal:Record<number,number[]>={0:[1,2,4,8,16],2:[8,16],3:[1,2,4,8],4:[8,16],6:[8,16]};
      if(!legal[type]?.includes(depth)||data[10]!==0||data[11]!==0||interlace>1)fail();
    }else if(kind==='PLTE'){if(palette||seenData||length===0||length%3||length>768||type===0||type===4)fail();palette=data;}
    else if(kind==='tRNS'){if(alpha||seenData||![0,2,3].includes(type)||type===3&&!palette)fail();alpha=data;}
    else if(kind==='IDAT'){if(dataEnded)fail();seenData=true;compressed.push(data);}
    else if(kind==='IEND'){if(length||!seenData||end!==bytes.length)fail();ended=true;}
    else {if(seenData)dataEnded=true;if(kind[0]===kind[0]!.toUpperCase())fail();}
    offset=end;
  }
  if(!ended||type===3&&(!palette||palette.length/3>2**depth)||alpha&&(type===0&&alpha.length!==2||type===2&&alpha.length!==6||type===3&&(!palette||alpha.length>palette.length/3)))fail();
  const channels=({0:1,2:3,3:1,4:2,6:4} as Record<number,number>)[type]!,bpp=Math.max(1,Math.ceil(channels*depth/8));
  const layouts=(interlace?passes:[[0,0,1,1]]).map(p=>{const [x,y,dx,dy]=p as [number,number,number,number];const w=Math.max(0,Math.ceil((width-x)/dx)),h=Math.max(0,Math.ceil((height-y)/dy));return {x,y,dx,dy,w,h,stride:Math.ceil(w*channels*depth/8)};});
  const expected=layouts.reduce((n,p)=>n+(p.w&&p.h?(p.stride+1)*p.h:0),0);
  let raw:Buffer;try{raw=inflateSync(Buffer.concat(compressed),{maxOutputLength:expected});}catch{fail();}if(raw.length!==expected)fail();
  const pixels=Buffer.alloc(width*height*4);let position=0;
  for(const p of layouts){if(!p.w||!p.h)continue;let previous=Buffer.alloc(p.stride);
    for(let row=0;row<p.h;row++){
      const filter=raw[position++]!;if(filter>4)fail();const scan=Buffer.from(raw.subarray(position,position+p.stride));position+=p.stride;
      for(let i=0;i<scan.length;i++){
        const a=i>=bpp?scan[i-bpp]!:0,b=previous[i]!,c=i>=bpp?previous[i-bpp]!:0;
        let predict=0;if(filter===1)predict=a;else if(filter===2)predict=b;else if(filter===3)predict=(a+b)>>>1;
        else if(filter===4){const n=a+b-c,pa=Math.abs(n-a),pb=Math.abs(n-b),pc=Math.abs(n-c);predict=pa<=pb&&pa<=pc?a:pb<=pc?b:c;}
        scan[i]=(scan[i]!+predict)&255;
      }
      const sample=(n:number)=>depth===16?scan.readUInt16BE(n*2):depth===8?scan[n]!:(scan[Math.floor(n*depth/8)]!>>>(8-depth-(n*depth)%8))&((1<<depth)-1);
      const byte=(n:number)=>depth===16?n>>>8:Math.round(n*255/((1<<depth)-1));
      for(let x=0;x<p.w;x++){
        const at=((p.y+row*p.dy)*width+p.x+x*p.dx)*4,n=x*channels,v=sample(n);let r=0,g=0,b=0,a=255;
        if(type===3){if(!palette||v*3+2>=palette.length)fail();r=palette[v*3]!;g=palette[v*3+1]!;b=palette[v*3+2]!;a=alpha?.[v]??255;}
        else if(type===0||type===4){r=g=b=byte(v);a=type===4?byte(sample(n+1)):alpha&&v===alpha.readUInt16BE(0)?0:255;}
        else{const vg=sample(n+1),vb=sample(n+2);r=byte(v);g=byte(vg);b=byte(vb);a=type===6?byte(sample(n+3)):alpha&&v===alpha.readUInt16BE(0)&&vg===alpha.readUInt16BE(2)&&vb===alpha.readUInt16BE(4)?0:255;}
        pixels[at]=r;pixels[at+1]=g;pixels[at+2]=b;pixels[at+3]=a;
      }previous=scan;
    }
  }return {width,height,pixels};
}
function chunk(type:string,data:Buffer){const b=Buffer.alloc(data.length+12);b.writeUInt32BE(data.length,0);b.write(type,4,'ascii');data.copy(b,8);b.writeUInt32BE(crc(b.subarray(4,b.length-4)),b.length-4);return b;}
export function encodeRgbaPng(image:RgbaImage):Buffer{
  bounds(image.width,image.height);if(image.pixels.length!==image.width*image.height*4)throw new Error('RGBA 픽셀 수가 이미지 크기와 다릅니다.');
  const header=Buffer.alloc(13);header.writeUInt32BE(image.width,0);header.writeUInt32BE(image.height,4);header[8]=8;header[9]=6;
  const stride=image.width*4,raw=Buffer.alloc((stride+1)*image.height);for(let y=0;y<image.height;y++)image.pixels.copy(raw,y*(stride+1)+1,y*stride,(y+1)*stride);
  return Buffer.concat([signature,chunk('IHDR',header),chunk('IDAT',deflateSync(raw)),chunk('IEND',Buffer.alloc(0))]);
}

export interface PngDimensions {readonly width:number;readonly height:number}
export const PNG_DIMENSION_CACHE_LIMIT=128;
const dimensionsBySha=new Map<string,PngDimensions>();
/** Cache only fully decoded PNGs, by their exact bytes, never by mutable paths or asset IDs. */
export function validatedPngDimensions(bytes:Buffer):PngDimensions {
  if(bytes.length>PNG_LIMITS.maxBytes)fail();
  const sha=createHash('sha256').update(bytes).digest('hex'),cached=dimensionsBySha.get(sha);
  if(cached){dimensionsBySha.delete(sha);dimensionsBySha.set(sha,cached);return cached;}
  const decoded=decodePng(bytes),dimensions=Object.freeze({width:decoded.width,height:decoded.height});
  if(dimensionsBySha.size>=PNG_DIMENSION_CACHE_LIMIT)dimensionsBySha.delete(dimensionsBySha.keys().next().value!);
  dimensionsBySha.set(sha,dimensions);return dimensions;
}
