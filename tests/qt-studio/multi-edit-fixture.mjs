// Creates a disposable, public-content Qt authoring test project and UI plan.
import {cp,readFile,writeFile,mkdir,access} from 'node:fs/promises';
import {resolve} from 'node:path';
const root=resolve(process.argv[2]??'.cache/multi-edit-fixture');
try{await access(root);throw Error('Refusing to overwrite '+root);}catch(e){if(e.code!=='ENOENT')throw e;}
await mkdir(root,{recursive:true});await cp(resolve('tests/fixtures/projects/character-state-demo'),root,{recursive:true});
const file=resolve(root,'project.json'),p=JSON.parse(await readFile(file,'utf8'));
const text=(id,x,y)=>({id,kind:'text',text:id,x,y,width:200,height:40});
p.visual.components??=[];p.visual.components.push({id:'shared-card',label:'Shared card',width:300,height:150,elements:[{...text('title',20,20),fontSize:17},{...text('details',20,70),fontSize:21}]});
p.visual.screens.push({id:'multi-home',label:'Multi edit',elements:[text('a',20,20),text('b',250,70),text('c',500,120),{id:'card',kind:'component',componentId:'shared-card',x:100,y:250,width:300,height:150}]},{id:'multi-other',label:'Second use',elements:[{id:'card',kind:'component',componentId:'shared-card',x:100,y:100,width:300,height:150,overrides:[{elementId:'title',values:{fontSize:25}}]}]});
await writeFile(file,JSON.stringify(p,null,2)+'\n');
await writeFile(root+'-plan.json',JSON.stringify({root,evidence:root+'-evidence',edits:[{screen:'multi-home',ids:['a','b','c'],operation:'move',dx:8},{screen:'multi-home',ids:['a','b','c'],operation:'y:start',patch:{fontSize:22}}],component:{screen:'multi-home',instance:'card',label:'Shared card',ids:['title','details'],patch:{fontSize:19},expectations:[{screen:'multi-home',id:'card__title',fontSize:19},{screen:'multi-other',id:'card__details',fontSize:19},{screen:'multi-other',id:'card__title',fontSize:25}]}},null,2));
console.log(root+'-plan.json');
