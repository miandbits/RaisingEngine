// Engine plugin host (CONTRACT.md, plugin api 1): package format, store, project
// lock resolution, node service modules and export bundling.
export { ENGINE_PLUGIN_API, PLUGIN_ID_PATTERN, RUNTIME_PLATFORMS, parseManifest, apiCompatible, findModule, moduleFiles,
  type PluginManifest, type PluginModule, type ModuleRole } from './manifest.js';
export { parseVersion, compareVersions, satisfies } from './semver.js';
export { readZip, writeZip, isSafeRelativePath, PluginError, MAX_UNPACKED_BYTES, MAX_ZIP_ENTRIES, type ZipEntry } from './zip.js';
export { pluginStorePath, engineRoot, installedDirectory, readRegistry, inspectBytes, inspectPackage, installPackage, uninstallPlugin,
  listInstalled, findInstalled, readInstalledManifest, type InstalledPlugin, type InstallResult, type InspectedPackage, type Registry, type RegistryEntry } from './store.js';
export { resolveProjectPlugins, lockEntries, pluginStatus, authoringModule, callProjectPlugin, projectPluginService,
  type LockEntry, type PluginState, type PluginIssue, type ResolvedPlugin, type PluginStatus, type PluginStatusInstalled, type PluginStatusProject } from './resolve.js';
export { loadService, dataValidators, readDocumentSchema, type PluginService, type ServiceDiagnostic, type ServiceValidateInput, type ServiceImportInput, type LoadablePlugin } from './service.js';
export { exportBundle, pluginElements, extensionPath, type GameExtensions, type GameExtensionPlugin, type PreparedExtensions, type PluginElementRef } from './bundle.js';
