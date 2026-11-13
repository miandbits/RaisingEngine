// Return a completed old passive snapshot after newer foreground replies.
// This exercises both lane independence and the UI's stale-reply guard.
import {createInterface} from 'node:readline';
import {fileURLToPath} from 'node:url';
const entry=process.argv[1];process.argv[1]=fileURLToPath(import.meta.url);
const {QtStudioService}=await import('../../apps/qt-studio/service.mjs');process.argv[1]=entry;
const service=new QtStudioService({shareViews:true}),pending=new Set();
for await(const line of createInterface({input:process.stdin,crlfDelay:Infinity})){
 let request;try{
  request=JSON.parse(line);const result=await service.handle(request),reply=JSON.stringify({id:request.id,...result})+'\n';
  if(request.method==='preview'&&request.params?.authoringContext?.canvasContext){const timer=setTimeout(()=>{pending.delete(timer);process.stdout.write(reply);},1200);pending.add(timer);}
  else process.stdout.write(reply);
 }catch(error){process.stdout.write(JSON.stringify({id:request?.id??null,...service.editor.reply(error.message)})+'\n');}
}
for(const timer of pending)clearTimeout(timer);
await service.releases.close();await service.editor.close();process.exit(0);
