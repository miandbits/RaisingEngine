/* Generated from JSON Schema. Do not edit; run npm run build. */

export type ActivityDefinition = Header & {
  iconAssetId?: string;
  id: string;
  label: string;
  description?: string;
  /**
   * Current-state attribute details shown when selecting this activity; not a forecast of a future scheduled period.
   *
   * @minItems 1
   * @maxItems 8
   */
  previewAttributes?:
    | [ActivityPreviewAttribute]
    | [ActivityPreviewAttribute, ActivityPreviewAttribute]
    | [ActivityPreviewAttribute, ActivityPreviewAttribute, ActivityPreviewAttribute]
    | [ActivityPreviewAttribute, ActivityPreviewAttribute, ActivityPreviewAttribute, ActivityPreviewAttribute]
    | [
        ActivityPreviewAttribute,
        ActivityPreviewAttribute,
        ActivityPreviewAttribute,
        ActivityPreviewAttribute,
        ActivityPreviewAttribute
      ]
    | [
        ActivityPreviewAttribute,
        ActivityPreviewAttribute,
        ActivityPreviewAttribute,
        ActivityPreviewAttribute,
        ActivityPreviewAttribute,
        ActivityPreviewAttribute
      ]
    | [
        ActivityPreviewAttribute,
        ActivityPreviewAttribute,
        ActivityPreviewAttribute,
        ActivityPreviewAttribute,
        ActivityPreviewAttribute,
        ActivityPreviewAttribute,
        ActivityPreviewAttribute
      ]
    | [
        ActivityPreviewAttribute,
        ActivityPreviewAttribute,
        ActivityPreviewAttribute,
        ActivityPreviewAttribute,
        ActivityPreviewAttribute,
        ActivityPreviewAttribute,
        ActivityPreviewAttribute,
        ActivityPreviewAttribute
      ];
  requirements: Condition;
  costs: {
    attributeId: string;
    amount: number;
  }[];
  baseEffects: Effects;
  successCheck?: {
    chance: NumericExpression;
  };
  successEffects?: Effects;
  failureEffects?: Effects;
  presentation?: Scene;
  expeditionId?: string;
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
    }
  | {
      op: "stateIs";
      stateGroupId: string;
      stateValueId: string;
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
  | {
      op: "setState";
      stateGroupId: string;
      stateValueId: string;
      owner?: CharacterOwner;
    }
)[];

export interface Header {
  $schema?: string;
  formatVersion: "0.1.0";
  /**
   * @maxItems 32
   */
  tags?: string[];
}
export interface ActivityPreviewAttribute {
  attributeId: string;
  label?: string;
  valueFormat?: string;
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
