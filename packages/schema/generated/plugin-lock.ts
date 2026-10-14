/* Generated from JSON Schema. Do not edit; run npm run build. */

/**
 * plugins/lock.json: exact plugin versions used by this project. Missing, disabled or incompatible plugins never delete documents.
 */
export interface PluginLock {
  $schema?: string;
  formatVersion: "0.1.0";
  /**
   * @maxItems 64
   */
  plugins: {
    id: string;
    version: string;
    enabled: boolean;
  }[];
}
