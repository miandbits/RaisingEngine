import { isObject, type JsonObject, type ProjectDocument } from './model.js';
import { SemanticContext, objectItems } from './context.js';

export function checkGallery(ctx: SemanticContext, doc: ProjectDocument, gallery: JsonObject): void {
  const seen = new Set<string>();
  objectItems(gallery.entries).forEach((entry, i) => {
    const path = `/gallery/entries/${i}`;
    const id = String(entry.id);
    if (seen.has(id)) ctx.report(doc, `${path}/id`, 'DUPLICATE_ID', `컬렉션 ID '${id}'가 중복됩니다.`, [id]);
    seen.add(id);
    ctx.resolve(entry.kind === 'event' ? ctx.events : ctx.endings, String(entry.sourceId), doc, `${path}/sourceId`, '회상 대본');
    if (typeof entry.thumbnailAssetId === 'string') {
      const asset = ctx.resolve(ctx.assets, entry.thumbnailAssetId, doc, `${path}/thumbnailAssetId`, '썸네일');
      if (asset && isObject(asset.data) && asset.data.kind !== 'image') ctx.report(doc, `${path}/thumbnailAssetId`, 'ASSET_TYPE', '도감 썸네일은 이미지여야 합니다.');
    }
  });
}
