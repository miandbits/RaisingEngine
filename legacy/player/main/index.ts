import { app, BrowserWindow, dialog, ipcMain, protocol } from 'electron';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { readFile, realpath } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { PlayerController } from '../../../packages/player-service/controller.js';

protocol.registerSchemesAsPrivileged([{ scheme: 'raising-asset', privileges: { standard: true, secure: true, supportFetchAPI: true, stream: true } }]);
const controller = new PlayerController();
let window: BrowserWindow;
let queue: Promise<unknown> = Promise.resolve();
const renderer = new URL('../renderer/index.html', import.meta.url).href;

async function confirmReplace(): Promise<boolean> {
  if (!controller.session) return true;
  const result = await dialog.showMessageBox(window, { type: 'question', title: '진행 중인 게임',
    message: '저장하지 않은 진행 내용은 사라집니다. 계속할까요?',
    buttons: ['돌아가기', '계속하기'], defaultId: 0, cancelId: 0 });
  return result.response === 1;
}

function handle(channel: string, operation: (...args: unknown[]) => unknown): void {
  ipcMain.handle(channel, (event, ...args: unknown[]) => {
    if (event.sender !== window.webContents || event.senderFrame !== window.webContents.mainFrame
      || event.senderFrame.url !== renderer) throw new Error('허용되지 않은 요청입니다.');
    const task = queue.then(() => operation(...args)).catch(error => controller.reply(error instanceof Error ? error.message : '요청을 처리하지 못했습니다.'));
    queue = task;
    return task;
  });
}

void app.whenReady().then(async () => {
app.setName('Raising Player');
// Only declared assets with the frozen snapshot's byte hash can be read by the renderer.
protocol.handle('raising-asset', async request => {
  try {
    const url = new URL(request.url);
    if (url.host !== 'project' || url.search !== `?g=${controller.generation}`) return new Response(null, { status: 404 });
    const asset = controller.content?.assets.find(x => x.id === decodeURIComponent(url.pathname.slice(1)));
    if (!asset || !controller.root) return new Response(null, { status: 404 });
    const root = await realpath(controller.root);
    const file = await realpath(path.resolve(root, asset.path));
    const relative = path.relative(root, file);
    if (relative.startsWith('..') || path.isAbsolute(relative)) return new Response(null, { status: 403 });
    const bytes = await readFile(file);
    const hash = createHash('sha256').update(bytes).digest('hex');
    if (hash !== controller.content!.assetHashes[asset.id]) return new Response(null, { status: 409 });
    const mime: Record<string, string> = { '.png': 'image/png', '.jpg': 'image/jpeg', '.jpeg': 'image/jpeg', '.webp': 'image/webp', '.gif': 'image/gif', '.ogg': 'audio/ogg', '.mp3': 'audio/mpeg', '.wav': 'audio/wav' };
    const type = mime[path.extname(file).toLowerCase()];
    if (!type || !type.startsWith(asset.kind === 'image' ? 'image/' : 'audio/')) return new Response(null, { status: 415 });
    return new Response(new Uint8Array(bytes), { headers: { 'Content-Type': type, 'Cache-Control': 'no-store' } });
  } catch { return new Response(null, { status: 404 }); }
});

let startupError: string | null = null;
try { startupError = (await controller.openProject(path.join(app.getAppPath(), 'examples/academy-demo'))).error; }
catch (error) { startupError = error instanceof Error ? error.message : '기본 프로젝트를 열 수 없습니다.'; }
window = new BrowserWindow({ width: 1180, height: 820, minWidth: 900, minHeight: 680, backgroundColor: '#f3f0e8',
  title: 'Raising Player', autoHideMenuBar: true,
  webPreferences: { preload: fileURLToPath(new URL('../preload/index.cjs', import.meta.url)),
    contextIsolation: true, nodeIntegration: false, sandbox: true, webSecurity: true } });
window.removeMenu();
window.webContents.setWindowOpenHandler(() => ({ action: 'deny' }));
window.webContents.on('will-navigate', event => event.preventDefault());
window.webContents.on('will-attach-webview', event => event.preventDefault());
window.webContents.session.setPermissionRequestHandler((_webContents, _permission, callback) => callback(false));
window.webContents.session.setPermissionCheckHandler(() => false);
window.webContents.session.webRequest.onBeforeRequest((details, callback) => {
  callback({ cancel: !details.url.startsWith('file://') && !details.url.startsWith('raising-asset://') && !details.url.startsWith('devtools://') });
});
handle('player:view', () => controller.reply(startupError));
handle('player:new', async (version, userValues) => {
  if (!controller.isCurrent(version)) return controller.reply('화면이 갱신되었습니다. 다시 선택해 주세요.');
  if (!await confirmReplace()) return controller.reply();
  return controller.newRun(version, userValues as Record<string, string> | undefined);
});
handle('player:act', (action, version) => controller.act(action, version));
handle('player:save', async version => {
  if (!controller.session || !controller.isCurrent(version)) return controller.reply('저장할 현재 게임이 없습니다.');
  const result = await dialog.showSaveDialog(window, { title: '게임 저장', defaultPath: 'my-game.raising-save.json',
    filters: [{ name: '게임 저장 파일', extensions: ['json'] }] });
  return result.canceled || !result.filePath ? controller.reply() : controller.save(result.filePath, version);
});
handle('player:load', async version => {
  if (!controller.isCurrent(version)) return controller.reply('화면이 갱신되었습니다. 다시 선택해 주세요.');
  const result = await dialog.showOpenDialog(window, { title: '게임 불러오기', properties: ['openFile'],
    filters: [{ name: '게임 저장 파일', extensions: ['json'] }] });
  if (result.canceled || !result.filePaths[0] || !await confirmReplace()) return controller.reply();
  return controller.load(result.filePaths[0], version);
});
handle('player:open', async version => {
  if (!controller.isCurrent(version)) return controller.reply('화면이 갱신되었습니다. 다시 선택해 주세요.');
  const result = await dialog.showOpenDialog(window, { title: '프로젝트 폴더 열기', properties: ['openDirectory'] });
  if (result.canceled || !result.filePaths[0] || !await confirmReplace()) return controller.reply();
  const reply = await controller.openProject(result.filePaths[0]);
  if (!reply.error) startupError = null;
  return reply;
});
handle('player:year-sample', async (version: unknown) => {
  if (!controller.isCurrent(version)) return controller.reply('화면이 갱신되었습니다. 다시 선택해 주세요.');
  if (!await confirmReplace()) return controller.reply();
  const reply = await controller.openProject(path.join(app.getAppPath(), 'examples/academy-year'));
  if (!reply.error) startupError = null;
  return reply;
});
let closing = false;
window.on('close', event => {
  if (closing || !controller.session) return;
  event.preventDefault();
  const task = queue.then(async () => { if (await confirmReplace()) { closing = true; window.close(); } });
  queue = task.catch(() => undefined);
});
app.on('window-all-closed', () => app.quit());
await window.loadURL(renderer);
}).catch(error => { dialog.showErrorBox('플레이어 시작 오류', String(error)); app.exit(1); });
