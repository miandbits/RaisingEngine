import {createHash} from 'node:crypto';
import {readFile,realpath,stat} from 'node:fs/promises';
import path from 'node:path';
import type {StudioEditor} from './editor.js';
import {decodePng,encodeRgbaPng} from './png.js';
import {publishImmutableAsset} from './immutable-asset.js';
export interface SpriteRegion {x:number;y:number;width:number;height:number}
export interface SpriteImportInput {filePath:string;prefix:string;regions:SpriteRegion[];colorKey?:{color:string;tolerance:number};expectedVersion?:string}
export interface SpriteFrame {id:string;path:string;region:SpriteRegion;width:number;height:number}
const hash=(bytes:Buffer|string)=>createHash('sha256').update(bytes).digest('hex');
const inside=(root:string,file:string)=>{const relative=path.relative(root,file);return !relative.startsWith('..')&&!path.isAbsolute(relative);};
/** All extracted assets enter one revision-guarded manifest edit and undo item. */
export async function importSprites(editor:StudioEditor,input:SpriteImportInput,revision:number){
  editor.current(revision);
  if(!editor.root||editor.readOnly)throw new Error('편집 가능한 프로젝트를 열어 주세요.');
  const root=await realpath(editor.root),draft=editor.draftVersion();
  const current=()=>{editor.current(revision);if(editor.root!==root||editor.draftVersion()!==draft)throw new Error('편집 내용이 갱신되었습니다. 다시 시도해 주세요.');if(input.expectedVersion!==undefined&&input.expectedVersion!==editor.diskVersion())throw new Error('프로젝트 버전이 변경되었습니다. 다시 불러오세요.');};
  current();await editor.assertDiskUnchanged();current();
  if(typeof input.prefix!=='string'||input.prefix.length>56||!/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(input.prefix))throw new Error('프레임 접두사는 56자 이하의 영문 소문자·숫자·하이픈 ID여야 합니다.');
  if(!Array.isArray(input.regions)||input.regions.length<1||input.regions.length>256)throw new Error('추출 영역은 1~256개여야 합니다.');
  if(typeof input.filePath!=='string'||!path.isAbsolute(input.filePath)||path.extname(input.filePath).toLowerCase()!=='.png')throw new Error('PNG 파일의 절대 경로를 선택하세요.');
  const info=await stat(input.filePath);if(!info.isFile()||info.size>20*1024*1024)throw new Error('20 MiB 이하의 PNG 파일을 선택하세요.');
  const key=input.colorKey;if(key&&(!/^#[0-9a-f]{6}$/i.test(key.color)||!Number.isInteger(key.tolerance)||key.tolerance<0||key.tolerance>255))throw new Error('색상 키는 #RRGGBB, 허용 오차는 0~255 정수여야 합니다.');
  const bytes=await readFile(input.filePath),image=decodePng(bytes),regions=input.regions.map(r=>({...r}));
  let pixels=0;for(const r of regions){if(!r||![r.x,r.y,r.width,r.height].every(Number.isSafeInteger)||r.x<0||r.y<0||r.width<1||r.height<1||r.x+r.width>image.width||r.y+r.height>image.height)throw new Error('추출 영역이 원본 이미지 경계를 벗어났습니다.');pixels+=r.width*r.height;}
  if(pixels>32*1024*1024)throw new Error('프레임의 총 픽셀 수는 32메가픽셀 이하여야 합니다.');
  const directory=await realpath(path.join(root,'assets'));if(!inside(root,directory))throw new Error('프로젝트 밖의 에셋 경로입니다.');
  const sourceHash=hash(bytes),sourceAssetId=`sprite-source-${sourceHash.slice(0,24)}`;
  const sourcePath=`assets/${sourceAssetId}.png`,colorKey=key?{color:key.color.toLowerCase(),tolerance:key.tolerance}:null;
  const importId=hash(JSON.stringify({version:1,sourceHash,prefix:input.prefix,regions,colorKey})).slice(0,24),recordPath=`assets/sprite-import-${importId}.json`;
  const files:{id?:string;path:string;bytes:Buffer}[]=[{id:sourceAssetId,path:sourcePath,bytes}],frames:SpriteFrame[]=[];
  const rgb=key?[1,3,5].map(i=>Number.parseInt(key.color.slice(i,i+2),16)):null;
  for(const [index,region]of regions.entries()){
    const data=Buffer.alloc(region.width*region.height*4);
    for(let y=0;y<region.height;y++)image.pixels.copy(data,y*region.width*4,((region.y+y)*image.width+region.x)*4,((region.y+y)*image.width+region.x+region.width)*4);
    if(rgb)for(let i=0;i<data.length;i+=4)if(Math.max(Math.abs(data[i]!-rgb[0]!),Math.abs(data[i+1]!-rgb[1]!),Math.abs(data[i+2]!-rgb[2]!))<=key!.tolerance)data[i+3]=0;
    const id=`${input.prefix}-${index+1}`,frameBytes=encodeRgbaPng({width:region.width,height:region.height,pixels:data});
    const assetPath=`assets/sprite-${hash(frameBytes).slice(0,24)}.png`;
    frames.push({id,path:assetPath,region,width:region.width,height:region.height});files.push({id,path:assetPath,bytes:frameBytes});
  }
  const record={version:1,algorithm:'rgba-channel-distance-v1',source:{assetId:sourceAssetId,path:sourcePath,sha256:sourceHash,width:image.width,height:image.height},colorKey,frames:frames.map((frame,i)=>({...frame,sha256:hash(files[i+1]!.bytes)}))};
  files.push({path:recordPath,bytes:Buffer.from(JSON.stringify(record,null,2)+'\n')});
  current();const manifest=editor.documents.find(d=>d.kind==='assets');if(!manifest)throw new Error('에셋 목록이 없습니다.');
  const data=manifest.data as {assets:{id:string;kind:string;path:string}[]},additions:{id:string;kind:'image';path:string}[]=[];
  // Validate every ID before producing files; repeated identical imports are no-ops.
  for(const file of files){if(!file.id)continue;const existing=data.assets.find(a=>a.id===file.id);
    if(existing){if(existing.kind!=='image'||existing.path!==file.path)throw new Error(`이미 사용 중인 에셋 ID입니다: ${file.id}`);}
    else additions.push({id:file.id,kind:'image',path:file.path});
  }
  for(const file of files)await publishImmutableAsset(directory,path.basename(file.path),file.bytes);
  // Async extraction/storage cannot overwrite a newer draft or externally changed manifest.
  await editor.assertDiskUnchanged();current();
  if(additions.length)editor.edit({type:'replace',file:manifest.file,data:{...data,assets:[...data.assets,...additions]}},revision);
  return {sourceAssetId,assetIds:frames.map(f=>f.id),frames,recordPath,importId,width:image.width,height:image.height};
}
