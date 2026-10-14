import type { ProjectDefinition } from './generated/project.js';
import type { AttributeDefinitions } from './generated/attributes.js';
import type { FlagDefinitions } from './generated/flags.js';
import type { CharacterDefinition } from './generated/character.js';
import type { ActivityDefinition } from './generated/activity.js';
import type { EventDefinition } from './generated/event.js';
import type { EndingDefinition } from './generated/ending.js';
import type { AssetManifest } from './generated/assets.js';
import type { ScenarioSpecification } from './generated/scenario.js';

export interface ContentByKind {
  project: ProjectDefinition;
  attributes: AttributeDefinitions;
  flags: FlagDefinitions;
  character: CharacterDefinition;
  activity: ActivityDefinition;
  event: EventDefinition;
  ending: EndingDefinition;
  assets: AssetManifest;
  scenario: ScenarioSpecification;
}
