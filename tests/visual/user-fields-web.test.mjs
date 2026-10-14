import test from 'node:test';
import assert from 'node:assert/strict';
import {build} from 'esbuild';
import {chromium} from 'playwright';

test('creation inputs keep local Unicode drafts, submit once, retain failures and show loaded values as text',async t=>{
  const bundle=await build({stdin:{contents:`
    import React from 'react';
    import {createRoot} from 'react-dom/client';
    import {VisualPlayer} from './apps/player/renderer/VisualPlayer.tsx';
    const root=createRoot(document.getElementById('root'));
    window.calls=[];window.commands=[];
    const field={id:'player-name',type:'text',label:'주인공 이름',initial:'처음',minLength:1,maxLength:8};
    const design={width:640,height:360,screens:[{id:'title',label:'Title',elements:[
      {id:'name',kind:'input',userFieldId:'player-name',x:20,y:20,width:500,height:50},
      {id:'greeting',kind:'text',binding:'user-field',userFieldId:'player-name',valueFormat:'안녕 {value} / {value}',x:20,y:90,width:600,height:50},
      {id:'start',kind:'button',action:'start',text:'Start',x:20,y:160,width:200,height:50}
    ]}]};
    let props={view:{generation:1,title:'User fields',description:'',state:null,initialState:{},attributes:[],characters:[],activities:[],systems:null,userFields:[field],userValues:{'player-name':'처음'}},design,busy:false,plan:['rest','rest','rest'],onPlan:()=>{},act:a=>window.commands.push(a),newRun:values=>window.calls.push(values),save:()=>{},load:()=>{},sound:()=>{},muted:true,preview:false,error:null,notice:null};
    window.update=p=>{props={...props,...p,view:{...props.view,...p.view}};root.render(<VisualPlayer {...props}/>);};
    window.update({});
  `,resolveDir:process.cwd(),loader:'tsx'},bundle:true,write:false,format:'iife',loader:{'.css':'empty'},logLevel:'silent'});
  const browser=await chromium.launch({headless:true,...(process.env.PLAYWRIGHT_CHANNEL?{channel:process.env.PLAYWRIGHT_CHANNEL}:{})});t.after(()=>browser.close());
  const page=await browser.newPage({viewport:{width:900,height:600}});
  await page.setContent('<div id="root"></div>');await page.addScriptTag({content:bundle.outputFiles[0].text});
  const input=page.getByRole('textbox',{name:'주인공 이름'});
  await input.waitFor();assert.equal(await input.inputValue(),'처음');
  // Do not use HTML maxLength: it counts UTF-16 units and can split non-BMP text.
  assert.equal(await input.getAttribute('maxlength'),null);
  await input.evaluate(element=>window.originalInput=element);
  const name='미😀<b>$&아주길게';await input.fill(name);
  assert.equal(await input.evaluate(element=>element===window.originalInput&&element===document.activeElement),true);
  assert.equal(await input.inputValue(),name);assert.equal(await page.locator('[data-element-id="greeting"]').textContent(),`안녕 ${name} / ${name}`);
  assert.equal(await page.locator('[data-element-id="greeting"] b').count(),0);assert.deepEqual(await page.evaluate(()=>window.commands),[]);assert.deepEqual(await page.evaluate(()=>window.calls),[]);
  await page.getByRole('button',{name:'Start',exact:true}).click();assert.deepEqual(await page.evaluate(()=>window.calls),[{'player-name':name}]);
  await page.evaluate(()=>window.update({busy:true}));await page.waitForFunction(()=>document.querySelector('input').disabled);
  await page.evaluate(()=>window.update({busy:false,error:'이름은 1~8자입니다. /userValues/player-name'}));await page.getByRole('alert').waitFor();assert.equal(await input.inputValue(),name);
  // Same-title declarations refreshed after a failed reply must not discard the draft.
  await page.evaluate(()=>window.update({view:{userFields:[{id:'player-name',type:'text',label:'주인공 이름',initial:'처음',minLength:1,maxLength:8}]}}));assert.equal(await input.inputValue(),name);
  await input.fill('수정🌸');await page.getByRole('button',{name:'Start',exact:true}).click();assert.deepEqual(await page.evaluate(()=>window.calls),[{'player-name':name},{'player-name':'수정🌸'}]);
  await page.evaluate(()=>window.update({error:null,view:{state:{phase:'ready',stateRevision:1,attributes:{},relationships:{},gameplay:{screenId:'title'},userValues:{'player-name':'불러온 이름'},presentation:{portraits:[]}}}}));
  await page.waitForFunction(()=>document.querySelector('input').value==='불러온 이름');assert.equal(await input.isDisabled(),true);assert.equal(await page.locator('[data-element-id="greeting"]').textContent(),'안녕 불러온 이름 / 불러온 이름');
  await page.getByRole('button',{name:'Start',exact:true}).click();assert.deepEqual(await page.evaluate(()=>window.calls.at(-1)),{'player-name':'불러온 이름'});
  assert.deepEqual(await page.evaluate(()=>window.commands),[]);
  // Loading a run clears the old creation draft; a subsequent title does not leak it.
  await page.evaluate(()=>window.update({view:{state:null}}));assert.equal(await input.inputValue(),'불러온 이름');
  await page.evaluate(()=>window.update({view:{generation:2,userFields:[],userValues:undefined,state:null}}));await page.getByRole('button',{name:'Start',exact:true}).click();assert.equal(await page.evaluate(()=>window.calls.at(-1)===undefined),true);
});
