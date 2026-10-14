import { app, BrowserWindow, dialog, ipcMain, protocol, nativeImage } from 'electron';
import { fileURLToPath } from 'node:url';
import { mkdir, readFile, realpath, writeFile, stat } from 'node:fs/promises';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { AnalysisJobs } from '../../../packages/analysis-service/jobs.js';
import type { AnalysisRequest } from '../../../packages/analysis-service/bridge.js';
import { traceScenario } from '../../../packages/analysis-service/scenario.js';
import { createSession, dispatchSession } from '../../../packages/runtime-service/session.js';
import { replayTrace } from '../../../packages/analysis-service/runner.js';
import { StudioEditor, type Edit } from '../../../packages/studio-service/editor.js';
import { checkStudioTemplate, createTemplateProject } from '../../../packages/studio-service/templates.js';
import { PlayerController } from '../../../packages/player-service/controller.js';
import { exportGame } from '../../../packages/export-service/export.js';
protocol.registerSchemesAsPrivileged([{ scheme: 'raising-asset', privileges: { standard: true, secure: true, supportFetchAPI: true, stream: true } }]);
const editor = new StudioEditor();
const player = new PlayerController();
const analysis = new AnalysisJobs(new URL('./analysis-worker.js', import.meta.url));
const renderer = new URL('../renderer/index.html', import.meta.url).href;
let window: BrowserWindow;
let queue: Promise<unknown> = Promise.resolve();
async function confirmDiscard() {
  return !editor.view().dirty || (await dialog.showMessageBox(window, { type: 'question', message: '저장하지 않은 편집 내용을 버릴까요?',
    buttons: ['돌아가기', '버리고 계속'], defaultId: 0, cancelId: 0 })).response === 1;
}
function handle(channel: string, fn: (...args: any[]) => unknown) {
  ipcMain.handle(channel, (event, ...args: unknown[]) => {
    if (event.sender !== window.webContents || event.senderFrame !== window.webContents.mainFrame || event.senderFrame.url !== renderer) throw new Error('허용되지 않은 요청입니다.');
    const task = queue.then(() => fn(...args)).catch(error => {
      const message = error instanceof Error ? error.message : String(error);
      return channel.startsWith('analysis:') ? { error: message } : channel.startsWith('preview:') ? player.reply(message) : editor.reply(message);
    });
    queue = task; return task;
  });
}
void app.whenReady().then(async () => {
  app.setName('Raising Studio');
  const sample = path.join(app.isPackaged ? app.getAppPath() : fileURLToPath(new URL('../../../', import.meta.url)), 'examples/academy-demo');
  await editor.open(sample, true);
  protocol.handle('raising-asset', async request => {
    try {
      const url = new URL(request.url);
      if (url.host === 'editor') {
        if (!editor.root || url.search !== `?r=${editor.revision}`) return new Response(null,{status:404});
        const manifest=editor.documents.find(d=>d.kind==='assets')?.data as {assets?:{id:string;kind:string;path:string}[]} | undefined;
        const asset=manifest?.assets?.find(a=>a.id===decodeURIComponent(url.pathname.slice(1))&&a.kind==='image');
        if(!asset)return new Response(null,{status:404});
        const root=await realpath(editor.root);const file=await realpath(path.resolve(root,asset.path));const relative=path.relative(root,file);
        if(relative.startsWith('..')||path.isAbsolute(relative)||!asset.path.startsWith('assets/'))return new Response(null,{status:403});
        const types:Record<string,string>={'.png':'image/png','.jpg':'image/jpeg','.jpeg':'image/jpeg','.webp':'image/webp','.gif':'image/gif'};
        const type=types[path.extname(file).toLowerCase()];if(!type)return new Response(null,{status:415});
        return new Response(new Uint8Array(await readFile(file)),{headers:{'Content-Type':type,'Cache-Control':'no-store'}});
      }
      if (url.host !== 'project' || url.search !== `?g=${player.generation}`) return new Response(null, { status: 404 });
      const content = player.content;
      const asset = content?.assets.find(x => x.id === decodeURIComponent(url.pathname.slice(1)));
      if (!asset || !player.root) return new Response(null, { status: 404 });
      const root = await realpath(player.root); const file = await realpath(path.resolve(root, asset.path));
      const relative = path.relative(root, file);
      if (relative.startsWith('..') || path.isAbsolute(relative)) return new Response(null, { status: 403 });
      const bytes = await readFile(file);
      if (createHash('sha256').update(bytes).digest('hex') !== content!.assetHashes[asset.id]) return new Response(null, { status: 409 });
      const types: Record<string, string> = { '.png': 'image/png', '.jpg': 'image/jpeg', '.jpeg': 'image/jpeg', '.webp': 'image/webp', '.gif': 'image/gif', '.wav': 'audio/wav', '.mp3': 'audio/mpeg', '.ogg': 'audio/ogg' };
      const type = types[path.extname(file).toLowerCase()];
      if (!type || !type.startsWith(`${asset.kind}/`)) return new Response(null, { status: 415 });
      return new Response(new Uint8Array(bytes), { headers: { 'Content-Type': type, 'Cache-Control': 'no-store' } });
    } catch { return new Response(null, { status: 404 }); }
  });
  window = new BrowserWindow({ width: 1360, height: 900, minWidth: 1000, minHeight: 700, title: 'Raising Studio', backgroundColor: '#f4f4ee',
    webPreferences: { preload: fileURLToPath(new URL('../preload/index.cjs', import.meta.url)), contextIsolation: true, sandbox: true, nodeIntegration: false } });
  window.removeMenu();
  window.webContents.setWindowOpenHandler(() => ({ action: 'deny' }));
  window.webContents.on('will-navigate', e => e.preventDefault());
  window.webContents.on('will-attach-webview', e => e.preventDefault());
  window.webContents.session.setPermissionRequestHandler((_w, _p, done) => done(false));
  window.webContents.session.setPermissionCheckHandler(() => false);
  window.webContents.session.webRequest.onBeforeRequest((d, done) => done({ cancel: !['file://', 'raising-asset://', 'devtools://'].some(prefix => d.url.startsWith(prefix)) }));
  handle('studio:view', () => editor.reply());
  handle('studio:export', async (target: unknown, revision: number) => {
    if (target !== 'web' && target !== 'windows') return editor.reply('지원하지 않는 내보내기 대상입니다.');
    if (!editor.root) return editor.reply('먼저 프로젝트를 열어 주세요.');
    const built = await editor.snapshot(revision);
    if (!built.ok) return editor.reply('내보내기 전에 진단 목록의 오류를 수정해 주세요.');
    const selected = await dialog.showSaveDialog(window, { title: '새 내보내기 폴더 이름',
      defaultPath: path.join(app.getPath('documents'), `${built.snapshot.project.gameId}-${target}`), buttonLabel: '내보내기' });
    if (selected.canceled || !selected.filePath) return editor.reply();
    const runtimeRoot = app.isPackaged ? path.join(process.resourcesPath, 'export-runtimes')
      : fileURLToPath(new URL('../../', import.meta.url));
    const result = await exportGame({ content: built.snapshot, projectRoot: editor.root, destination: selected.filePath,
      target, runtimes: { web: path.join(runtimeRoot, 'web'), windows: path.join(runtimeRoot, 'native-player-app') },
      extensions: await editor.exportExtensions(revision, built.snapshot, target) });
    return editor.reply(null, `게임을 내보냈습니다: ${result.directory}`);
  });
  handle('analysis:request', async (input: AnalysisRequest) => {
    if (!editor.root || editor.readOnly) throw new Error('분석 결과를 저장할 편집 가능한 프로젝트를 열어 주세요.');
    if (input.type === 'list') return { jobs: await analysis.list(editor.root) };
    if (input.type === 'get') return { job: await analysis.get(editor.root, input.id) };
    if (input.type === 'cancel') return { job: await analysis.cancel(editor.root, input.id) };
    if (input.type === 'start') {
      const snapshot = await editor.snapshot(input.revision); if (!snapshot.ok) throw new Error('검증 오류를 먼저 수정해 주세요.');
      return { job: await analysis.start(editor.root, snapshot.snapshot, editor.draftVersion(), editor.revision, input.options) };
    }
    if (input.type === 'replay' || input.type === 'export') {
      const stored = await analysis.stored(editor.root, input.id); const trace = stored.job.result.traces[input.trace]; if (!trace) throw new Error('재현 기록이 없습니다.');
      if (input.type === 'export') {
        const current = await editor.snapshot(input.revision); if (!current.ok || current.snapshot.contentFingerprint !== stored.job.fingerprint) throw new Error('이전 콘텐츠 버전 결과입니다. 현재 초안에서 다시 검사하세요.');
        const data = await traceScenario(stored.content, trace, input.scenarioId);
        editor.edit({ type: 'importScenario', id: input.scenarioId, data }, input.revision);
        await editor.save(editor.revision); return { notice: '재실행을 확인한 회귀 시나리오를 저장했습니다.' };
      }
      replayTrace(stored.content, trace, input.steps);
      const created = createSession(stored.content, trace.seed); if (!created.ok) throw new Error(created.error.message);
      let session = created.session;
      for (const command of trace.commands.slice(0, input.steps)) { const next = dispatchSession(stored.content, session, command, session.state.stateRevision); if (!next.ok) throw new Error(next.error.message); session = next.session; }
      player.content = stored.content; player.root = editor.root; player.session = session; player.generation++;
      return { notice: '검사 당시 스냅샷으로 재생합니다. 원본 에셋 파일은 현재 프로젝트에서 읽습니다.' };
    }
    throw new Error('지원하지 않는 분석 요청입니다.');
  });
  handle('studio:edit', (edit: Edit, revision: number) => editor.edit(edit, revision));
  handle('studio:import-image', async (revision:number) => {
    editor.current(revision);if(!editor.root||editor.readOnly)throw new Error('편집 가능한 프로젝트를 열어 주세요.');
    const selected=await dialog.showOpenDialog(window,{title:'이미지 가져오기',properties:['openFile'],filters:[{name:'이미지',extensions:['png','jpg','jpeg','webp']}]});
    if(selected.canceled||!selected.filePaths[0])return editor.reply();
    if((await stat(selected.filePaths[0])).size>20*1024*1024)throw new Error('이미지는 20MB 이하로 가져오세요.');
    const bytes=await readFile(selected.filePaths[0]);if(bytes.length>20*1024*1024)throw new Error('이미지는 20MB 이하로 가져오세요.');
    const extension=path.extname(selected.filePaths[0]).toLowerCase();if(!['.png','.jpg','.jpeg','.webp'].includes(extension)||nativeImage.createFromBuffer(bytes).isEmpty())throw new Error('지원하는 이미지 파일이 아닙니다.');
    const root=await realpath(editor.root);const directory=await realpath(path.join(root,'assets'));const relative=path.relative(root,directory);
    if(relative.startsWith('..')||path.isAbsolute(relative))throw new Error('프로젝트 밖의 에셋 경로는 사용할 수 없습니다.');
    const id=`image-${createHash('sha256').update(bytes).digest('hex').slice(0,16)}`;const filename=`${id}${extension}`;
    const manifest=editor.documents.find(d=>d.kind==='assets');if(!manifest)throw new Error('에셋 목록이 없습니다.');
    const data=manifest.data as {assets:{id:string;kind:string;path:string}[]};if(data.assets.some(a=>a.id===id))return editor.reply(null,`이미 등록된 이미지입니다: ${id}`);
    try{await writeFile(path.join(directory,filename),bytes,{flag:'wx'});}catch(e){if((e as NodeJS.ErrnoException).code!=='EEXIST')throw e;const existing=await realpath(path.join(directory,filename));if(path.dirname(existing)!==directory||!(await readFile(existing)).equals(bytes))throw new Error('같은 이름의 다른 파일이 있습니다.');}
    editor.current(revision);return editor.edit({type:'replace',file:manifest.file,data:{...data,assets:[...data.assets,{id,kind:'image',path:`assets/${filename}`}] }},revision);
  });
  handle('studio:validate', (revision: number) => editor.validate(revision));
  handle('studio:save', (revision: number) => editor.save(revision));
  handle('studio:reload', async (revision: number) => { editor.current(revision); return editor.root && await confirmDiscard() ? editor.open(editor.root, editor.readOnly) : editor.reply(); });
  handle('studio:open', async () => {
    const selected = await dialog.showOpenDialog(window, { title: '프로젝트 열기', properties: ['openDirectory'] });
    if (selected.canceled || !selected.filePaths[0] || !await confirmDiscard()) return editor.reply();
    return editor.open(selected.filePaths[0]);
  });
  handle('studio:create', async (template = 'academy-demo') => {
    checkStudioTemplate(template);
    const selected = await dialog.showOpenDialog(window, { title: '새 프로젝트를 만들 빈 폴더', properties: ['openDirectory', 'createDirectory'] });
    if (selected.canceled || !selected.filePaths[0] || !await confirmDiscard()) return editor.reply();
    const target = selected.filePaths[0];
    await createTemplateProject(path.dirname(sample), template, target);
    return editor.open(target);
  });
  handle('studio:preview', async (revision: number) => {
    const result = await editor.snapshot(revision);
    if (!result.ok) return editor.reply('미리보기 전에 진단 목록의 오류를 수정해 주세요.');
    player.content = result.snapshot; player.root = editor.root; player.session = null; player.generation++;
    return editor.reply(null, '현재 초안으로 미리보기를 시작합니다.');
  });
  handle('preview:view', () => player.reply());
  handle('preview:new', (version: unknown, userValues?: Record<string, string>) => player.newRun(version, userValues));
  handle('preview:act', (action: unknown, version: unknown) => player.act(action, version));
  handle('preview:save', async (version: unknown) => {
    if (!player.session || !player.isCurrent(version)) return player.reply('저장할 현재 게임이 없습니다.');
    const selected=await dialog.showSaveDialog(window,{title:'미리보기 게임 저장',defaultPath:'preview-save.json',filters:[{name:'게임 저장',extensions:['json']}]});
    return selected.canceled||!selected.filePath?player.reply():player.save(selected.filePath,version);
  });
  handle('preview:load', async (version: unknown) => {
    if (!player.isCurrent(version)) return player.reply('화면이 갱신되었습니다. 다시 선택해 주세요.');
    const selected=await dialog.showOpenDialog(window,{title:'미리보기 게임 불러오기',properties:['openFile'],filters:[{name:'게임 저장',extensions:['json']}]});
    return selected.canceled||!selected.filePaths[0]?player.reply():player.load(selected.filePaths[0],version);
  });
  let closing = false;
  window.on('close', event => {
    if (closing || !editor.view().dirty) return;
    event.preventDefault();
    queue = queue.then(async () => { if (await confirmDiscard()) { closing = true; window.close(); } }).catch(() => {});
  });
  app.on('window-all-closed', () => app.quit());
  await window.loadURL(renderer);
}).catch(error => { dialog.showErrorBox('편집기 시작 오류', String(error)); app.exit(1); });
