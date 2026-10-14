import test from 'node:test';
import assert from 'node:assert/strict';
import { cp, mkdir, mkdtemp, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { StudioEditor } from '../../dist/packages/studio-service/editor.js';
import { openJourneyStudio, click } from './gui-helpers.mjs';

const pad = i => String(i).padStart(4, '0');
async function scaleFixture(output, timings) {
  const root = resolve(await mkdtemp(`${output}/project-`));
  await cp(resolve('examples/academy-journey'), root, { recursive: true });
  const editor = new StudioEditor(); await editor.open(root);
  const start = performance.now(), documents = editor.view().documents;
  const projectDoc = documents.find(d => d.kind === 'project'), attributeDoc = documents.find(d => d.kind === 'attributes');
  const project = structuredClone(projectDoc.data), attributes = structuredClone(attributeDoc.data);
  for (let i = attributes.attributes.length; i < 100; i++) attributes.attributes.push({ id: `scale-attribute-${pad(i)}`, label: `확장 능력 ${pad(i)}`, kind: i % 2 ? 'stat' : 'resource', min: 0, max: 300, initial: 0 });
  const g = project.gameplay;
  for (let i = g.items.length; i < 1000; i++) g.items.push({ id: `scale-item-${pad(i)}`, name: `확장 상품 ${pad(i)}`, maxStack: 99 });
  for (let i = g.locations.length; i < 50; i++) g.locations.push({ id: `scale-location-${pad(i)}`, name: `확장 장소 ${pad(i)}`, screenId: 'home', connections: ['home'] });
  g.locations[0].connections = g.locations.slice(1).map(location => location.id);
  for (let i = g.shops.length; i < 50; i++) g.shops.push({ id: `scale-shop-${pad(i)}`, name: `확장 상점 ${pad(i)}`, locationId: g.locations[i].id, currencyAttributeId: 'money', offers: [{ itemId: 'scale-item-0999', price: 1, stock: null }] });
  for (let i = g.interactions.length; i < 100; i++) g.interactions.push({ id: `scale-interaction-${pad(i)}`, name: `확장 교류 ${pad(i)}`, locationId: 'city', characterId: 'mentor', itemId: 'scale-item-0999', quantity: 1, costs: [], effects: [], oncePerPeriod: true });
  const edits = [{ type: 'replace', file: projectDoc.file, data: project }, { type: 'replace', file: attributeDoc.file, data: attributes }];
  for (const [kind, target, folder] of [['ending', 500, 'endings'], ['event', 500, 'events'], ['character', 100, 'characters']]) {
    const existing = documents.filter(d => d.kind === kind);
    const base = existing.find(d => kind === 'ending' ? d.data.id === 'knight' : kind === 'event' ? d.data.id === 'flowers-bring-friendship' : d.data.id === 'mentor').data;
    for (let i = existing.length; i < target; i++) {
      const id = `scale-${kind}-${pad(i)}`, data = structuredClone(base); data.id = id;
      if (kind === 'character') data.name = `확장 인물 ${pad(i)}`;
      else {
        data.label = `확장 ${kind === 'event' ? '사건' : '엔딩'} ${pad(i)}`;
        data.requirements = { op: 'compare', comparator: 'gte', left: { op: 'read', target: { kind: 'attribute', attributeId: 'skill' } }, right: { op: 'literal', value: 1 + i % 250 } };
        data.priority = i % 100;
      }
      edits.push({ type: 'add', kind, id }, { type: 'replace', file: `${folder}/${id}.json`, data });
    }
  }
  for (let i = 0; i < edits.length; i += 100) editor.edit({ type: 'batch', edits: edits.slice(i, i + 100) }, editor.view().revision);
  timings.fixtureEditsMs = performance.now() - start;
  const validationStart = performance.now(); await editor.validate(editor.view().revision);
  const valid = editor.view(); const problems = valid.diagnostics.filter(d => d.severity === 'error'); assert.equal(problems.length, 0, JSON.stringify(problems.slice(0, 3)));
  timings.validationMs = performance.now() - validationStart;
  const invalid = structuredClone(project); invalid.gameplay.interactions[99].itemId = 'missing-stress-item';
  editor.edit({ type: 'replace', file: projectDoc.file, data: invalid }, valid.revision); await editor.validate(editor.view().revision);
  const diagnostic = editor.view().diagnostics.find(d => d.severity === 'error' && d.pointer.includes('/gameplay/interactions/99/itemId'));
  assert.ok(diagnostic, 'validator finds precise missing-item reference in large project');
  editor.edit({ type: 'undo' }, editor.view().revision);
  const saveStart = performance.now(); const saved = await editor.save(editor.view().revision); assert.equal(saved.error, null);
  timings.saveMs = performance.now() - saveStart;
  return { root, diagnostic, counts: { endings: 500, events: 500, characters: 100, attributes: 100, items: 1000, locations: 50, shops: 50, interactions: 100 } };
}

test('large project keeps item/endings search and relation graph bounded in actual editor', { timeout: 180000 }, async t => {
  const output = resolve('.cache/journey-scale-ui'); await mkdir(output, { recursive: true });
  const timings = {}, fixture = await scaleFixture(output, timings);
  const { app, page, errors } = await openJourneyStudio(t);
  await app.evaluate(({ dialog }, root) => { dialog.showOpenDialog = async () => ({ canceled: false, filePaths: [root] }); }, fixture.root);
  let start = performance.now(); await click(page, '열기'); await page.getByText(fixture.root, { exact: true }).waitFor(); timings.openMs = performance.now() - start;
  await page.getByLabel('자동 검증', { exact: true }).uncheck();
  await click(page, '육성 작업실'); await page.locator('.journey-steps button').filter({ hasText: '아이템' }).click();
  const list = page.locator('.gameplay-browser');
  const itemButtons = await list.getByRole('button').count(); assert.ok(itemButtons < 40, `virtualized item buttons: ${itemButtons}`);
  start = performance.now(); await page.getByLabel('육성 콘텐츠 검색', { exact: true }).fill('scale-item-0999');
  await list.getByRole('button').filter({ hasText: '확장 상품 0999' }).click();
  assert.equal(await page.getByLabel('아이템 이름', { exact: true }).inputValue(), '확장 상품 0999'); timings.itemSearchMs = performance.now() - start;
  await page.screenshot({ path: resolve(output, 'last-item.png') });
  await page.getByLabel('육성 콘텐츠 검색', { exact: true }).fill('');
  await page.locator('.gameplay-list-viewport').evaluate(node => { node.scrollTop = node.scrollHeight; node.dispatchEvent(new Event('scroll')); });
  await list.getByRole('button').filter({ hasText: '확장 상품 0999' }).waitFor();
  assert.ok(await list.getByRole('button').count() < 40, 'scrolling to item 1000 keeps DOM bounded');
  await page.locator('.journey-steps button').filter({ hasText: '장소' }).click();
  await page.getByLabel('육성 콘텐츠 검색', { exact: true }).fill('home');
  await list.getByRole('button').filter({ hasText: '우리 집' }).click();
  await page.getByLabel('연결할 장소 검색', { exact: true }).fill('scale-location-0049');
  assert.equal(await page.getByLabel('확장 장소 0049 연결', { exact: true }).isChecked(), true);
  await click(page, '콘텐츠'); await page.getByRole('navigation', { name: '콘텐츠 종류' }).getByRole('button').filter({ hasText: /^엔딩/ }).click();
  const catalogRows = await page.locator('.catalog-row').count(); assert.ok(catalogRows <= 22);
  await page.locator('.catalog-viewport').evaluate(node => { node.scrollTop = node.scrollHeight; node.dispatchEvent(new Event('scroll')); });
  assert.ok(await page.locator('.catalog-row').count() <= 22);
  start = performance.now(); await page.getByLabel('콘텐츠 검색', { exact: true }).fill('scale-ending-0499');
  await page.locator('.catalog-row button').filter({ hasText: '확장 엔딩 0499' }).click(); timings.endingSearchMs = performance.now() - start;
  await click(page, '흐름'); await page.getByLabel('관계 깊이', { exact: true }).selectOption('2');
  const graph = page.getByRole('img', { name: '조건 효과 관계도' });
  const graphNodes = await graph.getByRole('button').count(), graphEdges = await graph.locator('path[marker-end]').count();
  assert.ok(graphNodes > 1 && graphNodes <= 201, `graph nodes ${graphNodes}`); assert.ok(graphEdges <= 200);
  await page.screenshot({ path: resolve(output, 'bounded-flow.png') });
  assert.deepEqual(errors, []);
  await writeFile(resolve(output, 'evidence.json'), JSON.stringify({ ...fixture, timings, dom: { itemButtons, catalogRows, graphNodes, graphEdges } }, null, 2));
});
