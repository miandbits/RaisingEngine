/* Generated from JSON Schema. Do not edit; run npm run build. */

/**
 * Plugin-owned document at plugins/<plugin>/<type>/<id>.json. The engine validates this envelope; `data` is validated by the plugin (JSON schema, then its service module).
 */
export interface ExtensionDocument {
  $schema?: string;
  formatVersion: "0.1.0";
  plugin: string;
  type: string;
  id: string;
  data: {};
}
