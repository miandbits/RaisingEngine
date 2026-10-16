import { isObject, type JsonObject, type ProjectDocument } from './model.js';
import { SemanticContext, objectItems } from './context.js';

export function checkGallery(ctx: SemanticContext, doc: ProjectDocument, gallery: JsonObject): void {
  const visual=doc.data as JsonObject;
  const screens=objectItems(isObject(visual.visual)?visual.visual.screens:undefined);
  for(const key of ['screenId','detailScreenId'])if(gallery[key]&&!screens.some(s=>s.id===gallery[key]))ctx.report(doc,`/gallery/${key}`,'GALLERY_SCREEN_REFERENCE','도감에 연결한 화면이 없습니다.');
  if(gallery.detailScreenId&&!gallery.screenId)ctx.report(doc,'/gallery/detailScreenId','GALLERY_SCREEN_REQUIRED','상세 화면에는 도감 목록 화면도 연결하세요.');
  if(gallery.screenId&&gallery.screenId===gallery.detailScreenId)ctx.report(doc,'/gallery/detailScreenId','GALLERY_SCREEN_DISTINCT','목록과 상세는 서로 다른 화면으로 연결하세요.');
  const seen = new Set<string>();
  objectItems(gallery.entries).forEach((entry, i) => {
    const path = `/gallery/entries/${i}`;
    const id = String(entry.id);
    if (seen.has(id)) ctx.report(doc, `${path}/id`, 'DUPLICATE_ID', `컬렉션 ID '${id}'가 중복됩니다.`, [id]);
    seen.add(id);
    if (entry.kind === 'cg' || entry.kind === 'music') {
      const asset = ctx.resolve(ctx.assets, String(entry.sourceId), doc, `${path}/sourceId`, '도감 에셋');
      if (asset && isObject(asset.data) && asset.data.kind !== (entry.kind === 'cg' ? 'image' : 'audio')) ctx.report(doc, `${path}/sourceId`, 'ASSET_TYPE', 'CG는 이미지, 음악은 오디오 에셋이어야 합니다.');
    } else ctx.resolve(entry.kind === 'event' ? ctx.events : ctx.endings, String(entry.sourceId), doc, `${path}/sourceId`, '회상 대본');
    if (typeof entry.thumbnailAssetId === 'string') {
      const asset = ctx.resolve(ctx.assets, entry.thumbnailAssetId, doc, `${path}/thumbnailAssetId`, '썸네일');
      if (asset && isObject(asset.data) && asset.data.kind !== 'image') ctx.report(doc, `${path}/thumbnailAssetId`, 'ASSET_TYPE', '도감 썸네일은 이미지여야 합니다.');
    }
  });
}
