/* Generated from JSON Schema. Do not edit; run npm run build. */

export type ProjectDefinition = Header & {
  authoring?: {
    /**
     * @maxItems 128
     */
    storyFlows?: StoryFlowAuthoringMetadata[];
  };
  gallery?: CrossRunNarrativeGallery;
  gameId: string;
  title: string;
  description?: string;
  startYear: number;
  startMonth: number;
  durationMonths: number;
  periodsPerMonth: number;
  protagonistId: string;
  initialState: Overrides;
  defaultEndingId: string;
  visual?: VisualDesign;
  gameplay?: GameplayDefinition;
  systems?: SystemsDefinition;
  localization?: {
    defaultLocale: string;
    fallbackLocale: string;
    /**
     * @minItems 1
     * @maxItems 32
     */
    locales: [
      {
        id: string;
        label: string;
        fontAssetId?: string;
      },
      ...{
        id: string;
        label: string;
        fontAssetId?: string;
      }[]
    ];
    /**
     * @maxItems 50000
     */
    entries: {
      key: string;
      source: string;
      translations: {
        [k: string]: string;
      };
    }[];
  };
  /**
   * @maxItems 32
   */
  userFields?: UserFieldDefinition[];
  /**
   * 기간 준비 정산과 활동 뒤 자유 행동. 생략하면 기존 원자적 일정 실행을 유지합니다.
   */
  periodFlow?: {
    preparePolicies?: boolean;
    /**
     * 기간 종료 사건이 끝날 때 현재 조건을 다시 확인하여 다음 사건을 실행합니다. 기간 종료 규칙은 재적용하지 않습니다.
     */
    allPeriodEndEvents?: boolean;
    intermission?: {
      screenId: string;
      completionFlagId: string;
      entryEffects?: Effects;
    };
  };
  /**
   * 월 일정 실행 방식. continuous는 결과 연출 후 다음 일정으로 이어지며 대화·선택·전투에서는 입력을 기다립니다.
   */
  schedulePlayback?: "manual" | "continuous";
  roster?: {
    /**
     * @minItems 1
     * @maxItems 64
     */
    localAttributeIds: [string, ...string[]];
    /**
     * @minItems 1
     * @maxItems 16
     */
    members:
      | [
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          }
        ]
      | [
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          }
        ]
      | [
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          }
        ]
      | [
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          }
        ]
      | [
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          }
        ]
      | [
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          }
        ]
      | [
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          }
        ]
      | [
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          }
        ]
      | [
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          }
        ]
      | [
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          }
        ]
      | [
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          }
        ]
      | [
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          }
        ]
      | [
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          }
        ]
      | [
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          }
        ]
      | [
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          }
        ]
      | [
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          },
          {
            characterId: string;
            activityId: string;
            initialAttributes: {
              [k: string]: number;
            };
            initialInventory?: {
              [k: string]: number;
            };
            initialRelationships?: {
              [k: string]: {
                [k: string]: number;
              };
            };
            /**
             * @maxItems 128
             */
            actionIds?: string[];
          }
        ];
    stateModel?: "character-v1";
  };
  progression?: {
    unitLabel: string;
    actionPoints: {
      max: number;
      initial: number;
      recoveryPerTick: number;
    };
    /**
     * @maxItems 512
     */
    actions: {
      actionId: string;
      duration: number;
      actionPointCost: number;
    }[];
    /**
     * @maxItems 128
     */
    appointments: {
      id: string;
      eventId: string;
      atTick: number;
      actorId?: string;
      repeatEvery?: number;
      repeatCount?: number;
    }[];
    model?: "unified-v2";
    ticksPerPeriod?: number;
    parallel?: boolean;
    /**
     * @maxItems 32
     */
    actorPools?: {
      actorId: string;
      max: number;
      initial: number;
      recoveryPerTick: number;
    }[];
    /**
     * @maxItems 512
     */
    activities?: {
      activityId: string;
      actionPointCost: number;
    }[];
  };
  /**
   * @maxItems 128
   */
  stateGroups?: {
    id: string;
    name: string;
    scope: "shared" | "character";
    initialValueId: string;
    /**
     * @minItems 1
     * @maxItems 128
     */
    values: [
      {
        id: string;
        name: string;
      },
      ...{
        id: string;
        name: string;
      }[]
    ];
  }[];
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
    | "plugin"
    | "saveSlots"
    | "playerSettings"
    | "loadSlots";
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
    | "context-name"
    | "schedule-summary"
    | "activity-progress"
    | "save-summary"
    | "player-notice"
    | "reading-status"
    | "choice-result";
  imageSource?: "asset" | "background" | "portrait-left" | "portrait-center" | "portrait-right" | "speaker-portrait";
  imageAssetId?: string;
  fit?: "cover" | "contain";
  /**
   * Show the selected activity's authored description below each schedule column. Off by default.
   */
  showActivityDescription?: boolean;
  scheduleLayout?: "columns" | "calendar" | "menu" | "board";
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
    | "dialogueAuto"
    | "recollectionSpeed"
    | "recollectionSkip"
    | "galleryBranches"
    | "galleryBranch"
    | "assignActivity"
    | "commitPlan"
    | "timelinePause"
    | "timelineSpeed"
    | "timelineSkip";
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
        | "dialogueAuto"
        | "recollectionSpeed"
        | "recollectionSkip"
        | "galleryBranches"
        | "galleryBranch"
        | "timelinePause"
        | "timelineSpeed"
        | "timelineSkip";
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
  decreasingAttributeIds?: string[];
  resultLayout?: "summary" | "changes";
  choiceLayout?: "list" | "cards";
  /**
   * Show authored consequence text and immediate choice effects below each option.
   */
  showChoiceEffects?: boolean;
  meterThickness?: number;
  /**
   * @maxItems 1000
   */
  activityIcons?: {
    activityId: string;
    imageAssetId: string;
  }[];
};
export type ComponentChild = {
  [k: string]: unknown;
} & {
  id: string;
  kind:
    | "panel"
    | "text"
    | "image"
    | "button"
    | "meter"
    | "group"
    | "input"
    | "component"
    | "saveSlots"
    | "playerSettings"
    | "loadSlots";
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
    | "context-name"
    | "schedule-summary"
    | "activity-progress"
    | "save-summary"
    | "player-notice"
    | "reading-status";
  imageSource?: "asset" | "background" | "portrait-left" | "portrait-center" | "portrait-right" | "speaker-portrait";
  imageAssetId?: string;
  fit?: "cover" | "contain";
  scheduleLayout?: "columns" | "calendar" | "menu" | "board";
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
    | "dialogueAuto"
    | "recollectionSpeed"
    | "recollectionSkip"
    | "galleryBranches"
    | "galleryBranch"
    | "assignActivity"
    | "commitPlan"
    | "timelinePause"
    | "timelineSpeed"
    | "timelineSkip";
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
        | "dialogueAuto"
        | "recollectionSpeed"
        | "recollectionSkip"
        | "galleryBranches"
        | "galleryBranch"
        | "timelinePause"
        | "timelineSpeed"
        | "timelineSkip";
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
  decreasingAttributeIds?: string[];
  resultLayout?: "summary" | "changes";
  choiceLayout?: "list" | "cards";
  meterThickness?: number;
  /**
   * @maxItems 1000
   */
  activityIcons?: {
    activityId: string;
    imageAssetId: string;
  }[];
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

export interface Header {
  $schema?: string;
  formatVersion: "0.1.0";
  /**
   * @maxItems 32
   */
  tags?: string[];
}
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
        }
      ];
  ownedDocuments: {
    file: string;
    digest: string;
  }[];
}
export interface CrossRunNarrativeGallery {
  title: string;
  /**
   * @maxItems 2000
   */
  entries: {
    id: string;
    kind: "event" | "ending" | "cg" | "music";
    sourceId: string;
    title: string;
    category: string;
    hint: string;
    thumbnailAssetId?: string;
    unlockCondition?: Condition;
    lockedDisplay?: "hint" | "hidden";
    fit?: "contain" | "cover";
  }[];
  screenId?: string;
  detailScreenId?: string;
  replayControlsScreenId?: string;
  branchScreenId?: string;
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
export interface VisualDesign {
  saveSystem?: {
    manualSlots?: number;
    autoSlots?: number;
  };
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
export interface Item {
  id: string;
  name: string;
  description?: string;
  imageAssetId?: string;
  maxStack: number;
  initialQuantity?: number;
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
    price: number;
    stock: number | null;
    maxQuantity?: number;
    confirmation?: Confirmation;
  }[];
  /**
   * @maxItems 10000
   */
  sellOffers?: {
    itemId: string;
    price: number;
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
  quantity?: number;
  requirements?: Condition;
  effects: Effects;
  /**
   * @maxItems 100
   */
  costs?: {
    attributeId: string;
    amount: number;
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
    amount: number;
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
    amount: number;
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
    amount: number;
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
export interface UserFieldDefinition {
  id: string;
  type: "text";
  label: string;
  initial: string;
  minLength: number;
  maxLength: number;
}
