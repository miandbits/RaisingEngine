import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdir,mkdtemp} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {existsSync} from 'node:fs';
import {parseTree} from 'jsonc-parser';
test('portable save input enforces Web transport bounds and strict native Unicode without leaking contents',{timeout:120000},async()=>{
await mkdir('.cache/portable-save-input',{recursive:true});
const root=await mkdtemp(resolve('.cache/portable-save-input/run-')),exe=join(root,process.platform==='win32'?'probe.exe':'probe'),fixtures=join(root,'fixtures'),MAX=16*1024*1024;
await mkdir(fixtures,{recursive:true});
const source=join(root,'probe.cpp');
await writeFile(source,"#include \"portable-save-input.hpp\"\n#include <iostream>\n#include <sstream>\nusing namespace raising_player::portable_input;\nint main(int argc,char** argv){\n  try {\n    nlohmann::json value;\n    if(argc>1&&std::string(argv[1])==\"--stream\")value=parse(bounded_read(std::cin));\n    else if(argc>1&&std::string(argv[1])==\"--bad-stream\") {std::istringstream input(\"{}\");input.setstate(std::ios::badbit);value=parse(bounded_read(input));}\n    else if(argc>1)value=read_staged_file(std::filesystem::u8path(argv[1]));\n    else return 2;\n    nlohmann::json out={{\"ok\",true},{\"type\",value.type_name()}};\n    if(value.is_string())out[\"stringBytes\"]=value.get_ref<const std::string&>().size();\n    if(value.dump().size()<4096)out[\"value\"]=value;\n    std::cout<<out.dump()<<'\\n';return 0;\n  } catch(const Error& e) {std::cout<<nlohmann::json({{\"ok\",false},{\"code\",code_name(e.code)},{\"message\",e.what()}}).dump()<<'\\n';return 0;}\n    catch(const std::exception&) {std::cout<<\"{\\\"ok\\\":false,\\\"unexpected\\\":true}\\n\";return 1;}\n}\n");
const mingw=join(process.env.QT_COMPILER??'C:/Program Files/mingw64/bin','g++.exe'),compiler=process.env.CXX??(process.platform==='win32'?(existsSync(mingw)?mingw:'g++'):'c++');
const compiled=spawnSync(compiler,['-std=c++17','-O1','-Wall','-Wextra','-I',resolve('native/player'),'-I',resolve('native/vendor'),source,'-o',exe],{encoding:'utf8',timeout:90000});
assert.equal(compiled.status,0,compiled.stderr||String(compiled.error));
// Exact parseSave structural walk from packages/web-player/controller.ts.
function webText(text){
 if(new TextEncoder().encode(text).length>MAX)return {ok:false,code:'too-large'};
 const errors=[];let tree;try{tree=parseTree(text,errors,{disallowComments:true,allowTrailingComma:false,allowEmptyContent:false});}catch{return {ok:false,code:'parser-stack'};}
 if(!tree||errors.length)return {ok:false,code:'invalid-json'};
 const pending=[{node:tree,depth:0}];while(pending.length){const {node,depth}=pending.pop();if(depth>128)return {ok:false,code:'too-deep'};if(node.type==='object'){const keys=new Set;for(const p of node.children??[]){const key=p.children?.[0]?.value;if(keys.has(key))return {ok:false,code:'duplicate-key'};keys.add(key);}}for(const child of node.children??[])pending.push({node:child,depth:depth+1});}
 try{return {ok:true,value:JSON.parse(text)};}catch{return {ok:false,code:'invalid-json'};}
}
async function webBytes(bytes){if(bytes.length>MAX)return {ok:false,code:'too-large'};return webText(await new Blob([bytes]).text());}
const cases=[];
const add=(name,text,code=null,extra={})=>cases.push({name,bytes:Buffer.isBuffer(text)?text:Buffer.from(text),code,...extra});
add('plain-save-shape','{"version":1,"origin":{"seed":42},"commands":[],"state":{},"presentation":{}}');
add('all-json-primitives','[null,true,false,0,-0,1.25,-2e10,"string"]');
add('unicode','{"日本語":"안녕하세요 😺","é":"é","escape":"\\uD83D\\uDE3A"}');
add('unicode-distinct-normalization','{"é":1,"é":2}');
add('valid-sibling-keys','{"left":{"a":1},"right":{"a":2}}');
add('prototype-property-data','{"__proto__":{"a":1},"constructor":2}');
add('unicode-boundaries','["\\u007f\\u0080\\u07ff\\u0800\\ud7ff\\ue000\\uffff\\uD800\\uDC00\\uDBFF\\uDFFF"]');
add('duplicate-escaped-nul-key','{"a\\u0000b":1,"a\\u0000b":2}','duplicate-key');
add('escaped-nul','{"text":"before\\u0000after"}');
add('duplicate-key','{"secret key":1,"secret key":2}','duplicate-key');
add('duplicate-escaped-key','{"a":1,"\\u0061":2}','duplicate-key');
add('duplicate-unicode-key','{"😺":1,"\\uD83D\\uDE3A":2}','duplicate-key');
add('duplicate-nested','{"x":{"a":1,"a":2}}','duplicate-key');
add('array-depth-128','['.repeat(128)+'0'+']'.repeat(128));
add('array-depth-129','['.repeat(129)+'0'+']'.repeat(129),'too-deep');
add('object-depth-128','{"a":'.repeat(64)+'0'+'}'.repeat(64));
add('object-depth-130','{"a":'.repeat(65)+'0'+'}'.repeat(65),'too-deep');
add('object-empty-depth-128','{"a":'.repeat(64)+'{}'+'}'.repeat(64));
add('mixed-depth-128','{"a":'.repeat(32)+'['.repeat(64)+'0'+']'.repeat(64)+'}'.repeat(32));
add('mixed-depth-129','{"a":'.repeat(32)+'['.repeat(65)+'0'+']'.repeat(65)+'}'.repeat(32),'too-deep');
for(const [name,text] of [['empty',''],['space-only',' \n\t'],['comment','{/*secret*/"a":1}'],['trailing-comma','[1,]'],['trailing-token','{}{}'],['truncated','{"x":'],['unquoted-key','{a:1}'],['raw-newline','"a\nb"'],['leading-zero','01'],['nan','NaN'],['trailing-nul',Buffer.from([123,125,0])],['nul-then-secret',Buffer.from('{}\0private-data')],['string-nul',Buffer.from('"a\0b"')]])add(name,text,'invalid-json');
add('utf8-bom',Buffer.concat([Buffer.from([239,187,191]),Buffer.from('{}')]));
add('double-bom',Buffer.concat([Buffer.from([239,187,191,239,187,191]),Buffer.from('{}')]),'invalid-json');
for(const [name,bytes] of [['overlong',[0xc0,0xaf]],['surrogate',[0xed,0xa0,0x80]],['beyond-max',[0xf4,0x90,0x80,0x80]],['bad-lead',[0xf5,0x80,0x80,0x80]],['truncated-multibyte',[0xe2,0x82]],['orphan-continuation',[0x80]]])add('utf8-'+name,Buffer.from([34,...bytes,34]),'invalid-utf8',{deliberateDifference:'Web File.text silently replaces invalid UTF-8; strict native import rejects it.'});
for(const [name,text] of [['lone-high','"\\ud800"'],['lone-low','"\\udc00"'],['reversed-pair','"\\udc00\\ud800"']])add(name,text,'invalid-json',{deliberateDifference:'JavaScript accepts unpaired UTF-16 surrogate escapes; existing native JSON rejects them.'});
add('overflow-number','1e400','invalid-json',{deliberateDifference:'JavaScript JSON.parse produces Infinity; native JSON rejects non-finite numeric overflow.'});
add('exact-byte-limit','"'+'a'.repeat(MAX-2)+'"',null,{large:true});
add('one-byte-over','"'+'a'.repeat(MAX-1)+'"','too-large',{large:true});
add('multibyte-exact-limit','"'+'😺'.repeat((MAX-4)/4)+'aa"',null,{large:true});
add('very-deep-early-rejection','['.repeat(20000)+'0'+']'.repeat(20000),'too-deep',{skipWeb:true});
const report=[];
for(const c of cases){const file=join(fixtures,c.name+'.json');await writeFile(file,c.bytes);const started=performance.now();const result=spawnSync(exe,[file],{encoding:'utf8',timeout:15000,maxBuffer:256*1024});assert.equal(result.status,0,result.stderr);const native=JSON.parse(result.stdout);assert.equal(native.unexpected,undefined,c.name);assert.equal(native.ok,!c.code,JSON.stringify({case:c.name,native}));if(c.code){assert.equal(native.code,c.code,c.name);assert.equal(native.message,c.code,'Errors must contain only fixed portable codes');}let web=c.skipWeb?null:await webBytes(c.bytes);if(web&&!c.deliberateDifference){assert.equal(native.ok,web.ok,'Web parity '+c.name);if(!native.ok)assert.equal(native.code,web.code,c.name);else if(!c.large)assert.deepEqual(native.value,JSON.parse(JSON.stringify(web.value)),c.name);}report.push({name:c.name,bytes:c.bytes.length,milliseconds:Math.round(performance.now()-started),native:{ok:native.ok,...native.code?{code:native.code}:{}},web:web?{ok:web.ok,...web.code?{code:web.code}:{}}:null,...c.deliberateDifference?{deliberateDifference:c.deliberateDifference}:{}});}
for(const [name,args,input,code] of [['stream-exact',['--stream'],'"'+'a'.repeat(MAX-2)+'"',null],['stream-growth-over',['--stream'],'"'+'a'.repeat(MAX-1)+'"','too-large'],['failed-stream',['--bad-stream'],undefined,'unreadable'],['missing-path',[join(fixtures,'private-nonexistent.json')],undefined,'unreadable'],['directory-path',[fixtures],undefined,'unreadable']]){const r=spawnSync(exe,args,{input,encoding:'utf8',maxBuffer:256*1024,timeout:15000});assert.equal(r.status,0,r.stderr);const native=JSON.parse(r.stdout);assert.equal(native.ok,!code,name);if(code){assert.equal(native.code,code,name);assert.equal(native.message,code,name);}report.push({name,native:{ok:native.ok,...code?{code}:{}},web:null});}
const webSource=await readFile('packages/web-player/controller.ts');
await writeFile(join(root,'result.json'),JSON.stringify({ok:true,createdAt:new Date().toISOString(),scope:'strict portable-input transport/header validation; not full runtime or system picker acceptance',cases:report.length,webReference:{file:'packages/web-player/controller.ts',sha256:createHash('sha256').update(webSource).digest('hex'),structuralWalk:'parseSave plus Blob.text portable input boundary'},limits:{maxBytes:MAX,maxWebTreeDepth:128,objectChildIncrement:2,arrayChildIncrement:1},tests:report},null,2)+'\n');
console.log(JSON.stringify({ok:true,cases:report.length,receipt:join(root,'result.json')},null,2));

});
