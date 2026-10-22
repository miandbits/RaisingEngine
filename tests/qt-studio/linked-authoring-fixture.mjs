import fs from 'node:fs/promises';
import path from 'node:path';
const root=path.resolve(process.argv[2]??'.cache/linked-authoring-project');
try{await fs.access(root);throw Error('Use a new fixture directory');}catch(e){if(e.code!=='ENOENT')throw e;}
await fs.cp('tests/fixtures/projects/time-session-demo',root,{recursive:true});
const file=path.join(root,'project.json'),p=JSON.parse(await fs.readFile(file,'utf8'));
p.visual.screens.find(s=>s.id==='schedule').elements.push({id:'linked-gift',kind:'button',label:'Gift',text:'Gift',x:800,y:10,width:150,height:45,action:'executeAction',actionId:'action-1',actorId:'apprentice'});
await fs.writeFile(file,JSON.stringify(p,null,2));
await fs.writeFile(root+'-plan.json',JSON.stringify({project:root,screen:'schedule',element:'linked-gift',evidence:path.resolve('.cache/linked-authoring-evidence'),steps:[
  {target:'action:action-1',field:'workspaceField-name',text:'A thoughtful gift'},
  {target:'relationship:mentor/trust',field:'workspaceField-initial',number:1},
  {target:'action:action-1',logic:{button:'workspaceLogic-effects',path:['1','value'],number:16}},
  {target:'event:personal-gift'}
]},null,2));
console.log(root+'-plan.json');
