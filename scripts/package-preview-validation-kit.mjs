import {mkdir,cp,readFile,writeFile,access} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {createHash} from 'node:crypto';
const release=JSON.parse(await readFile('config/preview-release.json','utf8'));
const output=resolve(`release/${release.version}/validation-kit`);
try{await access(output);throw Error('Validation kit already exists');}catch(e){if(e.code!=='ENOENT')throw e;}
await mkdir(output,{recursive:true});
const studio=resolve(process.env.QT_STUDIO_OUT||'dist/qt-studio'),qt=resolve(process.env.QT_ROOT||'C:/Qt/6.10.1/mingw_64');
for(const [source,target]of [[join(studio,'qt-studio-test.exe'),'qt-studio-test.exe'],[join(studio,'qt-editor-language-test.exe'),'qt-editor-language-test.exe'],[join(qt,'bin/Qt6Test.dll'),'Qt6Test.dll'],[join(qt,'plugins/platforms/qoffscreen.dll'),'qoffscreen.dll'],['LICENSE','LICENSE'],['licenses/qt/LGPL-3.0-only.txt','Qt-LGPL-3.0.txt']])await cp(source,join(output,target));
await writeFile(join(output,'README.txt'),'Maintainer-only validation tools. Engine tests: MIT. QtTest/offscreen plugin: Qt 6.10.1 LGPL-3.0; corresponding QtBase sources are in the matching installer licenses/qt/sources. Requires the matching installed Qt libraries. Remove this temporary draft asset before public release.\n');
const archive=resolve(`release/${release.version}/validation-kit-windows-x64.zip`);
const result=spawnSync('tar.exe',['-a','-cf',archive,'-C',output,'.'],{windowsHide:true,encoding:'utf8'});if(result.status!==0)throw Error(result.stderr);
const bytes=await readFile(archive);const receipt={version:release.version,file:'validation-kit-windows-x64.zip',sha256:createHash('sha256').update(bytes).digest('hex'),bytes:bytes.length};
await writeFile('config/preview-validation-kit.json',JSON.stringify(receipt,null,2)+'\n');console.log(JSON.stringify(receipt));
