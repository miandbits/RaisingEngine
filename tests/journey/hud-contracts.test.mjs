import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdir, mkdtemp, cp } from 'node:fs/promises';
import { resolve } from 'node:path';
import { StudioEditor } from '../../dist/packages/studio-service/editor.js';
import { numericBinding } from '../../dist/packages/visual/bindings.js';
import { openJourneyStudio, click } from './gui-helpers.mjs';

async function fixture() {
  const output = resolve('.cache/journey-hud-contracts'); await mkdir(output, { recursive: true });
  const root = resolve(await mkdtemp(`${output}/project-`)); await cp(resolve('examples/academy-journey'), root, { recursive: true });
  const editor = new StudioEditor(); await editor.open(root);
  const document = editor.documents.find(doc => doc.kind === 'project'), project = structuredClone(document.data);
  const bindings = project.visual.screens.find(screen => screen.id === 'home').elements.filter(element => ['attribute-value', 'item-count', 'relationship-value'].includes(element.binding));
  assert.equal(bindings.length, 3, 'run HUD authoring or use persisted HUD sample');
  return { root, editor, document, project, bindings };
}

test('HUD semantic references and bounded display format reject invalid edits precisely', async () => {
  const { editor, document, project, bindings } = await fixture();
  const cases = [
    ['attribute-value', 'attributeId', 'missing-money', 'REFERENCE_MISSING'],
    ['item-count', 'itemId', 'missing-item', 'UI_ACTION_REFERENCE'],
    ['relationship-value', 'relationshipId', 'missing-relationship', 'REFERENCE_MISSING'],
    ['attribute-value', 'valueFormat', 'x'.repeat(257), 'SCHEMA_INVALID'],
    ['attribute-value', 'valueFormat', { expression: 'not-a-format' }, 'SCHEMA_INVALID'],
  ];
  for (const [binding, field, value, code] of cases) {
    const candidate = structuredClone(project), home = candidate.visual.screens.find(screen => screen.id === 'home');
    const element = home.elements.find(element => element.binding === binding); element[field] = value;
    editor.edit({ type: 'replace', file: document.file, data: candidate }, editor.revision);
    const result = await editor.validate(editor.revision);
    assert.ok(result.view.diagnostics.some(d => d.code === code && d.pointer.endsWith(`/${field}`)), `${binding}.${field}: ${JSON.stringify(result.view.diagnostics.slice(0, 2))}`);
  }
  assert.equal(numericBinding({ ...bindings[0], valueFormat: '{label}: {value} / {value} / {unknown}' }, { stats: [{ id: 'money', label: '돈', value: -7 }] }), '돈: -7 / -7 / {unknown}');
});

test('actual no-run player HUD shows project overrides and initial inventory before starting a run', { timeout: 90000 }, async t => {
  const { root, editor, document, project, bindings } = await fixture();
  project.initialState.attributes.money = 37; project.initialState.relationships.mentor = { trust: 12 };
  project.gameplay.items.find(item => item.id === 'flower').initialQuantity = 2;
  project.visual.screens.find(screen => screen.id === 'title').elements.push(...bindings.map((element, i) => ({ ...element, id: `initial-hud-${i}`, x: 40, y: 80 + i * 55, width: 350, height: 44 })));
  editor.edit({ type: 'replace', file: document.file, data: project }, editor.revision);
  assert.equal((await editor.save(editor.revision)).error, null);
  const { app, page, errors } = await openJourneyStudio(t);
  await app.evaluate(({ dialog }, root) => { dialog.showOpenDialog = async () => ({ canceled: false, filePaths: [root] }); }, root);
  await click(page, '열기'); await page.getByText(root, { exact: true }).waitFor(); await click(page, '플레이 미리보기');
  assert.equal((await page.evaluate(() => window.player.view())).view.state, null);
  const stage = page.locator('.visual-stage');
  for (const label of ['소지금 37 G', '감사의 꽃 2개', '유나 신뢰 12']) await stage.getByText(label, { exact: true }).waitFor();
  await click(page, '새 게임 시작');
  for (const label of ['소지금 37 G', '감사의 꽃 2개', '유나 신뢰 12']) await stage.getByText(label, { exact: true }).waitFor();
  assert.deepEqual(errors, []);
});
