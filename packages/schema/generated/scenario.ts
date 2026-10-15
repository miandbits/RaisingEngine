/* Generated from JSON Schema. Do not edit; run npm run build. */

export type ScenarioSpecification = Header & {
  id: string;
  description?: string;
  seed: number;
  initialOverrides: Overrides;
  /**
   * @minItems 1
   */
  commands: [
    (
      | {
          type: "planMonth";
          /**
           * @minItems 1
           * @maxItems 12
           */
          activityIds:
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
            | [string, string, string, string, string, string, string, string, string, string, string, string];
        }
      | {
          type: "runPeriod";
          choices: {
            eventId: string;
            nodeId: string;
            choiceId: string;
          }[];
          assertions?: Assertion[];
        }
      | {
          type: "dispatch";
          command:
            | {
                type: "setSchedule";
                /**
                 * @minItems 1
                 * @maxItems 12
                 */
                activityIds:
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
                  | [string, string, string, string, string, string, string, string, string, string, string, string];
              }
            | {
                type: "choose";
                choiceId: string;
              }
            | {
                type:
                  | "executeActivity"
                  | "acknowledgeResult"
                  | "completePeriodWithoutEvents"
                  | "resolvePeriodEnd"
                  | "advanceNarrative";
              }
            | {
                type: "navigate";
                screenId: string;
              }
            | {
                type: "visit";
                locationId: string;
              }
            | {
                type: "buy";
                shopId: string;
                itemId: string;
                quantity: number;
              }
            | {
                type: "interact";
                interactionId: string;
              }
            | {
                type: "acceptQuest";
                questId: string;
              }
            | {
                type: "claimQuest";
                questId: string;
              }
            | {
                type: "equip";
                equipmentId: string;
              }
            | {
                type: "unequip";
                slot: "weapon" | "armor" | "accessory";
              }
            | {
                type: "moveExpedition";
                nodeId: string;
              }
            | {
                type: "returnFromExpedition";
              }
            | {
                type: "battleAction";
                action: "attack" | "guard" | "flee";
              }
            | {
                type: "battleAction";
                action: "useItem";
                itemId: string;
              }
            | {
                type: "sell";
                shopId: string;
                itemId: string;
                quantity: number;
              }
            | {
                type: "useService";
                serviceId: string;
                quantity: number;
              }
            | {
                type: "setDiet";
                dietId: string | null;
              }
            | {
                type: "executeAction";
                actionId: string;
                quantity: number;
              }
            | {
                type: "selectPolicy";
                groupId: string;
                optionId: string | null;
              }
            | {
                type: "assignActivities";
                assignments: {
                  [k: string]: string;
                };
              }
            | {
                type: "executeActorAction";
                actorId: string;
                actionId: string;
                quantity: number;
              }
            | {
                type: "advanceSession";
              }
            | {
                type: "cancelSession";
              }
            | {
                type: "waitTime";
                ticks: number;
              }
            | {
                type: "tacticsAction";
                unitId: string;
                skillId: string;
                targetId: string;
              }
            | {
                type: "tacticsEquip";
                unitId: string;
                equipmentId: string;
              }
            | {
                type: "tacticsFlee";
              }
            | {
                type: "tacticsUnequip";
                unitId: string;
                slot: "weapon" | "armor" | "accessory";
              }
            | {
                type: "cancelActorSession";
                actorId: string;
              };
        }
    ),
    ...(
      | {
          type: "planMonth";
          /**
           * @minItems 1
           * @maxItems 12
           */
          activityIds:
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
            | [string, string, string, string, string, string, string, string, string, string, string, string];
        }
      | {
          type: "runPeriod";
          choices: {
            eventId: string;
            nodeId: string;
            choiceId: string;
          }[];
          assertions?: Assertion[];
        }
      | {
          type: "dispatch";
          command:
            | {
                type: "setSchedule";
                /**
                 * @minItems 1
                 * @maxItems 12
                 */
                activityIds:
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
                  | [string, string, string, string, string, string, string, string, string, string, string, string];
              }
            | {
                type: "choose";
                choiceId: string;
              }
            | {
                type:
                  | "executeActivity"
                  | "acknowledgeResult"
                  | "completePeriodWithoutEvents"
                  | "resolvePeriodEnd"
                  | "advanceNarrative";
              }
            | {
                type: "navigate";
                screenId: string;
              }
            | {
                type: "visit";
                locationId: string;
              }
            | {
                type: "buy";
                shopId: string;
                itemId: string;
                quantity: number;
              }
            | {
                type: "interact";
                interactionId: string;
              }
            | {
                type: "acceptQuest";
                questId: string;
              }
            | {
                type: "claimQuest";
                questId: string;
              }
            | {
                type: "equip";
                equipmentId: string;
              }
            | {
                type: "unequip";
                slot: "weapon" | "armor" | "accessory";
              }
            | {
                type: "moveExpedition";
                nodeId: string;
              }
            | {
                type: "returnFromExpedition";
              }
            | {
                type: "battleAction";
                action: "attack" | "guard" | "flee";
              }
            | {
                type: "battleAction";
                action: "useItem";
                itemId: string;
              }
            | {
                type: "sell";
                shopId: string;
                itemId: string;
                quantity: number;
              }
            | {
                type: "useService";
                serviceId: string;
                quantity: number;
              }
            | {
                type: "setDiet";
                dietId: string | null;
              }
            | {
                type: "executeAction";
                actionId: string;
                quantity: number;
              }
            | {
                type: "selectPolicy";
                groupId: string;
                optionId: string | null;
              }
            | {
                type: "assignActivities";
                assignments: {
                  [k: string]: string;
                };
              }
            | {
                type: "executeActorAction";
                actorId: string;
                actionId: string;
                quantity: number;
              }
            | {
                type: "advanceSession";
              }
            | {
                type: "cancelSession";
              }
            | {
                type: "waitTime";
                ticks: number;
              }
            | {
                type: "tacticsAction";
                unitId: string;
                skillId: string;
                targetId: string;
              }
            | {
                type: "tacticsEquip";
                unitId: string;
                equipmentId: string;
              }
            | {
                type: "tacticsFlee";
              }
            | {
                type: "tacticsUnequip";
                unitId: string;
                slot: "weapon" | "armor" | "accessory";
              }
            | {
                type: "cancelActorSession";
                actorId: string;
              };
        }
    )[]
  ];
  /**
   * @minItems 1
   */
  assertions: [Assertion, ...Assertion[]];
  stepLimit: number;
};
export type Assertion =
  | {
      kind: "condition";
      condition: Condition;
    }
  | {
      kind: "ending";
      endingId: string;
    };
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
export type CharacterOwner =
  | {
      kind: "actor";
    }
  | {
      kind: "character";
      characterId: string;
    };

export interface Header {
  $schema?: string;
  formatVersion: "0.1.0";
  /**
   * @maxItems 32
   */
  tags?: string[];
}
export interface Overrides {
  attributes: {
    [k: string]: number;
  };
  flags: {
    [k: string]: boolean;
  };
  relationships: {
    [k: string]: {
      [k: string]: number;
    };
  };
}
