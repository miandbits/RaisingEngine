import { cp, mkdtemp, readFile, writeFile, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { createVisual } from '../../dist/packages/visual/model.js';

export function gameplayDefinition() {
  return {
    startLocationId: 'home',
    locations: [
      { id: 'home', name: '집', screenId: 'home', connections: ['city'] },
      { id: 'city', name: '도시', screenId: 'city', connections: ['home', 'market'] },
      { id: 'market', name: '상점', screenId: 'market', connections: ['city'] },
    ],
    items: [{ id: 'flower', name: '꽃', maxStack: 3 }, { id: 'book', name: '책', maxStack: 2 }],
    shops: [{ id: 'florist', name: '꽃집', locationId: 'market', currencyAttributeId: 'money', offers: [
      { itemId: 'flower', price: 10, stock: 4 }, { itemId: 'book', price: 30, stock: null },
    ] }],
    interactions: [{ id: 'gift-mentor', name: '유나에게 꽃 선물', locationId: 'city', characterId: 'mentor',
      itemId: 'flower', quantity: 1, oncePerPeriod: true, costs: [{ attributeId: 'stamina', amount: 2 }],
      effects: [{ op: 'add', target: { kind: 'relationship', characterId: 'mentor', relationshipId: 'trust' }, value: { op: 'literal', value: 15 } }],
      eventId: 'gift-friendship',
    }],
  };
}

export async function fixture(t, mutate, afterWrite) {
  const root = await mkdtemp(path.join(tmpdir(), 'raising-gameplay-'));
  t.after(() => rm(root, { recursive: true, force: true }));
  await cp(path.resolve('examples/academy-year'), root, { recursive: true });
  const file = path.join(root, 'project.json');
  const project = JSON.parse(await readFile(file, 'utf8'));
  project.visual = createVisual();
  for (const id of ['home', 'city', 'market']) project.visual.screens.push({ id, label: id, elements: [] });
  project.gameplay = gameplayDefinition();
  if (mutate) mutate(project);
  await writeFile(file, JSON.stringify(project, null, 2));
  await writeFile(path.join(root, 'events/gift-friendship.json'), JSON.stringify({
    formatVersion: '0.1.0', id: 'gift-friendship', label: '꽃으로 맺은 우정', priority: 90, trigger: 'interaction',
    window: { fromPeriod: 0, throughPeriod: 35 }, requirements: {
      op: 'compare', comparator: 'gte', left: { op: 'read', target: { kind: 'relationship', characterId: 'mentor', relationshipId: 'trust' } }, right: { op: 'literal', value: 15 },
    }, entryNodeId: 'hello', nodes: [
      { id: 'hello', type: 'dialogue', speakerId: 'mentor', text: '고마워. 이 꽃을 오래 간직할게.', next: 'end' },
      { id: 'end', type: 'end' },
    ],
  }, null, 2));
  if (afterWrite) await afterWrite(root);
  const loaded = await loadProject(root), built = await buildSnapshot(loaded);
  return { root, project, loaded, built };
}
