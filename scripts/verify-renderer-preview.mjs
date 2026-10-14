import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,writeFile,stat} from 'node:fs/promises';
import path from 'node:path';
import {spawnSync} from 'node:child_process';

const latest=JSON.parse(await readFile('.cache/components/latest.json','utf8'));
await mkdir('.cache/renderer-preview',{recursive:true});
const work=await mkdtemp(path.resolve('.cache/renderer-preview/run-')),bundleRoot=path.join(work,'game');
await cp(latest.windows,bundleRoot,{recursive:true});
// Use the newly built runtime with an isolated copy of the previously validated bundle.
const executable=path.resolve('dist/native-player-app/RaisingPlayer.exe');
const bundle=JSON.parse(await readFile(path.join(bundleRoot,'game.json'),'utf8'));
bundle.content.project.visual.screens.push({id:'renderer-probe',label:'Renderer probe',elements:[
  {id:'long-text',kind:'text',x:20,y:20,width:120,height:20,fontSize:32,text:'Large text that wraps onto several lines and is clipped.',color:'#ffffff'},
  {id:'probe-button',kind:'button',x:20,y:100,width:300,height:70,fontSize:24,text:'Preview only',action:'navigate',screenId:'home',fill:'#335577',hoverFill:'#226644',pressedFill:'#884422',disabledColor:'#778899'},
  {id:'covered-button',kind:'button',x:400,y:100,width:300,height:70,text:'Covered',action:'navigate',screenId:'home'},
  {id:'cover',kind:'panel',x:400,y:100,width:300,height:70,fill:'#772233'},
  {id:'probe-locations',kind:'locations',x:20,y:300,width:500,height:500},
]});
await writeFile(path.join(bundleRoot,'game.json'),JSON.stringify(bundle));
const save=path.join(work,'sentinel-save.json'),sentinel='do not replace this save';await writeFile(save,sentinel);
function run(label,args,steps){
  const stepsFile=path.join(work,`${label}.steps.json`),reportFile=path.join(work,`${label}.report.json`);
  return (async()=>{await writeFile(stepsFile,JSON.stringify(steps));const child=spawnSync(executable,['--bundle',bundleRoot,'--save',save,'--hidden','--automation',stepsFile,'--report',reportFile,...args],{encoding:'utf8',windowsHide:true,timeout:60000});
    assert.equal(child.status,0,child.stderr||child.error?.message);return JSON.parse(await readFile(reportFile,'utf8'));})();
}
const shared=await run('shared',['--preview-screen','component-gallery'],[{waitFrames:4},{checkpoint:'preview'},{screenshot:path.join(work,'shared.png')}]);
assert.equal(shared.final.screenId,'component-gallery');assert.equal(shared.final.preview.active,true);
assert.ok(shared.final.nodes.some(n=>n.id==='card__confirm'&&n.hitbox));
assert.equal(shared.final.state.gameplay.screenId,'home','preview must not rewrite simulation screen');
const ko=await run('korean',['--preview-screen','title','--locale','ko'],[{waitFrames:4},{checkpoint:'preview'}]);
const en=await run('english',['--preview-screen','title','--locale','en'],[{waitFrames:4},{checkpoint:'preview'},{screenshot:path.join(work,'english.png')}]);
assert.equal(ko.final.locale,'ko');assert.equal(en.final.locale,'en');
assert.equal(ko.final.elements.find(e=>e.id==='start').label,'새 게임 시작');
assert.equal(en.final.elements.find(e=>e.id==='start').label,'New game');
assert.notEqual(ko.final.elements.find(e=>e.id==='start').label,en.final.elements.find(e=>e.id==='start').label);
assert.equal(ko.final.state,null);assert.equal(en.final.state,null);assert.equal(await readFile(save,'utf8'),sentinel);
assert.ok(en.final.presentation.pixelWidth>0);assert.ok(en.final.presentation.width>0);
const states={};
for(const state of ['normal','hover','pressed','focus','disabled']){
  const report=await run(state,['--preview-screen','renderer-probe','--preview-element','probe-button','--preview-state',state],[
    {waitFrames:4},{checkpoint:'before'},{screenshot:path.join(work,`${state}.png`)},
    {key:'F5'},{key:'F9'},{click:'probe-button'},{lifecycle:['background','foreground']},{restoreAutosave:true},{waitFrames:3},{checkpoint:'after'},
    {resize:{width:1600,height:720}},{waitFrames:5},{checkpoint:'wide'},
  ]);
  const node=report.final.nodes.find(n=>n.id==='probe-button');states[state]=node;
  if(state==='focus')assert.equal(report.final.focusedId,'probe-button');
  assert.equal(report.final.nodes.find(n=>n.id==='covered-button').occludedAtCenter,true);
  const widget=report.final.nodes.find(n=>n.id==='probe-locations'),children=report.final.nodes.filter(n=>n.ownerId==='probe-locations');
  assert.ok(children.length>=2);assert.equal('enabled' in widget,false);assert.equal('hitbox' in widget,false);
  assert.ok(children.every(n=>n.kind==='runtime-button'&&n.buttonId===n.id));
  assert.ok(children.some(n=>n.enabled));assert.ok(children.some(n=>!n.enabled));
  assert.equal(node.occludedAtCenter,false);assert.equal(node.clickableAtCenter,state!=='disabled');
  assert.deepEqual(report.steps.find(s=>s.command.checkpoint==='before').ui.state,report.final.state);
  assert.equal(report.final.screenId,'renderer-probe');assert.equal(report.final.lifecycleSaves,0);
  assert.equal(report.final.nodes.find(n=>n.id==='long-text').overflowY,true);
  assert.equal(report.final.nodes.find(n=>n.id==='long-text').textMeasured,true);
  assert.equal(await readFile(save,'utf8'),sentinel);
}
assert.notEqual(states.normal.backgroundColor,states.hover.backgroundColor);
assert.notEqual(states.hover.backgroundColor,states.pressed.backgroundColor);
assert.equal(states.disabled.enabled,false);assert.equal(states.normal.enabled,true);
assert.notEqual(states.disabled.color,states.normal.color);
const runtime=await run('runtime-focus',['--preview-screen','renderer-probe','--preview-element','visit-city','--preview-state','focus'],[{waitFrames:4},{checkpoint:'preview'}]);
assert.equal(runtime.final.focusedId,'visit-city');
await assert.rejects(stat(path.join(work,'autosave.json')));
const badReport=path.join(work,'bad.report.json');
const bad=spawnSync(executable,['--bundle',bundleRoot,'--hidden','--preview-screen','missing','--report',badReport],{encoding:'utf8',windowsHide:true,timeout:30000});
assert.notEqual(bad.status,0);assert.match(JSON.parse(await readFile(badReport,'utf8')).error,/Unknown preview screen/);
await writeFile('.cache/renderer-preview/latest-native.json',JSON.stringify({work,ok:true},null,2));
console.log(JSON.stringify({ok:true,work,sharedNodes:true,realStyleStates:true,textOverflow:true,isolatedSave:true,screenValidation:true,locales:true,compositeButtons:true,runtimeButtonFocus:true}));
