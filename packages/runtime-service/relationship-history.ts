import type { RunState } from '../core/model.js';

/** Last nonzero net change per shared relationship. Derived from commands, never trusted from saves. */
export type RelationshipChanges = Readonly<Record<string, Readonly<Record<string, number>>>>;
export function trackRelationshipChanges(history: RelationshipChanges, before: RunState, after: RunState): RelationshipChanges {
  const result = { ...history };
  for (const [characterId, relationships] of Object.entries(after.relationships)) {
    for (const [relationshipId, value] of Object.entries(relationships)) {
      const delta = value - (before.relationships[characterId]?.[relationshipId] ?? value);
      if (delta !== 0) result[characterId] = { ...result[characterId], [relationshipId]: delta };
    }
  }
  return result;
}
