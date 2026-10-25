import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile,cp,rm} from 'node:fs/promises';
import {existsSync} from 'node:fs';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {desktopTarget,gameDirectory,playerExecutable} from '../../dist/packages/export-service/targets.js';
import {resolveResumeContext} from '../../dist/packages/visual/resume-context.js';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/index.js';
import {encodeSave} from '../../dist/packages/runtime-service/save.js';
const touch=id=>({scrollIntoView:id,touchWindow:id,waitFrames:3});
const at=(r,id)=>r.steps.find(s=>s.command.checkpoint===id)?.ui;
const same=(a,b)=>{assert.deepEqual(a.state,b.state);assert.deepEqual(a.commands,b.commands);assert.deepEqual(a.scheduleDraft,b.scheduleDraft);};
const leaf=(id,extra={})=>({id,kind:'text',x:24,y:50,width:1180,height:480,text:id,...extra});
const button=(id,action,extra={})=>leaf(id,{kind:'button',x:24,y:620,width:250,height:52,action,autoLabel:false,...extra});
const layout=(items,actions)=>({items:items.map(elementId=>({elementId})),actions,actionLayout:'grouped',primaryAction:actions[0]});
const compiler=()=>process.env.CXX??(process.platform==='win32'?(existsSync(join(process.env.QT_COMPILER??'C:/Program Files/mingw64/bin','g++.exe'))?join(process.env.QT_COMPILER??'C:/Program Files/mingw64/bin','g++.exe'):'g++'):'c++');
const outdir=async prefix=>{await mkdir('.cache/native-resume-context',{recursive:true});return mkdtemp(resolve('.cache/native-resume-context/'+prefix+'-'));};
test('native current reminders exactly match shared order, inherited visibility, variants and UTF-8 limits',{timeout:90000},async()=>{
 const out=await outdir('parity'),source=join(out,'parity.cpp'),exe=join(out,process.platform==='win32'?'parity.exe':'parity');
 await writeFile(source,`#include "resume-context.hpp"
#include "presentation-variants.hpp"
#include <iostream>
int main(){Json cases;std::cin>>cases;Json out=Json::array();for(const auto& c:cases){auto evaluate=[](const Json& condition){return condition.value("op","")=="always";};out.push_back(raising_player::resolve_resume_context(c.at("visual"),c.at("phase"),evaluate,[](const Json& e){return e.value("text","");},[&](const Json& e){return raising_player::presentation_variant(e,"",evaluate);}));}std::cout<<out.dump();}`);
 const built=spawnSync(compiler(),['-std=c++17','-O1','-I',resolve('native/player'),'-I',resolve('native/vendor'),'-I',resolve('native/include'),source,'-o',exe],{encoding:'utf8',timeout:60000});assert.equal(built.status,0,built.stderr||String(built.error));
 const design={width:1280,height:720,resumeContext:{screenId:'recap',elementIds:['second','first','hidden','zero','false','wrong','unsafe','delta','grouped','card__copy']},screens:[{id:'recap',label:'Recap',elements:[leaf('first'),leaf('second'),leaf('hidden',{hidden:true}),leaf('zero',{opacity:0}),leaf('false',{visibleCondition:{op:'never'}}),leaf('wrong',{visibleWhen:'choice'}),leaf('unsafe',{binding:'story'}),leaf('delta',{binding:'relationship-value',valueFormat:'{delta}'}),leaf('group',{kind:'group',width:10,height:10,clipChildren:true,visibleWhen:'planning'}),leaf('grouped',{parentId:'group',x:400,text:'Outside canvas clip but explicitly selected'}),leaf('card',{kind:'component',componentId:'card'})]}],components:[{id:'card',label:'Card',width:400,height:200,elements:[leaf('copy',{text:'Original',variants:[{when:{op:'always'},values:{text:'約束 한글 🌅 & <visible>'}}]})]}]};
 const cases=['planning','ready','activity-result','period-end','choice'].map(phase=>({visual:structuredClone(design),phase}));
 const long=structuredClone(design);long.screens[0].elements=Array.from({length:64},(_,i)=>leaf('e'+i,{text:'界🙂'.repeat(1000)}));long.resumeContext.elementIds=long.screens[0].elements.map(e=>e.id);cases.push({visual:long,phase:'planning'});
 for(const mode of ['hidden','opacity','condition']){const d=structuredClone(design),g=d.screens[0].elements.find(e=>e.id==='group');if(mode==='hidden')g.hidden=true;else if(mode==='opacity')g.opacity=0;else g.visibleCondition={op:'never'};cases.push({visual:d,phase:'planning'});}
 const empty=structuredClone(design);delete empty.resumeContext;cases.push({visual:empty,phase:'planning'});
 const expected=cases.map(c=>resolveResumeContext(c.visual,{phase:c.phase,evaluate:c=>c.op==='always',text:e=>e.text??''}));
 const run=spawnSync(exe,[],{input:JSON.stringify(cases),encoding:'utf8',timeout:30000});assert.equal(run.status,0,run.stderr);assert.deepEqual(JSON.parse(run.stdout),expected);assert.equal(expected[0].entries.at(-1).text,'約束 한글 🌅 & <visible>');assert.ok(expected[5].omittedEntries>0);
 await writeFile(join(out,'result.json'),JSON.stringify({ok:true,cases:cases.length},null,2));
});
async function fixture(out,{automatic=true,reading=true,slots=false,opted=true,continuous=false}={}){
 const root=join(out,'project');await cp(resolve('examples/my-first-raising-game'),root,{recursive:true});await rm(join(root,'events'),{recursive:true,force:true});await mkdir(join(root,'events'));await rm(join(root,'scenarios'),{recursive:true,force:true});
 const file=join(root,'project.json'),p=JSON.parse(await readFile(file,'utf8'));for(const [id,label] of [['ja','日本語'],['ko','한국어']])if(!p.localization.locales.some(l=>l.id===id))p.localization.locales.push({id,label});
 if(continuous)p.schedulePlayback='continuous';
 p.userFields=[{id:'name',type:'text',label:'Name',initial:'Rowan',minLength:1,maxLength:64}];
 const utilities=[button('reminders','resumeContext'),button('save','save'),button('load','load'),button('restart','start')];
 const texts=[leaf('date',{binding:'calendar'}),leaf('month',{binding:'calendar-month'}),leaf('weekday',{binding:'calendar-weekday'}),leaf('identity',{binding:'user-field',userFieldId:'name',valueFormat:'Name: {value}'}),leaf('money',{binding:'attribute-value',attributeId:'money',valueFormat:'Money: {value}'}),leaf('before',{text:'No garden commitment yet.',visibleCondition:{op:'not',condition:{op:'flag',flagId:'garden-visited',equals:true}}}),leaf('chosen',{text:'You promised to tend the garden. 約束を守る。',visibleCondition:{op:'flag',flagId:'garden-visited',equals:true}}),leaf('note',{text:'Current responsibilities & <plain text>. '+('A careful reminder remains readable at your preferred size. '.repeat(18))})];
 p.visual={width:1280,height:720,readingHistory:{},...(opted?{resumeContext:{screenId:'recap',elementIds:texts.map(e=>e.id),...(automatic?{showAfterLoad:true}:{})}}:{}),...(reading?{readableLayout:{background:'#f5efdf'}}:{}),...(slots?{saveSystem:{manualSlots:3,autoSlots:2}}:{}),screens:[
 {id:'title',label:'Title',elements:[leaf('intro',{text:'Current reminders fixture'}),button('start','start'),button('load','load')],readableLayout:layout(['intro'],['start','load'])},
 {id:'schedule',label:'Plan',elements:[leaf('schedule',{kind:'schedule'}),button('advance','advance'),...utilities],readableLayout:layout(['schedule'],['advance',...utilities.map(e=>e.id)])},
 {id:'story',label:'Story',elements:[leaf('story',{kind:'dialogue'}),leaf('choices',{kind:'choices'}),button('advance','advance'),...utilities],readableLayout:layout(['story','choices'],['advance',...utilities.map(e=>e.id)])},
 {id:'ending',label:'Ending',elements:[leaf('end',{binding:'ending'}),...utilities],readableLayout:layout(['end'],utilities.map(e=>e.id))},
 {id:'recap',label:'Reminders source',elements:texts},
 ...(slots?[false,true].map(load=>({id:load?'load-records':'records',label:'Records',modal:true,elements:[leaf('slots',{kind:load?'loadSlots':'saveSlots'}),button('close','closeModal')],readableLayout:layout(['slots'],['close'])})):[])
 ]};
 if(!reading)for(const s of p.visual.screens)delete s.readableLayout;
 if(!opted)for(const s of p.visual.screens){s.elements=s.elements.filter(e=>e.action!=='resumeContext');if(s.readableLayout){s.readableLayout.actions=s.readableLayout.actions.filter(id=>id!=='reminders');s.readableLayout.primaryAction=s.readableLayout.actions[0];}}
 await writeFile(file,JSON.stringify(p));const built=await buildSnapshot(await loadProject(root));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
 const content=built.snapshot,target=desktopTarget(),destination=join(out,'game'),runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??'dist/native-player-app');await exportGame({projectRoot:root,content,target,destination,runtimes:{windows:runtime,macos:runtime,web:runtime}});
 return {content,root,target,destination,runtime,save:profile=>join(out,profile,'save.json'),run:async(name,steps,{profile='profile',scale=2,locale='en',extra=[]}={})=>{
 const directory=join(out,profile);await mkdir(directory,{recursive:true});const script=join(out,name+'.json'),report=join(out,name+'-report.json');await writeFile(script,JSON.stringify(steps));
 const r=spawnSync(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--save',join(directory,'save.json'),'--locale',locale,'--text-scale',String(scale),'--automation',script,'--report',report,'--report-checkpoints-only','--hidden',...extra],{encoding:'utf8',timeout:90000,windowsHide:true});await writeFile(join(out,name+'-process.txt'),(r.stdout??'')+'\n'+(r.stderr??''));assert.equal(r.status,0,r.error?.message??r.stderr);assert.doesNotMatch((r.stdout??'')+'\n'+(r.stderr??''),/Syntax error parsing property declaration|Invalid property declaration/,'reminder UI must parse without stylesheet diagnostics');const data=JSON.parse(await readFile(report,'utf8'));assert.equal(data.ok,true);return data;
 }};
}
const begin=[{resize:{width:390,height:844}},{waitFrames:3},touch('start')];
const menu=id=>[touch('engine-reading-menu'),touch(id)];
function fit(ui){const r=ui.readingLayout,m=r['reading-main'],a=r['reading-actions'];assert.equal(r.active,true);assert.ok(m.height>0);assert.ok(m.scrollWidth<=m.clientWidth+1);assert.ok(m.y+m.height<=a.y+.5);for(const c of r.controls)assert.ok(c.width>=43.5&&c.height>=43.5,JSON.stringify(c));}
test('native current reminders are read-only, restore exactly and pause until dismissed',{timeout:240000},async()=>{
 const out=await outdir('ui'),f=await fixture(out);
 const seed=await f.run('seed',[{checkpoint:'title'},...begin,{checkpoint:'new'},...menu('save'),{checkpoint:'saved'},...menu('engine-resume-context'),{checkpoint:'open'},{key:'F5'},{key:'F9'},{scheduleAdvanceMs:60000},{lifecycle:['background','foreground']},{checkpoint:'paused'},touch('engine-resume-context-close'),{checkpoint:'closed'},touch('engine-reading-menu-close'),{checkpoint:'returned'}]);
 assert.equal(at(seed,'title').resumeContext.enabled,false);assert.equal(at(seed,'new').resumeContext.open,false);for(const id of ['open','paused','closed','returned'])same(at(seed,'saved'),at(seed,id));assert.equal(at(seed,'paused').resumeContext.open,true);assert.equal(at(seed,'closed').focusId,'engine-resume-context');assert.equal(at(seed,'open').resumeContext.projection.entries.some(e=>e.elementId==='chosen'),false);assert.match(at(seed,'open').texts['engine-resume-context-panel'],/&amp; &lt;plain text&gt;/);assert.deepEqual(at(seed,'paused').readingHistory.snapshot.entries,[]);
 const bytes=await readFile(f.save('profile'));assert.doesNotMatch(bytes.toString(),/resumeContext|Current reminders/);
 for(const [locale,width,height,scale] of [['en',320,568,2],['ja',390,844,2],['ko',844,390,2],['en',390,844,1]]){
  const name=`restore-${locale}-${width}-${scale}`,r=await f.run(name,[{resize:{width,height}},{waitFrames:3},...menu('load'),{checkpoint:'loaded',screenshot:join(out,name+'.png')},{key:'Tab',checkpoint:'tab'},{key:'ShiftTab',checkpoint:'backtab'},{scheduleAdvanceMs:60000},{key:'F5'},{key:'F9'},{checkpoint:'held'},touch('engine-resume-context-close'),{checkpoint:'continued'}],{locale,scale});
  assert.equal(at(r,'loaded').locale,locale);const entries=Object.fromEntries(at(r,'loaded').resumeContext.projection.entries.map(e=>[e.elementId,e.text]));assert.equal(entries.month,locale==='ja'?'月 1':locale==='ko'?'월 1':'Month 1');assert.equal(entries.weekday,locale==='ja'?'月':locale==='ko'?'월':'Mon');assert.match(entries.date,locale==='ja'?/年/:locale==='ko'?/년/:/^Year/);
  assert.equal(at(r,'loaded').resumeContext.open,true);assert.equal(at(r,'loaded').focusId,'engine-resume-context-current');for(const key of ['tab','backtab']){assert.equal(at(r,key).focusId,'engine-resume-context-close','Tab stays on the sole reminder action');same(at(r,'loaded'),at(r,key));}same(at(r,'loaded'),at(r,'held'));same(at(r,'loaded'),at(r,'continued'));assert.equal(at(r,'continued').resumeContext.open,false);fit(at(r,'loaded'));assert.deepEqual(await readFile(f.save('profile')),bytes);
 }
 await mkdir(join(out,'broken'));await writeFile(f.save('broken'),'{broken');const failed=await f.run('failed',[...begin,{checkpoint:'before'},...menu('load'),{checkpoint:'failed'}],{profile:'broken'});same(at(failed,'before'),at(failed,'failed'));assert.equal(at(failed,'failed').resumeContext.open,false);assert.equal(await readFile(f.save('broken'),'utf8'),'{broken');
 const fresh=await f.run('new-again',[{resize:{width:390,height:844}},{waitFrames:3},...menu('load'),touch('engine-resume-context-close'),...menu('restart'),{checkpoint:'new'}]);assert.equal(at(fresh,'new').resumeContext.open,false);
 await writeFile(resolve('.cache/native-resume-context/latest.json'),JSON.stringify({ok:true,out},null,2));
});
test('accepted restores pause a running schedule, project current branches and leave saved bytes intact',{timeout:120000},async()=>{
 const out=await outdir('clock'),f=await fixture(out,{continuous:true});let session=createSession(f.content,42,{mode:'full'}).session;
 const next=dispatchSession(f.content,session,{type:'setSchedule',activityIds:['walk','rest','study']},session.state.stateRevision);assert.equal(next.ok,true);session=next.session;assert.equal(session.state.phase,'ready');
 const saved=await encodeSave(f.content,session);assert.equal(saved.ok,true);await mkdir(join(out,'profile'));await writeFile(f.save('profile'),saved.text);
 const r=await f.run('clock',[{resize:{width:390,height:844}},{waitFrames:3},...menu('load'),{checkpoint:'loaded'},{scheduleAdvanceMs:60000},{timelineAdvanceMs:60000},{dialogueAdvanceMs:60000},{extensionAdvanceMs:60000},{checkpoint:'paused'},touch('engine-resume-context-close'),{scheduleAdvanceMs:800},{checkpoint:'progressed'},...menu('engine-resume-context'),{checkpoint:'current'}]);
 assert.deepEqual(at(r,'loaded').state,session.state);assert.deepEqual(at(r,'loaded').commands,session.commands);same(at(r,'loaded'),at(r,'paused'));assert.equal(at(r,'paused').resumeContext.open,true);assert.equal(at(r,'progressed').commands.length,session.commands.length+1);assert.equal(at(r,'progressed').state.flags['garden-visited'],true);
 const entries=at(r,'current').resumeContext.projection.entries;assert.ok(entries.some(e=>e.elementId==='chosen'));assert.ok(!entries.some(e=>e.elementId==='before'));assert.ok(entries.some(e=>e.elementId==='identity'&&e.text==='Name: Rowan'));assert.equal(await readFile(f.save('profile'),'utf8'),saved.text);
});
test('slot review cancellation never opens reminders; only accepted slot restoration does',{timeout:120000},async()=>{
 const out=await outdir('slots'),f=await fixture(out,{slots:true});
 const r=await f.run('slots',[...begin,...menu('save'),touch('records__slots-request'),touch('records__close'),{checkpoint:'saved'},...menu('load'),touch('load-records__slots-request'),{checkpoint:'review'},touch('load-records__slots-cancel'),{checkpoint:'cancelled'},touch('load-records__slots-request'),touch('load-records__slots-confirm'),{checkpoint:'restored'},touch('engine-resume-context-close'),{checkpoint:'closed'}]);
 assert.equal(at(r,'review').resumeContext.open,false);assert.equal(at(r,'cancelled').resumeContext.open,false);assert.equal(at(r,'restored').resumeContext.open,true);same(at(r,'restored'),at(r,'closed'));assert.equal(at(r,'restored').uiModal,'');
});
test('manual opt-in, fixed-canvas fallback and opt-out keep their advertised behavior',{timeout:120000},async()=>{
 for(const options of [{automatic:false},{reading:false},{opted:false}]){
  const out=await outdir('options'),f=await fixture(out,options),session=createSession(f.content,42,{mode:'full'}).session,saved=await encodeSave(f.content,session);assert.equal(saved.ok,true);await mkdir(join(out,'profile'));await writeFile(f.save('profile'),saved.text);
  const r=await f.run('load',[{resize:{width:390,height:844}},{waitFrames:3},{key:'F9'},{checkpoint:'loaded'},...(options.automatic===false?[...menu('reminders'),{checkpoint:'manual'},touch('engine-resume-context-close'),{checkpoint:'closed'}]:options.reading===false?[touch('engine-resume-context-close'),{checkpoint:'closed'}]:[])]);
  assert.equal(at(r,'loaded').resumeContext.open,options.reading===false);assert.equal(at(r,'loaded').resumeContext.enabled,options.opted!==false);assert.deepEqual(at(r,'loaded').state,session.state);
  if(options.automatic===false){assert.equal(at(r,'manual').resumeContext.open,true);same(at(r,'loaded'),at(r,'closed'));}
  if(options.reading===false){fit(at(r,'loaded'));assert.equal(at(r,'closed').readingLayout.active,false);assert.deepEqual(at(r,'loaded').state,at(r,'closed').state);assert.deepEqual(at(r,'loaded').commands,at(r,'closed').commands);assert.equal(at(r,'loaded').scheduleDraft,null);assert.deepEqual(at(r,'closed').scheduleDraft.activities,['rest','rest','rest']);}
  assert.equal(await readFile(f.save('profile'),'utf8'),saved.text);
 }
});
