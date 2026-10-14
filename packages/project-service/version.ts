import { fileHash } from './transaction.js';

/** File bytes (including formatting) and membership, scoped to a project root. */
export function projectVersion(root: string, files: ReadonlyMap<string, string>): string {
  return `project-v1:${fileHash(JSON.stringify([root, [...files].sort(([a], [b]) => a < b ? -1 : a > b ? 1 : 0)]))}`;
}

export class ProjectConflict extends Error {
  readonly code = 'VERSION_CONFLICT';
  constructor(readonly expectedVersion: string, readonly actualVersion: string, readonly files: string[]) {
    super(`외부 변경 충돌(수정·추가·삭제): ${files.join(', ') || '프로젝트 버전'}. 초안은 유지했습니다. 다시 불러온 뒤 변경을 검토하세요.`);
  }
}
