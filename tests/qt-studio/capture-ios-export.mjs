// Qt UI-boundary fixture only: real project/edit methods, captured iOS export request.
// Start an explicit bridge loop so importing the normal CLI does not enter its loop
// before this test hook is installed. No Xcode project is needed for a params check.
import {appendFile} from 'node:fs/promises';
import {createInterface} from 'node:readline';
const entry=process.argv[1];process.argv[1]=new URL(import.meta.url).pathname;
const {QtStudioService}=await import('../../apps/qt-studio/service.mjs');
process.argv[1]=entry;
const service=new QtStudioService({shareViews:true}),dispatch=service.dispatch.bind(service);
service.dispatch=async request=>{
 if(request.method==='export'&&request.params?.target==='ios'){
  await appendFile(process.env.RAISING_TEST_IOS_EXPORT_CAPTURE,JSON.stringify(request.params)+'\n');
  return {error:null,exported:{target:'ios',directory:request.params.destination,contentFingerprint:'test-captured-request'}};
 }
 return dispatch(request);
};
for await(const line of createInterface({input:process.stdin,crlfDelay:Infinity})){
 let request;try{request=JSON.parse(line);const result=await service.handle(request);process.stdout.write(JSON.stringify({id:request.id,...result})+'\n');}
 catch(error){process.stdout.write(JSON.stringify({id:request?.id??null,...service.editor.reply(error.message)})+'\n');}
}
await service.releases.close();await service.editor.close();
process.exit(0);
