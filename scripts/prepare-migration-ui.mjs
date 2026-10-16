import assert from 'node:assert/strict';
import { cp,mkdir,mkdtemp,writeFile } from 'node:fs/promises';
import { resolve,join } from 'node:path';
import { loadProject,buildSnapshot } from '../dist/packages/project-service/index.js';
import { createSession,dispatchSession } from '../dist/packages/runtime-service/session.js';
import { saveRun } from '../dist/packages/runtime-service/save.js';
const evidence=resolve('.cache/durability');await mkdir(evidence,{recursive:true});const root=await mkdtemp(join(evidence,'migration-'));
const from=join(root,'before'),to=join(root,'after'),save=join(root,'original.json'),output=join(root,'migrated.json');
await cp(resolve('examples/academy-demo'),from,{recursive:true});await cp(from,to,{recursive:true});
const old=await buildSnapshot(await loadProject(from));assert.ok(old.ok);
let run=createSession(old.snapshot,42);assert.ok(run.ok);
for(const command of [{type:'setSchedule',activityIds:['rest','rest','rest']},{type:'executeActivity'}]){run=dispatchSession(old.snapshot,run.session,command,run.session.state.stateRevision);assert.ok(run.ok);}
assert.ok((await saveRun(save,old.snapshot,run.session)).ok);
const loaded=await loadProject(to),doc=loaded.documents.find(d=>d.kind==='attributes');doc.data.attributes.find(a=>a.id==='money').initial+=10;await writeFile(join(to,doc.file),JSON.stringify(doc.data,null,2));
await writeFile(join(evidence,'migration-ui.json'),JSON.stringify({from,to,save,output}));
console.log(join(evidence,'migration-ui.json'));
