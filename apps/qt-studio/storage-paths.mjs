import {readFileSync} from 'node:fs';
import os from 'node:os';
import path from 'node:path';

// Only packaged Qt hosts choose a per-user default. Repository/CLI stores and
// explicitly configured stores keep the plugin-host library's existing behavior.
export function packagedStudioPluginStore(engineRoot, {platform=process.platform, home=os.homedir(), environment=process.env}={}) {
  const marker={darwin:'raising-studio-services',win32:'raising-studio-service'}[platform];
  if(!marker)return undefined;
  let metadata;
  try {metadata=JSON.parse(readFileSync(path.join(engineRoot,'package.json'),'utf8'));}
  catch{return undefined;}
  if(metadata?.name!==marker)return undefined;
  if(platform==='darwin')return path.posix.join(home,'Library','Application Support','RaisingEngine','Raising Studio','plugin-store');
  const configured=environment.LOCALAPPDATA;
  const base=typeof configured==='string'&&path.win32.isAbsolute(configured)?configured:path.win32.join(home,'AppData','Local');
  return path.win32.join(base,'RaisingEngine','Raising Studio','plugin-store');
}
