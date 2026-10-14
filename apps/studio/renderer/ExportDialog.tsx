import React, { useState } from 'react';
import type { ExportTarget } from '../../../packages/export-service/model.js';
export function ExportDialog({ onClose, onExport, busy, error }: { onClose: () => void; onExport: (target: ExportTarget) => void; busy: boolean; error: string | null }) {
  const [target, setTarget] = useState<ExportTarget>('windows');
  return <div className="modal-backdrop"><section className="studio-modal export-dialog" role="dialog" aria-modal="true" aria-label="게임 내보내기">
    <h2>게임 내보내기</h2><p>현재 초안을 검증하고, 이미지와 실행에 필요한 파일을 새 폴더에 묶습니다.</p>
    <label>실행 대상<select aria-label="내보내기 대상" value={target} disabled={busy} onChange={e => setTarget(e.target.value as ExportTarget)}>
      <option value="windows">Windows · 독립 C++ 플레이어</option><option value="web">웹 · 브라우저 플레이어</option>
    </select></label>
    <p>{target === 'windows' ? '내보낸 폴더의 RaisingPlayer.exe를 실행합니다. 편집기를 설치하지 않아도 플레이할 수 있습니다.' : '내보낸 폴더 전체를 웹 호스팅에 올립니다. C++ 코어를 WebAssembly로 실행하며 저장 파일을 내려받아 이어갈 수 있습니다.'}</p>
    <p className="muted">모바일 앱·macOS·Linux·콘솔 패키징은 아직 제공되지 않습니다.</p>
    {error && <p role="alert" className="export-error">{error}</p>}
    <footer><button disabled={busy} onClick={onClose}>닫기</button><button className="studio-primary" disabled={busy} onClick={() => onExport(target)}>새 폴더로 내보내기</button></footer>
  </section></div>;
}
