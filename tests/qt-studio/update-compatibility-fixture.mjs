import assert from 'node:assert/strict';
import {cp,mkdir,readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {loadProject,buildSnapshot} from '../../dist/packages/project-service/index.js';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/session.js';
import {encodeSave} from '../../dist/packages/runtime-service/save.js';
export async function updateFixture(root) {
  await mkdir(root,{recursive:true});const from=path.join(root,'previous'),to=path.join(root,'current'),other=path.join(root,'other');
  await cp(path.resolve('examples/my-first-raising-game'),from,{recursive:true});await cp(from,to,{recursive:true});await cp(from,other,{recursive:true});
  const change=async(dir,edit)=>{const file=path.join(dir,'project.json'),value=JSON.parse(await readFile(file,'utf8'));edit(value);await writeFile(file,JSON.stringify(value,null,2));};
  await change(to,p=>{p.title+=' Updated';});await change(other,p=>{p.gameId+='-different';});
  const built=await buildSnapshot(await loadProject(from));assert.equal(built.ok,true,JSON.stringify(built));
  let session=createSession(built.snapshot,42);assert.equal(session.ok,true);const ids=Array(built.snapshot.project.periodsPerMonth).fill('rest');session=dispatchSession(built.snapshot,session.session,{type:'setSchedule',activityIds:ids},session.session.state.stateRevision);assert.equal(session.ok,true,JSON.stringify(session));
  const encoded=await encodeSave(built.snapshot,session.session,{version:1,timeline:null,extensions:{example:{cursor:4}}});assert.equal(encoded.ok,true,JSON.stringify(encoded));
  const save=path.join(root,'original-save.json'),output=path.join(root,'converted-save.json');await writeFile(save,encoded.text);const fixture={from,to,other,save,output};await writeFile(path.join(root,'fixture.json'),JSON.stringify(fixture));return fixture;
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url))console.log(JSON.stringify(await updateFixture(path.resolve(process.argv[2]))));
