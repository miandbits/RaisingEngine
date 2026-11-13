import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdir, readFile, mkdtemp, cp } from 'node:fs/promises';
import { resolve } from 'node:path';
import { openJourneyStudio, click, fill, select, state } from './gui-helpers.mjs';

test('designer binds resource, inventory and relationship HUD text and observes live purchase and gift values', { timeout: 120000 }, async t => {
  const output = resolve('.cache/journey-hud-ui'); await mkdir(output, { recursive: true });
  const root = process.env.AUTHOR_SAMPLE === '1' ? resolve('examples/academy-journey') : resolve(await mkdtemp(`${output}/project-`));
  if (process.env.AUTHOR_SAMPLE !== '1') await cp(resolve('examples/academy-journey'), root, { recursive: true });
  const { app, page, errors } = await openJourneyStudio(t);
  await app.evaluate(({ dialog }, root) => { dialog.showOpenDialog = async () => ({ canceled: false, filePaths: [root] }); }, root);
  await click(page, '열기'); await page.getByText(root, { exact: true }).waitFor();
  await click(page, '화면·연출'); await select(page, '편집 화면', 'home');
  const definitions = [
    { binding: 'attribute-value', match: element => element.attributeId === 'money', fields: [['표시할 능력·자원', 'money']], format: '소지금 {value} G', y: 500 },
    { binding: 'item-count', match: element => element.itemId === 'flower', fields: [['수량을 표시할 아이템', 'flower']], format: '감사의 꽃 {value}개', y: 560 },
    { binding: 'relationship-value', match: element => element.characterId === 'mentor' && element.relationshipId === 'trust', fields: [['관계 인물', 'mentor'], ['표시할 관계', 'trust']], format: '유나 신뢰 {value}', y: 620 },
  ];
  for (const definition of definitions) {
    const documents = (await page.evaluate(() => window.studio.view())).view.documents;
    const home = documents.find(doc => doc.kind === 'project').data.visual.screens.find(screen => screen.id === 'home');
    const existing = home.elements.find(element => element.binding === definition.binding && definition.match(element));
    if (existing) await page.locator('.visual-layers button').filter({ has: page.getByText(existing.id, { exact: true }) }).click();
    else { await select(page, '추가할 UI 종류', 'text'); await click(page, '요소 추가'); }
    await select(page, 'UI 데이터 연결', definition.binding);
    for (const [label, value] of definition.fields) await select(page, label, value);
    await fill(page, '숫자 표시 형식', definition.format);
    for (const [field, value] of Object.entries({ x: 850, y: definition.y, width: 350, height: 44 })) await fill(page, `UI ${field}`, value);
  }
  await click(page, '초안 저장'); await page.getByText('저장된 상태', { exact: true }).waitFor();
  const project = JSON.parse(await readFile(resolve(root, 'project.json'), 'utf8'));
  const elements = project.visual.screens.find(screen => screen.id === 'home').elements;
  for (const definition of definitions) assert.equal(elements.filter(element => element.binding === definition.binding && definition.match(element)).length, 1);
  await page.screenshot({ path: resolve(output, 'hud-editor.png') });
  await click(page, '실제 플레이로 확인'); await click(page, '새 게임 시작');
  const hud = page.locator('.visual-stage');
  async function expectValues(money, flowers, trust) {
    await hud.getByText(`소지금 ${money} G`, { exact: true }).waitFor();
    await hud.getByText(`감사의 꽃 ${flowers}개`, { exact: true }).waitFor();
    await hud.getByText(`유나 신뢰 ${trust}`, { exact: true }).waitFor();
  }
  await expectValues(60, 0, 0);
  await click(page, '작은 상점 방문'); await click(page, '감사의 꽃 구매'); await click(page, '우리 집 방문');
  await expectValues(20, 1, 0);
  await click(page, '도시 광장 방문'); await click(page, '유나에게 꽃 선물'); await click(page, '이야기 계속'); await click(page, '우리 집 방문');
  await expectValues(20, 0, 15);
  assert.equal((await state(page)).relationships.mentor.trust, 15);
  await page.evaluate(async () => { await Promise.all(document.getAnimations().map(animation => animation.finished.catch(() => {}))); });
  await page.screenshot({ path: resolve(output, 'hud-after-gift.png') });
  assert.deepEqual(errors, []);
});
