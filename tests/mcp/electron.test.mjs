import test from 'node:test';
import assert from 'node:assert/strict';
import { _electron } from 'playwright';
import { readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { connect, fixture } from './helpers.mjs';

const view = page => page.evaluate(() => window.studio.view()).then(reply => reply.view);
async function idle(page) {
  await page.waitForFunction(() => !!document.querySelector('.workspace-status') && !document.querySelector('.workspace-status').textContent.includes('처리 중'));
}
async function click(page, name) { await page.getByRole('button', { name, exact: true }).click(); await idle(page); }
async function label(page, value) {
  await page.getByLabel('/label', { exact: true }).fill(value);
  await page.getByLabel('/label', { exact: true }).press('Tab');
  await idle(page);
}

test('real Electron GUI and stdio MCP alternate edits without losing human drafts or persisted data', { timeout: 90000 }, async t => {
  const root = await fixture(); const mcp = await connect(t, root);
  const env = { ...process.env }; delete env.NODE_OPTIONS; delete env.ELECTRON_RUN_AS_NODE;
  const app = await _electron.launch({ ...(process.env.STUDIO_EXECUTABLE
    ? { executablePath: resolve(process.env.STUDIO_EXECUTABLE), args: [] } : { args: ['dist/studio/main/index.js'] }), cwd: resolve('.'), env, timeout: 30000 });
  t.after(() => app.evaluate(({ app }) => app.exit(0)).catch(() => {}));
  const page = await app.firstWindow(); const errors = []; page.on('pageerror', e => errors.push(e.message));
  await idle(page);
  await app.evaluate(({ dialog }, root) => {
    dialog.showOpenDialog = async () => ({ canceled: false, filePaths: [root] });
    dialog.showMessageBox = async () => ({ response: 1, checkboxChecked: false });
  }, root);
  await click(page, '열기');
  await page.getByRole('button', { name: /검술 수업/ }).click(); await idle(page);
  const first = await mcp.call('read_project');
  const original = await mcp.call('read_document', { file: 'activities/study.json', expectedVersion: first.version });
  await label(page, '인간 먼저 저장'); await click(page, '초안 저장');
  assert.equal((await view(page)).dirty, false);
  const old = await mcp.call('apply_project_changes', { expectedVersion: first.version,
    edits: [{ type: 'replace', file: original.document.file, data: { ...original.document.data, label: '오래된 LLM 수정' } }], reason: 'stale' });
  assert.equal(old.error.code, 'VERSION_CONFLICT');
  assert.equal(JSON.parse(await readFile(`${root}/activities/study.json`, 'utf8')).label, '인간 먼저 저장');

  await label(page, '보존할 인간 초안');
  const latest = await mcp.call('read_project');
  const doc = await mcp.call('read_document', { file: 'activities/study.json', expectedVersion: latest.version });
  const saved = await mcp.call('apply_project_changes', { expectedVersion: latest.version,
    edits: [{ type: 'replace', file: doc.document.file, data: { ...doc.document.data, description: 'LLM 설명 추가' } }], reason: '기존 이름을 보존하고 설명 추가' });
  assert.equal(saved.ok, true);
  await click(page, '초안 저장');
  assert.match(await page.getByRole('alert').innerText(), /외부 변경 충돌/);
  const draft = await view(page); assert.equal(draft.dirty, true);
  assert.equal(draft.documents.find(d => d.file === doc.document.file).data.label, '보존할 인간 초안');
  const disk = JSON.parse(await readFile(`${root}/activities/study.json`, 'utf8'));
  assert.equal(disk.label, '인간 먼저 저장'); assert.equal(disk.description, 'LLM 설명 추가');
  await page.screenshot({ path: resolve('.cache/mcp-tests/gui-mcp-conflict.png') });

  await click(page, '오류 닫기'); await click(page, '다시 불러오기');
  await page.locator('.catalog-row').getByRole('button', { name: /인간 먼저 저장/ }).click(); await idle(page);
  await label(page, '보존할 인간 초안'); await click(page, '초안 저장');
  const final = await mcp.call('read_project');
  const merged = await mcp.call('read_document', { file: doc.document.file, expectedVersion: final.version });
  assert.equal(merged.document.data.label, '보존할 인간 초안');
  assert.equal(merged.document.data.description, 'LLM 설명 추가');
  assert.deepEqual(errors, []);
});
