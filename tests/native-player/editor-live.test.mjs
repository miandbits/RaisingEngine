import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdtemp,mkdir,cp,rename} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {spawn} from 'node:child_process';
import {setTimeout as delay} from 'node:timers/promises';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';

test('live player advances, freezes, restores private saves and rejects ordinary loading',{skip:!['win32','darwin'].includes(process.platform)||!process.env.RAISING_NATIVE_PLAYER_RUNTIME,timeout:120000},async t=>{
  await mkdir('.cache/editor-live-test',{recursive:true});const out=await mkdtemp(resolve('.cache/editor-live-test/run-'));
  const root=join(out,'project');await cp('tests/fixtures/projects/direction-lab',root,{recursive:true});
  const original=await readFile(join(root,'project.json'));
  const service=new QtStudioService();await service.handle({method:'open',params:{root}});
  const response=await service.handle({method:'preview',params:{revision:service.editor.revision,interactive:true,authoringContext:{canvasContext:true,screenId:'home'}}});
  assert.ok(response.play,JSON.stringify(response));const p=response.play;
  const session=join(out,'session');await mkdir(session);const ignoredSave=join(out,'original.json');await writeFile(ignoredSave,'preserve me');
  const child=spawn(p.executable,['--bundle',p.directory,'--save',ignoredSave,'--hidden','--preview-screen',p.screenId,'--preview-inspection',p.inspection,'--editor-live-preview',session],{windowsHide:true});
  t.after(()=>child.kill());let buffer='',stderr='',ready=false;const reports=new Map();
  child.stderr.on('data',b=>stderr+=b);child.stdout.on('data',b=>{buffer+=b;while(buffer.includes('\n')){const at=buffer.indexOf('\n'),line=buffer.slice(0,at);buffer=buffer.slice(at+1);try{const r=JSON.parse(line).editorPreview;if(r?.ready)ready=r;else if(r)reports.set(r.serial,r);}catch{}}});
  async function until(predicate){for(let i=0;i<1000;i++){if(predicate())return;if(child.exitCode!==null)throw Error(stderr||'Player exited');await delay(20);}throw Error('Editor preview timed out: '+stderr);}
  await until(()=>ready);if(process.platform==='darwin')assert.equal(ready.embedded,false);let serial=0;
  async function command(value){const id=++serial;await writeFile(join(session,'next.json'),JSON.stringify({...value,serial:id}));await rename(join(session,'next.json'),join(session,'control.json'));await until(()=>reports.has(id));await writeFile(join(out,'last-report.json'),JSON.stringify(reports.get(id)));return reports.get(id);}
  await command({type:'pause',paused:false});await delay(100);
  const before=await command({type:'inspect'});assert.equal(before.ui.preview.active,false);
  await command({type:'click',id:'direction-greeting'});await delay(250);
  const moving=await command({type:'inspect'});assert.equal(moving.ui.direction.sequenceId,'greeting');assert.ok(moving.ui.direction.elapsedMs>0);
  const paused=await command({type:'pause',paused:true});await delay(250);
  const still=await command({type:'click',id:'schedule-link'});
  assert.equal(still.elapsedMs,paused.elapsedMs);assert.deepEqual(still.ui.direction,paused.ui.direction);assert.deepEqual(still.ui.state,paused.ui.state);
  await command({type:'pause',paused:false});await delay(150);
  const resumed=await command({type:'inspect'});assert.ok(resumed.ui.direction.elapsedMs>still.ui.direction.elapsedMs);
  const saved=await command({type:'click',id:'directionSave'});const envelope=JSON.parse(await readFile(join(session,'save.json'),'utf8'));assert.ok(envelope.inspectionSession);
  await command({type:'click',id:'directionStop'});const loaded=await command({type:'click',id:'load'});assert.deepEqual(loaded.ui.state,saved.ui.state);assert.ok(Math.abs(loaded.ui.direction.elapsedMs-envelope.playerPresentation.direction.elapsedMs)<100);
  assert.deepEqual(await readFile(join(root,'project.json')),original);assert.equal(await readFile(ignoredSave,'utf8'),'preserve me');
  const strict=await service.handle({method:'debugLoad',params:{revision:service.editor.revision,text:JSON.stringify(envelope)}});assert.equal(strict.debug.ok,false);assert.ok(strict.debug.error);
  await command({type:'stop'}).catch(()=>{});
});
