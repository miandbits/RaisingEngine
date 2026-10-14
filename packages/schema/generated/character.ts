/* Generated from JSON Schema. Do not edit; run npm run build. */

export type CharacterDefinition = Header & {
  id: string;
  name: string;
  description?: string;
  portraits: {
    [k: string]: string;
  };
  relationships: RelationshipDefinition[];
};
export type RelationshipDefinition = Range & {
  id: string;
  label: string;
  description?: string;
};

export interface Header {
  $schema?: string;
  formatVersion: "0.1.0";
  /**
   * @maxItems 32
   */
  tags?: string[];
}
export interface Range {
  min: number;
  max: number;
  initial: number;
}
