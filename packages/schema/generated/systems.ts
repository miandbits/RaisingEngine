/* Generated from JSON Schema. Do not edit; run npm run build. */

export type CharacterOwner =
  | {
      kind: "actor";
    }
  | {
      kind: "character";
      characterId: string;
    };
export type NumericExpression =
  | {
      op: "literal";
      value: number;
    }
  | {
      op: "read";
      target:
        | {
            kind: "attribute";
            attributeId: string;
            owner?: CharacterOwner;
          }
        | {
            kind: "relationship";
            characterId: string;
            relationshipId: string;
            owner?: CharacterOwner;
          }
        | {
            kind: "inventory";
            itemId: string;
            owner?: CharacterOwner;
          };
    }
  | {
      op: "time";
      field: "periodIndex" | "year" | "month" | "periodInMonth";
    }
  | {
      op: "add" | "subtract" | "multiply";
      left: NumericExpression;
      right: NumericExpression;
    }
  | {
      op: "clamp";
      value: NumericExpression;
      min: number;
      max: number;
    };
export type Effects = (
  | {
      op: "add" | "set";
      target:
        | {
            kind: "attribute";
            attributeId: string;
            owner?: CharacterOwner;
          }
        | {
            kind: "relationship";
            characterId: string;
            relationshipId: string;
            owner?: CharacterOwner;
          }
        | {
            kind: "inventory";
            itemId: string;
            owner?: CharacterOwner;
          };
      value: NumericExpression;
    }
  | {
      op: "setFlag";
      flagId: string;
      value: boolean;
    }
)[];
export type Condition =
  | {
      op: "always";
    }
  | {
      op: "all" | "any";
      /**
       * @minItems 1
       */
      conditions: [Condition, ...Condition[]];
    }
  | {
      op: "not";
      condition: Condition;
    }
  | {
      op: "compare";
      comparator: "eq" | "ne" | "gt" | "gte" | "lt" | "lte";
      left: NumericExpression;
      right: NumericExpression;
    }
  | {
      op: "flag";
      flagId: string;
      equals: boolean;
    }
  | {
      op: "eventCompleted";
      eventId: string;
      owner?: CharacterOwner;
    };

export interface SystemsDefinition {
  battleHistoryLimit?: number;
  hero: {
    maxHpAttributeId: string;
    attackAttributeId: string;
    defenseAttributeId: string;
  };
  /**
   * @maxItems 1000
   */
  equipment: {
    id: string;
    name: string;
    description?: string;
    itemId: string;
    slot: "weapon" | "armor" | "accessory";
    modifiers: {
      maxHp: number;
      attack: number;
      defense: number;
    };
  }[];
  /**
   * @maxItems 1000
   */
  consumables: {
    itemId: string;
    heal: number;
  }[];
  /**
   * @maxItems 1000
   */
  enemies: {
    id: string;
    name: string;
    description?: string;
    maxHp: number;
    attack: number;
    defense: number;
    rewards: {
      /**
       * @maxItems 1000
       */
      items: {
        itemId: string;
        quantity: number;
      }[];
      effects: Effects;
    };
  }[];
  /**
   * @maxItems 1000
   */
  expeditions: {
    id: string;
    name: string;
    description?: string;
    locationId: string;
    entryNodeId: string;
    /**
     * @minItems 1
     * @maxItems 1000
     */
    nodes: [
      {
        id: string;
        name: string;
        description?: string;
        /**
         * @maxItems 1000
         */
        connections: string[];
        enemyId?: string;
        exit?: boolean;
        presentation?: Scene;
      },
      ...{
        id: string;
        name: string;
        description?: string;
        /**
         * @maxItems 1000
         */
        connections: string[];
        enemyId?: string;
        exit?: boolean;
        presentation?: Scene;
      }[]
    ];
  }[];
  /**
   * @maxItems 1000
   */
  quests: {
    id: string;
    name: string;
    description?: string;
    locationId: string;
    requirements: Condition;
    enemyId: string;
    targetCount: number;
    rewards: {
      /**
       * @maxItems 1000
       */
      items: {
        itemId: string;
        quantity: number;
      }[];
      effects: Effects;
    };
  }[];
  /**
   * @maxItems 1000
   */
  relationshipStages: {
    id: string;
    name: string;
    description?: string;
    characterId: string;
    relationshipId: string;
    threshold: number;
    eventId?: string;
  }[];
}
export interface Scene {
  backgroundAssetId?: string | null;
  musicAssetId?: string | null;
  soundAssetId?: string;
  /**
   * @maxItems 3
   */
  portraits?:
    | []
    | [
        {
          characterId: string;
          portraitId: string;
          position: "left" | "center" | "right";
          scale?: number;
          offsetX?: number;
          offsetY?: number;
        }
      ]
    | [
        {
          characterId: string;
          portraitId: string;
          position: "left" | "center" | "right";
          scale?: number;
          offsetX?: number;
          offsetY?: number;
        },
        {
          characterId: string;
          portraitId: string;
          position: "left" | "center" | "right";
          scale?: number;
          offsetX?: number;
          offsetY?: number;
        }
      ]
    | [
        {
          characterId: string;
          portraitId: string;
          position: "left" | "center" | "right";
          scale?: number;
          offsetX?: number;
          offsetY?: number;
        },
        {
          characterId: string;
          portraitId: string;
          position: "left" | "center" | "right";
          scale?: number;
          offsetX?: number;
          offsetY?: number;
        },
        {
          characterId: string;
          portraitId: string;
          position: "left" | "center" | "right";
          scale?: number;
          offsetX?: number;
          offsetY?: number;
        }
      ];
}
