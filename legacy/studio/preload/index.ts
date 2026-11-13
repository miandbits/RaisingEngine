import { contextBridge, ipcRenderer } from 'electron';
import type { StudioBridge } from '../../../packages/studio-service/editor.js';
import type { PlayerBridge } from '../../../packages/player-service/controller.js';
const studio: StudioBridge = {
  view: () => ipcRenderer.invoke('studio:view'), edit: (edit, revision) => ipcRenderer.invoke('studio:edit', edit, revision),
  open: () => ipcRenderer.invoke('studio:open'), create: template => ipcRenderer.invoke('studio:create', template),
  reload: revision => ipcRenderer.invoke('studio:reload', revision), validate: revision => ipcRenderer.invoke('studio:validate', revision),
  save: revision => ipcRenderer.invoke('studio:save', revision), preview: revision => ipcRenderer.invoke('studio:preview', revision),
  importImage: revision => ipcRenderer.invoke('studio:import-image', revision),
  exportGame: (target, revision) => ipcRenderer.invoke('studio:export', target, revision),
};
const player: PlayerBridge = {
  forecastPhase: (ids,version) => ipcRenderer.invoke('preview:forecast-phase',ids,version),
  view: () => ipcRenderer.invoke('preview:view'), newRun: (version, userValues) => ipcRenderer.invoke('preview:new', version, userValues),
  act: (action, version) => ipcRenderer.invoke('preview:act', action, version),
  save: version => ipcRenderer.invoke('preview:save', version), load: version => ipcRenderer.invoke('preview:load', version), openProject: () => ipcRenderer.invoke('preview:view'),
  openYearSample: () => ipcRenderer.invoke('preview:view'),
};
contextBridge.exposeInMainWorld('studio', studio);
contextBridge.exposeInMainWorld('analysis', { request: (input: unknown) => ipcRenderer.invoke('analysis:request', input) });
contextBridge.exposeInMainWorld('player', player);
