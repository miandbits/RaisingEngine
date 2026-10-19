import { contextBridge, ipcRenderer } from 'electron';
import type { PlayerBridge } from '../../../packages/player-service/controller.js';

const bridge: PlayerBridge = {
  view: () => ipcRenderer.invoke('player:view'),
  newRun: (version, userValues) => ipcRenderer.invoke('player:new', version, userValues),
  act: (action, version) => ipcRenderer.invoke('player:act', action, version),
  save: version => ipcRenderer.invoke('player:save', version),
  load: version => ipcRenderer.invoke('player:load', version),
  openProject: version => ipcRenderer.invoke('player:open', version),
  openYearSample: version => ipcRenderer.invoke('player:year-sample', version),
};
contextBridge.exposeInMainWorld('player', bridge);
