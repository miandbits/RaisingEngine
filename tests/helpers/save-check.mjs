import {setTimeout as delay} from 'node:timers/promises';

// Tests that need completed comparisons explicitly wait; the save API itself
// intentionally returns before long replay or comparison work finishes.
export async function saveAndWait(service,{timeoutMs=120000}={}){
  const reply=await service.handle({method:'save',params:{revision:service.editor.revision}});
  if(!reply.goalSaveCheck)return reply;
  const root=service.editor.root,deadline=Date.now()+timeoutMs;
  let check=reply.goalSaveCheck;
  while(['queued','running','cancelling'].includes(check.status)){
    if(Date.now()>=deadline)throw Error('Saved checks did not finish: '+JSON.stringify(check));
    await delay(20);
    check=(await service.handle({method:'goalSaveCheckStatus',params:{root}})).goalSaveCheck;
    if(!check)throw Error('Saved check disappeared before completion.');
  }
  return {...reply,goalSaveCheck:check,goalReport:check,goalImpact:check.goalImpact};
}
