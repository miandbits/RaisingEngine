// Disposable projects and an actual canonical save for the Qt review boundary.
import {cp,readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';
import {updateFixture} from './update-compatibility-fixture.mjs';
import {loadProject,buildSnapshot} from '../../dist/packages/project-service/index.js';
const root=path.resolve(process.argv[2]),f=await updateFixture(root);
const built=await buildSnapshot(await loadProject(f.from));
if(!built.ok)throw Error(JSON.stringify(built));
const project=JSON.parse(await readFile(path.join(f.from,'project.json'),'utf8'));
const button=project.visual.screens[0].elements.find(e=>e.kind==='button');
button.text='Begin this new chapter';button.x+=4;button.fill='#254358';
await writeFile(path.join(f.to,'project.json'),JSON.stringify(project,null,2));
const previousBundle=path.join(root,'game.json');
await writeFile(previousBundle,JSON.stringify({formatVersion:'1',content:built.snapshot,assets:{}}));
const functional=path.join(root,'functional');await cp(f.to,functional,{recursive:true});
project.durationMonths+=1;await writeFile(path.join(functional,'project.json'),JSON.stringify(project,null,2));
const invalidSave=path.join(root,'invalid-save.json');await writeFile(invalidSave,'{"corrupt":true}');
console.log(JSON.stringify({...f,previousBundle,functional,invalidSave}));
