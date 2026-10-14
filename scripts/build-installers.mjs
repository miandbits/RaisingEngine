import { execFileSync } from 'node:child_process';
import { mkdir, readFile, writeFile, stat } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { resolve } from 'node:path';
const pkg = JSON.parse(await readFile('package.json', 'utf8'));
await mkdir('.cache/installer-config', { recursive: true });
await mkdir('release/installers', { recursive: true });
const artifacts = [];
for (const kind of ['player', 'studio']) {
  const title = `Raising ${kind === 'player' ? 'Player' : 'Studio'}`;
  const filename = `${title.replaceAll(' ', '-')}-Setup-${pkg.version}-x64.exe`;
  const config = {
    appId: `org.raisingengine.${kind}`, productName: title,
    directories: { output: resolve('release/installers') },
    artifactName: filename,
    win: { target: ['nsis'], executableName: title, signAndEditExecutable: false },
    nsis: { oneClick: false, perMachine: false, allowElevation: false, allowToChangeInstallationDirectory: true,
      createDesktopShortcut: false, createStartMenuShortcut: true, runAfterFinish: false,
      deleteAppDataOnUninstall: false, installerLanguages: ['en_US','ko_KR'] },
    publish: null,
  };
  const configFile = resolve(`.cache/installer-config/${kind}.json`);
  await writeFile(configFile, JSON.stringify(config, null, 2));
  execFileSync(process.execPath, [resolve('node_modules/electron-builder/out/cli/cli.js'), '--win','nsis','--x64',
    '--prepackaged',resolve(`release/${title}-win32-x64`),'--config',configFile,'--publish','never'], {
    stdio: 'inherit', windowsHide: true, env: { ...process.env, CSC_IDENTITY_AUTO_DISCOVERY: 'false', ELECTRON_BUILDER_CACHE: resolve('.cache/electron-builder') },
  });
  const file = `release/installers/${filename}`; const bytes = await readFile(file);
  artifacts.push({ file: filename, bytes: (await stat(file)).size, sha256: createHash('sha256').update(bytes).digest('hex'), signed: false });
}
await writeFile('release/installers/manifest.json', JSON.stringify({ version:pkg.version,platform:'win32-x64',artifacts },null,2)+'\n');
await writeFile('release/installers/SHA256SUMS.txt',artifacts.map(a=>`${a.sha256}  ${a.file}`).join('\n')+'\n');
console.log(JSON.stringify({ installers: artifacts }));
