import path from 'node:path';
import type { ExportTarget } from './model.js';
export const desktopTarget = (platform = process.platform): 'windows' | 'macos' => platform === 'darwin' ? 'macos' : 'windows';
export function gameDirectory(target: ExportTarget, directory: string): string {
  return target === 'macos' ? path.join(directory, 'RaisingPlayer.app/Contents/Resources') : directory;
}
export function playerExecutable(target: ExportTarget, directory: string): string {
  if (target === 'macos') return path.join(directory, 'RaisingPlayer.app/Contents/MacOS/RaisingPlayer');
  if (target === 'windows') return path.join(directory, 'RaisingPlayer.exe');
  throw new Error(`No desktop executable for ${target}`);
}
