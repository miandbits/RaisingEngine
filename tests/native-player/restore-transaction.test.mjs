import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,rm,readFile,writeFile} from 'node:fs/promises';
import {existsSync} from 'node:fs';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';

test('native load rejects automatic settlement failures before state, observers, history or gallery change', {timeout:180000}, async()=>{
 await mkdir('.cache/native-restore-transaction',{recursive:true});const out=await mkdtemp(resolve('.cache/native-restore-transaction/run-')),projectRoot=join(out,'project');
 await cp(resolve('examples/my-first-raising-game'),projectRoot,{recursive:true});await rm(join(projectRoot,'scenarios'),{recursive:true,force:true});
 const project=JSON.parse(await readFile(join(projectRoot,'project.json'),'utf8'));delete project.visual;project.gameId='restore-transaction';
 const event=(id,trigger,nodes)=>({formatVersion:'0.1.0',id,label:id,trigger,window:{fromPeriod:0,throughPeriod:8},requirements:{op:'always'},priority:90,entryNodeId:nodes[0].id,nodes});
 await writeFile(join(projectRoot,'events/garden-invitation.json'),JSON.stringify(event('garden-invitation','period-start',[
  {id:'hello',type:'dialogue',speakerId:'mira',text:'A remembered meeting.',next:'choice'},
  {id:'choice',type:'choice',prompt:'Which calculation?',choices:[false,true].map(safe=>({id:safe?'safe':'unsafe',text:safe?'Use bounded growth':'Use an overflowing calculation',requirements:{op:'always'},effects:[{op:'setFlag',flagId:'invitation-accepted',value:safe}],next:'end'}))},
  {id:'end',type:'end'}])));
 await writeFile(join(projectRoot,'events/garden-promise.json'),JSON.stringify(event('garden-promise','after-event',[
  {id:'gain',type:'effect',effects:[{op:'add',target:{kind:'attribute',attributeId:'skill'},value:{op:'literal',value:1}},{op:'add',target:{kind:'relationship',characterId:'mira',relationshipId:'affection'},value:{op:'literal',value:2}}],next:'branch'},
  {id:'branch',type:'branch',condition:{op:'flag',flagId:'invitation-accepted',equals:true},then:'end',else:'overflow'},
  {id:'overflow',type:'effect',effects:[{op:'add',target:{kind:'attribute',attributeId:'skill'},value:{op:'literal',value:2147483647}}],next:'end'},{id:'end',type:'end'}])));
 const inputs=[];
 for(const gallery of [false,true]){
  if(gallery)project.gallery={title:'Restored meetings',entries:[{id:'meeting',kind:'event',sourceId:'garden-invitation',title:'Meeting',category:'Story',hint:'Complete the meeting.'}]};else delete project.gallery;
  await writeFile(join(projectRoot,'project.json'),JSON.stringify(project));const built=await buildSnapshot(await loadProject(projectRoot));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  const input=join(out,gallery?'gallery.json':'ordinary.json');await writeFile(input,JSON.stringify(built.snapshot));inputs.push(input);
 }
 // A deliberately unvalidated loop exercises the runtime guard for imported/legacy snapshots.
 const loop=JSON.parse(await readFile(inputs[0],'utf8')),branch=loop.events.find(e=>e.id==='garden-promise').nodes.find(n=>n.id==='branch');
 branch.then='branch';branch.else='branch';const loopInput=join(out,'loop.json');await writeFile(loopInput,JSON.stringify(loop));
 const continuous=await buildSnapshot(await loadProject(resolve('tests/fixtures/projects/last-grandson-polished')));assert.equal(continuous.ok,true,JSON.stringify(continuous.diagnostics));
 const continuousInput=join(out,'continuous.json');await writeFile(continuousInput,JSON.stringify(continuous.snapshot));
 const galleryInputs=[];
 for(const late of [false,true]){
  const overflowing={op:'compare',comparator:'gte',left:{op:'add',left:{op:'literal',value:2147483647},right:{op:'literal',value:1}},right:{op:'literal',value:0}};
  project.gallery={title:'Condition failure',entries:[{id:'meeting',kind:'event',sourceId:'garden-invitation',title:'Meeting',category:'Story',hint:'Complete the meeting.'},...(late?[{id:'promise',kind:'event',sourceId:'garden-promise',title:'Promise',category:'Story',hint:'Complete the promise.'}]:[])]};
  project.gallery.entries.at(-1).unlockCondition=overflowing;await writeFile(join(projectRoot,'project.json'),JSON.stringify(project));
  const built=await buildSnapshot(await loadProject(projectRoot));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));const input=join(out,late?'gallery-late-condition.json':'gallery-condition.json');await writeFile(input,JSON.stringify(built.snapshot));galleryInputs.push(input);
 }
 // Shared media sources must drain their staged unlocks just like the live profile.
 await cp(resolve('examples/academy-journey/assets/visual/courtyard.png'),join(projectRoot,'assets/test-cg.png'));
 await writeFile(join(projectRoot,'assets/manifest.json'),JSON.stringify({formatVersion:'0.1.0',assets:[{id:'test-cg',kind:'image',path:'assets/test-cg.png'}]}));
 const laterEvent=JSON.parse(await readFile(join(projectRoot,'events/garden-promise.json'),'utf8'));laterEvent.nodes[0].effects.push({op:'setFlag',flagId:'garden-visited',value:true});await writeFile(join(projectRoot,'events/garden-promise.json'),JSON.stringify(laterEvent));
 const overflowing={op:'compare',comparator:'gte',left:{op:'add',left:{op:'literal',value:2147483647},right:{op:'literal',value:1}},right:{op:'literal',value:0}};
 project.gallery={title:'Shared media',entries:[{id:'first-cg',kind:'cg',sourceId:'test-cg',title:'First',category:'CG',hint:'First record.'},{id:'later-cg',kind:'cg',sourceId:'test-cg',title:'Later',category:'CG',hint:'Later record.',unlockCondition:{op:'all',conditions:[{op:'flag',flagId:'garden-visited',equals:true},overflowing]}}]};
 await writeFile(join(projectRoot,'project.json'),JSON.stringify(project));const shared=await buildSnapshot(await loadProject(projectRoot));assert.equal(shared.ok,true,JSON.stringify(shared.diagnostics));const sharedInput=join(out,'gallery-shared-source.json');await writeFile(sharedInput,JSON.stringify(shared.snapshot));galleryInputs.push(sharedInput);
 const source=join(out,'restore.cpp'),exe=join(out,process.platform==='win32'?'restore.exe':'restore');
 await writeFile(source,`#include "session.hpp"
#include "reading-history.hpp"
#include <iostream>
Json snapshot(GameSession& g){return {{"state",g.state},{"commands",g.commands},{"origin",g.origin},{"seed",g.seed},{"projection",g.projection},{"relationshipChanges",g.relationship_changes},{"choiceFeedback",g.choice_feedback},{"scheduleSpeed",g.schedule_speed},{"timelinePlan",g.timeline_plan},{"timelineCursor",g.timeline_cursor},{"battleSelection",g.battle_selection},{"loadedExtensions",g.loaded_extensions},{"directionCursor",g.direction_cursor},{"galleryCapture",g.gallery_capture.pending},{"galleryPending",g.gallery_pending},{"galleryData",g.gallery.data},{"soundCues",g.sound_cues}};}
void expect(bool value,const char* why){if(!value)throw std::runtime_error(why);}
std::string bytes(const fs::path& p){std::ifstream in(p,std::ios::binary);return std::string(std::istreambuf_iterator<char>(in),{});}
int main(int argc,char** argv){
 const auto out=fs::u8path(argv[8]);Json results=Json::array();
 for(int variant=1;variant<=2;++variant){const auto content=read_json(fs::u8path(argv[variant]));const bool gallery=variant==2;
  for(const bool safe:{false,true}){const auto dir=out/(std::to_string(variant)+(safe?"-safe":"-unsafe"));fs::create_directories(dir);
   GameSession source;source.content=content;if(gallery)source.gallery.open(dir/"source-gallery.json","restore-transaction");source.start();source.send({{"type","advanceNarrative"}});
   source.extension_state=[](){return Json{{"probe",{{"saved",true}}}};};source.save(dir/"pending.json");
   GameSession pending;pending.content=content;pending.gallery_recording=false;pending.seed=99;pending.start();int pendingObservers=0,pendingReplacements=0;
   pending.observer=[&](const Json&,const Json&,const Json&,const std::string&){++pendingObservers;};pending.accepted_replacement=[&](const std::string&){++pendingReplacements;};pending.load(dir/"pending.json");
   expect(pending.state==source.state&&pending.commands==source.commands&&pending.projection.at("narrative").at("node").at("type")=="choice","Pending choice did not restore exactly");
   expect(pendingObservers==0&&pendingReplacements==1&&pending.loaded_extensions==source.extension_state(),"Pending choice changed callback/extension behavior");
   if(gallery)expect(pending.gallery_capture.pending==source.gallery_capture.pending,"Pending gallery capture was not restored");
   source.send({{"type","choose"},{"choiceId",safe?"safe":"unsafe"}});source.extension_state=[](){return Json{{"probe",{{"saved",true}}}};};source.save(dir/"save.json");
   const auto saved=read_json(dir/"save.json");expect(source.call({{"operation","replay"},{"origin",saved.at("origin")},{"commands",saved.at("commands")}}).at("state")==saved.at("state"),"Strict replay must accept the fixture save");
   GameSession live;live.content=content;live.seed=77;if(gallery){live.gallery.open(dir/"live-gallery.json","restore-transaction");std::ofstream(dir/"live-gallery.json")<<live.gallery.data.dump();}live.start();live.schedule_speed=4;
   raising_player::ReadingHistory history("old");history.record({{"epoch","old"},{"key","old"},{"kind","dialogue"},{"text","Keep prior reading on failure"}});int replacements=0,plannerRestores=0;Json observed=Json::array(),observedLive=Json::array();
   live.accepted_replacement=[&](const std::string& reason){++replacements;history.reset("new",reason);};live.observer=[&](const Json& before,const Json& after,const Json& command,const std::string& error){observed.push_back({{"before",before},{"after",after},{"command",command},{"error",error}});observedLive.push_back(live.state);};live.planner_restore=[&](const Json&){++plannerRestores;};
   const auto before=snapshot(live),readingBefore=history.snapshot();const auto diskBefore=gallery?bytes(dir/"live-gallery.json"):"";
   Json invalid=saved;invalid["playerPresentation"]["scheduleSpeed"]=3;std::ofstream(dir/"invalid-presentation.json")<<invalid.dump();std::string rejected;
   try{live.load(dir/"invalid-presentation.json");}catch(const std::exception& e){rejected=e.what();}
   expect(rejected=="Invalid schedule speed","Invalid saved presentation was accepted");expect(snapshot(live)==before&&history.snapshot()==readingBefore&&replacements==0&&observed.empty()&&plannerRestores==0,"Invalid saved presentation changed the live run");
   if(gallery)expect(bytes(dir/"live-gallery.json")==diskBefore,"Invalid saved presentation changed gallery storage");
   std::string error;try{live.load(dir/"save.json");}catch(const std::exception& e){error=e.what();}
   if(!safe){expect(error.find("INTEGER_OVERFLOW")!=std::string::npos,"Expected validated automatic effect overflow");expect(snapshot(live)==before,"Failed restore replaced live session or presentation");expect(history.snapshot()==readingBefore&&replacements==0,"Failed restore reset accepted-run history");expect(observed.empty(),"Failed restore leaked observer callbacks");expect(plannerRestores==0,"Failed restore restored planner");if(gallery)expect(bytes(dir/"live-gallery.json")==diskBefore,"Failed restore changed persistent gallery");}
   else{expect(error.empty(),"Valid restore failed");source.settle();expect(live.state==source.state&&live.commands==source.commands,"Successful settlement differs from normal command execution");expect(live.relationship_changes==source.relationship_changes&&live.choice_feedback==source.choice_feedback,"Successful feedback differs");expect(live.schedule_speed==1&&live.loaded_extensions==saved.at("playerPresentation").at("extensions"),"Successful presentation was not restored");expect(replacements==1&&plannerRestores==1&&history.snapshot().at("reason")=="restored","Successful boundary callbacks not delivered once");expect(observed.size()==4,"Automatic observers must run exactly once per accepted step");for(size_t i=0;i<observed.size();++i){expect(observed[i].at("error")=="","Successful observer received an error");expect(observed[i].at("after").at("state")==observedLive[i],"Observer lost matching live step state");}if(gallery){expect(live.gallery.records(content.at("project").at("gallery").at("entries").at(0)).size()==1,"Successful gallery record missing");expect(live.gallery.data==read_json(dir/"live-gallery.json"),"Gallery memory and disk differ");}}
   results.push_back({{"gallery",gallery},{"safe",safe},{"error",error},{"observerCount",observed.size()},{"replacementCount",replacements}});
  }
 }
 for(int input=5;input<=7;++input){
  const auto dir=out/(input==5?"gallery-condition":input==6?"gallery-late-condition":"gallery-shared-source");fs::create_directories(dir);
  GameSession source;source.content=read_json(fs::u8path(argv[input]));source.gallery.open(dir/"source.json","restore-transaction");source.start();source.send({{"type","advanceNarrative"}});source.send({{"type","choose"},{"choiceId","safe"}});source.save(dir/"save.json");source.gallery_recording=false;source.settle();
  GameSession live;live.content=source.content;live.seed=77;live.gallery.open(dir/"live.json","restore-transaction");std::ofstream(dir/"live.json")<<live.gallery.data.dump();live.start();const auto before=snapshot(live);const auto stored=bytes(dir/"live.json");int callbacks=0;
  live.observer=[&](const Json&,const Json&,const Json&,const std::string&){++callbacks;};live.accepted_replacement=[&](const std::string&){++callbacks;};live.planner_restore=[&](const Json&){++callbacks;};live.extension_state=[&](){++callbacks;return Json::object();};live.gallery_content=[&](){++callbacks;return live.content;};std::string error;
  try{live.load(dir/"save.json");}catch(const std::exception& e){error=e.what();}
  expect(error.find("INTEGER_OVERFLOW")!=std::string::npos,"Expected gallery unlock condition overflow");expect(snapshot(live)==before&&callbacks==0,"Gallery condition failure changed the live run or called host hooks");expect(bytes(dir/"live.json")==stored,"Gallery condition failure leaked a prior unlock to disk");results.push_back({{"kind",input==5?"gallery-condition":input==6?"gallery-condition-after-earlier-unlock":"gallery-shared-source-delayed-condition"},{"error",error},{"callbacks",callbacks}});
  if(input==7){
   // A record already earned in a prior playthrough must not reevaluate its now-failing condition.
   auto earned=live.gallery.records(live.content.at("project").at("gallery").at("entries").at(0)).at(0);earned["_entryIds"]=Json::array({"later-cg"});live.gallery.unlock(live.content.at("project").at("gallery"),earned);
   const auto profileBefore=bytes(dir/"live.json");live.load(dir/"save.json");expect(live.state==source.state&&live.commands==source.commands,"Previously unlocked media caused a false restore rejection");expect(bytes(dir/"live.json")==profileBefore,"Restoring earned media rewrote the profile");results.push_back({{"kind","already-unlocked-shared-media"},{"ok",true}});
  }
 }
 { // The save is produced by accepted commands; only the cyclic content is intentionally unvalidated.
  GameSession source;source.content=read_json(fs::u8path(argv[3]));source.start();source.send({{"type","advanceNarrative"}});source.send({{"type","choose"},{"choiceId","unsafe"}});source.save(out/"loop-save.json");
  GameSession live;live.content=source.content;live.seed=77;live.start();const auto before=snapshot(live);int callbacks=0;
  live.observer=[&](const Json&,const Json&,const Json&,const std::string&){++callbacks;};live.accepted_replacement=[&](const std::string&){++callbacks;};live.planner_restore=[&](const Json&){++callbacks;};std::string error;
  try{live.load(out/"loop-save.json");}catch(const std::exception& e){error=e.what();}
  expect(error=="대본 자동 진행 한도를 초과했습니다.","Expected automatic progression loop guard");expect(snapshot(live)==before&&callbacks==0,"Loop guard changed the live run or called host hooks");results.push_back({{"kind","automatic-loop-limit"},{"error",error},{"callbacks",callbacks}});
 }
 { // Real continuous scheduling: a committed result retains its paused presentation cursor.
  GameSession source;source.content=read_json(fs::u8path(argv[4]));source.gallery_recording=false;source.start();expect(source.continuous(),"Fixture must use continuous playback");
  source.send({{"type","setSchedule"},{"activityIds",Json::array({"rest","rest","rest"})}});source.send({{"type","executeActivity"}});expect(source.timeline_active(),"Fixture must have a result timeline");
  source.tick_timeline(225);source.timeline_cursor["paused"]=true;source.schedule_speed=4;source.timeline_cursor["speed"]=4;source.extension_state=[](){return Json{{"animation",{{"time",123}}}};};source.save(out/"continuous-save.json");
  GameSession live;live.content=source.content;live.gallery_recording=false;live.seed=77;live.start();int observers=0,replacements=0;live.observer=[&](const Json&,const Json&,const Json&,const std::string&){++observers;};live.accepted_replacement=[&](const std::string&){++replacements;};live.load(out/"continuous-save.json");
  expect(live.state==source.state&&live.commands==source.commands&&live.timeline_plan==source.timeline_plan&&live.timeline_cursor==source.timeline_cursor&&live.schedule_speed==4,"Continuous timeline restoration changed committed progress or cursor");expect(live.loaded_extensions==source.extension_state()&&observers==0&&replacements==1,"Continuous timeline callback/extension parity failed");
  live.tick_timeline(1000);expect(live.timeline_cursor==source.timeline_cursor,"Paused restored timeline advanced");results.push_back({{"kind","continuous-paused-timeline"},{"callbacks",replacements},{"observerCount",observers}});
 }std::cout<<Json({{"ok",true},{"cases",results}}).dump();}
`);
 const mingw=join(process.env.QT_COMPILER??'C:/Program Files/mingw64/bin','g++.exe');const compiler=process.env.CXX??(process.platform==='win32'?(existsSync(mingw)?mingw:'g++'):'c++');
 const compiled=spawnSync(compiler,['-std=c++17','-O0','-I',resolve('native/player'),'-I',resolve('native/include'),'-I',resolve('native/vendor'),source,resolve('dist/native-player/core/libraising_core.a'),'-o',exe],{encoding:'utf8',timeout:60000});assert.equal(compiled.status,0,compiled.stderr||String(compiled.error));
 const result=spawnSync(exe,[...inputs,loopInput,continuousInput,...galleryInputs,out],{encoding:'utf8',timeout:60000});await writeFile(join(out,'result.txt'),(result.stdout??'')+'\n'+(result.stderr??''));assert.equal(result.status,0,result.stderr||String(result.error));const report=JSON.parse(result.stdout);assert.equal(report.ok,true);await writeFile(join(out,'result.json'),JSON.stringify(report,null,2));await writeFile('.cache/native-restore-transaction/latest.json',JSON.stringify({ok:true,out},null,2));
});
