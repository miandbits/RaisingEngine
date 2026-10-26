import {spawn} from 'node:child_process';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';
const args=process.argv.slice(2),supported=['--web','--desktop','--package'];
for(let i=0;i<args.length;i++){if(!supported.includes(args[i]))throw Error('Usage: npm run test:creator-smoke -- [--web] [--desktop] [--package /path/to/editor.app]');if(args[i]==='--package'&&!args[++i])throw Error('--package requires an editor app path');}
await mkdir('.cache/creator-smoke',{recursive:true});const out=await mkdtemp(path.resolve('.cache/creator-smoke/run-')),results=[];
async function run(name,argv,env={},command=process.execPath,testLog) {
  const start=Date.now(),chunks=[];console.log('Checking '+name+'...');
  const status=await new Promise(resolve=>{const child=spawn(command,argv,{timeout:300000,env:{...process.env,...env},stdio:['ignore','pipe','pipe']});for(const stream of [child.stdout,child.stderr])stream.on('data',b=>{chunks.push(b);process.stdout.write(b);});child.on('error',error=>resolve({code:null,signal:null,error:error.message}));child.on('exit',(code,signal)=>resolve({code,signal}));});
  // Windows GUI-subsystem Qt tests may have no stdout. An explicit QTest file
  // retains each case result; copy it into the ordinary smoke log for CI review.
  if(testLog){try{const log=await readFile(testLog);if(!log.length)throw Error('QTest result log is empty');chunks.push(log);process.stdout.write(log);}catch(error){status.logError=error.message;}}
  await writeFile(path.join(out,name+'.log'),Buffer.concat(chunks));results.push({name,...status,...(testLog?{testLog:path.basename(testLog)}:{}),elapsedMs:Date.now()-start});await writeFile(path.join(out,'result.json'),JSON.stringify({ok:results.every(r=>r.code===0&&!r.logError),results},null,2));if(status.code!==0||status.logError)throw Error(name+' failed; inspect '+out);
}
await run('services',['--test','--test-concurrency=1',...['first-game','creator-release','analysis-priorities','storage-paths','authoring-lock','unsaved-assets','rule-debug','authoring-workspace','workspace-links','integrated-authoring','update-compatibility'].map(n=>'tests/qt-studio/'+n+'.test.mjs'),'tests/mobile/ios-package.test.mjs','tests/mobile/ios-input-memory.test.mjs']);
await run('verified-download',['--test','tests/release/verified-download.test.mjs']);
await run('phase-forecast-core',['--test','tests/native/phase-forecast.test.mjs'],args.includes('--web')?{RAISING_REQUIRE_FORECAST_WASM:'1'}:{});
await run('response-wait',['--test','tests/native/response-wait.test.mjs']);
await run('balance-priorities',['--test','--test-concurrency=1','tests/analysis/priorities.test.mjs']);
await run('named-playtests',['--test','--test-concurrency=1','tests/analysis/goal-regression-cli.test.mjs','tests/analysis/goal-ci.test.mjs','tests/analysis/goal-compare.test.mjs','tests/analysis/goal-routes.test.mjs']);
await run('route-batches',['--test','--test-concurrency=1','tests/qt-studio/goal-route-batches.test.mjs']);
await run('readable-contract',['--test','tests/visual/readable-layout.test.mjs','tests/visual/reading-history.test.mjs','tests/visual/resume-context.test.mjs']);
await run('calendar-core',['--test','tests/runtime/calendar.test.mjs']);
if(args.includes('--web')&&args.includes('--desktop'))await run('calendar-players',['--test','tests/native-player/receipt-text.test.mjs','tests/journey/calendar-players.test.mjs']);
if(args.includes('--web'))await run('plan-reuse-projection',['--test','tests/player/plan-reuse.test.mjs']);
if(args.includes('--web')){
  // Each thematic group keeps the same bounded deadline. The combined suite
  // outgrew five minutes on CI even when every completed assertion passed.
  await run('web-runtime',['--test','--test-concurrency=1','tests/native/web-content-cache.test.mjs','tests/journey/first-game-web.test.mjs','tests/native/web-save-slots.test.mjs','tests/journey/save-slots-web.test.mjs','tests/visual/save-slots-layout.test.mjs']);
  await run('web-reading',['--test','--test-concurrency=1','tests/journey/readable-layout-web.test.mjs','tests/journey/readable-ux-web.test.mjs','tests/journey/reading-history-web.test.mjs']);
  await run('web-planning',['--test','--test-concurrency=1','tests/journey/first-game-phone-web.test.mjs','tests/journey/planning-guidance-web.test.mjs','tests/journey/plan-reuse-web.test.mjs']);
  await run('web-schedule-details',['--test','tests/visual/schedule-details.test.mjs','tests/visual/progression-labels.test.mjs','tests/visual/control-sizing.test.mjs']);
  await run('web-phase-forecast',['--test','tests/journey/phase-forecast-web.test.mjs']);
  await run('web-resume-context',['--test','--test-concurrency=1','tests/journey/resume-context-web.test.mjs']);
}
if(args.includes('--desktop')){
  await run('editor',['scripts/test-first-game-ui.mjs']);
  await run('native',['--test','tests/native-player/first-game.test.mjs','tests/native-player/automation-wait.test.mjs']);
  await run('native-user-fields',['--test','tests/native-player/user-fields.test.mjs']);
  await run('native-rendering',['--test','tests/native-player/asset-text-rendering.test.mjs']);
  await run('native-readable',['--test','tests/native-player/readable-layout.test.mjs','tests/native-player/reading-workflow.test.mjs','tests/native-player/planning-guidance.test.mjs','tests/native-player/reading-history.test.mjs']);
  await run('native-resume-context',['--test','tests/native-player/resume-context.test.mjs','tests/native-player/resume-context-settlement.test.mjs']);
  await run('native-schedule-details',['--test','--test-concurrency=1','tests/native-player/schedule-details.test.mjs','tests/native-player/control-sizing.test.mjs']);
  await run('native-phase-forecast',['--test','--test-concurrency=1','tests/native-player/phase-forecast.test.mjs','tests/native-player/phase-forecast-layouts.test.mjs']);
  await run('native-layout-diagnostics',['--test','tests/native-player/layout-diagnostics.test.mjs']);
  await run('native-plan-reuse',['--test','tests/native-player/plan-reuse.test.mjs']);
  await run('native-portable-input',['--test','tests/native-player/portable-save-input.test.mjs','tests/native-player/portable-save-files.test.mjs']);
  if(process.platform==='darwin')await run('ios-keyboard-patch',['--test','tests/mobile/ios-keyboard-patch.test.mjs']);
  await run('native-portable-host',['--test','tests/native-player/portable-save-host.test.mjs']);
  await run('native-autosaves',['--test','--test-concurrency=1','tests/native-player/choice-autosave.test.mjs','tests/native-player/load-records-button.test.mjs','tests/native-player/restore-transaction.test.mjs','tests/native-player/save-slots-layout.test.mjs']);
  const executable=name=>path.resolve(process.env.QT_STUDIO_OUT??'dist/qt-studio',name+(process.platform==='win32'?'.exe':''));
  const runQt=(name,env,command)=>{const log=path.join(out,name+'.qt.txt');return run(name,['-o',log+',txt'],env,command,log);};
  const commandFixture=path.join(out,'editor-command-project');
  await run('editor-command-fixture',['tests/qt-studio/editor-basics-fixture.mjs',commandFixture]);
  await runQt('editor-commands',{RAISING_INTERACTION_PROJECT:commandFixture,QT_QPA_PLATFORM:'offscreen',RAISING_QT_TEST:'1',RAISING_EDITOR_LANGUAGE:'en',RAISING_ENGINE_ROOT:path.resolve('.'),RAISING_NODE:process.execPath,RAISING_PROJECT_HISTORY:path.join(out,'editor-commands-history.ini')},executable('qt-editor-interaction-test'));
  await runQt('layout-diagnostics-authoring',{QT_QPA_PLATFORM:'offscreen',RAISING_QT_TEST:'1',RAISING_EDITOR_LANGUAGE:'en',RAISING_ENGINE_ROOT:path.resolve('.'),RAISING_NODE:process.execPath,RAISING_PROJECT_HISTORY:path.join(out,'layout-diagnostics-history.ini')},executable('qt-layout-diagnostics-test'));
  await runQt('readable-authoring',{QT_QPA_PLATFORM:'offscreen',RAISING_ENGINE_ROOT:path.resolve('.'),RAISING_NODE:process.execPath,RAISING_PROJECT_HISTORY:path.join(out,'readable-history.ini')},executable('qt-readable-layout-test'));
  await runQt('update-compatibility-authoring',{QT_QPA_PLATFORM:'offscreen',RAISING_ENGINE_ROOT:path.resolve('.'),RAISING_NODE:process.execPath,RAISING_PROJECT_HISTORY:path.join(out,'update-compatibility-history.ini')},executable('qt-update-compatibility-test'));
  await runQt('analysis-priorities-authoring',{QT_QPA_PLATFORM:'offscreen',RAISING_QT_TEST:'1',RAISING_EDITOR_LANGUAGE:'en',RAISING_ENGINE_ROOT:path.resolve('.'),RAISING_NODE:process.execPath,RAISING_PROJECT_HISTORY:path.join(out,'analysis-priorities-history.ini')},executable('qt-analysis-priorities-test'));
  await runQt('resume-context-authoring',{QT_QPA_PLATFORM:'offscreen',RAISING_QT_TEST:'1',RAISING_EDITOR_LANGUAGE:'en',RAISING_ENGINE_ROOT:path.resolve('.'),RAISING_NODE:process.execPath,RAISING_PROJECT_HISTORY:path.join(out,'resume-context-history.ini')},executable('qt-resume-context-test'));
  await runQt('route-batches-authoring',{QT_QPA_PLATFORM:'offscreen',RAISING_QT_TEST:'1',RAISING_EDITOR_LANGUAGE:'en',RAISING_ENGINE_ROOT:path.resolve('.'),RAISING_NODE:process.execPath,RAISING_PROJECT_HISTORY:path.join(out,'route-batches-history.ini')},executable('qt-route-batches-test'));
  await run('story-form',[],{},executable('qt-story-flow-dialog-test'));
  const fixture=path.join(out,'diagnostic-game');
  await run('diagnostic-fixture',['tests/qt-studio/validation-location-fixture.mjs',fixture]);
  await runQt('diagnostic-form',{RAISING_VALIDATION_PLAN:fixture+'-plan.json',RAISING_ENGINE_ROOT:path.resolve('.'),RAISING_NODE:process.execPath,RAISING_PROJECT_HISTORY:path.join(out,'diagnostic-history.ini')},executable('qt-validation-location-test'));
}
if(args.includes('--package')) {
  if(process.platform!=='darwin')throw Error('--package currently validates a macOS editor app; use test:qt-package for the Windows installation gate.');
  const app=path.resolve(args[args.indexOf('--package')+1]),engine=path.join(app,'Contents/Resources/engine');
  await run('standalone',['scripts/test-macos-package.mjs',app]);
  await run('packaged-editor',['scripts/test-first-game-ui.mjs'],{RAISING_ENGINE_ROOT:engine,RAISING_NODE:path.join(engine,'node')});
}
console.log('Creator smoke report: '+out);
