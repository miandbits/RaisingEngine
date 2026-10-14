/* Generated from JSON Schema. Do not edit; run npm run build. */

export type AttributeDefinitions = Header & {
  /**
   * @minItems 1
   */
  attributes: [AttributeDefinition, ...AttributeDefinition[]];
};
export type AttributeDefinition = Range & {
  id: string;
  label: string;
  kind: "stat" | "resource" | "condition";
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
