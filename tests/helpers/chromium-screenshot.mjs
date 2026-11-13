// Playwright 1.64.0's bundled Chrome for Testing can crash in the Windows
// stack sampling profiler. Upstream added this switch by default in #43364.
// Keep this scoped to the two suites where the screenshot failure was observed.
export function screenshotLaunchOptions(options, platform = process.platform) {
 if(platform !== 'win32' || options.channel !== 'chromium' || options.args?.includes('--disable-stack-profiler')) return options;
 return {...options, args:[...(options.args ?? []), '--disable-stack-profiler']};
}

// Only emit bounded lifecycle evidence on a screenshot failure; preserve the
// original exception and leave the caller's normal cleanup untouched.
export function screenshotDiagnostics(browser, context, page) {
 const events=[];
 for(const [emitter,event,label] of [[page,'crash','page.crash'],[page,'close','page.close'],[context,'close','context.close'],[browser,'disconnected','browser.disconnected']])
  emitter.on(event,()=>{if(events.length<16)events.push({event:label,at:new Date().toISOString()});});
 return async (stage,options)=>{
  try{return await page.screenshot(options);}
  catch(error){
   try{
    const message=String(error?.message ?? ''),pid=message.match(/<launched> pid=(\d+)/)?.[1],exit=message.match(/<process did exit: exitCode=(-?\d+|null), signal=([A-Z0-9]+|null)>/);
    console.error('Screenshot lifecycle: '+JSON.stringify({stage,events,pageClosed:page.isClosed(),browserConnected:browser.isConnected(),...(pid?{pid:Number(pid)}:{}),...(exit?{exitCode:exit[1],signal:exit[2]}:{})}));
   }catch{}
   throw error;
  }
 };
}
