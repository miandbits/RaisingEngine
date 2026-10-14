import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { readFile, writeFile } from 'node:fs/promises';
import { resolve, extname, sep } from 'node:path';
import { chromium } from 'playwright';

export async function browserSmoke(t, directory, screenshot) {
  const mime = { '.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.wasm': 'application/wasm', '.json': 'application/json', '.css': 'text/css', '.png': 'image/png', '.svg': 'image/svg+xml' };
  const server = createServer(async (req, res) => {
    try {
      const path = decodeURIComponent(new URL(req.url, 'http://127.0.0.1').pathname);
      const file = resolve(directory, `.${path === '/' ? '/index.html' : path}`);
      if (!file.startsWith(`${directory}${sep}`)) { res.writeHead(403); res.end(); return; }
      const bytes = await readFile(file);
      res.writeHead(200, { 'Content-Type': mime[extname(file)] ?? 'application/octet-stream' }); res.end(bytes);
    } catch { res.writeHead(404); res.end(); }
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve)); t.after(() => { server.closeAllConnections(); return new Promise(resolve => server.close(resolve)); });
  const browser = await chromium.launch({ channel: 'chrome', headless: true }); t.after(() => browser.close());
  const page = await browser.newPage({ viewport: { width: 1280, height: 800 } }), errors = [];
  page.on('pageerror', error => errors.push(error.message));
  await page.goto(`http://127.0.0.1:${server.address().port}/`);
  await page.locator('.visual-stage').waitFor({ timeout: 10000 }).catch(async error => {
    await page.screenshot({ path: screenshot });
    await writeFile(`${screenshot}.error.json`, JSON.stringify({ error: error.message, errors, body: await page.locator('body').innerText() }, null, 2)); throw error;
  });
  const click = name => page.getByRole('button', { name, exact: true }).click();
  await click('새 게임 시작'); await click('작은 상점 방문'); await click('감사의 꽃 구매');
  await click('도시 광장 방문'); await click('유나에게 꽃 선물');
  const reply = await page.evaluate(() => window.player.view()); assert.equal(reply.view.runtimeBackend, 'wasm');
  assert.equal(reply.view.state.relationships.mentor.trust, 15);
  await page.waitForFunction(() => [...document.querySelectorAll('.visual-stage img')].every(image => image.complete && image.naturalWidth > 0));
  await page.evaluate(async () => { await Promise.all(document.getAnimations().map(animation => animation.finished.catch(() => {}))); });
  await page.screenshot({ path: screenshot }); assert.deepEqual(errors, []);
  await browser.close(); server.closeAllConnections();
}
