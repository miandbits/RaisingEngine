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
    | "user-field"
    | "selected-field"
    | "gallery-summary"
    | "gallery-filter"
    | "gallery-error"
    | "replay-position"
    | "replay-notice"
    | "replay-title"
    | "replay-mode"
    | "replay-speed"
    | "data-value"
    | "context-name";
  imageSource?: "asset" | "background" | "portrait-left" | "portrait-center" | "portrait-right" | "speaker-portrait";
  imageAssetId?: string;
  fit?: "cover" | "contain";
  /**
   * Show the selected activity's authored description below each schedule column. Off by default.
   */
  showActivityDescription?: boolean;
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
    | "cancelBattleAction"
    | "selectRow"
    | "openModal"
    | "closeModal"
    | "executeActorAction"
    | "playSequence"
    | "galleryOpen"
    | "galleryClose"
    | "galleryBack"
    | "galleryView"
    | "galleryReplay"
    | "galleryFilter"
    | "galleryMusicToggle"
    | "galleryMusicStop"
    | "galleryVariant"
    | "recollectionPrev"
    | "recollectionNext"
    | "recollectionClose"
    | "recollectionSave"
    | "recollectionLoad"
    | "recollectionAuto"
    | "recollectionSpeed"
    | "recollectionSkip"
    | "galleryBranches"
    | "galleryBranch"
    | "assignActivity";
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
  visibleWhen?:
    "always" | "planning" | "result" | "dialogue" | "choice" | "finished" | "title" | "battle" | "expedition";
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
        | "cancelBattleAction"
        | "selectRow"
        | "openModal"
        | "closeModal"
        | "executeActorAction"
        | "playSequence"
        | "galleryOpen"
        | "galleryClose"
        | "galleryBack"
        | "galleryView"
        | "galleryReplay"
        | "galleryFilter"
        | "galleryMusicToggle"
        | "galleryMusicStop"
        | "galleryVariant"
        | "recollectionPrev"
        | "recollectionNext"
        | "recollectionClose"
        | "recollectionSave"
        | "recollectionLoad"
        | "recollectionAuto"
        | "recollectionSpeed"
        | "recollectionSkip"
        | "galleryBranches"
        | "galleryBranch";
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
      tooltip?: string;
      selectionKey?: string;
      selectionField?: string;
      /**
       * @maxItems 16
       */
      selectionBindings?:
        | []
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ];
      actorId?: string;
      sequenceId?: string;
      entryId?: string;
      galleryFilter?: string;
      branchId?: string;
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
      | "battleRewards"
      | "galleryEntries"
      | "gallerySelection"
      | "galleryBranches";
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
      | "battleTarget"
      | "entryId"
      | "branchId";
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
  flowLayout?: {
    direction: "row" | "column" | "grid";
    gap?: number;
    padding?: number;
    columns?: number;
  };
  overflow?: "visible" | "hidden" | "scroll";
  sort?: {
    field: string;
    direction: "asc" | "desc";
  };
  selectionKey?: string;
  selectionField?: string;
  /**
   * @maxItems 16
   */
  selectionBindings?:
    | []
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ];
  tooltip?: string;
  actorId?: string;
  tacticsPanel?: "all" | "units" | "actions" | "log" | "growth";
  sequenceId?: string;
  entryId?: string;
  galleryFilter?: string;
  branchId?: string;
  dataBinding?:
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
  /**
   * @maxItems 64
   */
  variants?: PresentationVariant[];
  uiContext?: UiContext;
  /**
   * @maxItems 8
   */
  contextBindings?:
    | []
    | ["actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId"]
    | [
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId"
      ]
    | [
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId"
      ]
    | [
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId"
      ]
    | [
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId"
      ]
    | [
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId"
      ]
    | [
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId"
      ]
    | [
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId"
      ];
  contextField?:
    "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId";
  activityId?: string;
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
export type ComponentChild = {
  [k: string]: unknown;
} & {
  id: string;
  kind: "panel" | "text" | "image" | "button" | "meter" | "group" | "input" | "component";
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
    | "user-field"
    | "selected-field"
    | "gallery-summary"
    | "gallery-filter"
    | "gallery-error"
    | "replay-position"
    | "replay-notice"
    | "replay-title"
    | "replay-mode"
    | "replay-speed"
    | "data-value"
    | "context-name";
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
    | "cancelBattleAction"
    | "selectRow"
    | "openModal"
    | "closeModal"
    | "executeActorAction"
    | "playSequence"
    | "galleryOpen"
    | "galleryClose"
    | "galleryBack"
    | "galleryView"
    | "galleryReplay"
    | "galleryFilter"
    | "galleryMusicToggle"
    | "galleryMusicStop"
    | "galleryVariant"
    | "recollectionPrev"
    | "recollectionNext"
    | "recollectionClose"
    | "recollectionSave"
    | "recollectionLoad"
    | "recollectionAuto"
    | "recollectionSpeed"
    | "recollectionSkip"
    | "galleryBranches"
    | "galleryBranch"
    | "assignActivity";
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
  visibleWhen?:
    "always" | "planning" | "result" | "dialogue" | "choice" | "finished" | "title" | "battle" | "expedition";
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
  flowLayout?: {
    direction: "row" | "column" | "grid";
    gap?: number;
    padding?: number;
    columns?: number;
  };
  overflow?: "visible" | "hidden" | "scroll";
  sort?: {
    field: string;
    direction: "asc" | "desc";
  };
  selectionKey?: string;
  selectionField?: string;
  /**
   * @maxItems 16
   */
  selectionBindings?:
    | []
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ]
    | [
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        },
        {
          property:
            | "actionId"
            | "actorId"
            | "itemId"
            | "shopId"
            | "characterId"
            | "groupId"
            | "optionId"
            | "quantity"
            | "screenId"
            | "entryId"
            | "branchId";
          field: string;
        }
      ];
  tooltip?: string;
  actorId?: string;
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
        | "cancelBattleAction"
        | "selectRow"
        | "openModal"
        | "closeModal"
        | "executeActorAction"
        | "playSequence"
        | "galleryOpen"
        | "galleryClose"
        | "galleryBack"
        | "galleryView"
        | "galleryReplay"
        | "galleryFilter"
        | "galleryMusicToggle"
        | "galleryMusicStop"
        | "galleryVariant"
        | "recollectionPrev"
        | "recollectionNext"
        | "recollectionClose"
        | "recollectionSave"
        | "recollectionLoad"
        | "recollectionAuto"
        | "recollectionSpeed"
        | "recollectionSkip"
        | "galleryBranches"
        | "galleryBranch";
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
      tooltip?: string;
      selectionKey?: string;
      selectionField?: string;
      /**
       * @maxItems 16
       */
      selectionBindings?:
        | []
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ]
        | [
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            },
            {
              property:
                | "actionId"
                | "actorId"
                | "itemId"
                | "shopId"
                | "characterId"
                | "groupId"
                | "optionId"
                | "quantity"
                | "screenId"
                | "entryId"
                | "branchId";
              field: string;
            }
          ];
      actorId?: string;
      sequenceId?: string;
      entryId?: string;
      galleryFilter?: string;
      branchId?: string;
    };
  }[];
  tacticsPanel?: "all" | "units" | "actions" | "log" | "growth";
  sequenceId?: string;
  entryId?: string;
  galleryFilter?: string;
  branchId?: string;
  dataBinding?:
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
  /**
   * @maxItems 64
   */
  variants?: PresentationVariant[];
  uiContext?: UiContext;
  /**
   * @maxItems 8
   */
  contextBindings?:
    | []
    | ["actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId"]
    | [
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId"
      ]
    | [
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId"
      ]
    | [
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId"
      ]
    | [
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId"
      ]
    | [
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId"
      ]
    | [
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId"
      ]
    | [
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId",
        "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId"
      ];
  contextField?:
    "actorId" | "activityId" | "characterId" | "itemId" | "actionId" | "locationId" | "shopId" | "attributeId";
  activityId?: string;
};

export interface VisualDesign {
  widgetTheme?: "default" | "ornate";
  confirmPurchases?: boolean;
  width: number;
  height: number;
  defaultScene?: Scene;
  /**
   * @minItems 1
   * @maxItems 100
   */
  screens: [Screen, ...Screen[]];
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
   * Show committed activity changes sequentially inside result widgets. Existing authored activity timelines take precedence.
   */
  resultPlayback?: {
    leadInMs: number;
    changeMs: number;
    holdMs: number;
  };
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
  direction?: {
    /**
     * @minItems 1
     * @maxItems 32
     */
    actors: [
      {
        id: string;
        name: string;
        assetId: string;
        /**
         * @maxItems 100
         */
        expressions?: {
          id: string;
          assetId: string;
        }[];
        /**
         * @maxItems 100
         */
        outfits?: {
          id: string;
          assetId: string;
        }[];
        /**
         * @maxItems 100
         */
        motions?: {
          id: string;
          /**
           * @minItems 1
           * @maxItems 120
           */
          frames: [string, ...string[]];
          frameMs: number;
        }[];
      },
      ...{
        id: string;
        name: string;
        assetId: string;
        /**
         * @maxItems 100
         */
        expressions?: {
          id: string;
          assetId: string;
        }[];
        /**
         * @maxItems 100
         */
        outfits?: {
          id: string;
          assetId: string;
        }[];
        /**
         * @maxItems 100
         */
        motions?: {
          id: string;
          /**
           * @minItems 1
           * @maxItems 120
           */
          frames: [string, ...string[]];
          frameMs: number;
        }[];
      }[]
    ];
    /**
     * @minItems 1
     * @maxItems 100
     */
    sequences: [
      {
        id: string;
        name: string;
        /**
         * @minItems 1
         * @maxItems 100
         */
        stages: [
          {
            name: string;
            durationMs: number;
            /**
             * @minItems 1
             * @maxItems 32
             */
            cues: [
              {
                actorId: string;
                x: number;
                y: number;
                width: number;
                height: number;
                toX?: number;
                toY?: number;
                opacity?: number;
                expressionId?: string;
                outfitId?: string;
                motionId?: string;
              },
              ...{
                actorId: string;
                x: number;
                y: number;
                width: number;
                height: number;
                toX?: number;
                toY?: number;
                opacity?: number;
                expressionId?: string;
                outfitId?: string;
                motionId?: string;
              }[]
            ];
          },
          ...{
            name: string;
            durationMs: number;
            /**
             * @minItems 1
             * @maxItems 32
             */
            cues: [
              {
                actorId: string;
                x: number;
                y: number;
                width: number;
                height: number;
                toX?: number;
                toY?: number;
                opacity?: number;
                expressionId?: string;
                outfitId?: string;
                motionId?: string;
              },
              ...{
                actorId: string;
                x: number;
                y: number;
                width: number;
                height: number;
                toX?: number;
                toY?: number;
                opacity?: number;
                expressionId?: string;
                outfitId?: string;
                motionId?: string;
              }[]
            ];
          }[]
        ];
      },
      ...{
        id: string;
        name: string;
        /**
         * @minItems 1
         * @maxItems 100
         */
        stages: [
          {
            name: string;
            durationMs: number;
            /**
             * @minItems 1
             * @maxItems 32
             */
            cues: [
              {
                actorId: string;
                x: number;
                y: number;
                width: number;
                height: number;
                toX?: number;
                toY?: number;
                opacity?: number;
                expressionId?: string;
                outfitId?: string;
                motionId?: string;
              },
              ...{
                actorId: string;
                x: number;
                y: number;
                width: number;
                height: number;
                toX?: number;
                toY?: number;
                opacity?: number;
                expressionId?: string;
                outfitId?: string;
                motionId?: string;
              }[]
            ];
          },
          ...{
            name: string;
            durationMs: number;
            /**
             * @minItems 1
             * @maxItems 32
             */
            cues: [
              {
                actorId: string;
                x: number;
                y: number;
                width: number;
                height: number;
                toX?: number;
                toY?: number;
                opacity?: number;
                expressionId?: string;
                outfitId?: string;
                motionId?: string;
              },
              ...{
                actorId: string;
                x: number;
                y: number;
                width: number;
                height: number;
                toX?: number;
                toY?: number;
                opacity?: number;
                expressionId?: string;
                outfitId?: string;
                motionId?: string;
              }[]
            ];
          }[]
        ];
      }[]
    ];
  };
  routing?: {
    defaultScreenId: string;
    useLocationScreens?: boolean;
    /**
     * @maxItems 10
     */
    phases:
      | []
      | [
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          }
        ]
      | [
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          }
        ]
      | [
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          }
        ]
      | [
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          }
        ]
      | [
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          }
        ]
      | [
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          }
        ]
      | [
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          }
        ]
      | [
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          }
        ]
      | [
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          }
        ]
      | [
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          },
          {
            phase:
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle";
            screenId: string;
          }
        ];
    /**
     * @maxItems 64
     */
    conditions?: {
      label: string;
      screenId: string;
      when: Condition;
      /**
       * @minItems 1
       * @maxItems 10
       */
      phases?:
        | [
            | "title"
            | "planning"
            | "ready"
            | "activity-result"
            | "period-end"
            | "dialogue"
            | "choice"
            | "finished"
            | "expedition"
            | "battle"
          ]
        | [
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            )
          ]
        | [
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            )
          ]
        | [
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            )
          ]
        | [
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            )
          ]
        | [
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            )
          ]
        | [
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            )
          ]
        | [
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            )
          ]
        | [
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            )
          ]
        | [
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            ),
            (
              | "title"
              | "planning"
              | "ready"
              | "activity-result"
              | "period-end"
              | "dialogue"
              | "choice"
              | "finished"
              | "expedition"
              | "battle"
            )
          ];
    }[];
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
export interface Screen {
  id: string;
  label: string;
  /**
   * @maxItems 100
   */
  elements: Element[];
  modal?: boolean;
}
export interface PresentationVariant {
  label: string;
  when: Condition;
  /**
   * @minItems 1
   */
  activityIds?: [string, ...string[]];
  values: {
    text?: string;
    imageAssetId?: string;
    color?: string;
    fill?: string;
    opacity?: number;
    extensionRef?: string;
  };
}
export interface UiContext {
  actorId?: string;
  activityId?: string;
  characterId?: string;
  itemId?: string;
  actionId?: string;
  locationId?: string;
  shopId?: string;
  attributeId?: string;
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
