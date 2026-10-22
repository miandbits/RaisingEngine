// Fault injection is confined to a disposable copy; repairs are performed by Qt widgets.
import {cp,readFile,writeFile,mkdir,access,readdir} from 'node:fs/promises';
import {resolve} from 'node:path';
const root=resolve(process.argv[2]??'.cache/validation-location-fixture');
const source=resolve(process.argv[3]??'tests/fixtures/projects/character-state-demo');
try{await access(root);throw Error('Refusing to overwrite '+root);}catch(e){if(e.code!=='ENOENT')throw e;}
await mkdir(root,{recursive:true});await cp(source,root,{recursive:true,filter:p=>!p.includes('.studio')});
const files={};const read=async file=>files[file]??=(JSON.parse(await readFile(resolve(root,file),'utf8')));
await read('project.json');
async function pick(folder,predicate){
  for(const name of (await readdir(resolve(root,folder))).filter(n=>n.endsWith('.json')).sort()){
    const file=folder+'/'+name,data=JSON.parse(await readFile(resolve(root,file),'utf8'));
    if(predicate(data))return file;
  }
  throw Error('No suitable test document in '+folder);
}
const eventFile=await pick('events',d=>d.window&&typeof d.nodes?.[0]?.next==='string');
const activityFile=await pick('activities',d=>d.costs?.[0]?.attributeId);
const event=await read(eventFile),activity=await read(activityFile);
const repairs=[];
function fault(file,pointer,bad){const parts=pointer.slice(1).split('/');let owner=files[file];for(const p of parts.slice(0,-1))owner=owner[p];const key=parts.at(-1);repairs.push({file,pointer,value:owner[key]});owner[key]=bad;}
fault('project.json','/visual/screens/0/elements/0/width',-1);
fault(eventFile,'/window/throughPeriod',Math.max(0,event.window.fromPeriod-1));
// Period zero needs a reversed start instead of a negative end (a semantic error).
if(event.window.fromPeriod===0){event.window.throughPeriod=repairs.pop().value;fault(eventFile,'/window/fromPeriod',1);repairs.at(-1).diagnosticPointer='/window/throughPeriod';}
fault(eventFile,'/nodes/0/next','missing-validation-node');
fault(activityFile,'/costs/0/attributeId','missing-validation-stat');
for(const [file,data] of Object.entries(files))await writeFile(resolve(root,file),JSON.stringify(data,null,2)+'\n');
const plan={root,source,evidence:root+'-evidence',repairs};
await writeFile(root+'-plan.json',JSON.stringify(plan,null,2)+'\n');console.log(root+'-plan.json');
