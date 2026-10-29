import {readFile,writeFile,mkdir} from 'node:fs/promises';
import path from 'node:path';
import {decodePng,encodeRgbaPng,type RgbaImage} from '../media/png.js';
import type {ExportTarget} from './model.js';
import {gameDirectory,playerExecutable} from './targets.js';
import {releaseHash,type PreparedRelease} from './release-metadata.js';
const xml=(text:string)=>text.replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('>','&gt;').replaceAll('"','&quot;').replaceAll("'",'&apos;');
/** Area-average resampling from the required opaque square master. */
export function iconPng(image:RgbaImage,size:number):Buffer {
 const pixels=Buffer.alloc(size*size*4);const ratio=image.width/size;
 for(let y=0;y<size;y++)for(let x=0;x<size;x++){let weight=0;const sums=[0,0,0];for(let sy=Math.floor(y*ratio);sy<Math.ceil((y+1)*ratio);sy++)for(let sx=Math.floor(x*ratio);sx<Math.ceil((x+1)*ratio);sx++){
  const w=(Math.min(sx+1,(x+1)*ratio)-Math.max(sx,x*ratio))*(Math.min(sy+1,(y+1)*ratio)-Math.max(sy,y*ratio));weight+=w;for(let c=0;c<3;c++)sums[c]!+=image.pixels[(sy*image.width+sx)*4+c]!*w;
 }const at=(y*size+x)*4;for(let c=0;c<3;c++)pixels[at+c]=Math.round(sums[c]!/weight);pixels[at+3]=255;}
 return encodeRgbaPng({width:size,height:size,pixels});
}
export function icns(images:Map<number,Buffer>):Buffer {
 const kinds:Record<number,string>={16:'icp4',32:'icp5',64:'icp6',128:'ic07',256:'ic08',512:'ic09',1024:'ic10'},chunks=[];
 for(const [size,bytes] of images){const kind=kinds[size];if(!kind)continue;const h=Buffer.alloc(8);h.write(kind);h.writeUInt32BE(bytes.length+8,4);chunks.push(h,bytes);}
 const header=Buffer.alloc(8);header.write('icns');header.writeUInt32BE(8+chunks.reduce((n,b)=>n+b.length,0),4);return Buffer.concat([header,...chunks]);
}
export function plistString(source:string,key:string,value:string):string {
 if(!source.includes('<plist')||!source.includes('</dict>'))throw Error('Branding requires an XML application Info.plist.');
 const entry='<key>'+key+'</key><string>'+xml(value)+'</string>',pattern=new RegExp('<key>\\s*'+key+'\\s*</key>\\s*<string>[\\s\\S]*?</string>');
 return pattern.test(source)?source.replace(pattern,()=>entry):source.replace(/<\/dict>(\s*<\/plist>\s*)$/,()=>entry+'\n</dict></plist>');
}
export async function brandWindows(file:string,name:string|undefined,images:Map<number,Buffer>):Promise<void>{
 const {NtExecutable,NtExecutableResource,Resource,Data}=await import('resedit');
 let exe;try{exe=NtExecutable.from(await readFile(file));}catch(error){throw Error('Windows branding requires a valid unsigned PE runtime. Signed runtimes are not modified; sign the branded game after export. '+(error as Error).message);}
 const resource=NtExecutableResource.from(exe);
 if(images.size){const icons=[16,32,48,64,128,256].map(size=>Data.RawIconItem.from(images.get(size)!,size,size,32));const groups=Resource.IconGroupEntry.fromEntries(resource.entries);if(groups.length)for(const group of groups)Resource.IconGroupEntry.replaceIconsForResource(resource.entries,group.id,group.lang,icons);else Resource.IconGroupEntry.replaceIconsForResource(resource.entries,1,0,icons);}
 if(name){const versions=Resource.VersionInfo.fromEntries(resource.entries);if(!versions.length)versions.push(Resource.VersionInfo.createEmpty());for(const version of versions){const langs=version.getAllLanguagesForStringValues();if(!langs.length)langs.push({lang:1033,codepage:1200});for(const lang of langs)version.setStringValues(lang,{...version.getStringValues(lang),ProductName:name,FileDescription:name});version.outputToResourceEntries(resource.entries);}}
 resource.outputResource(exe);const bytes=Buffer.from(exe.generate()),check=NtExecutableResource.from(NtExecutable.from(bytes));
 if(images.size){const groups=Resource.IconGroupEntry.fromEntries(check.entries);if(!groups.length||groups.some(group=>{const items=group.getIconItemsFromEntries(check.entries);return [16,32,48,64,128,256].some(size=>!items.some(item=>item.isRaw()&&(item.width||256)===size&&decodePng(Buffer.from(item.bin)).width===size&&Buffer.from(item.bin).equals(images.get(size)!)));}))throw Error('Windows icon resource readback failed.');}
 if(name&&!Resource.VersionInfo.fromEntries(check.entries).every(v=>v.getAllLanguagesForStringValues().every(l=>v.getStringValues(l).ProductName===name&&v.getStringValues(l).FileDescription===name)))throw Error('Windows application name readback failed.');
 await writeFile(file,bytes);
}
/** Called before platform signing and before release archives are made. */
export async function applyReleaseBranding(directory:string,target:ExportTarget,prepared:PreparedRelease):Promise<void>{
 const {displayName,icon}=prepared.metadata;if(!displayName&&!icon)return;
 const data=gameDirectory(target,directory),images=new Map<number,Buffer>();
 if(icon){const image=decodePng(prepared.files.find(f=>f.path===icon.path)!.bytes);for(const size of [16,20,29,32,40,48,58,60,64,76,80,87,120,128,152,167,180,192,256,512,1024])images.set(size,iconPng(image,size));}
 const resources:Record<string,string>={};const put=async(relative:string,bytes:Buffer|string)=>{const file=path.join(directory,relative);await mkdir(path.dirname(file),{recursive:true});await writeFile(file,bytes,{flag:'wx'});resources[relative]=releaseHash(typeof bytes==='string'?Buffer.from(bytes):bytes);};
 if(target==='web'){
  let html=await readFile(path.join(directory,'index.html'),'utf8');if(displayName){if(/<title>[\s\S]*?<\/title>/i.test(html))html=html.replace(/<title>[\s\S]*?<\/title>/i,()=>'<title>'+xml(displayName)+'</title>');else html=html.replace('</head>','<title>'+xml(displayName)+'</title></head>');}
  if(icon){for(const size of [192,512])await put('branding/icon-'+size+'.png',images.get(size)!);await put('branding/icon-32.png',images.get(32)!);}
  const manifest={name:displayName??'Raising Game',short_name:displayName??'Raising Game',start_url:'./',display:'standalone',...(icon?{icons:[192,512].map(size=>({src:'branding/icon-'+size+'.png',sizes:size+'x'+size,type:'image/png'}))}:{})};
  await put('manifest.webmanifest',JSON.stringify(manifest,null,2));html=html.replace('</head>','<link rel="manifest" href="manifest.webmanifest">'+(icon?'<link rel="icon" type="image/png" sizes="32x32" href="branding/icon-32.png">':'')+'</head>');await writeFile(path.join(directory,'index.html'),html);
 }else if(target==='macos'){
  const file=path.join(directory,'RaisingPlayer.app/Contents/Info.plist');let plist=await readFile(file,'utf8');if(displayName)for(const key of ['CFBundleName','CFBundleDisplayName'])plist=plistString(plist,key,displayName);
  if(icon){await put('RaisingPlayer.app/Contents/Resources/ReleaseIcon.icns',icns(images));plist=plistString(plist,'CFBundleIconFile','ReleaseIcon.icns');}await writeFile(file,plist);
 }else if(target==='windows'){if(displayName||icon)await brandWindows(playerExecutable(target,directory),displayName,images);
 }
 if(icon){const relative=path.relative(directory,path.join(data,'branding/app-icon.png')).split(path.sep).join('/');await put(relative,prepared.files.find(f=>f.path===icon.path)!.bytes);}
 await writeFile(path.join(data,'release-branding.json'),JSON.stringify({formatVersion:1,...(displayName?{displayName}:{}),icon:icon?{source:icon.path,sha256:icon.sha256}:null,resources,windowsSigning:target==='windows'?'Unsigned branded executable; sign this final export before public distribution.':undefined},null,2)+'\n',{flag:'wx'});
}

export async function writeIOSIconCatalog(directory:string,bytes:Buffer):Promise<void>{
 const image=decodePng(bytes),cache=new Map<number,Buffer>(),folder=path.join(directory,'branding/AppIcon.xcassets/AppIcon.appiconset');await mkdir(folder,{recursive:true});const entries=[];
 for(const [idiom,size,scales] of [['iphone',20,[2,3]],['iphone',29,[2,3]],['iphone',40,[2,3]],['iphone',60,[2,3]],['ipad',20,[1,2]],['ipad',29,[1,2]],['ipad',40,[1,2]],['ipad',76,[1,2]],['ipad',83.5,[2]],['ios-marketing',1024,[1]]] as const)for(const scale of scales){const pixels=size*scale,filename='icon-'+pixels+'.png';if(!cache.has(pixels)){const png=iconPng(image,pixels);cache.set(pixels,png);await writeFile(path.join(folder,filename),png,{flag:'wx'});}entries.push({idiom,size:size+'x'+size,scale:scale+'x',filename});}
 await writeFile(path.join(folder,'Contents.json'),JSON.stringify({images:entries,info:{version:1,author:'RaisingEngine'}},null,2),{flag:'wx'});await writeFile(path.join(folder,'../Contents.json'),JSON.stringify({info:{version:1,author:'RaisingEngine'}}),{flag:'wx'});
}
