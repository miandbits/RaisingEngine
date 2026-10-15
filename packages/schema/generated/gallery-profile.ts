/* Generated from JSON Schema. Do not edit; run npm run build. */

export interface CrossRunGalleryProfileV1 {
  version: 1;
  gameId: string;
  entries: {
    [k: string]: {
      /**
       * @maxItems 32
       */
      records: Record[];
    };
  };
}
export interface Record {
  kind: "event" | "ending" | "cg" | "music";
  sourceId: string;
  title: string;
  unlockedAt: string;
  contentFingerprint: string;
  periodIndex: number;
  attributes: {
    [k: string]: number;
  };
  attributeLabels: {
    [k: string]: string;
  };
  relationships: {};
  presentation: {};
  /**
   * @maxItems 10000
   */
  frames: {
    nodeId: string;
    text: string;
    speaker: string;
    scene: {};
    choiceId?: string;
    choice?: string;
  }[];
  content?: {};
  /**
   * @maxItems 10000
   */
  replayFrames?: {
    state: {};
    extensions: {} | null;
    sounds: string[];
    choiceId?: string;
  }[];
  extensionManifest?: {} | null;
}
