import {createHash} from 'node:crypto';
import {readFile,realpath,stat,writeFile} from 'node:fs/promises';
import path from 'node:path';
import type {StudioEditor} from './editor.js';
/** Validate the SFNT directory before either GUI or MCP can register an immutable font. */
export function validateFont(bytes:Buffer):void {
 if(bytes.length<12||bytes.length>32*1024*1024||![0x00010000,0x4f54544f].includes(bytes.readUInt32BE(0)))throw Error('32 MiB 이하의 TTF 또는 OTF 파일이 필요합니다.');
 const count=bytes.readUInt16BE(4);if(count<1||count>256||12+count*16>bytes.length)throw Error('폰트 테이블 목록이 손상되었습니다.');
 const tags=new Set<string>();
 for(let i=0;i<count;i++){const p=12+i*16,tag=bytes.toString('ascii',p,p+4),offset=bytes.readUInt32BE(p+8),length=bytes.readUInt32BE(p+12);if(tags.has(tag)||offset>bytes.length||length>bytes.length-offset)throw Error('폰트 테이블 범위가 잘못되었습니다.');tags.add(tag);}
 if(!['name','cmap','head'].every(t=>tags.has(t)))throw Error('필수 폰트 테이블이 없습니다.');
}
export async function importFont(editor:StudioEditor,source:string,revision:number):Promise<string>{
 editor.current(revision);if(!editor.root||editor.readOnly)throw Error('편집 가능한 프로젝트를 열어 주세요.');
 const ext=path.extname(source).toLowerCase();if(!['.ttf','.otf'].includes(ext)||(await stat(source)).size>32*1024*1024)throw Error('32 MiB 이하의 TTF 또는 OTF 파일을 선택하세요.');
 const bytes=await readFile(source);validateFont(bytes);
 const root=await realpath(editor.root),directory=await realpath(path.join(root,'assets'));
 const inside=(p:string)=>{const rel=path.relative(root,p);return !rel.startsWith('..')&&!path.isAbsolute(rel);};
 if(!inside(directory))throw Error('프로젝트 밖의 에셋 경로입니다.');
 const id='font-'+createHash('sha256').update(bytes).digest('hex').slice(0,24),manifest=editor.documents.find(d=>d.kind==='assets');
 if(!manifest)throw Error('에셋 목록이 없습니다.');
 const data=manifest.data as {assets:{id:string;kind:string;path:string}[]},existing=data.assets.find(a=>a.id===id);
 editor.current(revision);
 if(existing){const actual=await realpath(path.resolve(root,existing.path));if(existing.kind!=='font'||!inside(actual)||!(await readFile(actual)).equals(bytes))throw Error('기존 폰트 ID가 다른 파일을 가리킵니다.');editor.current(revision);return id;}
 const destination=path.join(directory,id+ext);
 try{await writeFile(destination,bytes,{flag:'wx'});}catch(error){if((error as NodeJS.ErrnoException).code!=='EEXIST')throw error;if(await realpath(destination)!==destination||!(await readFile(destination)).equals(bytes))throw Error('같은 이름의 다른 파일이 있습니다.');}
 editor.current(revision);editor.edit({type:'replace',file:manifest.file,data:{...data,assets:[...data.assets,{id,kind:'font',path:'assets/'+id+ext}]}},revision);return id;
}
