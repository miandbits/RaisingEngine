import test from 'node:test';
import assert from 'node:assert/strict';
import {EventEmitter} from 'node:events';
import {screenshotLaunchOptions,screenshotDiagnostics} from './chromium-screenshot.mjs';

test('upstream profiler switch is scoped, preserves options and does not mutate caller arguments',()=>{
 const args=['--example'],options={channel:'chromium',headless:true,args};
 const result=screenshotLaunchOptions(options,'win32');
 assert.deepEqual(result,{...options,args:['--example','--disable-stack-profiler']});
 assert.deepEqual(args,['--example']);
 assert.equal(screenshotLaunchOptions(options,'linux'),options);
 assert.equal(screenshotLaunchOptions({...options,channel:'chrome'},'win32').args,args);
 assert.equal(screenshotLaunchOptions({...options,channel:undefined},'win32').args,args);
 const existing={channel:'chromium',args:['--disable-stack-profiler']};
 assert.equal(screenshotLaunchOptions(existing,'win32'),existing);
 assert.deepEqual(screenshotLaunchOptions({channel:'chromium'},'win32').args,['--disable-stack-profiler']);
});

test('screenshot diagnostic logs only on failure and rethrows the original error',async()=>{
 const browser=new EventEmitter(),context=new EventEmitter(),page=new EventEmitter();
 browser.isConnected=()=>false;page.isClosed=()=>true;
 const failure=new Error('original\n<launched> pid=123\n<process did exit: exitCode=null, signal=SIGABRT>'),options={path:'fixture.png'};let fail=false,received;
 page.screenshot=async value=>{received=value;if(fail)throw failure;return 'image';};
 const screenshot=screenshotDiagnostics(browser,context,page),messages=[],original=console.error;
 console.error=message=>messages.push(message);
 try{
  assert.equal(await screenshot('success',options),'image');assert.equal(received,options);assert.deepEqual(messages,[]);
  page.emit('crash');context.emit('close');browser.emit('disconnected');fail=true;
  await assert.rejects(screenshot('wrapped',options),error=>error===failure);
  assert.equal(messages.length,1);
  const diagnostic=JSON.parse(messages[0].slice('Screenshot lifecycle: '.length));
  assert.equal(diagnostic.stage,'wrapped');assert.deepEqual(diagnostic.events.map(e=>e.event),['page.crash','context.close','browser.disconnected']);
  assert.equal(diagnostic.pid,123);assert.equal(diagnostic.exitCode,'null');assert.equal(diagnostic.signal,'SIGABRT');
  assert.equal(diagnostic.pageClosed,true);assert.equal(diagnostic.browserConnected,false);
 }finally{console.error=original;}
});

if(process.env.RAISING_VERIFY_WINDOWS_SCREENSHOT_BROWSER==='1')test('Windows bundled Chromium launch applies the upstream profiler switch',{timeout:30000},async()=>{
 assert.equal(process.platform,'win32','This control must run on Windows');
 const {chromium}=await import('playwright');
 const browser=await chromium.launch(screenshotLaunchOptions({channel:'chromium',headless:true}));
 try{
  const version=browser.version(),session=await browser.newBrowserCDPSession();
  const {arguments:args}=await session.send('Browser.getBrowserCommandLine');
  const count=args.filter(arg=>arg==='--disable-stack-profiler').length;
  console.log(JSON.stringify({version,profilerSwitchCount:count}));
  assert.equal(version,'156.0.8078.4');assert.equal(count,1);
 }finally{await browser.close();}
});
