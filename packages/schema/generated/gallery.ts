/* Generated from JSON Schema. Do not edit; run npm run build. */

export interface CrossRunNarrativeGallery {
  title: string;
  /**
   * @maxItems 2000
   */
  entries: {
    id: string;
    kind: "event" | "ending";
    sourceId: string;
    title: string;
    category: string;
    hint: string;
    thumbnailAssetId?: string;
  }[];
}
