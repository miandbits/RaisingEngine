import { createHash } from 'node:crypto';
import type { ContentSnapshot } from '../core/model.js';
import type { GameExtensions } from '../plugin-host/bundle.js';
import { canonicalJson } from '../project-service/snapshot.js';
import type { GameBundle, PresentationRevision } from './model.js';

function verifySnapshot(content: ContentSnapshot): void {
  const { contentFingerprint, ...payload } = content;
  const expected = `sha256:${createHash('sha256').update(canonicalJson(payload)).digest('hex')}`;
  if (contentFingerprint !== expected) throw new Error('PRESENTATION_BASE_INVALID: snapshot fingerprint does not match its content.');
  const ids = content.assets.map(asset => asset.id).sort();
  if (canonicalJson(ids) !== canonicalJson(Object.keys(content.assetHashes).sort()) ||
      Object.values(content.assetHashes).some(value => !/^[a-f0-9]{64}$/.test(value)))
    throw new Error('PRESENTATION_BASE_INVALID: asset checksums are incomplete.');
}

/** Keep a published simulation snapshot intact while explicitly revising its image files.
 * This is deliberately narrower than save migration: no rules, layouts, text, audio,
 * fonts or plugin documents/runtimes may change. Actual source bytes are still checked
 * against the NEW snapshot by exportGame before publishing.
 */
export function preparePresentationRevision(content: ContentSnapshot, base: GameBundle,
  extensions?: GameExtensions): { content: ContentSnapshot; presentationRevision: PresentationRevision } {
  if (base.formatVersion !== '1') throw new Error('PRESENTATION_BASE_INVALID: unsupported bundle.');
  verifySnapshot(base.content); verifySnapshot(content);
  const { contentFingerprint: _oldFingerprint, assetHashes: oldHashes, ...oldData } = base.content;
  const { contentFingerprint: _newFingerprint, assetHashes: newHashes, ...newData } = content;
  if (canonicalJson(oldData) !== canonicalJson(newData))
    throw new Error('PRESENTATION_CONTENT_CHANGED: image-only updates cannot change content, rules, layouts or asset definitions.');
  if (canonicalJson(base.extensions ?? null) !== canonicalJson(extensions ?? null))
    throw new Error('PRESENTATION_PLUGINS_CHANGED: image-only updates cannot change plugin documents or runtimes.');
  const images: PresentationRevision['images'] = {};
  for (const asset of content.assets) {
    const previous = oldHashes[asset.id]!, current = newHashes[asset.id]!;
    if (previous === current) continue;
    if (asset.kind !== 'image') throw new Error(`PRESENTATION_ASSET_CHANGED: only image bytes may change (${asset.id}).`);
    images[asset.id] = { baselineSha256: previous, sha256: current };
  }
  return { content: structuredClone(base.content), presentationRevision: {
    formatVersion: '1', sourceContentFingerprint: content.contentFingerprint, images,
  } };
}
