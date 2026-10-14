import { mkdir, mkdtemp, cp, readFile, writeFile } from 'node:fs/promises';
import { resolve, join } from 'node:path';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { createVisual } from '../../dist/packages/visual/model.js';

export function systemsDefinition() {
  return {
    hero: { maxHpAttributeId: 'stamina', attackAttributeId: 'skill', defenseAttributeId: 'stress' },
    equipment: [{ id: 'iron-sword', name: '철 검', itemId: 'sword', slot: 'weapon', modifiers: { maxHp: 0, attack: 8, defense: 0 } }],
    consumables: [{ itemId: 'potion', heal: 20 }],
    enemies: [{ id: 'slime', name: '숲 슬라임', maxHp: 16, attack: 12, defense: 1, rewards: { items: [{ itemId: 'gem', quantity: 1 }], effects: [] } }],
    expeditions: [{ id: 'forest', name: '숲 탐험', locationId: 'home', entryNodeId: 'gate', nodes: [
      { id: 'gate', name: '숲 입구', connections: ['clearing'] },
      { id: 'clearing', name: '전투 공터', connections: ['gate', 'exit'], enemyId: 'slime' },
      { id: 'exit', name: '숲 출구', connections: ['clearing'], exit: true },
    ] }],
    quests: [{ id: 'forest-help', name: '숲을 도와줘', locationId: 'home', requirements: { op: 'always' }, enemyId: 'slime', targetCount: 1,
      rewards: { items: [{ itemId: 'flower', quantity: 1 }], effects: [{ op: 'add', target: { kind: 'relationship', characterId: 'mentor', relationshipId: 'trust' }, value: { op: 'literal', value: 15 } }] } }],
    relationshipStages: [{ id: 'friendship', name: '친구', characterId: 'mentor', relationshipId: 'trust', threshold: 15, eventId: 'friendship-unlock' }],
  };
}
export async function fixture(mutate, afterWrite) {
  await mkdir('.cache/systems-tests', { recursive: true });
  const root = await mkdtemp(resolve('.cache/systems-tests/project-'));
  await cp(resolve('examples/academy-demo'), root, { recursive: true });
  const file = join(root, 'project.json'), project = JSON.parse(await readFile(file, 'utf8'));
  project.visual = createVisual(); project.visual.screens.push({ id: 'home', label: '집', elements: [] });
  project.gameplay = { startLocationId: 'home', locations: [{ id: 'home', name: '집', screenId: 'home', connections: [] }],
    items: [{ id: 'sword', name: '철 검', maxStack: 1, initialQuantity: 1 }, { id: 'potion', name: '회복약', maxStack: 5, initialQuantity: 2 }, { id: 'gem', name: '보석', maxStack: 2 }, { id: 'flower', name: '꽃', maxStack: 2 }], shops: [],
    interactions: [{ id: 'gift-sword', name: '검 선물', locationId: 'home', itemId: 'sword', effects: [] }] };
  project.systems = systemsDefinition();
  project.initialState.attributes = { ...project.initialState.attributes, stamina: 60, skill: 5, stress: 0 };
  project.initialState.relationships = { ...project.initialState.relationships, mentor: { trust: 0 } };
  if (mutate) mutate(project);
  await writeFile(file, JSON.stringify(project, null, 2));
  await writeFile(join(root, 'activities/forest-trip.json'), JSON.stringify({ formatVersion: '0.1.0', id: 'forest-trip', label: '숲 탐험', requirements: { op: 'always' }, costs: [{ attributeId: 'money', amount: 5 }], baseEffects: [], expeditionId: 'forest' }, null, 2));
  await writeFile(join(root, 'events/friendship-unlock.json'), JSON.stringify({ formatVersion: '0.1.0', id: 'friendship-unlock', label: '숲에서 시작된 우정', trigger: 'interaction', priority: 20, window: { fromPeriod: 0, throughPeriod: project.durationMonths * 3 - 1 }, requirements: { op: 'always' }, entryNodeId: 'hello', nodes: [{ id: 'hello', type: 'dialogue', speakerId: 'mentor', text: '도와줘서 고마워!', next: 'end' }, { id: 'end', type: 'end' }] }, null, 2));
  if (afterWrite) await afterWrite(root);
  const loaded = await loadProject(root), built = await buildSnapshot(loaded);
  return { root, project, loaded, built };
}
