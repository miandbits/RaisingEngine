import test from 'node:test';
import assert from 'node:assert/strict';
import { _electron } from 'playwright';
import { Client } from '@modelcontextprotocol/client';
import { StdioClientTransport } from '@modelcontextprotocol/client/stdio';
import { mkdir, mkdtemp, readFile, access } from 'node:fs/promises';
import { resolve, join } from 'node:path';

const studioExe = process.env.STUDIO_EXECUTABLE;
const playerExe = process.env.PLAYER_EXECUTABLE;
const mcpRoot = process.env.MCP_RUNTIME;
// Applications inherit no npm/Node/developer PATH and start outside the source tree.
const cleanEnv = { ...process.env, PATH: `${process.env.SystemRoot}\\System32;${process.env.SystemRoot}` };
for (const key of ['NODE_OPTIONS','NODE_PATH','ELECTRON_RUN_AS_NODE','ELECTRON_ENABLE_LOGGING']) delete cleanEnv[key];
const studioView = page => page.evaluate(() => window.studio.view()).then(r => r.view);
const playerView = page => page.evaluate(() => window.player.view()).then(r => r.view);
async function studioIdle(page) { await page.waitForFunction(() => !!document.querySelector('.workspace-status') && !document.querySelector('.workspace-status').textContent.includes('처리 중')); }
async function playerIdle(page, preview = false) {
  if (preview) await page.waitForFunction(() => ![...document.querySelectorAll('button')].some(b => b.disabled && /계속|다음으로|활동 시작|일정 확정/.test(b.textContent)));
  else await page.waitForFunction(() => [...document.querySelectorAll('button')].some(b => b.textContent === '프로젝트 열기' && !b.disabled));
}
async function click(page, name, preview = false) { await page.getByRole('button', { name, exact: true }).click(); await playerIdle(page, preview); }
async function runYear(page, route, { preview = false, checkpoint, resume = false } = {}) {
  if (!resume) await click(page, '새 게임 시작', preview);
  let saved = false;
  for (let step = 0; step < 250; step++) {
    const view = await playerView(page); const state = view.state;
    if (state.phase === 'finished') return state;
    if (checkpoint && !saved && state.periodIndex === 18 && state.phase === 'planning') { saved = true; await checkpoint(); }
    if (state.phase === 'planning') {
      for (const [i, period] of ['상순','중순','하순'].entries()) await page.getByLabel(`${period} 활동`, { exact: true }).selectOption([route === 'wanderer' ? 'expedition' : 'study','work','rest'][i]);
      await click(page, '일정 확정', preview);
    } else if (state.phase === 'ready') await click(page, `${['상순','중순','하순'][view.time.periodInMonth]} 활동 시작`, preview);
    else if (state.phase === 'activity-result') await click(page, '다음으로', preview);
    else if (state.phase === 'narrative' && view.narrative.node.type === 'choice') {
      const id = view.narrative.node.id === 'path' ? (route === 'wanderer' ? 'free' : 'knight') : (route === 'wanderer' ? 'observe' : 'support');
      await click(page, view.narrative.choices.find(c => c.id === id).text, preview);
    } else await click(page, '계속', preview);
  }
  assert.fail('Year did not finish within UI step bound');
}

test('installed Studio → MCP → preview → Player: annual parity, conflict protection, restart and RNG saves without development runtime', { timeout: 240000 }, async t => {
  assert.ok(studioExe && playerExe && mcpRoot, 'Run via npm run test:installed or test:portable:year');
  await mkdir('.cache/release-tests', { recursive: true });
  const workspace = await mkdtemp(resolve('.cache/release-tests/work-'));
  const project = join(workspace, 'my-year'); await mkdir(project);
  const save = join(workspace, 'half-year.save.json');
  const apps = []; t.after(async () => { for (const app of apps) await app.evaluate(({ app }) => app.exit(0)).catch(() => {}); });
  const errors = [];
  async function launch(executablePath) {
    const app = await _electron.launch({ executablePath: resolve(executablePath), args: [], cwd: workspace, env: cleanEnv, timeout: 30000 });
    apps.push(app); const page = await app.firstWindow(); page.on('pageerror', error => errors.push(error.message)); return { app, page };
  }
  const studio = await launch(studioExe); await studioIdle(studio.page);
  console.log('Release integration: Studio launched outside the source tree.');
  await studio.app.evaluate(({ dialog }, root) => {
    dialog.showOpenDialog = async () => ({ canceled: false, filePaths: [root] });
    dialog.showMessageBox = async () => ({ response: 1, checkboxChecked: false });
  }, project);
  await studio.page.getByRole('button', { name: '12개월 새 프로젝트', exact: true }).click(); await studioIdle(studio.page);
  assert.equal((await studioView(studio.page)).documents.find(d => d.kind === 'project').data.durationMonths, 12);
  console.log('Release integration: copied the 12-month sample through Studio.');

  const client = new Client({ name:'installed-release-test',version:'1.0.0' }, {versionNegotiation:{mode:{pin:'2026-07-28'}}});
  const transport = new StdioClientTransport({command:resolve(mcpRoot,'node.exe'),args:[resolve(mcpRoot,'main/index.js'),'--project',project],cwd:workspace,env:cleanEnv,stderr:'pipe'});
  let stderr = ''; transport.stderr?.on('data', bytes => stderr += bytes.toString());
  t.after(() => client.close());
  try { await client.connect(transport); } catch (error) { throw new Error(`Installed MCP connection failed: ${stderr}`, {cause:error}); }
  console.log('Release integration: bundled Node MCP connected.');
  const call = async (name, args={}) => { const r = await client.callTool({name,arguments:args}); assert.ok(r.structuredContent,JSON.stringify(r)); return r.structuredContent; };
  assert.equal((await client.listTools()).tools.length,7);
  const first = await call('read_project');
  await studio.page.getByRole('button', {name:/검술 수업/}).click();
  await studio.page.getByLabel('/label',{exact:true}).fill('함께 만든 검술 수업'); await studio.page.getByLabel('/label',{exact:true}).press('Tab'); await studioIdle(studio.page);
  await studio.page.getByRole('button',{name:'초안 저장',exact:true}).click(); await studioIdle(studio.page);
  const stale = await call('apply_project_changes',{expectedVersion:first.version,edits:[{type:'add',kind:'activity',id:'stale'}],reason:'stale version check'});
  assert.equal(stale.error.code,'VERSION_CONFLICT'); await assert.rejects(access(join(project,'activities/stale.json')));
  const fresh = await call('read_project');
  const activity = (await call('read_document',{file:'activities/study.json',expectedVersion:fresh.version})).document.data;
  const scenario = (await call('read_document',{file:'scenarios/knight-year.json',expectedVersion:fresh.version})).document.data;
  activity.baseEffects[0].value.value = 9; scenario.assertions[1].condition.right.value = 118;
  const changed = await call('apply_project_changes',{expectedVersion:fresh.version,edits:[{type:'replace',file:'activities/study.json',data:activity},{type:'replace',file:'scenarios/knight-year.json',data:scenario}],reason:'검술 성장량과 독립 기대값을 함께 조정'});
  assert.equal(changed.ok,true,JSON.stringify(changed)); assert.equal((await call('validate_project',{expectedVersion:changed.version})).ok,true);
  const results = {};
  for(const route of ['knight','scholar','artisan','neighbor','wanderer']) {
    results[route]=await call('run_scenario',{expectedVersion:changed.version,scenarioId:`${route}-year`});
    assert.equal(results[route].ok,true,JSON.stringify(results[route]));
  }
  await studio.page.getByRole('button',{name:'다시 불러오기',exact:true}).click(); await studioIdle(studio.page);
  assert.equal((await studioView(studio.page)).documents.find(d=>d.file==='activities/study.json').data.baseEffects[0].value.value,9);
  await studio.page.getByRole('button',{name:'플레이 미리보기',exact:true}).click();
  const previewState = await runYear(studio.page,'knight',{preview:true});
  console.log('Release integration: preview completed the edited year.');
  assert.deepEqual(previewState,results.knight.state);
  await studio.page.screenshot({path:join(workspace,'preview-year-ending.png')});

  let player = await launch(playerExe); await playerIdle(player.page);
  // Bundled sample button must work outside the repository before opening the edited project.
  await click(player.page,'12개월 샘플'); assert.equal((await playerView(player.page)).durationMonths,12);
  async function dialogs(target) {
    await target.app.evaluate(({dialog},paths)=>{
      dialog.showOpenDialog=async (_window, options)=>({canceled:false,filePaths:[options.properties?.includes('openDirectory')?paths.project:paths.save]});
      dialog.showSaveDialog=async()=>({canceled:false,filePath:paths.save});
      dialog.showMessageBox=async()=>({response:1,checkboxChecked:false});
    },{project,save});
  }
  await dialogs(player); await click(player.page,'프로젝트 열기');
  const final = await runYear(player.page,'knight',{checkpoint:async()=>{
    const before=(await playerView(player.page)).state;
    await click(player.page,'저장하기'); await click(player.page,'불러오기'); assert.deepEqual((await playerView(player.page)).state,before);
  }});
  assert.deepEqual(final,results.knight.state); assert.equal(final.attributes.skill,118);
  console.log('Release integration: Player completed the edited year.');
  await player.page.screenshot({path:join(workspace,'player-year-ending.png')});
  // A new OS process resumes the actual midpoint file, preserving the frozen fingerprint.
  await player.app.evaluate(({app})=>app.exit(0));
  player=await launch(playerExe); await playerIdle(player.page); await dialogs(player); await click(player.page,'프로젝트 열기'); await click(player.page,'불러오기');
  assert.equal((await playerView(player.page)).state.periodIndex,18);
  assert.equal((await playerView(player.page)).state.attributes.skill,59);
  assert.deepEqual(await runYear(player.page,'knight',{resume:true}),results.knight.state);
  // RNG path uses fresh game; saves after completion still reproduce exact RNG and command history.
  await player.app.evaluate(({dialog})=>{dialog.showMessageBox=async()=>({response:1,checkboxChecked:false});});
  await click(player.page,'프로젝트 열기');
  const randomFinal=await runYear(player.page,'wanderer'); assert.deepEqual(randomFinal,results.wanderer.state);
  await click(player.page,'저장하기'); await click(player.page,'불러오기'); assert.deepEqual((await playerView(player.page)).state,randomFinal);
  assert.equal(stderr,''); assert.deepEqual(errors,[]);
  const saved=JSON.parse(await readFile(save,'utf8')); assert.equal(saved.state.phase,'finished');
  console.log(JSON.stringify({project,knight:final.attributes,wanderer:randomFinal.attributes,installedMcp:resolve(mcpRoot),developerPathRemoved:true}));
});
