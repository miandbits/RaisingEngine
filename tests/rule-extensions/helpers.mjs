import {cp,mkdir,readFile,writeFile,mkdtemp} from 'node:fs/promises';
import {resolve} from 'node:path';
export const json=async file=>JSON.parse(await readFile(file,'utf8'));
export async function baseProject(parent='.cache/rule-extensions') {
  await mkdir(parent,{recursive:true});const root=await mkdtemp(resolve(parent,'game-'));
  await cp('examples/academy-demo',root,{recursive:true});
  const p=await json(root+'/project.json');
  p.gameId='rule-extension-lab';p.title='무공 규칙 확장 실습';p.durationMonths=2;p.periodsPerMonth=3;
  p.gameplay={startLocationId:'home',locations:[{id:'home',name:'수련장',connections:[]}],items:[],shops:[],interactions:[],actions:[]};
  const visual=(await json('examples/novice-three-month-trial/project.json')).visual;
  visual.components=visual.components.filter(c=>c.id==='save-controls');
  visual.screens[0].elements=visual.screens[0].elements.filter(e=>e.kind!=='component');
  const screen=visual.screens.find(s=>s.id==='schedule');
  for(const e of screen.elements) {if(e.id==='stats'){e.x=960;e.y=120;e.width=280;}if(e.id==='schedule'){e.x=40;e.y=520;e.width=760;e.height=100;}}
  p.visual=visual;await writeFile(root+'/project.json',JSON.stringify(p,null,2));
  return root;
}
export async function addRules(root) {
  const project=await json(root+'/project.json');
  project.ruleExtensions={apiVersion:1,packages:[await json('examples/rule-extension-lab/dojo.raiserules')],progressions:[{id:'flow-1',name:'막내의 돌파 수련',packageId:'dojo',version:'1.0.0',ruleId:'training-cycle',scope:'shared',args:{stat:'skill',gain:12}}]};
  await writeFile(root+'/project.json',JSON.stringify(project,null,2));return project;
}
