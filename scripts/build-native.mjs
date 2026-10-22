import { spawnSync } from 'node:child_process';
import { mkdirSync, readFileSync, writeFileSync, renameSync, existsSync, openSync, closeSync, unlinkSync, copyFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { resolve, dirname } from 'node:path';
const root=resolve(import.meta.dirname,'..');
const output=resolve(root,'dist/native');
mkdirSync(output,{recursive:true});
const lock=resolve(output,'.build.lock');
const until=Date.now()+120_000;
let acquired=false;
while(!acquired){
  try{const fd=openSync(lock,'wx');writeFileSync(fd,JSON.stringify({pid:process.pid}));closeSync(fd);acquired=true;}
  catch(error){
    if(error.code!=='EEXIST')throw error;
    try{const owner=JSON.parse(readFileSync(lock,'utf8'));try{process.kill(owner.pid,0);}catch(e){if(e.code==='ESRCH'){unlinkSync(lock);continue;}}}catch{}
    if(Date.now()>until)throw new Error(`Another native build owns ${lock}. Wait for it to finish.`);
    Atomics.wait(new Int32Array(new SharedArrayBuffer(4)),0,0,100);
  }
}
process.once('exit',()=>{if(acquired){try{unlinkSync(lock);}catch{}}});
const names=['CMakeLists.txt','include/raising/runtime.hpp','include/raising/relationship-history.hpp','include/raising/choice-feedback.hpp','src/runtime.cpp','src/user-fields.inc','src/gameplay.inc','src/generic.inc','src/roster.inc', 'src/character-state.inc','src/custom-states.inc','src/progression.inc','src/unified-time.inc','src/view.inc','src/tactics.inc','src/systems.inc','src/systems-view.inc','src/protocol.inc','src/main.cpp','src/wasm.cpp','vendor/json.hpp'];
const files=names.map(name=>[name,readFileSync(resolve(root,'native',name))]);
const hash=createHash('sha256').update('native-release-isolated-v2:'+process.platform);
for(const [name,bytes] of files)hash.update(name).update(bytes);
const revision=hash.digest('hex').slice(0,16),stem=`raising-native-${revision}`;
const executable=stem+(process.platform==='win32'?'.exe':'');
const manifest={runtime:'raising-native',coreVersion:'0.2.0',protocolVersion:1,backend:'native',executable,sourceRevision:revision};
function publish(){const temp=resolve(output,`runtime-${process.pid}.json`);writeFileSync(temp,JSON.stringify(manifest,null,2));renameSync(temp,resolve(output,'runtime.json'));}
if(existsSync(resolve(output,executable))){publish();console.log(`Native C++ runtime current: ${resolve(output,executable)}`);process.exit(0);}
// Immutable source/build directories prevent concurrent builds from relinking
// a running executable or changing another invocation's CMake target name.
const source=resolve(root,'.cache/native-sources',revision),build=resolve(root,'.cache/native-build',revision);
for(const [name,bytes] of files){const path=resolve(source,name);mkdirSync(dirname(path),{recursive:true});writeFileSync(path,bytes);}
mkdirSync(build,{recursive:true});
const run=(cmd,args)=>{const r=spawnSync(cmd,args,{cwd:root,stdio:'inherit',env:{...process.env,NODE_OPTIONS:''}});if(r.error)throw r.error;if(r.status!==0)process.exit(r.status??1);};
const generator=process.platform==='win32'?['-G','MinGW Makefiles']:[];
run('cmake',['-S',source,'-B',build,...generator,'-DCMAKE_BUILD_TYPE=Release',`-DRAISING_BINARY_NAME=${stem}`]);
run('cmake',['--build',build,'--target','raising-native','--parallel','2']);
copyFileSync(resolve(build,executable),resolve(output,executable));publish();
console.log(`Native C++ runtime: ${resolve(output,executable)}`);
