/* Generated from JSON Schema. Do not edit; run npm run build. */

export type AssetManifest = Header & {
  assets: {
    id: string;
    kind: "image" | "audio" | "font";
    path: string;
  }[];
  /**
   * Explicit packaging-only declarations. These do not participate in gameplay snapshots or save fingerprints.
   */
  release?: {
    displayName?: string;
    icon?: {
      path: string;
      sha256: string;
    };
    /**
     * @maxItems 64
     */
    notices?: {
      path: string;
      sha256: string;
      id: string;
      label?: string;
    }[];
  };
};

export interface Header {
  $schema?: string;
  formatVersion: "0.1.0";
  /**
   * @maxItems 32
   */
  tags?: string[];
}
