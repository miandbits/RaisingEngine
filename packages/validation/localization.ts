import {contentFromDocuments,extractMessages,placeholders,type Localization} from '../localization/catalog.js';
import type {SemanticContext} from './context.js';
import type {ProjectDocument} from './model.js';
export function checkLocalization(ctx:SemanticContext,doc:ProjectDocument,documents:readonly ProjectDocument[]):void{
 const config=(doc.data as unknown as {localization?:Localization}).localization;if(!config)return;
 const locales=new Set<string>();
 for(const [i,l] of config.locales.entries()){
  if(locales.has(l.id))ctx.report(doc,`/localization/locales/${i}/id`,'LOCALE_DUPLICATE','언어 ID가 중복되었습니다.');locales.add(l.id);
  if(l.fontAssetId){const font=ctx.resolve(ctx.assets,l.fontAssetId,doc,`/localization/locales/${i}/fontAssetId`,'폰트');if(font&&font.data.kind!=='font')ctx.report(doc,`/localization/locales/${i}/fontAssetId`,'LOCALE_FONT_KIND','언어에는 폰트 에셋을 연결하세요.');}
 }
 for(const field of ['defaultLocale','fallbackLocale'] as const)if(!locales.has(config[field]))ctx.report(doc,'/localization/'+field,'LOCALE_REFERENCE','등록된 언어를 선택하세요.');
 const sources=new Map(extractMessages(contentFromDocuments(documents)).map(e=>[e.key,e.source])),keys=new Set<string>();
 for(const [i,e] of config.entries.entries()){
  const p=`/localization/entries/${i}`;
  if(keys.has(e.key))ctx.report(doc,p+'/key','TRANSLATION_DUPLICATE','번역 키가 중복되었습니다.');keys.add(e.key);
  if(!sources.has(e.key))ctx.report(doc,p+'/key','TRANSLATION_UNUSED','연결된 원문이 없습니다. 삭제되었거나 ID가 변경되었습니다.',[],'warning');
  else if(sources.get(e.key)!==e.source)ctx.report(doc,p+'/source','TRANSLATION_STALE','원문이 변경되었습니다. 원문 확인 전에는 번역 대신 현재 원문을 표시합니다.',[],'warning');
  for(const [locale,text] of Object.entries(e.translations)){
   if(!locales.has(locale))ctx.report(doc,p+'/translations/'+locale,'TRANSLATION_LOCALE','등록되지 않은 언어의 번역입니다.');
   if(text.trim()&&placeholders(text)!==placeholders(e.source))ctx.report(doc,p+'/translations/'+locale,'TRANSLATION_PLACEHOLDERS','원문과 번역의 {변수}가 일치하지 않습니다.');
  }
 }
 const missing=config.entries.filter(e=>!e.translations[config.defaultLocale]?.trim()).length;
 if(missing)ctx.report(doc,'/localization/entries','TRANSLATION_FALLBACK',`${missing}개 문구는 기본 언어 번역이 없어 대체 언어 또는 원문으로 표시합니다.`,[],'warning');
}
