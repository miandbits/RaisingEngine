import {readFile,stat,readdir,mkdir,writeFile} from 'node:fs/promises';
import {resolve,join,dirname} from 'node:path';
import {createHash} from 'node:crypto';
import assert from 'node:assert/strict';
import {auditReleaseMedia} from './release-sample-media.mjs';
const root=resolve(process.env.QT_PACKAGE_OUT||'release/qt-preview/RaisingStudio');
const manifest=JSON.parse(await readFile(join(root,'package-manifest.json'),'utf8'));
const present=new Map(manifest.files.map(f=>[f.path,f]));
const required=['LICENSE','THIRD_PARTY_NOTICES.md','licenses/README.md','licenses/qt/LGPL-3.0-only.txt','licenses/qt/GPL-3.0-only.txt',
 'licenses/mingw/gcc/COPYING.RUNTIME','licenses/node/LICENSE-v22.14.0.txt','licenses/player/stb_image.txt','licenses/player/RmlUi-containers.txt',
 'licenses/player/Freetype.txt','licenses/player/libwebp.txt','licenses/player/libwebp-PATENTS.txt','licenses/player/miniaudio.txt','licenses/nlohmann-json.txt',
 'fonts/NotoSansJP.ttf','fonts/OFL-NotoSansJP.txt',
 ...['ko','en','ja'].flatMap(l=>['translations/qtbase_'+l+'.qm','guides/'+l+'.md']),
 ...['react','react-dom','scheduler','NotoSansKR-OFL','nlohmann-json'].map(n=>'engine/dist/web/licenses/'+n+'.txt')];
for(const file of required)assert.ok(present.has(file),'Missing notice/source/guide: '+file);
for(const f of manifest.files){
 assert.ok(!/Qt6Test\.dll|qoffscreen\.dll|D3Dcompiler_47\.dll|dxcompiler\.dll|dxil\.dll/.test(f.path),'Unreviewed optional DLL: '+f.path);
 const bytes=await readFile(join(root,f.path));
 assert.equal(createHash('sha256').update(bytes).digest('hex'),f.sha256,'Payload changed: '+f.path);
}
for(const source of manifest.qtCorrespondingSource)assert.equal(present.get('licenses/qt/sources/'+source.file)?.sha256,source.sha256);
for(const dependency of manifest.dependencies){
 const folder=join(root,'engine/node_modules',dependency.name);
 assert.ok((await readdir(folder)).some(file=>/^(license|copying|notice)/i.test(file)),'No retained dependency notice: '+dependency.name);
}
assert.ok(present.has('Qt6Core.dll')&&present.has('Qt6Widgets.dll'),'Qt must remain dynamically linked');
const sampleMedia=await auditReleaseMedia(root,JSON.parse(await readFile('config/release-sample-media.json','utf8')));
const result={ok:true,releaseVersion:manifest.releaseVersion,sourceCommit:manifest.sourceCommit,sourceTreeDirty:manifest.sourceTreeDirty,
 payloadFiles:manifest.files.length,qtSourceArchives:manifest.qtCorrespondingSource.length,editorLanguages:manifest.editorLanguages,
 noticesChecked:required.length,sampleMedia,sourceRepository:'https://github.com/miandbits/RaisingEngine',legalClearance:false};
const output=process.argv[2]||'.cache/preview-release/package-audit.json';await mkdir(dirname(output),{recursive:true});await writeFile(output,JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result));
