// MIT. Original geometric demonstration art; no source image pixels are read.
import {deflateSync} from 'node:zlib';
import {createHash} from 'node:crypto';
import {readFile,writeFile,mkdir,readdir} from 'node:fs/promises';
import {join,relative} from 'node:path';

const sha=bytes=>createHash('sha256').update(bytes).digest('hex');
function crc(bytes){let c=0xffffffff;for(const b of bytes){c^=b;for(let k=0;k<8;k++)c=(c>>>1)^((c&1)?0xedb88320:0);}return(c^0xffffffff)>>>0;}
function chunk(type,bytes){const t=Buffer.from(type),n=Buffer.alloc(4),c=Buffer.alloc(4);n.writeUInt32BE(bytes.length);c.writeUInt32BE(crc(Buffer.concat([t,bytes])));return Buffer.concat([n,t,bytes,c]);}
export function samplePng(kind){
  if(!['haru','yuna','flowers','courtyard','library','home','florist'].includes(kind))throw Error('Unknown media kind: '+kind);
  const character=['haru','yuna'].includes(kind),flower=kind==='flowers';
  const width=character?320:flower?256:960,height=character?640:flower?256:540;
  const rgba=Buffer.alloc(width*height*4);
  const pixel=(x,y,color)=>{if(x>=0&&x<width&&y>=0&&y<height)rgba.set(color,(y*width+x)*4);};
  const rect=(x,y,w,h,c)=>{for(let j=Math.max(0,y);j<Math.min(height,y+h);j++)for(let i=Math.max(0,x);i<Math.min(width,x+w);i++)pixel(i,j,c);};
  const ellipse=(cx,cy,rx,ry,c)=>{for(let y=Math.max(0,cy-ry);y<Math.min(height,cy+ry);y++)for(let x=Math.max(0,cx-rx);x<Math.min(width,cx+rx);x++)if(((x-cx)/rx)**2+((y-cy)/ry)**2<=1)pixel(x,y,c);};
  const ink=[45,61,72,255],cream=[239,229,199,255],green=[83,139,121,255],gold=[218,172,91,255];
  if(character){
    const coat=kind==='haru'?green:[78,99,139,255];
    rect(104,477,43,140,ink);rect(173,477,43,140,ink);
    ellipse(160,315,100,170,coat);rect(67,275,38,193,coat);rect(215,275,38,193,coat);
    ellipse(86,463,22,25,cream);ellipse(234,463,22,25,cream);
    ellipse(160,125,66,79,ink);ellipse(160,142,51,59,cream);
    rect(117,82,87,40,ink);ellipse(142,145,4,5,ink);ellipse(178,145,4,5,ink);
    rect(150,172,20,3,gold);rect(150,211,20,227,cream);rect(69,382,182,18,gold);
    if(kind==='yuna'){rect(76,320,78,96,cream);rect(81,325,68,86,ink);}
  }else if(flower){
    for(const [x,y]of [[88,92],[128,61],[169,90],[114,119],[155,128]]){
      rect(x-2,y,5,107,green);for(let a=0;a<6;a++)ellipse(x+Math.round(Math.cos(a)*18),y+Math.round(Math.sin(a)*18),13,13,cream);ellipse(x,y,10,10,gold);
    }rect(94,177,76,43,gold);rect(90,177,84,10,green);
  }else{
    for(let y=0;y<height;y++)rect(0,y,width,1,[236-Math.floor(y/18),232-Math.floor(y/21),209-Math.floor(y/30),255]);
    rect(0,395,width,145,[137,121,104,255]);for(let y=420;y<height;y+=30)rect(0,y,width,2,cream);
    if(kind==='courtyard'){
      rect(0,0,width,225,[159,197,199,255]);for(let x=70;x<920;x+=190){rect(x,150,150,245,cream);ellipse(x+75,210,47,65,ink);rect(x+28,210,94,185,ink);}rect(0,375,width,25,gold);
      ellipse(780,160,100,110,green);rect(770,225,20,170,ink);
    }else{
      rect(65,80,235,260,ink);rect(77,92,211,236,[160,201,204,255]);rect(176,92,12,236,cream);rect(77,202,211,12,cream);
      for(let x=470;x<920;x+=150){rect(x,100,130,285,ink);for(let y=120;y<365;y+=70){rect(x+8,y+48,114,8,gold);for(let b=0;b<6;b++)rect(x+13+b*18,y,12,48,b%2?green:cream);}}
      rect(340,350,315,28,gold);rect(360,378,20,97,ink);rect(615,378,20,97,ink);
      if(kind==='home'){rect(20,374,280,55,cream);rect(30,428,18,60,ink);rect(270,428,18,60,ink);}
      if(kind==='florist'){for(let x=400;x<650;x+=80){rect(x,280,45,70,green);ellipse(x+22,258,35,32,gold);}}
    }
  }
  const scan=Buffer.alloc((width*4+1)*height);for(let y=0;y<height;y++)rgba.copy(scan,y*(width*4+1)+1,y*width*4,(y+1)*width*4);
  const header=Buffer.alloc(13);header.writeUInt32BE(width);header.writeUInt32BE(height,4);header[8]=8;header[9]=6;
  return Buffer.concat([Buffer.from([137,80,78,71,13,10,26,10]),chunk('IHDR',header),chunk('IDAT',deflateSync(scan)),chunk('IEND',Buffer.alloc(0))]);
}
export async function replaceReleaseMedia(root,policy){
  const ledger=[];
  for(const item of policy.replacements){
    if(!/^engine\/examples\/[a-z0-9-]+\/assets\/[a-zA-Z0-9/_.-]+\.png$/.test(item.path)||item.path.includes('..'))throw Error('Unsafe release media path');
    const target=join(root,item.path),original=await readFile(target);
    if(sha(original)!==item.originalSha256)throw Error('Unreviewed sample media: '+item.path);
    const bytes=samplePng(item.kind);await writeFile(target,bytes);
    ledger.push({path:item.path,sha256:sha(bytes),bytes:bytes.length,kind:item.kind,license:'MIT',generator:'scripts/release-sample-media.mjs'});
  }
  await mkdir(join(root,'licenses'),{recursive:true});
  await writeFile(join(root,'licenses/sample-media.json'),JSON.stringify({version:1,policy:'Original geometric media only; generated/reference art excluded from this binary distribution.',files:ledger},null,2)+'\n');
  return ledger;
}
export async function auditReleaseMedia(root,policy){
  const ledger=JSON.parse(await readFile(join(root,'licenses/sample-media.json'),'utf8'));
  const allowed=new Map(ledger.files.map(f=>[f.path,f]));
  if(allowed.size!==policy.replacements.length)throw Error('Incomplete sample media ledger');
  for(const item of policy.replacements)if(allowed.get(item.path)?.kind!==item.kind)throw Error('Media ledger differs from policy: '+item.path);
  const originalHashes=new Set(policy.replacements.map(f=>f.originalSha256));let count=0;
  async function visit(dir){for(const entry of await readdir(dir,{withFileTypes:true})){
    const file=join(dir,entry.name);if(entry.isDirectory()){await visit(file);continue;}
    const path=relative(root,file).replaceAll('\\','/');
    if(!/\.(png|jpg|jpeg|webp|svg|gif|wav|ogg|mp3|mp4|ttf|otf)$/i.test(path))continue;
    const bytes=await readFile(file),hash=sha(bytes),record=allowed.get(path);
    if(originalHashes.has(hash)||!record||record.sha256!==hash||sha(samplePng(record.kind))!==hash)throw Error('Unapproved packaged sample media: '+path);
    count++;
  }}await visit(join(root,'engine/examples'));
  if(count!==allowed.size)throw Error('Missing licensed sample media');
  return {images:count,license:'MIT',originalMediaExcluded:true};
}
