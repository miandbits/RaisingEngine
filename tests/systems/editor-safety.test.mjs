import test from 'node:test';
import assert from 'node:assert/strict';
import { createServer } from 'vite';
import { chromium } from 'playwright';
import { resolve } from 'node:path';

// Exercise the real React components in an isolated browser, without rebuilding
// or launching the user's running Electron editor.
test('system controls fall back, equipped items can be removed, and expedition linking preserves success data', async t => {
  const source = `
    import React from 'react';
    import {createRoot} from 'react-dom/client';
    import {VisualPlayer} from '/apps/player/renderer/VisualPlayer.tsx';
    import {SystemsEditor} from '/apps/studio/renderer/SystemsEditor.tsx';
    import {SystemsWidget} from '/apps/player/renderer/SystemsWidget.tsx';
    import {SchemaSection} from '/apps/studio/renderer/SchemaForm.tsx';
    const root=createRoot(document.getElementById('root'));
    const noop=()=>{};
    window.commands=[];window.changes=[];
    window.showStory=()=>root.render(React.createElement(SchemaSection,{schemaRef:'event.schema.json',value:{nodes:[{id:'end',type:'end'}]},document:{kind:'event',file:'events/story.json',data:{nodes:[{id:'end',type:'end'}]}},documents:[],onChange:noop}));
    window.showEquipment=(canUnequip)=>root.render(React.createElement(SystemsWidget,{kind:'equipment',busy:false,editing:false,onCommand:c=>window.commands.push(c),systems:{heroStats:{maxHp:50,attack:10,defense:1},equipment:[{id:'sword',name:'검',slot:'weapon',owned:1,equipped:true,enabled:false,canUnequip,reason:canUnequip?'이미 장착한 장비입니다.':'현재 단계에서는 장비를 변경할 수 없습니다.'}]}}));
    window.showPlayer=(phase,visibility)=>{
      const element={id:'authored-control',kind:phase,x:0,y:0,width:900,height:500,...visibility};
      const systems={heroStats:{maxHp:50,attack:10,defense:1},equipment:[],quests:[],consumables:[],stages:[],lastBattle:null,
        battle:{enemyName:'슬라임',turn:1,heroHp:50,heroMaxHp:50,enemyHp:10,enemyMaxHp:10},
        expedition:{name:'숲',nodeName:'입구',heroHp:50,stats:{maxHp:50},connections:[],canReturn:true}};
      const view={title:'테스트',description:'',characters:[],attributes:[],activities:[],initialState:{},time:null,
        state:{phase,presentation:{portraits:[],backgroundAssetId:null}},systems};
      root.render(React.createElement(VisualPlayer,{view,design:{width:1280,height:720,screens:[{id:phase,elements:[element]}]},busy:false,plan:['','',''],onPlan:noop,act:c=>window.commands.push(c),newRun:noop,save:noop,load:noop,sound:noop,muted:true,preview:true,error:null,notice:null}));
    };
    window.showEditor=()=>{
      const successCheck={chance:{op:'literal',value:5000}};
      const rewards={successEffects:[{op:'add',target:{kind:'attribute',attributeId:'skill'},value:{op:'literal',value:2}}],failureEffects:[]};
      const documents=[{kind:'project',file:'project.json',data:{gameplay:{items:[],locations:[]},systems:{hero:{},equipment:[],consumables:[],enemies:[],expeditions:[{id:'forest',name:'숲',locationId:'home',entryNodeId:'entrance',nodes:[{id:'entrance',name:'입구',connections:[],exit:true}]}],quests:[],relationshipStages:[]}}},
        {kind:'activity',file:'checked.json',data:{id:'checked',label:'성공 판정 활동',successCheck,...rewards}},
        {kind:'activity',file:'plain.json',data:{id:'plain',label:'탐험 활동',baseEffects:[]}},
        {kind:'activity',file:'linked.json',data:{id:'linked',label:'기존 연결 활동',expeditionId:'forest',successCheck,...rewards}}];
      window.originalActivities=structuredClone(documents.slice(1));
      const render=()=>root.render(React.createElement(SystemsEditor,{view:{documents},focus:{category:'expeditions',id:'forest',filter:''},onFocus:noop,onPreview:noop,onWorkshop:noop,onChange:(doc,value)=>{window.changes.push({file:doc.file,value});documents.find(d=>d.file===doc.file).data=value;render();}}));render();
    };
  `;
  const server = await createServer({ configFile: false, root: resolve('.'), cacheDir: resolve('.cache/systems-editor-review-vite'),
    // This virtual-entry test must not crawl local projects or exported source copies.
    optimizeDeps: { entries: [], include: ['react', 'react-dom/client'] },
    server: { host: '127.0.0.1', port: 0, watch: null }, appType: 'custom',
    plugins: [{ name: 'systems-review-fixture', resolveId(id) { if (id === '/review-entry.js') return id; }, load(id) { if (id === '/review-entry.js') return source; }, configureServer(server) {
      server.middlewares.use((req,res,next) => { if (req.url !== '/') return next(); res.setHeader('Content-Type','text/html'); res.end('<div id="root"></div><script type="module" src="/review-entry.js"></script>'); });
    } }] });
  await server.listen(); t.after(() => server.close());
  const browser = await chromium.launch({ channel: 'chrome', headless: true }); t.after(() => browser.close());
  const page = await browser.newPage(); const errors=[]; page.on('pageerror',e=>errors.push(e.message));
  await page.goto(`http://127.0.0.1:${server.httpServer.address().port}`); await page.waitForFunction(()=>window.showPlayer);
  for (const phase of ['battle','expedition']) {
    for (const visibility of [{hidden:true},{visibleWhen:'planning'}]) {
      await page.evaluate(({phase,visibility})=>window.showPlayer(phase,visibility),{phase,visibility});
      const button=page.getByRole('button',{name:phase==='battle'?'공격':'탐험에서 귀환',exact:true});
      await button.click();
      assert.deepEqual(await page.evaluate(()=>window.commands.pop()),phase==='battle'?{type:'battleAction',action:'attack'}:{type:'returnFromExpedition'});
    }
    await page.evaluate(phase=>window.showPlayer(phase,{visibleWhen:'always'}),phase);
    await page.locator('[data-element-id="authored-control"]').waitFor();
  }
  await page.evaluate(()=>window.showEquipment(true));
  await page.getByRole('button',{name:'장착 해제 · 검',exact:true}).click();
  assert.deepEqual(await page.evaluate(()=>window.commands.pop()),{type:'unequip',slot:'weapon'});
  assert.equal(await page.getByText('이미 장착한 장비입니다.',{exact:true}).count(),0);
  await page.evaluate(()=>window.showEquipment(false));
  await page.waitForFunction(()=>document.querySelector('button')?.disabled===true);
  assert.equal(await page.getByRole('button',{name:'장착 해제 · 검',exact:true}).isDisabled(),true);
  await page.evaluate(()=>window.showEditor());
  await page.getByRole('button',{name:'탐험 노드 추가',exact:true}).waitFor();
  assert.equal(await page.getByRole('button',{name:'대본 노드 추가',exact:true}).count(),0);
  assert.equal(await page.getByRole('checkbox',{name:'성공 판정 활동 탐험 연결',exact:true}).isDisabled(),true);
  await page.getByRole('checkbox',{name:'탐험 활동 탐험 연결',exact:true}).check();
  await page.getByRole('checkbox',{name:'기존 연결 활동 탐험 연결',exact:true}).uncheck();
  const changes=await page.evaluate(()=>window.changes);
  assert.deepEqual(changes.map(c=>c.file),['plain.json','linked.json']);
  assert.equal(changes[0].value.expeditionId,'forest');
  const original=await page.evaluate(()=>window.originalActivities.find(d=>d.file==='linked.json').data);delete original.expeditionId;
  assert.deepEqual(changes[1].value,original,'unlink must preserve authored success check and both effect lists');
  await page.evaluate(()=>window.showStory());
  await page.getByRole('button',{name:'대본 노드 추가',exact:true}).waitFor();
  assert.equal(await page.getByRole('button',{name:'탐험 노드 추가',exact:true}).count(),0);
  assert.deepEqual(errors,[]);
});
