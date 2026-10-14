/* Generated from JSON Schema. Do not edit; run npm run build. */

export type Element = {
  [k: string]: unknown;
} & {
  [k: string]: unknown;
} & {
  [k: string]: unknown;
} & {
  [k: string]: unknown;
} & {
  [k: string]: unknown;
} & {
  [k: string]: unknown;
} & {
  [k: string]: unknown;
} & {
  [k: string]: unknown;
} & {
  [k: string]: unknown;
} & {
  [k: string]: unknown;
} & {
  [k: string]: unknown;
} & {
  [k: string]: unknown;
} & {
  [k: string]: unknown;
} & {
  [k: string]: unknown;
} & {
  [k: string]: unknown;
} & {
  [k: string]: unknown;
} & {
  [k: string]: unknown;
} & {
  [k: string]: unknown;
} & {
  [k: string]: unknown;
} & {
  [k: string]: unknown;
} & {
  id: string;
  kind:
    | "panel"
    | "text"
    | "image"
    | "stats"
    | "schedule"
    | "dialogue"
    | "choices"
    | "result"
    | "button"
    | "locations"
    | "shop"
    | "inventory"
    | "interactions"
    | "relationships"
    | "equipment"
    | "quests"
    | "expedition"
    | "battle"
    | "relationship-stages"
    | "component"
    | "diets"
    | "services"
    | "sell"
    | "actions"
    | "policies"
    | "collection"
    | "meter"
    | "group"
    | "input"
    | "plugin";
  label?: string;
  x: number;
  y: number;
  width: number;
  height: number;
  text?: string;
  binding?:
    | "none"
    | "title"
    | "description"
    | "calendar"
    | "calendar-year"
    | "calendar-month"
    | "calendar-day"
    | "calendar-weekday"
    | "schedule-prompt"
    | "ending"
    | "speaker"
    | "story"
    | "location"
    | "attribute-value"
    | "relationship-value"
    | "item-count"
    | "user-field";
  imageSource?: "asset" | "background" | "portrait-left" | "portrait-center" | "portrait-right" | "speaker-portrait";
  imageAssetId?: string;
  fit?: "cover" | "contain";
  scheduleLayout?: "columns" | "calendar" | "menu";
  statsLayout?: "bars" | "compact";
  shopLayout?: "list" | "cards";
  /**
   * @minItems 1
   * @maxItems 100
   */
  attributeIds?: [string, ...string[]];
  autoLabel?: boolean;
  /**
   * @maxItems 12
   */
  scheduleGroups?:
    | []
    | [
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        }
      ]
    | [
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        }
      ]
    | [
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        }
      ]
    | [
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        }
      ]
    | [
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        }
      ]
    | [
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        }
      ]
    | [
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        }
      ]
    | [
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        }
      ]
    | [
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        }
      ]
    | [
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        }
      ]
    | [
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        }
      ]
    | [
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        }
      ];
  action?:
    | "start"
    | "advance"
    | "save"
    | "load"
    | "sound"
    | "navigate"
    | "visit"
    | "buy"
    | "interact"
    | "acceptQuest"
    | "claimQuest"
    | "equip"
    | "unequip"
    | "moveExpedition"
    | "returnFromExpedition"
    | "battleAction"
    | "sell"
    | "useService"
    | "setDiet"
    | "executeAction"
    | "selectPolicy"
    | "selectBattleAction"
    | "selectBattleTarget"
    | "confirmBattleAction"
    | "cancelBattleAction";
  screenId?: string;
  locationId?: string;
  shopId?: string;
  itemId?: string;
  interactionId?: string;
  questId?: string;
  equipmentId?: string;
  nodeId?: string;
  slot?: "weapon" | "armor" | "accessory";
  battleAction?: "attack" | "guard" | "useItem" | "flee";
  quantity?: number;
  visibleWhen?: "always" | "planning" | "result" | "dialogue" | "choice" | "finished";
  hidden?: boolean;
  locked?: boolean;
  fill?: string;
  color?: string;
  fontSize?: number;
  radius?: number;
  opacity?: number;
  align?: "left" | "center" | "right";
  motion?: "none" | "fade" | "slide-left" | "slide-right" | "zoom";
  durationMs?: number;
  attributeId?: string;
  characterId?: string;
  relationshipId?: string;
  valueFormat?: string;
  frameAssetId?: string;
  entryFrameAssetId?: string;
  portraitId?: string;
  styleId?: string;
  fontAssetId?: string;
  padding?: number;
  borderWidth?: number;
  borderColor?: string;
  hoverFill?: string;
  pressedFill?: string;
  disabledColor?: string;
  anchorX?: "left" | "center" | "right" | "stretch";
  anchorY?: "top" | "center" | "bottom" | "stretch";
  componentId?: string;
  /**
   * @maxItems 100
   */
  overrides?: {
    elementId: string;
    values: {
      text?: string;
      label?: string;
      imageAssetId?: string;
      action?:
        | "start"
        | "advance"
        | "save"
        | "load"
        | "sound"
        | "navigate"
        | "visit"
        | "buy"
        | "interact"
        | "acceptQuest"
        | "claimQuest"
        | "equip"
        | "unequip"
        | "moveExpedition"
        | "returnFromExpedition"
        | "battleAction"
        | "sell"
        | "useService"
        | "setDiet"
        | "executeAction"
        | "selectPolicy"
        | "selectBattleAction"
        | "selectBattleTarget"
        | "confirmBattleAction"
        | "cancelBattleAction";
      screenId?: string;
      locationId?: string;
      shopId?: string;
      itemId?: string;
      interactionId?: string;
      quantity?: number;
      styleId?: string;
      fill?: string;
      color?: string;
      fontSize?: number;
      serviceId?: string;
      dietId?: string;
      actionId?: string;
      groupId?: string;
      optionId?: string | null;
      value?: number;
      maxValue?: number;
      battleAction?: "attack" | "guard" | "useItem" | "flee";
      battleTarget?: "self" | "enemy";
      clipChildren?: boolean;
      userFieldId?: string;
    };
  }[];
  serviceId?: string;
  dietId?: string;
  actionId?: string;
  groupId?: string;
  optionId?: string | null;
  /**
   * @maxItems 1000
   */
  actionIds?: string[];
  /**
   * @maxItems 1000
   */
  groupIds?: string[];
  visibleCondition?: Condition;
  enabledCondition?: Condition;
  source?: {
    type:
      | "shopOffers"
      | "policyOptions"
      | "relationships"
      | "table"
      | "battleActors"
      | "battleActions"
      | "battleTargets"
      | "battleLog"
      | "battleRewards";
    shopId?: string;
    groupId?: string;
    characterId?: string;
    tableId?: string;
  };
  layout?: "list" | "grid";
  columns?: number;
  gap?: number;
  rowHeight?: number;
  /**
   * @maxItems 200
   */
  bindings?: {
    elementId: string;
    property:
      | "text"
      | "label"
      | "imageAssetId"
      | "action"
      | "actionId"
      | "shopId"
      | "itemId"
      | "groupId"
      | "optionId"
      | "screenId"
      | "locationId"
      | "interactionId"
      | "quantity"
      | "visible"
      | "enabled"
      | "value"
      | "maxValue"
      | "battleAction"
      | "battleTarget";
    field: string;
    format?: string;
  }[];
  rowFilter?: {
    field: string;
    operator: "eq" | "ne" | "gt" | "gte" | "lt" | "lte";
    value: string | number | boolean;
  };
  value?: number;
  maxValue?: number;
  battleTarget?: "self" | "enemy";
  parentId?: string;
  clipChildren?: boolean;
  userFieldId?: string;
  extension?: {
    plugin: string;
    type: string;
    ref: string;
    props?: {
      [k: string]: string | number | boolean | null;
    };
  };
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
          }
        | {
            kind: "relationship";
            characterId: string;
            relationshipId: string;
          }
        | {
            kind: "inventory";
            itemId: string;
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
export type ComponentChild = {
  [k: string]: unknown;
} & {
  id: string;
  kind: "panel" | "text" | "image" | "button" | "meter" | "group" | "input" | "plugin";
  label?: string;
  x: number;
  y: number;
  width: number;
  height: number;
  text?: string;
  binding?:
    | "none"
    | "title"
    | "description"
    | "calendar"
    | "calendar-year"
    | "calendar-month"
    | "calendar-day"
    | "calendar-weekday"
    | "schedule-prompt"
    | "ending"
    | "speaker"
    | "story"
    | "location"
    | "attribute-value"
    | "relationship-value"
    | "item-count"
    | "user-field";
  imageSource?: "asset" | "background" | "portrait-left" | "portrait-center" | "portrait-right" | "speaker-portrait";
  imageAssetId?: string;
  fit?: "cover" | "contain";
  scheduleLayout?: "columns" | "calendar" | "menu";
  statsLayout?: "bars" | "compact";
  shopLayout?: "list" | "cards";
  /**
   * @minItems 1
   * @maxItems 100
   */
  attributeIds?: [string, ...string[]];
  autoLabel?: boolean;
  /**
   * @maxItems 12
   */
  scheduleGroups?:
    | []
    | [
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        }
      ]
    | [
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        }
      ]
    | [
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        }
      ]
    | [
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        }
      ]
    | [
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        }
      ]
    | [
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        }
      ]
    | [
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        }
      ]
    | [
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        }
      ]
    | [
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        }
      ]
    | [
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        }
      ]
    | [
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        }
      ]
    | [
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        },
        {
          id: string;
          label: string;
          iconAssetId?: string;
          /**
           * @minItems 1
           * @maxItems 100
           */
          activityIds: [string, ...string[]];
        }
      ];
  action?:
    | "start"
    | "advance"
    | "save"
    | "load"
    | "sound"
    | "navigate"
    | "visit"
    | "buy"
    | "interact"
    | "acceptQuest"
    | "claimQuest"
    | "equip"
    | "unequip"
    | "moveExpedition"
    | "returnFromExpedition"
    | "battleAction"
    | "sell"
    | "useService"
    | "setDiet"
    | "executeAction"
    | "selectPolicy"
    | "selectBattleAction"
    | "selectBattleTarget"
    | "confirmBattleAction"
    | "cancelBattleAction";
  screenId?: string;
  locationId?: string;
  shopId?: string;
  itemId?: string;
  interactionId?: string;
  questId?: string;
  equipmentId?: string;
  nodeId?: string;
  slot?: "weapon" | "armor" | "accessory";
  battleAction?: "attack" | "guard" | "useItem" | "flee";
  quantity?: number;
  visibleWhen?: "always" | "planning" | "result" | "dialogue" | "choice" | "finished";
  hidden?: boolean;
  locked?: boolean;
  fill?: string;
  color?: string;
  fontSize?: number;
  radius?: number;
  opacity?: number;
  align?: "left" | "center" | "right";
  motion?: "none" | "fade" | "slide-left" | "slide-right" | "zoom";
  durationMs?: number;
  attributeId?: string;
  characterId?: string;
  relationshipId?: string;
  valueFormat?: string;
  frameAssetId?: string;
  entryFrameAssetId?: string;
  portraitId?: string;
  styleId?: string;
  fontAssetId?: string;
  padding?: number;
  borderWidth?: number;
  borderColor?: string;
  hoverFill?: string;
  pressedFill?: string;
  disabledColor?: string;
  anchorX?: "left" | "center" | "right" | "stretch";
  anchorY?: "top" | "center" | "bottom" | "stretch";
  serviceId?: string;
  dietId?: string;
  actionId?: string;
  groupId?: string;
  optionId?: string | null;
  /**
   * @maxItems 1000
   */
  actionIds?: string[];
  /**
   * @maxItems 1000
   */
  groupIds?: string[];
  visibleCondition?: Condition;
  enabledCondition?: Condition;
  value?: number;
  maxValue?: number;
  battleTarget?: "self" | "enemy";
  parentId?: string;
  clipChildren?: boolean;
  userFieldId?: string;
  extension?: {
    plugin: string;
    type: string;
    ref: string;
    props?: {
      [k: string]: string | number | boolean | null;
    };
  };
};

export interface VisualDesign {
  widgetTheme?: "default" | "ornate";
  confirmPurchases?: boolean;
  width: number;
  height: number;
  defaultScene?: Scene;
  /**
   * @minItems 4
   * @maxItems 100
   */
  screens: [Screen, Screen, Screen, Screen, ...Screen[]];
  defaultStyle?: StyleValues;
  /**
   * @maxItems 256
   */
  styles?: NamedStyle[];
  viewportMode?: "fit" | "responsive";
  /**
   * @maxItems 256
   */
  components?: Component[];
  /**
   * @maxItems 100
   */
  dataTables?: {
    id: string;
    name: string;
    /**
     * @minItems 1
     * @maxItems 100
     */
    columns: [
      {
        id: string;
        name: string;
        type: "text" | "number" | "boolean";
      },
      ...{
        id: string;
        name: string;
        type: "text" | "number" | "boolean";
      }[]
    ];
    /**
     * @maxItems 1000
     */
    rows: {
      id: string;
      values: {
        [k: string]: string | number | boolean;
      };
    }[];
  }[];
  /**
   * @maxItems 500
   */
  timelines?: {
    id: string;
    name: string;
    /**
     * @minItems 1
     * @maxItems 100
     */
    clips: [
      {
        id: string;
        durationMs: number;
        stage?: {
          width: number;
          height: number;
          /**
           * @minItems 1
           * @maxItems 32
           */
          layers: [
            {
              id: string;
              x: number;
              y: number;
              width: number;
              height: number;
              loop?: boolean;
              /**
               * @minItems 1
               * @maxItems 500
               */
              frames: [
                {
                  imageAssetId: string;
                  durationMs: number;
                },
                ...{
                  imageAssetId: string;
                  durationMs: number;
                }[]
              ];
            },
            ...{
              id: string;
              x: number;
              y: number;
              width: number;
              height: number;
              loop?: boolean;
              /**
               * @minItems 1
               * @maxItems 500
               */
              frames: [
                {
                  imageAssetId: string;
                  durationMs: number;
                },
                ...{
                  imageAssetId: string;
                  durationMs: number;
                }[]
              ];
            }[]
          ];
        };
        imageAssetId?: string;
        text?: string;
        transition?: "none" | "fade";
      },
      ...{
        id: string;
        durationMs: number;
        stage?: {
          width: number;
          height: number;
          /**
           * @minItems 1
           * @maxItems 32
           */
          layers: [
            {
              id: string;
              x: number;
              y: number;
              width: number;
              height: number;
              loop?: boolean;
              /**
               * @minItems 1
               * @maxItems 500
               */
              frames: [
                {
                  imageAssetId: string;
                  durationMs: number;
                },
                ...{
                  imageAssetId: string;
                  durationMs: number;
                }[]
              ];
            },
            ...{
              id: string;
              x: number;
              y: number;
              width: number;
              height: number;
              loop?: boolean;
              /**
               * @minItems 1
               * @maxItems 500
               */
              frames: [
                {
                  imageAssetId: string;
                  durationMs: number;
                },
                ...{
                  imageAssetId: string;
                  durationMs: number;
                }[]
              ];
            }[]
          ];
        };
        imageAssetId?: string;
        text?: string;
        transition?: "none" | "fade";
      }[]
    ];
  }[];
  /**
   * @maxItems 1000
   */
  activityTimelines?: {
    activityId: string;
    timelineId: string;
    successTimelineId?: string;
    failureTimelineId?: string;
    repeat?: "once" | "logicalDays";
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
export interface Screen {
  id: string;
  label: string;
  /**
   * @maxItems 100
   */
  elements: Element[];
}
export interface StyleValues {
  fontSize?: number;
  color?: string;
  fill?: string;
  radius?: number;
  align?: "left" | "center" | "right";
  fontAssetId?: string;
  padding?: number;
  borderWidth?: number;
  borderColor?: string;
  hoverFill?: string;
  pressedFill?: string;
  disabledColor?: string;
}
export interface NamedStyle {
  id: string;
  label: string;
  fontSize?: number;
  color?: string;
  fill?: string;
  radius?: number;
  align?: "left" | "center" | "right";
  fontAssetId?: string;
  padding?: number;
  borderWidth?: number;
  borderColor?: string;
  hoverFill?: string;
  pressedFill?: string;
  disabledColor?: string;
}
export interface Component {
  id: string;
  label: string;
  width: number;
  height: number;
  /**
   * @maxItems 100
   */
  elements: ComponentChild[];
}
