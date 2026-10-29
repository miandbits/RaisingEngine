// Actual service checks and token verification, with only packaging replaced by
// a parameter capture. Packaging and canonical-save replay have service tests.
import {appendFile} from 'node:fs/promises';
import {createInterface} from 'node:readline';
import {fileURLToPath} from 'node:url';
const entry=process.argv[1];process.argv[1]=fileURLToPath(import.meta.url);
const {QtStudioService}=await import('../../apps/qt-studio/service.mjs');
process.argv[1]=entry;
const service=new QtStudioService({shareViews:true}),dispatch=service.dispatch.bind(service);
service.dispatch=async request=>{
 if(request.method==='export'){
  if(request.params.presentationCheck)await service.presentationUpdates.verify(service,request.params.presentationCheck,{revision:request.params.revision,target:request.params.target});
  await appendFile(process.env.RAISING_TEST_PRESENTATION_CAPTURE,JSON.stringify({method:request.method,params:request.params})+'\n');
  return {error:null,exported:{target:request.params.target,directory:request.params.destination,contentFingerprint:'ui-boundary-capture'}};
 }
 const result=await dispatch(request);
 if(request.method==='presentationUpdateCheck'){
  await appendFile(process.env.RAISING_TEST_PRESENTATION_CAPTURE,JSON.stringify({method:request.method,params:request.params,report:result.presentationUpdate})+'\n');
  // Makes cancellation and changed-input delivery races deterministic in Qt.
  await new Promise(resolve=>setTimeout(resolve,100));
 }
 return result;
};
for await(const line of createInterface({input:process.stdin,crlfDelay:Infinity})){
 let request;try{request=JSON.parse(line);const result=await service.handle(request);process.stdout.write(JSON.stringify({id:request.id,...result})+'\n');}
 catch(error){process.stdout.write(JSON.stringify({id:request?.id??null,...service.editor.reply(error.message)})+'\n');}
}
await service.releases.close();await service.editor.close();process.exit(0);
