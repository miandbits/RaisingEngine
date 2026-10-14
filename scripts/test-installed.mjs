import { execFileSync, spawnSync } from 'node:child_process';
import { access, mkdir, mkdtemp, readFile } from 'node:fs/promises';
import { resolve, join, relative, isAbsolute } from 'node:path';
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';

if (process.platform !== 'win32') throw new Error('Windows installation test requires Windows.');
const portable = process.argv.includes('--portable');
const pkg = JSON.parse(await readFile('package.json','utf8'));
await mkdir('.cache/installed-tests',{recursive:true});
const root = await mkdtemp(resolve('.cache/installed-tests/run-'));
const installed = [];
const paths = {};
function within(target) {
  const rel = relative(root,resolve(target));
  if (!rel || rel.startsWith('..') || isAbsolute(rel)) throw new Error(`Test install path escaped its workspace: ${target}`);
}
function registeredProducts() {
  const script = "$ErrorActionPreference='Stop'; $items=@(Get-ItemProperty 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\*','HKLM:\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\*','HKLM:\\Software\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\*' -ErrorAction SilentlyContinue | Where-Object { $_.DisplayName -like 'Raising Player*' -or $_.DisplayName -like 'Raising Studio*' } | Select-Object DisplayName,InstallLocation,UninstallString); ConvertTo-Json -InputObject $items -Compress";
  return JSON.parse(execFileSync('powershell.exe',['-NoProfile','-NonInteractive','-Command',script],{encoding:'utf8',windowsHide:true}) || '[]');
}
// Never replace a real installation during an automated workspace test.
if (!portable && registeredProducts().length) throw new Error('An existing Raising installation is registered. Use test:portable:year or an isolated Windows user for installation tests.');
let testCode = 1;
try {
  for(const kind of ['player','studio']) {
    const title = `Raising ${kind === 'player' ? 'Player' : 'Studio'}`;
    const directory = portable ? resolve(`release/${title}-win32-x64`) : join(root,kind);
    if (!portable) {
      within(directory);
      const installer = resolve(`release/installers/${title.replaceAll(' ','-')}-Setup-${pkg.version}-x64.exe`);
      const manifest = JSON.parse(await readFile('release/installers/manifest.json','utf8'));
      const artifact = manifest.artifacts.find(item => item.file === `${title.replaceAll(' ','-')}-Setup-${pkg.version}-x64.exe`);
      assert.equal(createHash('sha256').update(await readFile(installer)).digest('hex'),artifact?.sha256,'Installer SHA-256 mismatch');
      execFileSync(installer,['/S','/currentuser',`/D=${directory}`],{windowsHide:true,timeout:120000});
      installed.push({directory,title});
    }
    const executable = join(directory,`${title}.exe`); await access(executable); paths[kind] = executable;
    if(kind==='studio') paths.mcp = join(directory,'resources/mcp');
  }
  if(!portable) {
    const products = registeredProducts(); assert.equal(products.length,2);
    for(const product of products) {
      const owned = installed.find(item => product.DisplayName.startsWith(item.title));
      assert.ok(owned);
      // NSIS may omit InstallLocation in the Uninstall key; its removal command must point to our known directory.
      assert.ok((product.InstallLocation ?? product.UninstallString ?? '').toLowerCase().includes(owned.directory.toLowerCase()));
    }
  }
  const result=spawnSync(process.execPath,['--test','tests/release/installed.test.mjs'],{stdio:'inherit',windowsHide:true,
    env:{...process.env,STUDIO_EXECUTABLE:paths.studio,PLAYER_EXECUTABLE:paths.player,MCP_RUNTIME:paths.mcp},timeout:260000});
  testCode=result.status ?? 1;
  if(result.error) throw result.error;
} finally {
  if(!portable) for(const item of installed.reverse()) {
    within(item.directory);
    const uninstaller=join(item.directory,`Uninstall ${item.title}.exe`); within(uninstaller);
    execFileSync(uninstaller,['/S','/currentuser',`_?=${item.directory}`],{windowsHide:true,timeout:120000});
    await assert.rejects(access(join(item.directory,`${item.title}.exe`)),`${item.title} executable was not removed`);
  }
}
if(!portable) { assert.deepEqual(registeredProducts(),[]); console.log('Installed applications and uninstall registry entries removed; game projects remain in .cache/release-tests.'); }
process.exitCode=testCode;
