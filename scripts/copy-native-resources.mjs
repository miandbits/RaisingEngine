import { cp, mkdir, readFile, writeFile, rename, access } from 'node:fs/promises';
import { resolve } from 'node:path';
export async function copyNativeResources(destination) {
  const manifest=JSON.parse(await readFile('dist/native/runtime.json','utf8'));
  const name=manifest.executable;
  await mkdir(destination,{recursive:true});
  try {await access(resolve(destination,name));}catch {await cp(resolve('dist/native',name),resolve(destination,name));}
  await cp('native/vendor/README.md',resolve(destination,'THIRD-PARTY-NOTICES.md'));
  await cp('native/vendor/LICENSE.MIT',resolve(destination,'NLOHMANN-JSON-LICENSE.txt'));
  try {
    const addon = JSON.parse(await readFile('dist/native/addon.json','utf8'));
    if (addon.executable === name && /^raising-addon-[a-f0-9]{16}\.node$/.test(addon.filename)) {
      try { await access(resolve(destination,addon.filename)); } catch { await cp(resolve('dist/native',addon.filename),resolve(destination,addon.filename)); }
      await cp('native/addon/vendor/NODE-LICENSE',resolve(destination,'NODE-API-LICENSE.txt'));
      const staged = resolve(destination,`addon-${process.pid}.json`);
      await writeFile(staged,JSON.stringify(addon,null,2));
      await rename(staged,resolve(destination,'addon.json'));
    }
  } catch (error) { if (error.code !== 'ENOENT') throw error; }
  const temporary=resolve(destination,`runtime-${process.pid}.json`);
  await writeFile(temporary,JSON.stringify(manifest,null,2));
  await rename(temporary,resolve(destination,'runtime.json'));
}
