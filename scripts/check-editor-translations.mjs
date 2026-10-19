import {readFile,readdir,mkdir,writeFile} from 'node:fs/promises';
import {join,dirname} from 'node:path';
import assert from 'node:assert/strict';

const rows=JSON.parse(await readFile('native/studio/translations/editor.json','utf8'));
const catalog=new Map();
const slots=text=>[...text.matchAll(/%L?[1-9][0-9]*|%n|\{[a-zA-Z_][a-zA-Z0-9_.]*\}/g)].map(m=>m[0]).sort();
for(const row of rows){
  assert.equal(row.length,3,'Expected source, English, Japanese');
  const [source,...translations]=row;
  assert.ok(!catalog.has(source),`Duplicate translation: ${source}`);
  for(const text of translations){
    assert.ok(typeof text==='string'&&text.trim(),`Empty translation: ${source}`);
    assert.deepEqual(slots(text),slots(source),`Placeholder mismatch: ${source}`);
  }
  catalog.set(source,translations);
}
// This measures literal tr() calls, not arbitrary service errors or user content.
const files=[];
async function visit(dir){for(const e of await readdir(dir,{withFileTypes:true})){
  const file=join(dir,e.name);
  if(e.isDirectory()){if(e.name!=='translations')await visit(file);}
  else if(/\.(cpp|hpp|inc)$/.test(e.name)&&!/(?:test|tests)/.test(e.name))files.push(file);
}}
await visit('native/studio');
const strings=new Set(),missingByFile={},coverage=[];
const complete=['project-manager.cpp','export-dialog.cpp','localization.cpp','editor-language.cpp','getting-started.cpp','authoring.cpp'];
for(const file of files){
  const text=await readFile(file,'utf8'),seen=new Set();
  for(const m of text.matchAll(/\btr\(\s*((?:"(?:[^"\\]|\\.)*"\s*)+)/g)){
    let value;try{value=[...m[1].matchAll(/"(?:[^"\\]|\\.)*"/g)].map(s=>JSON.parse(s[0])).join('');}catch{continue;}
    if(/[가-힣]/.test(value)){strings.add(value);seen.add(value);}
  }
  const missing=[...seen].filter(s=>!catalog.has(s));
  if(missing.length)missingByFile[file.replaceAll('\\','/')]=missing;
  coverage.push({file:file.replaceAll('\\','/'),total:seen.size,translated:seen.size-missing.length});
  if(complete.some(name=>file.replaceAll('\\','/').endsWith('/'+name)))assert.deepEqual(missing,[],`Incomplete preview workflow: ${file}`);
}
const report={version:1,locales:['ko','en','ja'],sourceLanguage:'ko',translated: [...strings].filter(s=>catalog.has(s)).length,total:strings.size,
  scope:'Literal Korean tr() messages; not a full UI coverage guarantee. Untranslated strings fall back to Korean.',coverage,missingByFile};
const output=process.argv[2]||'.cache/preview-release/editor-translation-coverage.json';
await mkdir(dirname(output),{recursive:true});await writeFile(output,JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify({ok:true,entries:rows.length,translated:report.translated,total:report.total,completePreviewFiles:complete,report:output}));
