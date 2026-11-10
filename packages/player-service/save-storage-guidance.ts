import {slotWords} from './save-slots.js';

/** Host capabilities, never inferred from a browser's user agent or authored text. */
export interface SaveStorageGuidance {
  location: 'browser' | 'device' | 'file' | 'preview';
  available?: boolean;
  writeFailed?: boolean;
  portableBackup?: boolean;
}

export function saveStorageGuidanceText(storage?: SaveStorageGuidance, locale?: string): string {
  const words=(ko:string,en:string,ja:string)=>slotWords(locale,ko,en,ja);
  if(!storage||storage.location==='preview')return words(
    '저장 위치와 백업 방법은 플레이어에 따라 달라집니다. 내보낸 게임에서 확인하세요.',
    'Save location and backup options depend on the player. Check them in the exported game.',
    '保存先とバックアップ方法はプレイヤーによって異なります。書き出したゲームで確認してください。');
  if(storage.location==='browser'&&storage.writeFailed)return words(
    '최근 기록을 저장하지 못했습니다. 백업 내려받기로 현재 진행을 보관하세요.',
    'The last record could not be saved. Use Download backup to keep your current progress.',
    '直前の記録を保存できませんでした。バックアップ保存で現在の進行を残してください。');
  if(storage.location==='browser')return storage.available===false?words(
    '브라우저 저장소를 열지 못했습니다. 백업 내려받기로 현재 진행을 보관하세요.',
    'Browser storage is unavailable. Use Download backup to keep your current progress.',
    'ブラウザーの保存領域を開けません。バックアップ保存で現在の進行を残してください。'):words(
    '기록은 이 브라우저에 보관됩니다. 브라우저 데이터를 지우기 전에 백업을 내려받으세요.',
    'Records stay in this browser. Use Download backup before clearing browser data.',
    '記録はこのブラウザーに保存されます。ブラウザーのデータを消す前にバックアップを保存してください。');
  if(storage.location==='file')return words(
    '진행은 내려받은 세이브 파일에 보관됩니다. 이어 하려면 이 파일을 보관하세요.',
    'Progress is kept in downloaded save files. Keep the file to resume later.',
    '進行はダウンロードしたセーブファイルに保存されます。続けるためにファイルを保管してください。');
  return storage.portableBackup?words(
    '기록은 이 기기에 보관됩니다. 현재 진행 내보내기로 파일에 백업하세요. 자동 동기화되지 않습니다.',
    'Records stay on this device. Use Export current progress for a Files backup; records do not sync automatically.',
    '記録はこの端末に保存されます。「現在の進行を書き出す」でファイルにバックアップできます。自動同期はされません。'):words(
    '기록은 이 기기에 보관됩니다. 다른 기기로 자동 동기화되지 않습니다.',
    'Records stay on this device. They do not sync automatically to other devices.',
    '記録はこの端末に保存されます。他の端末には自動同期されません。');
}
