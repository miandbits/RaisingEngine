/* Generated from JSON Schema. Do not edit; run npm run build. */

/**
 * Editor round-trip metadata. Execution uses ordinary character, activity and event documents.
 */
export interface StoryFlowAuthoringMetadata {
  id: string;
  name: string;
  newCharacter: boolean;
  characterId: string;
  characterName?: string;
  newRelationship: boolean;
  relationshipId: string;
  relationshipLabel?: string;
  relationshipInitial?: number;
  newActivity: boolean;
  activityId: string;
  activityLabel?: string;
  costAttributeId?: string;
  cost?: number;
  gainAttributeId?: string;
  gain?: number;
  scheduleLink?: {
    screenId?: string;
    elementId?: string;
    groupId?: string;
  };
  encounterTitle: string;
  encounterText: string;
  prompt: string;
  predecessorEventId?: string;
  meetingTrigger?: "period-end" | "after-activity";
  fromPeriod: number;
  throughPeriod: number;
  /**
   * @minItems 2
   * @maxItems 8
   */
  choices:
    | [
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        }
      ]
    | [
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        }
      ]
    | [
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        }
      ]
    | [
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        }
      ]
    | [
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        }
      ]
    | [
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        }
      ]
    | [
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        },
        {
          id: string;
          label: string;
          reply: string;
          delta: number;
          followupEnabled: boolean;
          followupTitle?: string;
          followupText?: string;
          minimumRelationship?: number;
          followupFrom?: number;
          followupThrough?: number;
          followupTrigger?: "period-end" | "after-activity";
        }
      ];
  ownedDocuments: {
    file: string;
    digest: string;
  }[];
}
