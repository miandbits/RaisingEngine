import test from 'node:test';
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFile,mkdtemp,writeFile} from 'node:fs/promises';
import {resolve,extname,sep} from 'node:path';
import {chromium} from 'playwright';
import {actorStoryFixture} from '../native/actor-story-fixture.mjs';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {nativeReplay} from '../../dist/packages/native-bridge/index.js';

test('exported personal story preserves actor conditions, promises and followups through browser saves',{timeout:120000},async t=>{
 const {built,root}=await actorStoryFixture(t);assert.ok(built.ok);const c=structuredClone(built.snapshot);
 c.project.initialState.flags['personal-promise']=false;
 const first=c.events.find(e=>e.id==='personal-choice'),follow=c.events.find(e=>e.id==='personal-follow');
 first.trigger='after-activity';first.activityId='study';first.repeatable=false;
 first.requirements={op:'compare',comparator:'gte',left:{op:'read',target:{kind:'attribute',attributeId:'skill',owner:{kind:'actor'}}},right:{op:'literal',value:0}};
 const choices=first.nodes.find(n=>n.type==='choice').choices;
 choices[0].effects.push({op:'add',target:{kind:'attribute',attributeId:'skill',owner:{kind:'actor'}},value:{op:'literal',value:3}});
 choices[1].effects=[{op:'setFlag',flagId:'personal-promise',value:false,owner:{kind:'actor'}},{op:'setFlag',flagId:'personal-choice-record',value:false,owner:{kind:'actor'}}];
 follow.trigger='after-activity';follow.activityId='study';follow.window.fromPeriod=1;
 const button=(id,action,x)=>({id,kind:'button',x,y:640,width:120,height:48,fontSize:20,text:id,action});
 const controls=[button('save','save',20),button('load','load',160),button('advance','advance',300)];
 c.project.visual={...c.project.visual,width:1280,height:720,routing:{defaultScreenId:'home',useLocationScreens:false,phases:[{phase:'title',screenId:'title'},{phase:'choice',screenId:'story'},{phase:'dialogue',screenId:'story'},{phase:'activity-result',screenId:'results'}]},screens:[
  {id:'title',label:'Title',elements:[button('start','start',20),button('load','load',160)]},
  {id:'home',label:'Home',elements:[...controls,{id:'schedule',kind:'schedule',x:20,y:80,width:1200,height:450,fontSize:20}]},
  {id:'schedule',label:'Schedule',elements:[]},{id:'ending',label:'Ending',elements:controls},
  {id:'results',label:'Results',elements:[...controls,{id:'result',kind:'result',x:20,y:80,width:1200,height:450,fontSize:20}]},
  {id:'story',label:'Story',elements:[...controls,
   {id:'actor',kind:'text',x:20,y:10,width:500,height:30,fontSize:20,binding:'narrative-actor'},
   {id:'compact',kind:'text',x:20,y:45,width:500,height:44,fontSize:30,text:'Compact heading'},
   {id:'padded',kind:'text',x:540,y:45,width:500,height:38,fontSize:24,padding:5,borderWidth:2,text:'Padded label'},
   {id:'long',kind:'text',x:900,y:100,width:320,height:80,fontSize:22,text:'Line one\nLine two\nLine three\nLine four'},
   {id:'dialogue',kind:'dialogue',x:20,y:110,width:850,height:160,fontSize:22},
   {id:'choices',kind:'choices',x:20,y:300,width:1100,height:260,fontSize:22,showChoiceEffects:true}]}]};
 const out=await mkdtemp(resolve('.cache/personal-story-web-')),destination=resolve(out,'web');
 await exportGame({content:c,projectRoot:root,destination,target:'web',runtimes:{web:resolve(process.env.WEB_TEST_RUNTIME??'dist/web')}});
 const mime={'.html':'text/html','.js':'text/javascript','.mjs':'text/javascript','.wasm':'application/wasm','.json':'application/json','.css':'text/css'};
 const server=createServer(async(req,res)=>{try{const path=decodeURIComponent(new URL(req.url,'http://127.0.0.1').pathname),file=resolve(destination,'.'+(path==='/'?'/index.html':path));if(!file.startsWith(destination+sep)){res.writeHead(403).end();return;}const bytes=await readFile(file);res.writeHead(200,{'Content-Type':mime[extname(file)]??'application/octet-stream'});res.end(bytes);}catch{res.writeHead(404).end();}});
 await new Promise(r=>server.listen(0,'127.0.0.1',r));t.after(()=>{server.closeAllConnections();server.close();});
 const browser=await chromium.launch({channel:process.env.PLAYWRIGHT_CHANNEL??'msedge',headless:true});t.after(()=>browser.close());
 const reports=[];
 for(const acceptor of ['apprentice','mentor']){
  const context=await browser.newContext({viewport:{width:1280,height:800},acceptDownloads:true});let page=await context.newPage();const errors=[];const watch=p=>p.on('pageerror',e=>errors.push(e.message));watch(page);
  const url=`http://127.0.0.1:${server.address().port}/`,element=id=>page.locator(`[data-element-id="${id}"]`),click=id=>element(id).getByRole('button').click();
  const state=()=>page.evaluate(async()=>(await window.player.view()).view.state);
  const load=async file=>{const pending=page.waitForEvent('filechooser');await click('load');await(await pending).setFiles(file);};
  let saves=0,fresh=false,assigned=-1,layout=false;
  const restore=async()=>{
   const before=await state(),file=resolve(out,`${acceptor}-${saves++}.json`),pending=page.waitForEvent('download');await click('save');await(await pending).saveAs(file);
   const saved=JSON.parse(await readFile(file,'utf8'));assert.deepEqual(saved.state,before);assert.deepEqual(nativeReplay(c,saved.origin,saved.commands),{ok:true,state:before});
   if(!fresh){const old=page;page=await context.newPage();watch(page);await page.goto(url);await old.close();fresh=true;}
   await load(file);await page.waitForFunction(async revision=>(await window.player.view()).view.state?.stateRevision===revision,before.stateRevision);assert.deepEqual(await state(),before);return file;
  };
  await page.goto(url);await click('start');
  for(let guard=0;guard<80;guard++){
   const s=await state();if(s.phase==='planning'&&s.roster.completedEvents[acceptor].includes('personal-coda'))break;
   if(s.phase==='planning'||s.phase==='ready'){
    if(assigned!==s.periodIndex){for(const member of c.project.roster.members){const name=c.characters.find(x=>x.id===member.characterId).name;await page.getByRole('combobox',{name:name+' 활동',exact:true}).selectOption('study');}await page.getByRole('button',{name:'활동 배정 적용',exact:true}).click();assigned=s.periodIndex;}
    else await page.getByRole('button',{name:'배정한 활동 함께 실행',exact:true}).click();
   }else if(s.phase==='narrative'){
    const event=c.events.find(e=>e.id===s.activeNarrative.id),node=event?.nodes.find(n=>n.id===s.activeNarrative.nodeId);
    if(event?.id==='personal-choice'||event?.id==='personal-follow'){
     assert.equal((await element('actor').innerText()).trim(),c.characters.find(x=>x.id===s.activeNarrative.actorId).name);await restore();
     if(!layout){for(const viewport of [{width:1280,height:800},{width:960,height:720}]){await page.setViewportSize(viewport);for(const id of ['actor','compact','padded']){const box=await element(id).evaluate(e=>({height:e.clientHeight,scroll:e.scrollHeight}));assert.ok(box.scroll<=box.height,`${id} must fit without a spurious scrollbar`);}const box=await element('long').evaluate(e=>({height:e.clientHeight,scroll:e.scrollHeight,leading:getComputedStyle(e).lineHeight,overflow:getComputedStyle(e).overflowY}));assert.ok(box.scroll>box.height);assert.equal(box.leading,'33px');assert.equal(box.overflow,'auto');}await page.setViewportSize({width:1280,height:800});layout=true;}
    }
    if(node?.type==='choice'){const choice=event.id==='personal-choice'?choices[s.activeNarrative.actorId===acceptor?0:1]:node.choices[0];await element('choices').getByRole('button').filter({hasText:choice.text}).click();}
    else await click('advance');
   }else if(s.phase==='activity-result'||s.phase==='period-end')await click('advance');
   else assert.fail(`unexpected phase ${s.phase}`);
  }
  const final=await state(),rejector=acceptor==='apprentice'?'mentor':'apprentice';assert.ok(final.roster.completedEvents[acceptor].includes('personal-coda'));assert.ok(!final.roster.completedEvents[rejector].includes('personal-follow'));assert.equal(final.roster.flags[acceptor]['personal-promise'],true);assert.equal(final.roster.flags[rejector]['personal-promise'],false);assert.ok(layout);
  const file=await restore(),tampered=JSON.parse(await readFile(file,'utf8'));tampered.state.roster.flags[rejector]['personal-promise']=true;const bad=resolve(out,`${acceptor}-tampered.json`);await writeFile(bad,JSON.stringify(tampered));await load(bad);await page.getByRole('alert').waitFor();assert.deepEqual(await state(),final);assert.deepEqual(errors,[]);
  reports.push({acceptor,saves,freshRestore:fresh,statesMatchNative:true,wrongActorBlocked:true,tamperRejected:true});await context.close();
 }
 await writeFile(resolve(out,'verification.json'),JSON.stringify(reports,null,2));
});
