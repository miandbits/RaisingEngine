/** Presentation-only translation. Stable ID segments survive list reordering. */
export interface Locale {id:string;label:string;fontAssetId?:string}
export interface Message {key:string;source:string;translations:Record<string,string>}
export interface Localization {defaultLocale:string;fallbackLocale:string;locales:readonly Locale[];entries:readonly Message[]}
export interface SourceMessage {key:string;source:string}
const fields=new Set(['title','description','label','name','text','prompt','valueFormat']);
const skip=new Set(['localization','assets','initialState']);
const segment=(s:string)=>s.replace(/~/g,'~0').replace(/\//g,'~1');
function visit(value:unknown,path:string,found:(key:string,source:string)=>string):unknown {
 if(Array.isArray(value))return value.map((x,i)=>visit(x,path+'/'+segment(x&&typeof x==='object'&&typeof x.id==='string'?'@'+x.id:String(i)),found));
 if(!value||typeof value!=='object')return value;
 const out:Record<string,unknown>={};
 for(const [key,v] of Object.entries(value)){const p=path+'/'+segment(key);out[key]=skip.has(key)?v:typeof v==='string'&&fields.has(key)&&v.trim()?found(p,v):visit(v,p,found);}
 return out;
}
export function extractMessages(content:unknown):SourceMessage[]{const result:SourceMessage[]=[];visit(content,'',(key,source)=>{result.push({key,source});return source;});return result;}
export function contentFromDocuments(documents:readonly {kind:string;data:unknown}[]):Record<string,unknown>{
 const result:Record<string,unknown>={};const collections:Record<string,string>={character:'characters',activity:'activities',event:'events',ending:'endings'};
 for(const d of documents){if(d.kind==='project')result.project=d.data;else if(d.kind==='attributes'||d.kind==='flags')result[d.kind]=(d.data as Record<string,unknown>)[d.kind];else if(collections[d.kind]){const key=collections[d.kind]!;((result[key]??=[]) as unknown[]).push(d.data);}}
 return result;
}
export function mergeCatalog(sources:readonly SourceMessage[],entries:readonly Message[]):Message[]{const known=new Map(entries.map(e=>[e.key,e]));return [...entries.map(e=>structuredClone(e)),...sources.filter(e=>!known.has(e.key)).map(e=>({...e,translations:{}}))];}
export function localeId(config:Localization|undefined,requested?:string):string {return config?.locales.some(l=>l.id===requested)?requested!:config?.defaultLocale??'';}
export function translate(entry:Message|undefined,source:string,config:Localization|undefined,locale:string):string {
 if(!config||!entry||entry.source!==source)return source;
 for(const id of [locale,config.fallbackLocale]){const text=entry.translations[id];if(text?.trim())return text;}
 return source;
}
export function localizeContent<T>(content:T,config:Localization|undefined,requested?:string):T {
 if(!config)return content;
 const locale=localeId(config,requested),entries=new Map(config.entries.map(e=>[e.key,e]));
 const out=visit(content,'',(key,source)=>translate(entries.get(key),source,config,locale)) as T;
 const font=config.locales.find(l=>l.id===locale)?.fontAssetId;
 // An explicitly selected locale font overrides per-element fonts for glyph coverage.
 const project=(out as {project?:{visual?:{defaultStyle?:Record<string,unknown>;screens:{elements:Record<string,unknown>[]}[];components?:{elements:Record<string,unknown>[]}[]}}}).project;
 if(font&&project?.visual){project.visual.defaultStyle={...project.visual.defaultStyle,fontAssetId:font};for(const screen of [...project.visual.screens,...project.visual.components??[]])for(const e of screen.elements)e.fontAssetId=font;}
 return out;
}
export const placeholders=(value:string)=>[...new Set(value.match(/\{[A-Za-z][A-Za-z0-9_]*\}/g)??[])].sort().join('|');
