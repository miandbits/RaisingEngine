import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile,rename} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {spawn,spawnSync} from 'node:child_process';
import {existsSync} from 'node:fs';
import {setTimeout as delay} from 'node:timers/promises';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {desktopTarget,gameDirectory,playerExecutable} from '../../dist/packages/export-service/targets.js';
import {ReadingHistory} from '../../dist/packages/visual/reading-history.js';
const touch=id=>({scrollIntoView:id,touchWindow:id,waitFrames:3});
const at=(r,id)=>r.steps.find(s=>s.command.checkpoint===id)?.ui;
const history=ui=>ui.readingHistory.snapshot;
const same=(a,b)=>{assert.deepEqual(a.state,b.state);assert.deepEqual(a.commands,b.commands);};
const first='A lantern crosses the quiet garden. 日本語と한글 🌅 & <visible words>. '+('The gardener keeps a careful account of every borrowed seed. '.repeat(10));
const reply='The selected branch is now visible. 選んだ約束だけが残ります。';
const prompt='Which promise will be kept?';
async function fixture(out,{reading=true,typed=false,hidden=false,offscreen=false,covered=false,padded=false,speaker=false,slots=false,opted=true}={}){
 const source=resolve('examples/my-first-raising-game'),built=await buildSnapshot(await loadProject(source));assert.equal(built.ok,true);const content=structuredClone(built.snapshot);if(!content.project.localization.locales.some(l=>l.id==='ja'))content.project.localization.locales.push({id:'ja',label:'日本語'});
 const e=(id,kind,extra={})=>({id,kind,x:24,y:60,width:1180,height:480,...extra});const b=(id,action,extra={})=>e(id,'button',{x:24+(['start','talk','next','close'].includes(id)?0:['recent','save','load','restart','settings'].indexOf(id)+1)*200,y:620,width:190,height:50,text:id,action,autoLabel:false,...extra});
 const layout=(items,actions,primary)=>({items:items.map(elementId=>({elementId})),actions,actionLayout:'grouped',primaryAction:primary});
 const utilities=[b('recent','readingHistory'),b('save','save'),b('load','load'),b('restart','start')];
 const story=e('story','dialogue',{...(hidden?{opacity:0}:{}),...(offscreen?{y:2000}:{}),...(padded?{padding:900}:{}),fontSize:24});
 content.project.visual={width:1280,height:720,...(opted?{readingHistory:{}}:{}),...(reading?{readableLayout:{background:'#f5efdf'}}:{}),...(slots?{saveSystem:{manualSlots:3,autoSlots:2}}:{}),routing:{defaultScreenId:'home',useLocationScreens:false,phases:[{phase:'title',screenId:'title'},{phase:'planning',screenId:'home'},{phase:'dialogue',screenId:'story'},{phase:'choice',screenId:'story'}]},screens:[
 {id:'title',label:'Title',elements:[b('start','start'),...(typed?[b('settings','openModal',{screenId:'prefs'})]:[])],readableLayout:layout([],['start',...(typed?['settings']:[])],'start')},
 {id:'home',label:'Home',elements:[b('talk','executeAction',{actionId:'read-story'}),...utilities],readableLayout:layout([],['talk','recent','save','load','restart'],'talk')},
 {id:'schedule',label:'Schedule',elements:[]},{id:'ending',label:'Ending',elements:[]},
 {id:'story',label:'Story',elements:[...(speaker?[e('speaker','text',{binding:'speaker',height:44})]:[]),story,e('choices','choices',{y:380,height:190}),b('next','advance',{visibleWhen:'dialogue'}),...utilities,...(covered?[e('cover','panel',{fill:'#334455',height:540})]:[])],readableLayout:layout([...(speaker?['speaker']:[]),'story','choices'],['next','recent','save','load','restart'],'next')},
 ...(typed?[{id:'prefs',label:'Preferences',modal:true,elements:[e('settings','playerSettings'),b('close','closeModal')],readableLayout:layout(['settings'],['close'],'close')}]:[]),
 ...(slots?[false,true].map(load=>({id:load?'load-records':'records',label:'Records',modal:true,elements:[e('slots',load?'loadSlots':'saveSlots'),b('close','closeModal')],readableLayout:layout(['slots'],['close'],'close')})):[])
 ]};
 if(!reading)for(const screen of content.project.visual.screens)delete screen.readableLayout;
 if(!opted)for(const screen of content.project.visual.screens){screen.elements=screen.elements.filter(e=>e.action!=='readingHistory');if(screen.readableLayout)screen.readableLayout.actions=screen.readableLayout.actions.filter(id=>id!=='recent');}
 content.project.gameplay={startLocationId:'home',locations:[{id:'home',name:'Quiet garden',requirements:{op:'always'},connections:[]}],items:[],shops:[],interactions:[],actions:[{id:'read-story',name:'Read',requirements:{op:'always'},costs:[],effects:[],eventId:'history-story',confirmation:{enabled:false}}]};
 content.events.push({formatVersion:'0.1.0',id:'history-story',label:'Reading fixture',window:{fromPeriod:0,throughPeriod:100},requirements:{op:'always'},priority:100,trigger:'interaction',repeatable:true,entryNodeId:'one',nodes:[
 {id:'one',type:'dialogue',speakerId:'mira',text:first,next:'choice'},
 {id:'choice',type:'choice',prompt,choices:[{id:'accept',text:'Keep the seed safe',requirements:{op:'always'},effects:[],next:'reply'},{id:'refuse',text:'UNSELECTED RESPONSE SECRET',requirements:{op:'always'},effects:[],next:'secret'},{id:'blocked',text:'DISABLED RESPONSE SECRET',requirements:{op:'not',condition:{op:'always'}},effects:[],next:'secret'}]},
 {id:'reply',type:'dialogue',speakerId:'mira',text:reply,next:'end'},{id:'secret',type:'dialogue',text:'UNVISITED BRANCH SECRET',next:'end'},{id:'end',type:'end'}]});
 const target=desktopTarget(),destination=join(out,'game'),runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app');await exportGame({projectRoot:source,content,target,destination,runtimes:{windows:runtime,macos:runtime,web:runtime}});
 return {destination,content,run:async(name,steps,{profile='profile',extra=[]}={})=>{
 const directory=join(out,profile);await mkdir(directory,{recursive:true});const save=join(directory,'save.json'),script=join(out,name+'.json'),report=join(out,name+'-report.json');await writeFile(script,JSON.stringify(steps));
 const r=spawnSync(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--save',save,'--locale','en','--text-scale','2','--automation',script,'--report',report,'--report-checkpoints-only','--hidden',...extra],{encoding:'utf8',timeout:90000,windowsHide:true});await writeFile(join(out,name+'-process.txt'),(r.stdout??'')+'\n'+(r.stderr??''));assert.equal(r.status,0,r.stderr||String(r.error));return JSON.parse(await readFile(report,'utf8'));
 }};
}
const begin=[{resize:{width:390,height:844}},{waitFrames:3},touch('start'),touch('talk')];
const menu=(id)=>[touch('engine-reading-menu'),touch(id)];

test('native recent reading records only displayed story and accepted choices; overlays and saves do not mutate play',{timeout:180000},async()=>{
 await mkdir('.cache/native-reading-history',{recursive:true});const out=await mkdtemp(resolve('.cache/native-reading-history/run-')),f=await fixture(out);
 const r=await f.run('story',[...begin,{checkpoint:'first'},{touchDrag:{fromX:190,fromY:450,toX:190,toY:150}},{checkpoint:'scrolled'},...menu('recent'),{checkpoint:'history',screenshot:join(out,'history-390.png')},{key:'F5'},{key:'F9'},{checkpoint:'shortcuts'},{resize:{width:844,height:390}},{waitFrames:3},{checkpoint:'landscape'},touch('engine-reading-history-close'),{checkpoint:'return'},touch('engine-reading-menu-close'),{checkpoint:'scroll-return'},touch('next'),{checkpoint:'prompt'},touch('choose-blocked'),{checkpoint:'rejected'},...menu('engine-reading-history'),{checkpoint:'pending'},{key:'Escape'},{key:'Escape'},touch('choose-accept'),{checkpoint:'reply'},...menu('save'),{checkpoint:'saved'},...menu('engine-reading-history'),{checkpoint:'after-save'},touch('engine-reading-history-close'),touch('engine-reading-menu-close'),...menu('restart'),{checkpoint:'restarted'}]);
 assert.deepEqual(history(at(r,'first')).entries.map(e=>e.text),[first]);assert.equal(history(at(r,'first')).entries[0].speaker,'');assert.ok(at(r,'scrolled').readingLayout['reading-main'].scrollTop>0);assert.equal(at(r,'scroll-return').readingLayout.position.anchor,at(r,'scrolled').readingLayout.position.anchor);same(at(r,'first'),at(r,'history'));same(at(r,'history'),at(r,'shortcuts'));assert.equal(at(r,'shortcuts').readingHistory.open,true);
 assert.equal(at(r,'landscape').readingLayout.active,true);same(at(r,'first'),at(r,'return'));assert.deepEqual(history(at(r,'rejected')),history(at(r,'prompt')));same(at(r,'prompt'),at(r,'rejected'));
 assert.match(at(r,'pending').texts['engine-reading-history-panel'],/Waiting for your choice/);assert.doesNotMatch(at(r,'pending').texts['engine-reading-history-panel'],/UNSELECTED|UNVISITED|DISABLED/);same(at(r,'pending'),at(r,'prompt'));
 assert.deepEqual(history(at(r,'reply')).entries.map(e=>[e.kind,e.text]),[['dialogue',first],['prompt',prompt],['chosen-response','Keep the seed safe'],['dialogue',reply]]);
 assert.deepEqual(history(at(r,'saved')),history(at(r,'after-save')));same(at(r,'saved'),at(r,'after-save'));assert.deepEqual(history(at(r,'restarted')).entries,[]);assert.equal(history(at(r,'restarted')).reason,'new-run');
 const saved=await readFile(join(out,'profile/save.json'),'utf8');assert.doesNotMatch(saved,/readingHistory|Recent reading|chosen-response/);
 const fresh=await f.run('fresh',[{checkpoint:'empty'},...begin,...menu('load'),{checkpoint:'restored'},...menu('engine-reading-history'),{checkpoint:'restored-panel'}]);assert.deepEqual(history(at(fresh,'empty')).entries,[]);assert.equal(history(at(fresh,'restored')).reason,'restored');assert.deepEqual(history(at(fresh,'restored')).entries.map(e=>e.text),[reply]);assert.match(at(fresh,'restored-panel').texts['engine-reading-history-panel'],/Earlier reading is not restored/);assert.equal(await readFile(join(out,'profile/save.json'),'utf8'),saved);
 for(const [name,w,h] of [['small',320,640],['cjk',390,844]]){const s=await f.run(name,[...begin,...menu('engine-reading-history'),{resize:{width:w,height:h}},{waitFrames:3},{checkpoint:'panel',screenshot:join(out,name+'.png')}],{profile:name,extra:name==='cjk'?['--locale','ja']:[]});const ui=at(s,'panel');if(name==='cjk'){assert.equal(ui.locale,'ja');assert.match(ui.texts['engine-reading-history-panel'],/今回の起動/);assert.match(history(ui).entries[0].date,/1年 · 1月 · 予定 1/);assert.doesNotMatch(history(ui).entries[0].date,/년|월|일정/);}const m=ui.readingLayout['reading-main'],a=ui.readingLayout['reading-actions'];assert.ok(m.height>0&&m.scrollWidth<=m.clientWidth+1);assert.ok(m.y+m.height<=a.y+.5);for(const c of ui.readingLayout.controls)assert.ok(c.width>=43.5&&c.height>=43.5);}
 await writeFile(resolve('.cache/native-reading-history/latest.json'),JSON.stringify({ok:true,out},null,2));console.log('Native recent reading: '+out);
});

test('native history excludes typewriter suffixes, hidden and offscreen stories, previews and opt-out projects',{timeout:180000},async()=>{
 await mkdir('.cache/native-reading-history',{recursive:true});const root=await mkdtemp(resolve('.cache/native-reading-history/visibility-'));
 for(const [name,options] of [['hidden',{reading:false,hidden:true}],['offscreen',{reading:false,offscreen:true}],['covered',{reading:false,covered:true}],['padding-only',{reading:false,padded:true}],['optout',{opted:false}]]){const out=join(root,name);await mkdir(out);const f=await fixture(out,options),r=await f.run(name,[...(options.reading===false?[{click:'start'},{waitFrames:3},{click:'talk'},{waitFrames:3}]:begin),{checkpoint:'shown'}]);assert.deepEqual(history(r.final).entries,[],name);}
 const out=join(root,'typed');await mkdir(out);const f=await fixture(out,{typed:true});
 const r=await f.run('typed',[...begin,{checkpoint:'unrevealed'},{dialogueAdvanceMs:500},{waitFrames:3},{checkpoint:'partial'},...menu('engine-reading-history'),{dialogueAdvanceMs:10000},{checkpoint:'paused'},touch('engine-reading-history-close'),touch('engine-reading-menu-close'),touch('next'),{checkpoint:'revealed'},touch('next'),{checkpoint:'prompt'}]);assert.deepEqual(history(at(r,'unrevealed')).entries,[]);const text=history(at(r,'partial')).entries[0].text;assert.ok(first.startsWith(text)&&text.length>0&&text.length<first.length);assert.deepEqual(history(at(r,'paused')),history(at(r,'partial')));assert.deepEqual(history(at(r,'revealed')).entries.map(e=>e.text),[first]);assert.equal(history(at(r,'prompt')).entries.length,2);
 const preview=await f.run('preview',[{resize:{width:390,height:844}},{checkpoint:'preview'}],{extra:['--preview-screen','story']});assert.deepEqual(history(preview.final).entries,[]);
 const legacyOut=join(root,'legacy');await mkdir(legacyOut);const legacy=await fixture(legacyOut,{reading:false});const panel=await legacy.run('canvas',[{click:'start'},{waitFrames:3},{click:'talk'},{waitFrames:3},{checkpoint:'story'},{click:'recent'},{waitFrames:3},{checkpoint:'panel'},{key:'Escape'},{checkpoint:'return'}]);assert.equal(at(panel,'panel').readingHistory.open,true);same(at(panel,'story'),at(panel,'return'));assert.equal(at(panel,'return').readingLayout.active,false);assert.deepEqual(history(at(panel,'panel')).entries.map(e=>e.text),[first]);const settings=await legacy.run('legacy-settings',[{click:'start'},{waitFrames:3},{click:'talk'},{waitFrames:3},{checkpoint:'story'},{click:'engine-reading-options'},{waitFrames:3},touch('engine-reading-history'),{checkpoint:'panel'},{key:'Escape'},{key:'Escape'},{checkpoint:'return'}]);assert.equal(at(settings,'panel').readingHistory.open,true);same(at(settings,'story'),at(settings,'return'));
});

test('native reading tracker matches shared UTF-8 caps, coalescing, epoch rejection and eviction',{timeout:90000},async()=>{
 const root=await mkdtemp(resolve('.cache/native-history-parity-'));const source=join(root,'tracker.cpp'),exe=join(root,process.platform==='win32'?'tracker.exe':'tracker');await writeFile(source,'#include "reading-history.hpp"\n#include <iostream>\nint main(){nlohmann::json ops;std::cin>>ops;raising_player::ReadingHistory h("run-1");auto out=nlohmann::json::array();for(auto op:ops){if(op.contains("reset"))h.reset(op.at("reset"),op.value("reason","new-run"));else h.record(op);out.push_back(h.snapshot());}std::cout<<out.dump();}');
 const mingw=join(process.env.QT_COMPILER??'C:/Program Files/mingw64/bin','g++.exe');const compiler=process.env.CXX??(process.platform==='win32'?(existsSync(mingw)?mingw:'g++'):'c++');const built=spawnSync(compiler,['-std=c++17','-O1','-I',resolve('native/player'),'-I',resolve('native/vendor'),source,'-o',exe],{encoding:'utf8',timeout:60000});assert.equal(built.status,0,built.stderr||String(built.error));
 const line=(text,extra={})=>({epoch:'run-1',key:'node-1',kind:'dialogue',text,locale:'en',speaker:'Mira',date:'Week 1',...extra});const ops=[line(''),line(' \n '),line('A'),line('A'),line('A new horizon'),line('A new'),line('Different words'),line('こんにちは',{locale:'ja'}),line('Keep promise',{kind:'chosen-response'}),line('Keep promise',{kind:'chosen-response'}),line('🌅'.repeat(10000),{speaker:'人'.repeat(1000),date:'D'.repeat(2000)}),line('Rejected',{key:'x'.repeat(1025)})];for(let i=0;i<205;i++)ops.push(line('Line '+i,{key:'node-'+i}));ops.push({reset:'restored',reason:'restored'},line('stale'));for(let i=0;i<50;i++)ops.push(line('界'.repeat(10000),{epoch:'restored',key:'large-'+i}));
 const expected=[],tracker=new ReadingHistory('run-1');for(const op of ops){if(op.reset)tracker.reset(op.reset,op.reason);else tracker.record(op);expected.push(tracker.snapshot());}const result=spawnSync(exe,[],{input:JSON.stringify(ops),encoding:'utf8',timeout:30000,maxBuffer:30*1024*1024});assert.equal(result.status,0,result.stderr);assert.deepEqual(JSON.parse(result.stdout),expected);
});

test('native successful slot and autosave restore reset history, while canceled and failed loads retain it',{timeout:120000},async()=>{
 await mkdir('.cache/native-reading-history',{recursive:true});const out=await mkdtemp(resolve('.cache/native-reading-history/restores-')),f=await fixture(out,{slots:true,speaker:true});
 const r=await f.run('slots',[...begin,{checkpoint:'first'},...menu('save'),touch('records__slots-request'),touch('records__close'),touch('next'),touch('choose-accept'),{checkpoint:'reply'},...menu('load'),touch('load-records__slots-request'),{checkpoint:'confirm'},touch('load-records__slots-cancel'),touch('load-records__close'),{checkpoint:'canceled'},...menu('load'),touch('load-records__slots-request'),touch('load-records__slots-confirm'),{checkpoint:'restored'},...menu('engine-reading-history'),{checkpoint:'panel'},touch('engine-reading-history-close'),touch('engine-reading-menu-close'),{lifecycle:['background','foreground']},{checkpoint:'autosaved'},touch('next'),touch('choose-accept'),{checkpoint:'newer'},{restoreAutosave:true},{waitFrames:3},{checkpoint:'auto-restored'}]);
 assert.ok(history(at(r,'first')).entries[0].speaker);assert.deepEqual(history(at(r,'canceled')),history(at(r,'reply')));same(at(r,'canceled'),at(r,'reply'));assert.equal(history(at(r,'restored')).reason,'restored');assert.deepEqual(history(at(r,'restored')).entries.map(e=>e.text),[first]);assert.match(at(r,'panel').texts['engine-reading-history-panel'],/Earlier reading is not restored/);assert.equal(history(at(r,'auto-restored')).reason,'restored');assert.deepEqual(history(at(r,'auto-restored')).entries.map(e=>e.text),[first]);assert.notEqual(history(at(r,'auto-restored')).epoch,history(at(r,'restored')).epoch);
 const plainOut=join(out,'plain');await mkdir(plainOut);const plain=await fixture(plainOut);await mkdir(join(plainOut,'profile'));await writeFile(join(plainOut,'profile/save.json'),'{broken original');const bad=await plain.run('failed',[...begin,{checkpoint:'before'},{key:'F9'},{waitFrames:3},{checkpoint:'failed'}]);same(at(bad,'before'),at(bad,'failed'));assert.deepEqual(history(at(bad,'before')),history(at(bad,'failed')));assert.equal(await readFile(join(plainOut,'profile/save.json'),'utf8'),'{broken original');
});

test('native live editor preview never captures displayed or chosen text into session reading',{timeout:90000},async t=>{
 await mkdir('.cache/native-reading-history',{recursive:true});const out=await mkdtemp(resolve('.cache/native-reading-history/live-')),f=await fixture(out),baseline=await f.run('baseline',[...begin,{checkpoint:'story'}]);
 const session=join(out,'live');await mkdir(session);const inspection=join(out,'inspection.json');await writeFile(inspection,JSON.stringify({version:1,contentFingerprint:f.content.contentFingerprint,screenId:'story',state:baseline.final.state,visual:f.content.project.visual}));const preserved=join(out,'ordinary.json');await writeFile(preserved,'preserved original');const target=desktopTarget();
 const child=spawn(playerExecutable(target,f.destination),['--bundle',gameDirectory(target,f.destination),'--save',preserved,'--hidden','--locale','en','--preview-screen','story','--preview-inspection',inspection,'--editor-live-preview',session,'--reading-layout','on','--text-scale','2'],{windowsHide:true});t.after(()=>child.kill());let buffer='',stderr='',ready=false,serial=0;const reports=new Map();child.stderr.on('data',b=>stderr+=b);child.stdout.on('data',b=>{buffer+=b;while(buffer.includes('\n')){const at=buffer.indexOf('\n'),line=buffer.slice(0,at);buffer=buffer.slice(at+1);try{const r=JSON.parse(line).editorPreview;if(r?.ready)ready=true;else if(r)reports.set(r.serial,r);}catch{}}});
 async function until(predicate){for(let i=0;i<1000;i++){if(predicate())return;if(child.exitCode!==null)throw Error(stderr||'Preview exited');await delay(20);}throw Error('Preview timed out: '+stderr);}
 async function command(value){const id=++serial;await writeFile(join(session,'next.json'),JSON.stringify({...value,serial:id}));await rename(join(session,'next.json'),join(session,'control.json'));await until(()=>reports.has(id));return reports.get(id).ui;}
 await until(()=>ready);await command({type:'pause',paused:false});const first=await command({type:'inspect'});assert.ok(first.texts.story.includes('lantern'));assert.deepEqual(history(first).entries,[]);
 const prompt=await command({type:'click',id:'next'});assert.equal(prompt.preview.screenId,'');assert.deepEqual(history(prompt).entries,[]);
 const reply=await command({type:'click',id:'choose-accept'});assert.ok(reply.commands.some(c=>c.type==='choose'));assert.deepEqual(history(reply).entries,[]);assert.equal(await readFile(preserved,'utf8'),'preserved original');await writeFile(join(out,'report.json'),JSON.stringify({first,prompt,reply},null,2));
});


test('native new-run acceptance clears reading, while rejected automatic restore preserves it',{timeout:90000},async()=>{
 const out=await mkdtemp(resolve('.cache/native-history-boundary-')),built=await buildSnapshot(await loadProject(resolve('examples/my-first-raising-game')));assert.equal(built.ok,true);
 const content=structuredClone(built.snapshot);content.events=[{id:'boundary-error',label:'Settlement failure',trigger:'period-start',priority:100,window:{fromPeriod:0,throughPeriod:100},requirements:{op:'always'},entryNodeId:'error',nodes:[{id:'error',type:'effect',effects:[{op:'add',target:{kind:'attribute',attributeId:'missing-attribute'},value:{op:'literal',value:1}}],next:'end'},{id:'end',type:'end'}]}];
 const input=join(out,'content.json');await writeFile(input,JSON.stringify(content));const source=join(out,'boundary.cpp'),exe=join(out,process.platform==='win32'?'boundary.exe':'boundary');
 await writeFile(source,`#include "session.hpp"
#include "reading-history.hpp"
#include <iostream>
int main(int argc,char** argv){
 GameSession game;game.content=read_json(fs::u8path(argv[1]));raising_player::ReadingHistory history("old");int epoch=0;
 game.accepted_replacement=[&](const std::string& reason){history.reset(std::to_string(++epoch),reason);};
 auto seed=[&]{history.record({{"epoch",history.epoch()},{"key","old-node"},{"kind","dialogue"},{"text","Previous session text"}});};
 auto expect=[&](bool value,const char* why){if(!value)throw std::runtime_error(why);};
 seed();game.start();const auto saved=fs::u8path(argv[2]);game.save(saved);bool failed=false;try{game.settle();}catch(...){failed=true;}
 expect(failed,"New-run automatic settlement must fail");expect(history.snapshot().at("entries").empty(),"New-run history was not cleared before failure");expect(history.snapshot().at("reason")=="new-run","Wrong new-run boundary");
 seed();const auto beforeRestore=history.snapshot(),beforeState=game.state;failed=false;try{game.load(saved);}catch(...){failed=true;}expect(failed,"Automatic restore settlement must fail");expect(history.snapshot()==beforeRestore&&game.state==beforeState,"Rejected automatic restore changed reading or state");
 seed();const auto before=history.snapshot(),state=game.state;failed=false;try{game.load(saved.parent_path()/"missing.json");}catch(...){failed=true;}expect(failed,"Invalid load must fail");expect(history.snapshot()==before&&game.state==state,"Pre-accept failure changed history/state");
 std::cout<<Json({{"ok",true},{"replacements",epoch},{"snapshot",history.snapshot()}}).dump();}
`);
 const mingw=join(process.env.QT_COMPILER??'C:/Program Files/mingw64/bin','g++.exe');const compiler=process.env.CXX??(process.platform==='win32'?(existsSync(mingw)?mingw:'g++'):'c++');
 const library=resolve('dist/native-player/core/libraising_core.a');assert.ok(existsSync(library),'Native build must provide the actual portable core library');
 const compiled=spawnSync(compiler,['-std=c++17','-O0','-I',resolve('native/player'),'-I',resolve('native/include'),'-I',resolve('native/vendor'),source,library,'-o',exe],{encoding:'utf8',timeout:60000});assert.equal(compiled.status,0,compiled.stderr||String(compiled.error));
 const result=spawnSync(exe,[input,join(out,'saved.json')],{encoding:'utf8',timeout:30000});await writeFile(join(out,'result.txt'),result.stdout+'\n'+result.stderr);assert.equal(result.status,0,result.stderr);assert.equal(JSON.parse(result.stdout).replacements,1);
});
