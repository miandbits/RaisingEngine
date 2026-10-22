// An isolated public-content fixture; never point the Qt test at a user's project.
import {cp,readFile,writeFile,mkdir,access} from 'node:fs/promises';
import {resolve} from 'node:path';
import {execFileSync} from 'node:child_process';

const root=resolve(process.argv[2]??'.cache/editor-basics-project');
try {await access(root);throw new Error(`Fixture already exists: ${root}`);} catch(error){if(error.code!=='ENOENT')throw error;}
await mkdir(root,{recursive:true});
await cp(resolve('tests/fixtures/projects/character-state-demo'),root,{recursive:true});
const file=resolve(root,'project.json'),p=JSON.parse(await readFile(file,'utf8'));
const text=(id,parentId)=>({id,kind:'text',text:id,x:20,y:20,width:200,height:40,...(parentId?{parentId}:{})});
p.visual.screens.push({id:'home',label:'Editing basics',elements:[
 {id:'group-a',kind:'group',label:'UI group',x:100,y:80,width:600,height:400},
 text('child-a','group-a'),text('root-b'),{...text('child-b','group-a'),y:90}
]},{id:'other',label:'Other screen',elements:[text('existing')]});
await writeFile(file,JSON.stringify(p,null,2)+'\n');
const git=args=>execFileSync('git',args,{cwd:root,stdio:'pipe'});
git(['init']);git(['add','.']);git(['-c','user.name=Editor Test','-c','user.email=editor-test@example.invalid','commit','-qm','Fixture baseline']);
console.log(root);
