import { createHash } from 'node:crypto';
import { readFile, realpath, stat } from 'node:fs/promises';
import path from 'node:path';
import type { ContentSnapshot, ContentData } from '../core/model.js';
import { freeze } from '../core/util.js';
import type { ContentByKind } from '../schema/types.js';
import { validateProject } from '../validation/index.js';
import { diagnostic, sortDiagnostics, type Diagnostic, type LoadedProject } from '../validation/model.js';
import { isWithin } from './loader.js';

export type SnapshotResult = { ok: true; snapshot: ContentSnapshot; diagnostics: Diagnostic[] }
  | { ok: false; diagnostics: Diagnostic[] };

/** v1: object keys use UTF-16 ordinal order; arrays retain their semantic order. */
export function canonicalJson(value: unknown): string {
  if (Array.isArray(value)) return `[${value.map(canonicalJson).join(',')}]`;
  if (value !== null && typeof value === 'object') {
    const obj = value as Record<string, unknown>;
    return `{${Object.keys(obj).sort().map(key => `${JSON.stringify(key)}:${canonicalJson(obj[key])}`).join(',')}}`;
  }
  return JSON.stringify(value);
}
const hash = (input: string | Buffer) => createHash('sha256').update(input).digest('hex');

/**
 * A deep copy of a loaded project that later editor changes (or the caller's own edits) cannot alter.
 * Source locations depend only on a document's text and never change, so they are shared, not
 * cloned: a large plugin document has ~100k of them, possibly still unbuilt (see parseDocument),
 * and a structured clone of an unbuilt location map would be empty.
 */
export function detachProject(loaded: LoadedProject): LoadedProject {
  const detached = structuredClone({ ...loaded, documents: loaded.documents.map(({ locations: _locations, ...document }) => document) }) as LoadedProject;
  detached.documents = detached.documents.map((document, i) => ({ ...document, locations: loaded.documents[i]!.locations }));
  return detached;
}

export async function buildSnapshot(loaded: LoadedProject): Promise<SnapshotResult> {
  // Detach before awaiting validation: subsequent editor changes cannot alter this snapshot.
  const detached = detachProject(loaded);
  const validation = await validateProject(detached);
  if (!validation.valid) return { ok: false, diagnostics: validation.diagnostics };
  const documents = validation.structuredDocuments;
  const all = <K extends keyof ContentByKind>(kind: K): ContentByKind[K][] => documents.filter(d => d.kind === kind).map(d => {
    const data = structuredClone(d.data) as ContentByKind[K];
    delete data.$schema;
    return data;
  });
  const byId = <T extends { id: string }>(items: T[]) => items.sort((a, b) => a.id < b.id ? -1 : a.id > b.id ? 1 : 0);
  const content: ContentData = {
    project: all('project')[0]!, attributes: all('attributes')[0]!.attributes, flags: all('flags')[0]!.flags,
    characters: byId(all('character')), activities: byId(all('activity')), events: byId(all('event')), endings: byId(all('ending')),
    assets: all('assets')[0]?.assets ?? [],
  };
  const assetHashes: Record<string, string> = {};
  for (const doc of documents.filter(d => d.kind === 'assets')) {
    const source = detached.documents.find(d => d.file === doc.file)!;
    for (const [i, asset] of doc.data.assets.entries()) {
      const pointer = `/assets/${i}/path`;
      try {
        const resolved = await realpath(path.resolve(detached.root, asset.path));
        if (!isWithin(detached.root, resolved)) {
          validation.diagnostics.push(diagnostic(source, pointer, 'ASSET_OUTSIDE_PROJECT', '에셋이 프로젝트 밖을 가리킵니다.', [asset.id]));
        } else if (!(await stat(resolved)).isFile()) {
          validation.diagnostics.push(diagnostic(source, pointer, 'ASSET_NOT_FILE', '에셋 경로는 일반 파일이어야 합니다.', [asset.id]));
        } else assetHashes[asset.id] = hash(await readFile(resolved));
      } catch (error) {
        const code = (error as NodeJS.ErrnoException).code;
        validation.diagnostics.push(diagnostic(source, pointer, code === 'ENOENT' ? 'ASSET_MISSING' : 'ASSET_READ', `에셋을 읽을 수 없습니다: ${code ?? 'UNKNOWN'}`, [asset.id]));
      }
    }
  }
  sortDiagnostics(validation.diagnostics);
  if (validation.diagnostics.some(d => d.severity === 'error')) return { ok: false, diagnostics: validation.diagnostics };
  const payload = { snapshotVersion: '1' as const, ...content, assetHashes, ...(validation.ruleLock ? { ruleExtensionLock: validation.ruleLock } : {}) };
  const canonical = canonicalJson(payload);
  // Normalize the returned objects too, so equivalent source key order cannot leak into replay logs.
  const normalized = JSON.parse(canonical) as typeof payload;
  return { ok: true, snapshot: freeze({ ...normalized, contentFingerprint: `sha256:${hash(canonical)}` }), diagnostics: validation.diagnostics };
}
