import {readFile,writeFile,mkdir,readdir} from 'node:fs/promises';
import {resolve,join,dirname} from 'node:path';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
const bundle=resolve(process.env.QT_PACKAGE_OUT||'release/qt-preview/RaisingStudio');
const output=resolve(process.env.QT_INSTALLER_OUT||'release/qt-preview/RaisingStudio-Qt-Preview-0.1.0-x64.exe');
const manifest=JSON.parse(await readFile(join(bundle,'package-manifest.json'),'utf8'));
// Hash-pinned payload; never package files added after the reviewed manifest.
for(const f of manifest.files){const hash=createHash('sha256').update(await readFile(join(bundle,f.path))).digest('hex');if(hash!==f.sha256)throw Error('Package changed: '+f.path);}
let nsis=process.env.NSIS_COMPILER;
if(!nsis){const parent=resolve('.cache/electron-builder/nsis-3.0.4.1');const dir=(await readdir(parent)).find(n=>n.startsWith('nsis-'));if(!dir)throw Error('Set NSIS_COMPILER to makensis.exe');nsis=join(parent,dir,'makensis.exe');}
const escape=s=>{if(/[\r\n]/.test(s))throw Error('Newline in installer path');return s.replaceAll('$','$$').replaceAll('"','$\\"');};
const files=[...manifest.files.map(f=>f.path),'package-manifest.json'];
const dirs=new Set();for(const f of files){let d=dirname(f).replaceAll('\\','/');while(d!=='.'){dirs.add(d);d=dirname(d).replaceAll('\\','/');}}
const lines=[
 'Unicode true','RequestExecutionLevel user','SetCompressor /SOLID lzma','Name "Raising Studio Qt Preview"',
 `OutFile "${escape(output)}"`,'InstallDir "$LOCALAPPDATA\\Programs\\RaisingStudio-Qt-Preview"',
 '!include "MUI2.nsh"',`!insertmacro MUI_PAGE_LICENSE "${escape(join(bundle,'LICENSE'))}"`,
 '!define MUI_DIRECTORYPAGE_TEXT_TOP "Choose a new installation folder. Existing folders are preserved; keep game projects outside the installation."','!insertmacro MUI_PAGE_DIRECTORY','!insertmacro MUI_PAGE_INSTFILES','!insertmacro MUI_UNPAGE_CONFIRM','!insertmacro MUI_UNPAGE_INSTFILES','!insertmacro MUI_LANGUAGE "English"','!insertmacro MUI_LANGUAGE "Korean"',
 'Function .onInit','IfSilent 0 fresh','IfFileExists "$INSTDIR\\." existing fresh','existing:','SetErrorLevel 2','Abort','fresh:','FunctionEnd',
 'Function .onVerifyInstDir','IfFileExists "$INSTDIR\\." 0 valid','Abort','valid:','FunctionEnd',
 'Section',
 ...files.flatMap(f=>[`SetOutPath "$INSTDIR\\${escape(dirname(f).replaceAll('/','\\'))}"`,`File "${escape(join(bundle,f))}"`]),
 'SetOutPath "$INSTDIR"','WriteUninstaller "$INSTDIR\\Uninstall.exe"','SectionEnd','Section "Uninstall"',
 ...files.map(f=>`Delete "$INSTDIR\\${escape(f.replaceAll('/','\\'))}"`),
 'Delete "$INSTDIR\\Uninstall.exe"',...[...dirs].sort((a,b)=>b.length-a.length).map(d=>`RMDir "$INSTDIR\\${escape(d.replaceAll('/','\\'))}"`),'RMDir "$INSTDIR"','SectionEnd'
 ];
await mkdir('.cache/qt-installer',{recursive:true});await mkdir(dirname(output),{recursive:true});
const script=resolve('.cache/qt-installer/preview.nsi');await writeFile(script,lines.join('\n'));
execFileSync(nsis,['/V2',script],{stdio:'inherit',windowsHide:true});
const bytes=await readFile(output),result={file:output,bytes:bytes.length,sha256:createHash('sha256').update(bytes).digest('hex'),signed:false,publicReleaseReady:false};
await writeFile(output+'.json',JSON.stringify(result,null,2));console.log(JSON.stringify(result));
