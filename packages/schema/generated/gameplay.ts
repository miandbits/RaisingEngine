/* Generated from JSON Schema. Do not edit; run npm run build. */

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
export type Positive = number;
export type Nonnegative = number;
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
/**
 * Optional chance in basis points (10000 = 100%). Evaluated after the condition passes; clamped to 0..10000.
 */
export type NumericExpression1 =
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

export interface GameplayDefinition {
  startLocationId: string;
  /**
   * @minItems 1
   * @maxItems 1000
   */
  locations: [Location, ...Location[]];
  /**
   * @maxItems 10000
   */
  items: Item[];
  /**
   * @maxItems 1000
   */
  shops: Shop[];
  /**
   * @maxItems 10000
   */
  interactions: Interaction[];
  /**
   * @maxItems 1000
   */
  diets?: Diet[];
  /**
   * @maxItems 1000
   */
  services?: Service[];
  /**
   * @maxItems 1000
   */
  actions?: Action[];
  /**
   * @maxItems 1000
   */
  policyGroups?: PolicyGroup[];
  /**
   * @maxItems 1000
   */
  policyBudgets?: PolicyBudget[];
  /**
   * @maxItems 1000
   */
  rules?: Rule[];
  ruleCalendar?: {
    daysPerMonth: number;
  };
}
export interface Location {
  id: string;
  name: string;
  screenId?: string;
  connections: string[];
  requirements?: Condition;
  presentation?: Scene;
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
export interface Item {
  id: string;
  name: string;
  description?: string;
  imageAssetId?: string;
  maxStack: Positive;
  initialQuantity?: Nonnegative;
}
export interface Shop {
  id: string;
  name: string;
  locationId: string;
  currencyAttributeId: string;
  /**
   * @maxItems 10000
   */
  offers: {
    itemId: string;
    price: Nonnegative;
    stock: Nonnegative | null;
    maxQuantity?: number;
    confirmation?: Confirmation;
  }[];
  /**
   * @maxItems 10000
   */
  sellOffers?: {
    itemId: string;
    price: Nonnegative;
    maxQuantity?: number;
    confirmation?: Confirmation;
  }[];
}
export interface Confirmation {
  title?: string;
  message?: string;
  confirmLabel?: string;
  cancelLabel?: string;
  /**
   * False executes immediately without a confirmation dialog. Existing content keeps confirmation enabled.
   */
  enabled?: boolean;
}
export interface Interaction {
  id: string;
  name: string;
  locationId: string;
  characterId?: string;
  itemId?: string;
  quantity?: Positive;
  requirements?: Condition;
  effects: Effects;
  /**
   * @maxItems 100
   */
  costs?: {
    attributeId: string;
    amount: Nonnegative;
  }[];
  oncePerPeriod?: boolean;
  eventId?: string;
}
export interface Diet {
  id: string;
  name: string;
  locationId: string;
  /**
   * @maxItems 100
   */
  costs: {
    attributeId: string;
    amount: Nonnegative;
  }[];
  effects: Effects;
  confirmation?: Confirmation;
  intervalPeriods?: number;
}
export interface Service {
  id: string;
  name: string;
  locationId: string;
  /**
   * @maxItems 100
   */
  costs: {
    attributeId: string;
    amount: Nonnegative;
  }[];
  effects: Effects;
  confirmation?: Confirmation;
  maxQuantity?: number;
  requirements?: Condition;
}
export interface Action {
  id: string;
  name: string;
  /**
   * @maxItems 100
   */
  costs: {
    attributeId: string;
    amount: Nonnegative;
  }[];
  effects: Effects;
  confirmation?: Confirmation;
  maxQuantity?: number;
  requirements?: Condition;
  /**
   * @maxItems 1000
   */
  locationIds?: string[];
  rejectNoEffect?: boolean;
  eventId?: string;
  expeditionId?: string;
  /**
   * @maxItems 128
   */
  flow?: {
    id: string;
    condition?: Condition;
    chance?: NumericExpression1;
    thenActionId: string;
    elseActionId?: string;
    quantity?: number;
  }[];
}
export interface PolicyGroup {
  id: string;
  name: string;
  /**
   * @maxItems 1000
   */
  locationIds?: string[];
  trigger: {
    type: "beforeActivity" | "afterActivity";
    intervalPeriods?: number;
  };
  /**
   * @minItems 1
   * @maxItems 1000
   */
  options: [
    {
      id: string;
      name: string;
      actionId: string;
      /**
       * @maxItems 100
       */
      budgetUses?: {
        budgetId: string;
        amount: number;
      }[];
    },
    ...{
      id: string;
      name: string;
      actionId: string;
      /**
       * @maxItems 100
       */
      budgetUses?: {
        budgetId: string;
        amount: number;
      }[];
    }[]
  ];
}
export interface PolicyBudget {
  id: string;
  name: string;
  capacity: number;
  unit?: string;
}
export interface Rule {
  id: string;
  name: string;
  trigger: {
    type:
      | "periodStart"
      | "periodEnd"
      | "monthStart"
      | "monthEnd"
      | "dayStart"
      | "dayEnd"
      | "locationEnter"
      | "purchaseComplete"
      | "relationshipChanged";
    locationId?: string;
    shopId?: string;
    itemId?: string;
    characterId?: string;
    relationshipId?: string;
  };
  condition?: Condition;
  actionId: string;
  quantity?: number;
}
