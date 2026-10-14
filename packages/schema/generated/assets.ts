/* Generated from JSON Schema. Do not edit; run npm run build. */

export type AssetManifest = Header & {
  assets: {
    id: string;
    kind: "image" | "audio" | "font";
    path: string;
  }[];
};

export interface Header {
  $schema?: string;
  formatVersion: "0.1.0";
  /**
   * @maxItems 32
   */
  tags?: string[];
}
