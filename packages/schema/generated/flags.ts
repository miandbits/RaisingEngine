/* Generated from JSON Schema. Do not edit; run npm run build. */

export type FlagDefinitions = Header & {
  flags: {
    id: string;
    label: string;
    initial: boolean;
    description?: string;
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
