import test from 'node:test';
import assert from 'node:assert/strict';
import { fixture } from '../gameplay/fixture.mjs';
import { buildCatalog, buildRelations, searchCatalog, relationNeighborhood } from '../../dist/packages/analysis-service/catalog.js';

test('embedded raising content is individually searchable with exact editable JSON pointers', async t => {
  const { loaded } = await fixture(t);
  const issue = { code: 'QA_TEST', severity: 'error', file: 'project.json', pointer: '/gameplay/items/0/name', line: 1, column: 1, message: 'Fixture issue', relatedIds: [] };
  const items = buildCatalog(loaded.documents, [issue]);
  const counts = Object.fromEntries(['location', 'item', 'shop', 'interaction', 'screen'].map(kind => [kind, items.filter(item => item.kind === kind).length]));
  assert.deepEqual(counts, { location: 3, item: 2, shop: 1, interaction: 1, screen: 7 });
  const flower = items.find(item => item.kind === 'item' && item.id === 'flower');
  assert.equal(flower.file, 'project.json');
  assert.equal(flower.pointer, '/gameplay/items/0');
  assert.equal(flower.key, 'project.json#/gameplay/items/0');
  assert.equal(flower.issues, 1);
  assert.equal(items.find(item => item.kind === 'item' && item.id === 'book').issues, 0);
  const city = searchCatalog(items, { text: '도시', kind: 'location' });
  assert.equal(city.length, 1); assert.equal(city[0].id, 'city');
  assert.equal(new Set(items.map(item => item.key)).size, items.length);
});

test('raising flow connects location paths, offers, gift items, effect writes, relation events and UI actions', async t => {
  const { loaded } = await fixture(t, project => {
    project.visual.screens.find(screen => screen.id === 'city').elements.push({ id: 'go-market', kind: 'button', x: 0, y: 0, width: 200, height: 50, action: 'visit', locationId: 'market', text: '상점' });
  });
  const items = buildCatalog(loaded.documents), relations = buildRelations(items);
  const item = (kind, id) => items.find(row => row.kind === kind && row.id === id);
  const edge = (fromKind, fromId, toKind, toId, pointerPart) => {
    const found = relations.filter(relation => relation.from === item(fromKind, fromId).key && relation.to === item(toKind, toId).key && relation.pointer.includes(pointerPart));
    assert.ok(found.length > 0, `missing ${fromKind}:${fromId} -> ${toKind}:${toId} ${pointerPart}`);
    return found;
  };
  edge('location', 'city', 'location', 'market', '/connections/');
  edge('shop', 'florist', 'location', 'market', '/locationId');
  edge('shop', 'florist', 'attributes', 'money', '/currencyAttributeId');
  edge('shop', 'florist', 'item', 'flower', '/offers/0/itemId');
  edge('interaction', 'gift-mentor', 'item', 'flower', '/itemId');
  edge('interaction', 'gift-mentor', 'event', 'gift-friendship', '/eventId');
  assert.equal(edge('interaction', 'gift-mentor', 'character', 'mentor', '/effects/0/target/characterId')[0].kind, 'writes');
  edge('screen', 'city', 'location', 'market', '/elements/0/locationId');
  const neighborhood = relationNeighborhood(relations, item('interaction', 'gift-mentor').key, 2, 200);
  const text = JSON.stringify(neighborhood);
  assert.ok(text.includes(item('event', 'gift-friendship').key));
  assert.ok(text.includes(item('shop', 'florist').key));
});
