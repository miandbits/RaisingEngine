import {spawn} from 'node:child_process';
import {mkdir,mkdtemp,writeFile} from 'node:fs/promises';
import path from 'node:path';
const args=process.argv.slice(2),supported=['--web','--desktop','--package'];
for(let i=0;i<args.length;i++){if(!supported.includes(args[i]))throw Error('Usage: npm run test:creator-smoke -- [--web] [--desktop] [--package /path/to/editor.app]');if(args[i]==='--package'&&!args[++i])throw Error('--package requires an editor app path');}
await mkdir('.cache/creator-smoke',{recursive:true});const out=await mkdtemp(path.resolve('.cache/creator-smoke/run-')),results=[];
async function run(name,argv,env={},command=process.execPath) {
  const start=Date.now(),chunks=[];console.log('Checking '+name+'...');
  const status=await new Promise((resolve,reject)=>{const child=spawn(command,argv,{timeout:300000,env:{...process.env,...env},stdio:['ignore','pipe','pipe']});for(const stream of [child.stdout,child.stderr])stream.on('data',b=>{chunks.push(b);process.stdout.write(b);});child.on('error',reject);child.on('exit',(code,signal)=>resolve({code,signal}));});
  await writeFile(path.join(out,name+'.log'),Buffer.concat(chunks));results.push({name,...status,elapsedMs:Date.now()-start});await writeFile(path.join(out,'result.json'),JSON.stringify({ok:results.every(r=>r.code===0),results},null,2));if(status.code!==0)throw Error(name+' failed; inspect '+out);
}
await run('services',['--test','--test-concurrency=1',...['first-game','creator-release','unsaved-assets','rule-debug','authoring-workspace','workspace-links','integrated-authoring'].map(n=>'tests/qt-studio/'+n+'.test.mjs'),'tests/mobile/ios-package.test.mjs']);
await run('named-playtests',['--test','--test-concurrency=1','tests/analysis/goal-regression-cli.test.mjs','tests/analysis/goal-ci.test.mjs','tests/analysis/goal-compare.test.mjs','tests/analysis/goal-routes.test.mjs']);
await run('calendar-core',['--test','tests/runtime/calendar.test.mjs']);
if(args.includes('--web')&&args.includes('--desktop'))await run('calendar-players',['--test','tests/journey/calendar-players.test.mjs']);
if(args.includes('--web'))await run('web',['--test','--test-concurrency=1','tests/journey/first-game-web.test.mjs','tests/native/web-save-slots.test.mjs','tests/journey/save-slots-web.test.mjs']);
if(args.includes('--desktop')){
  await run('editor',['scripts/test-first-game-ui.mjs']);
  await run('native',['--test','tests/native-player/first-game.test.mjs']);
  const executable=name=>path.resolve(process.env.QT_STUDIO_OUT??'dist/qt-studio',name+(process.platform==='win32'?'.exe':''));
  await run('story-form',[],{},executable('qt-story-flow-dialog-test'));
  const fixture=path.join(out,'diagnostic-game');
  await run('diagnostic-fixture',['tests/qt-studio/validation-location-fixture.mjs',fixture]);
  await run('diagnostic-form',[],{RAISING_VALIDATION_PLAN:fixture+'-plan.json',RAISING_ENGINE_ROOT:path.resolve('.'),RAISING_NODE:process.execPath,RAISING_PROJECT_HISTORY:path.join(out,'diagnostic-history.ini')},executable('qt-validation-location-test'));
}
if(args.includes('--package')) {
  if(process.platform!=='darwin')throw Error('--package currently validates a macOS editor app; use test:qt-package for the Windows installation gate.');
  const app=path.resolve(args[args.indexOf('--package')+1]),engine=path.join(app,'Contents/Resources/engine');
  await run('standalone',['scripts/test-macos-package.mjs',app]);
  await run('packaged-editor',['scripts/test-first-game-ui.mjs'],{RAISING_ENGINE_ROOT:engine,RAISING_NODE:path.join(engine,'node')});
}
console.log('Creator smoke report: '+out);
