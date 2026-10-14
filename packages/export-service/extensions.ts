import { createHash } from 'node:crypto';
import { readFile, readdir, realpath, mkdir, writeFile, lstat } from 'node:fs/promises';
import path from 'node:path';
import type { ContentSnapshot } from '../core/model.js';
import Ajv2020 from 'ajv/dist/2020.js';

export interface RuntimeExtensionBundle { version: 1; plugins: Array<{id:string;version:string;abi:string;entry:string;documents:unknown[]}> }
const validId=(v:string)=>/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(v);
async function within(root:string,relative:string){
  if(path.isAbsolute(relative)||relative.split(/[\\/]/).some(x=>x==='..'))throw new Error('PLUGIN_PATH: '+relative);
  const resolved=await realpath(path.join(root,relative)),rel=path.relative(await realpath(root),resolved);
  if(rel.startsWith('..')||path.isAbsolute(rel)||!(await lstat(resolved)).isFile())throw new Error('PLUGIN_PATH: '+relative);
  return resolved;
}
/** Package only explicitly locked runtime modules. Never load authoring code during export. */
export async function prepareExtensions(projectRoot:string,content:ContentSnapshot,target:'windows'|'web') {
  const elements=[...(content.project.visual?.screens??[]).flatMap(s=>s.elements),...(content.project.visual?.components??[]).flatMap(c=>c.elements)].filter(e=>e.kind==='plugin');
  if(!elements.length)return null;
  const lock=JSON.parse(await readFile(path.join(projectRoot,'plugins/lock.json'),'utf8'));
  if(lock.formatVersion!=='0.1.0'||!Array.isArray(lock.plugins))throw new Error('PLUGIN_LOCK: invalid lock document');
  const store=path.resolve(process.env.RAISING_PLUGIN_STORE??path.join(import.meta.dirname,'../../../plugin-store'));
  const files=new Map<string,Uint8Array>(),bundle:RuntimeExtensionBundle={version:1,plugins:[]};
  for(const id of new Set(elements.map(e=>e.extension!.plugin))) {
    const entries=lock.plugins.filter((p:{id:string})=>p.id===id);
    if(entries.length!==1||entries[0].enabled!==true)throw new Error('PLUGIN_NOT_ENABLED: '+id);
    const pin=entries[0];if(!validId(id)||!/^\d+\.\d+\.\d+$/.test(pin.version))throw new Error('PLUGIN_LOCK: invalid identity');
    const installed=path.join(store,'installed',id,pin.version);
    const manifest=JSON.parse(await readFile(path.join(installed,'plugin.json'),'utf8'));
    if(manifest.id!==id||manifest.version!==pin.version||manifest.manifestVersion!==1||manifest.engineApi?.min>1||manifest.engineApi?.max<1)throw new Error('PLUGIN_API_INCOMPATIBLE: '+id);
    const module=manifest.modules.find((m:{role:string;platform:string})=>m.role==='runtime'&&m.platform===(target==='windows'?'windows-x64':'web-wasm'));
    if(!module||module.abi!=='raising-extension-1')throw new Error('PLUGIN_RUNTIME_MISSING: '+id+' / '+target);
    const sources=[module.path,...(module.files??[])];
    for(const relative of sources) {
      const bytes=await readFile(await within(installed,relative));
      if(createHash('sha256').update(bytes).digest('hex')!==manifest.files[relative])throw new Error('PLUGIN_INTEGRITY: '+relative);
      const output=`plugins/${id}/${path.basename(relative)}`;if(files.has(output))throw new Error('PLUGIN_PATH_COLLISION: '+output);files.set(output,bytes);
    }
    const documents:any[]=[];
    for(const type of manifest.documentTypes??[]) {
      if(!validId(type.type))throw new Error('PLUGIN_DOCUMENT_TYPE: '+id);
      const directory=path.join(projectRoot,'plugins',id,type.type);
      const schema=JSON.parse(await readFile(await within(installed,type.schema),'utf8'));
      const validate=new Ajv2020.default({strict:false,allErrors:true}).compile(schema);
      let names:string[]=[];try{names=await readdir(directory);}catch(e){if((e as NodeJS.ErrnoException).code!=='ENOENT')throw e;}
      for(const name of names.filter(n=>n.endsWith('.json')).sort()) {
        const file=await within(projectRoot,path.relative(projectRoot,path.join(directory,name))),doc=JSON.parse(await readFile(file,'utf8'));
        if(doc.formatVersion!=='0.1.0'||doc.plugin!==id||doc.type!==type.type||!validId(doc.id)||name!==doc.id+'.json')throw new Error('PLUGIN_DOCUMENT: '+file);
        if(!validate(doc.data))throw new Error('PLUGIN_DOCUMENT: '+file+' '+JSON.stringify(validate.errors));
        documents.push({id:doc.id,type:doc.type,data:doc.data});
      }
    }
    for(const e of elements.filter(e=>e.extension!.plugin===id)) {
      const t=manifest.elementTypes?.find((t:{type:string})=>t.type===e.extension!.type);
      if(!t||!documents.some(d=>d.type===t.refType&&d.id===e.extension!.ref))throw new Error('PLUGIN_REFERENCE: '+e.id);
    }
    bundle.plugins.push({id,version:pin.version,abi:module.abi,entry:`plugins/${id}/${path.basename(module.path)}`,documents});
  }
  return {bundle,async write(staging:string){for(const [relative,bytes]of files){await mkdir(path.dirname(path.join(staging,relative)),{recursive:true});await writeFile(path.join(staging,relative),bytes,{flag:'wx'});}}};
}
