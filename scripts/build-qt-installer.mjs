import {readFile,writeFile,mkdir,readdir} from 'node:fs/promises';
import {resolve,join,dirname} from 'node:path';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
const bundle=resolve(process.env.QT_PACKAGE_OUT||'release/qt-preview/RaisingStudio');
const output=resolve(process.env.QT_INSTALLER_OUT||'release/qt-preview/RaisingStudio-Qt-Preview-0.1.0-x64.exe');
const manifest=JSON.parse(await readFile(join(bundle,'package-manifest.json'),'utf8'));
const version=JSON.parse(await readFile('config/preview-release.json','utf8')).version;
if(!/^[0-9A-Za-z.-]+$/.test(version))throw Error('Unsafe release version');
const product=`RaisingEngine ${version}`;
const uninstallKey=`Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\RaisingEngine-${version}`;
// Hash-pinned payload; never package files added after the reviewed manifest.
for(const f of manifest.files){const hash=createHash('sha256').update(await readFile(join(bundle,f.path))).digest('hex');if(hash!==f.sha256)throw Error('Package changed: '+f.path);}
let nsis=process.env.NSIS_COMPILER;
if(!nsis){const parent=resolve('.cache/electron-builder/nsis-3.0.4.1');const dir=(await readdir(parent)).find(n=>n.startsWith('nsis-'));if(!dir)throw Error('Set NSIS_COMPILER to makensis.exe');nsis=join(parent,dir,'makensis.exe');}
const escape=s=>{if(/[\r\n]/.test(s))throw Error('Newline in installer path');return s.replaceAll('$','$$').replaceAll('"','$\\"');};
const files=[...manifest.files.map(f=>f.path),'package-manifest.json'];
const dirs=new Set();for(const f of files){let d=dirname(f).replaceAll('\\','/');while(d!=='.'){dirs.add(d);d=dirname(d).replaceAll('\\','/');}}
const lines=[
 'Unicode true','RequestExecutionLevel user','SetCompressor /SOLID lzma','Name "Raising Studio Qt Preview"',
 `OutFile "${escape(output)}"`,`InstallDir "$LOCALAPPDATA\\Programs\\RaisingEngine-${version}"`,
 '!include "MUI2.nsh"',`!insertmacro MUI_PAGE_LICENSE "${escape(join(bundle,'LICENSE'))}"`,
 '!define MUI_DIRECTORYPAGE_TEXT_TOP "$(FreshDirectoryHint)"','!insertmacro MUI_PAGE_DIRECTORY','!insertmacro MUI_PAGE_INSTFILES','!insertmacro MUI_UNPAGE_CONFIRM','!insertmacro MUI_UNPAGE_INSTFILES','!insertmacro MUI_LANGUAGE "English"','!insertmacro MUI_LANGUAGE "Korean"','!insertmacro MUI_LANGUAGE "Japanese"',
 'LangString FreshDirectoryHint ${LANG_ENGLISH} "Choose a new installation folder. Keep game projects outside the installation. Existing folders are preserved."',
 'LangString FreshDirectoryHint ${LANG_KOREAN} "새 설치 폴더를 선택하세요. 게임 프로젝트는 설치 폴더 밖에 보관하세요. 기존 폴더는 보존합니다."',
 'LangString FreshDirectoryHint ${LANG_JAPANESE} "新しいインストール先を選んでください。ゲームプロジェクトはインストール先とは別に保存してください。既存のフォルダーは保持されます。"',
 'Function .onInit','IfSilent silent interactive','interactive:','!insertmacro MUI_LANGDLL_DISPLAY','Goto fresh','silent:','IfFileExists "$INSTDIR\\." existing fresh','existing:','SetErrorLevel 2','Abort','fresh:','FunctionEnd',
 'Function .onVerifyInstDir','IfFileExists "$INSTDIR\\." 0 valid','Abort','valid:','FunctionEnd',
 'Section',
 ...files.flatMap(f=>[`SetOutPath "$INSTDIR\\${escape(dirname(f).replaceAll('/','\\'))}"`,`File "${escape(join(bundle,f))}"`]),
 'SetOutPath "$INSTDIR"','WriteUninstaller "$INSTDIR\\Uninstall.exe"',
 `CreateDirectory "$SMPROGRAMS\\${product}"`,
 `CreateShortCut "$SMPROGRAMS\\${product}\\Raising Studio.lnk" "$INSTDIR\\RaisingStudioQt.exe"`,
 `CreateShortCut "$SMPROGRAMS\\${product}\\Uninstall.lnk" "$INSTDIR\\Uninstall.exe"`,
 `WriteRegStr HKCU "${uninstallKey}" "DisplayName" "${product}"`,
 `WriteRegStr HKCU "${uninstallKey}" "DisplayVersion" "${version}"`,
 `WriteRegStr HKCU "${uninstallKey}" "Publisher" "RaisingEngine contributors"`,
 `WriteRegStr HKCU "${uninstallKey}" "InstallLocation" "$INSTDIR"`,
 `WriteRegStr HKCU "${uninstallKey}" "UninstallString" '$\\"$INSTDIR\\Uninstall.exe$\\"'`,
 `WriteRegDWORD HKCU "${uninstallKey}" "NoModify" 1`,
 `WriteRegDWORD HKCU "${uninstallKey}" "NoRepair" 1`,
 'SectionEnd','Section "Uninstall"',
 `ReadRegStr $0 HKCU "${uninstallKey}" "InstallLocation"`,
 'StrCmp $0 $INSTDIR 0 keepRegistration',
 `DeleteRegKey HKCU "${uninstallKey}"`,
 `Delete "$SMPROGRAMS\\${product}\\Raising Studio.lnk"`,
 `Delete "$SMPROGRAMS\\${product}\\Uninstall.lnk"`,
 `RMDir "$SMPROGRAMS\\${product}"`,'keepRegistration:',
 ...files.map(f=>`Delete "$INSTDIR\\${escape(f.replaceAll('/','\\'))}"`),
 'Delete "$INSTDIR\\Uninstall.exe"',...[...dirs].sort((a,b)=>b.length-a.length).map(d=>`RMDir "$INSTDIR\\${escape(d.replaceAll('/','\\'))}"`),'RMDir "$INSTDIR"','SectionEnd'
 ];
await mkdir('.cache/qt-installer',{recursive:true});await mkdir(dirname(output),{recursive:true});
// NSIS 3.0 detects UTF-8 reliably with a BOM, including on Korean Windows.
const script=resolve('.cache/qt-installer/preview.nsi');await writeFile(script,'\uFEFF'+lines.join('\n'));
execFileSync(nsis,['/V2',script],{stdio:'inherit',windowsHide:true});
const bytes=await readFile(output),result={file:output,bytes:bytes.length,sha256:createHash('sha256').update(bytes).digest('hex'),signed:false,publicReleaseReady:false};
await writeFile(output+'.json',JSON.stringify(result,null,2));console.log(JSON.stringify(result));
