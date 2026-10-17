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
    /**
     * @minItems 1
     * @maxItems 64
     */
    attackPattern?: [number, ...number[]];
    damageVariance?: number;
    guardRoundDown?: boolean;
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
        manualBattle?: boolean;
        battleRequirements?: Condition;
        exit?: boolean;
        presentation?: Scene;
        encounterId?: string;
        requirements?: Condition;
        entryActionId?: string;
        repeatEntryAction?: boolean;
        /**
         * @maxItems 100
         */
        actionIds?: string[];
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
        manualBattle?: boolean;
        battleRequirements?: Condition;
        exit?: boolean;
        presentation?: Scene;
        encounterId?: string;
        requirements?: Condition;
        entryActionId?: string;
        repeatEntryAction?: boolean;
        /**
         * @maxItems 100
         */
        actionIds?: string[];
      }[]
    ];
    entryActionId?: string;
    moveActionId?: string;
    completionActionId?: string;
    retreatActionId?: string;
    defeatActionId?: string;
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
    encounterId?: string;
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
  tactics?: {
    /**
     * @maxItems 128
     */
    units: {
      id: string;
      name: string;
      characterId?: string;
      faction: string;
      control: "player" | "ai";
      row: number;
      column: number;
      stats: {
        maxHp: number;
        attack: number;
        defense: number;
        speed: number;
      };
      /**
       * @maxItems 32
       */
      skillIds: string[];
      statAttributes?: {
        maxHp?: string;
        attack?: string;
        defense?: string;
        speed?: string;
      };
    }[];
    /**
     * @maxItems 128
     */
    skills: {
      id: string;
      name: string;
      target: "enemy" | "frontEnemy" | "ally" | "self" | "allEnemies" | "allAllies";
      kind: "damage" | "heal" | "status";
      power: number;
      accuracy: number;
      castTurns: number;
      cooldown: number;
      statusId?: string;
    }[];
    /**
     * @maxItems 128
     */
    statuses: {
      id: string;
      name: string;
      turns: number;
      damage: number;
      heal: number;
      attack: number;
      defense: number;
      stun: boolean;
    }[];
    /**
     * @maxItems 128
     */
    encounters: {
      id: string;
      name: string;
      /**
       * @maxItems 8
       */
      party:
        | []
        | [string]
        | [string, string]
        | [string, string, string]
        | [string, string, string, string]
        | [string, string, string, string, string]
        | [string, string, string, string, string, string]
        | [string, string, string, string, string, string, string]
        | [string, string, string, string, string, string, string, string];
      /**
       * @maxItems 20
       */
      waves:
        | []
        | [
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            }
          ]
        | [
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            }
          ]
        | [
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            }
          ]
        | [
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            }
          ]
        | [
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            }
          ]
        | [
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            }
          ]
        | [
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            }
          ]
        | [
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            }
          ]
        | [
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            }
          ]
        | [
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            }
          ]
        | [
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            }
          ]
        | [
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            }
          ]
        | [
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            }
          ]
        | [
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            }
          ]
        | [
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            }
          ]
        | [
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            }
          ]
        | [
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            }
          ]
        | [
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            }
          ]
        | [
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            }
          ]
        | [
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            },
            {
              /**
               * @maxItems 16
               */
              unitIds:
                | []
                | [string]
                | [string, string]
                | [string, string, string]
                | [string, string, string, string]
                | [string, string, string, string, string]
                | [string, string, string, string, string, string]
                | [string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string]
                | [string, string, string, string, string, string, string, string, string, string, string, string]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ]
                | [
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string,
                    string
                  ];
            }
          ];
      experience: number;
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
    growth: {
      experiencePerLevel: number;
      maxLevel: number;
      perLevel: {
        maxHp: number;
        attack: number;
        defense: number;
        speed: number;
      };
    };
  };
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
