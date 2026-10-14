import { StudioEditor } from '../../../packages/studio-service/editor.js';
import { AnalysisJobs } from '../../../packages/analysis-service/jobs.js';
import type { AnalysisOptions } from '../../../packages/analysis-service/model.js';
const [root, command = 'start', value = '{}'] = process.argv.slice(2);
try {
  if (!root) throw new Error('Usage: analyze <project> start [JSON options] | list | status <id> | cancel <id>');
  const jobs = new AnalysisJobs();
  if (command === 'list') console.log(JSON.stringify(await jobs.list(root), null, 2));
  else if (command === 'status') console.log(JSON.stringify(await jobs.get(root, value), null, 2));
  else if (command === 'cancel') console.log(JSON.stringify(await jobs.cancel(root, value), null, 2));
  else if (command === 'start') {
    const editor = new StudioEditor(); await editor.open(root, true); const snapshot = await editor.snapshot(editor.revision);
    if (!snapshot.ok) throw new Error(JSON.stringify(snapshot.diagnostics));
    const options: AnalysisOptions = { runs:100,seed:42,strategy:'random',maxCommands:2000,timeoutMs:60000,...JSON.parse(value) };
    let job = await jobs.start(root,snapshot.snapshot,editor.diskVersion(),null,options);
    console.error(`analysis ${job.id}`);
    process.once('SIGINT',()=>{void jobs.cancel(root,job.id);});
    while(job.status==='running'){await new Promise(r=>setTimeout(r,500));job=await jobs.get(root,job.id);}
    console.log(JSON.stringify(job,null,2)); if(job.status==='error')process.exitCode=1;
  } else throw new Error('Unknown command');
} catch(error){console.error(error instanceof Error?error.message:String(error));process.exitCode=1;}
