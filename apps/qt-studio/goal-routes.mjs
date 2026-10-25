import { createHash, randomUUID } from 'node:crypto';
import { mkdir, readFile, writeFile, rename, unlink, open, readdir, realpath, stat } from 'node:fs/promises';
import path from 'node:path';
import { validateGoalRoute, routeFromRecording, runGoalRoute } from '../../dist/packages/analysis-service/goal-routes.js';

const digest = text => createHash('sha256').update(text).digest('hex');
function inside(root, file) { const relative = path.relative(root, file); if (relative.startsWith('..') || path.isAbsolute(relative)) throw Error('프로젝트 밖의 경로는 사용할 수 없습니다.'); }
async function folder(root) {
  const base = await realpath(root), tests = path.join(base, 'playtests');
  await mkdir(tests, { recursive: true }); inside(base, await realpath(tests));
  const routes = path.join(tests, 'routes'); await mkdir(routes, { recursive: true }); inside(base, await realpath(routes)); return routes;
}
// Batch capture uses a bounded read even if another editor grows the file
// between the directory/stat check and the actual read. Legacy callers keep
// their existing validation and listing behavior.
export async function readGoalRouteBytes(file,limit=4*1024*1024){
  const handle=await open(file,'r');
  try{
    const info=await handle.stat();if(!info.isFile()||info.size>limit)throw Error('Route batch input exceeds its byte limit or is not a regular file.');
    const bytes=Buffer.allocUnsafe(limit+1);let length=0;
    while(length<bytes.length){const result=await handle.read(bytes,length,bytes.length-length,null);if(!result.bytesRead)break;length+=result.bytesRead;}
    if(length>limit)throw Error('Route batch input grew beyond its byte limit.');
    return bytes.subarray(0,length);
  }finally{await handle.close();}
}
async function boundedJson(file, budget) {
  if ((await stat(file)).size > 4 * 1024 * 1024) throw Error('경로 파일이 4 MiB를 넘습니다.');
  const text = budget?(await readGoalRouteBytes(file,Math.max(0,Math.min(4*1024*1024,budget.remaining)))).toString('utf8'):await readFile(file, 'utf8'); if(budget){budget.remaining-=Buffer.byteLength(text);if(budget.remaining<0)throw Error('Route batch files exceed the 64 MiB input budget. Select fewer routes.');} return { value: JSON.parse(text), revision: digest(text) };
}
export async function listGoalRoutes(root, options = {}) {
  const dir = await folder(root); let files = (await readdir(dir)).filter(f => /^[a-zA-Z0-9][a-zA-Z0-9_-]{0,63}\.json$/.test(f)).sort();
  if (files.length > 128) throw Error('한 프로젝트에서 목표 경로는 최대 128개입니다.');
  if(options.ids?.length)files=files.filter(file=>options.ids.includes(file.slice(0,-5)));
  const budget=options.maxBytes===undefined?null:{remaining:options.maxBytes};
  if(budget){let total=0;for(const file of files){const full=path.join(dir,file);inside(dir,await realpath(full));total+=(await stat(full)).size;if(total>options.maxBytes)throw Error('Route batch files exceed the 64 MiB input budget. Select fewer routes.');}}
  const read=async file => {
    const full = path.join(dir, file); inside(dir, await realpath(full));
    try { const data = await boundedJson(full,budget), route = validateGoalRoute(data.value); if (route.id + '.json' !== file) throw Error('파일명과 경로 ID가 다릅니다.'); return { route, revision: data.revision, file: full }; }
    catch (error) { return { file: full, error: error.message }; }
  };
  if(budget){const entries=[];for(const file of files)entries.push(await read(file));return entries;}
  return Promise.all(files.map(read));
}
export async function importGoalRecording(file, metadata) { if(typeof file!=='string'||!path.isAbsolute(file))throw Error('기록 파일의 절대 경로를 지정하세요.');return routeFromRecording((await boundedJson(file)).value, metadata); }
export async function saveGoalRoute(root, input, expectedRevision = null) {
  const route = validateGoalRoute(input), dir = await folder(root), file = path.join(dir, route.id + '.json');
  // Exclusive project lock + content hash prevents two editor windows losing changes.
  const lockFile = path.join(dir, '.write-lock');
  let lock; try { lock = await open(lockFile, 'wx'); } catch { throw Error('다른 창이 목표 경로를 저장 중입니다. 잠시 후 새로고침하세요.'); }
  const temp = path.join(dir, '.' + randomUUID() + '.tmp');
  try {
    let actual = null;
    try { inside(dir, await realpath(file)); actual = (await boundedJson(file)).revision; } catch (e) { if (e.code !== 'ENOENT') throw e; }
    if (actual !== expectedRevision) throw Error('목표 경로 버전 충돌: 새로고침 후 변경을 다시 확인하세요. 기존 경로를 덮어쓰지 않았습니다.');
    if (actual === null && (await readdir(dir)).filter(f => f.endsWith('.json')).length >= 128) throw Error('목표 경로는 최대 128개입니다.');
    const text = JSON.stringify(route, null, 2) + '\n';
    await writeFile(temp, text, { flag: 'wx' }); await rename(temp, file);
    return { route, revision: digest(text), file };
  } finally { await unlink(temp).catch(() => {}); await lock.close(); await unlink(lockFile); }
}
export async function checkGoalRoutes(root, content, ids) {
  const entries = await listGoalRoutes(root);
  const selected = ids?.length ? entries.filter(e => e.route && ids.includes(e.route.id)) : entries;
  if (!selected.length || (ids?.length && selected.length !== new Set(ids).size)) throw Error('검사할 목표 경로를 찾지 못했습니다.');
  const reports = [];
  const started=Date.now();
  for (const entry of selected) {
    if (entry.error) reports.push({ ok: false, status: 'invalid', name: path.basename(entry.file), failure: { step: 0, code: 'INVALID_ROUTE', message: entry.error } });
    else if(Date.now()-started>=60000)reports.push({id:entry.route.id,name:entry.route.name,ok:false,status:'incomplete',failure:{step:0,code:'TIMEOUT',message:'전체 검사 시간 제한에 도달했습니다. 선택 경로 검사로 나눠 실행하세요.'}});
    else reports.push({ ...await runGoalRoute(content, entry.route,{timeoutMs:60000-(Date.now()-started)}), routeRevision: entry.revision });
  }
  const result = { version: 1, contentFingerprint: content.contentFingerprint, ok: reports.every(r => r.ok), reports,
    limitations: ['기록된 seed와 명령 경로의 회귀 검사입니다. 실패는 모든 경로의 도달 불가능 증명이 아닙니다.', '게임 상태와 명령·난수·중간 저장 복원을 검사합니다. 화면 배치·입력·연출 시간은 별도 실제 UI 시험 대상입니다.'] };
  const base = await realpath(root), dir = path.join(base, '.studio'); await mkdir(dir, { recursive: true }); inside(base, await realpath(dir));
  const file = path.join(dir, 'goal-report-' + Date.now() + '-' + randomUUID() + '.json');
  await writeFile(file, JSON.stringify(result, null, 2), { flag: 'wx' }); return { ...result, file };
}
