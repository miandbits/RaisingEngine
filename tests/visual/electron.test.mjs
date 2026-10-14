import test from 'node:test';
import assert from 'node:assert/strict';
import { _electron } from 'playwright';
import { mkdir,mkdtemp,readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
const output=resolve('.cache/visual-ui');const env={...process.env};delete env.NODE_OPTIONS;delete env.ELECTRON_RUN_AS_NODE;
test('visual editor drag, resize, scene authoring, assets, real controls, save/load and ending',{timeout:180000},async t=>{
 await mkdir(output,{recursive:true});const root=await mkdtemp(`${output}/project-`);const save=resolve(root,'preview-save.json');
 const app=await _electron.launch({...(process.env.STUDIO_EXECUTABLE?{executablePath:resolve(process.env.STUDIO_EXECUTABLE),args:[]}:{args:['dist/studio/main/index.js']}),cwd:resolve('.'),env});
 t.after(async()=>{await app.evaluate(({app})=>app.exit(0)).catch(()=>{});});const page=await app.firstWindow();const errors=[];page.on('pageerror',e=>errors.push(e.message));
 await page.waitForSelector('.workspace-status');await app.evaluate(({dialog},{root,save,image})=>{dialog.showOpenDialog=async(_w,options)=>({canceled:false,filePaths:[options.title==='이미지 가져오기'?image:options.properties.includes('openFile')?save:root]});dialog.showSaveDialog=async()=>({canceled:false,filePath:save});dialog.showMessageBox=async()=>({response:1});},{root,save,image:resolve('assets/visual-sample/courtyard.png')});
 await page.getByRole('button',{name:'비주얼 샘플 새 프로젝트',exact:true}).click();await page.waitForFunction(()=>document.querySelector('.workspace-status')?.textContent.includes('project-'));
 await page.getByRole('button',{name:'화면·연출',exact:true}).click();await page.locator('.visual-stage').waitFor();
 await page.getByRole('button',{name:'이미지 가져오기',exact:true}).click();await page.waitForFunction(async()=>{const r=await window.studio.view();return r.view.documents.find(d=>d.kind==='assets').data.assets.some(a=>a.id.startsWith('image-'));});
 await page.waitForFunction(()=>[...document.querySelectorAll('.visual-stage img')].length>=2&&[...document.querySelectorAll('.visual-stage img')].every(i=>i.complete&&i.naturalWidth>0));
 await page.locator('.visual-layers button').filter({hasText:'stats'}).first().click();
 const originalX=Number(await page.getByLabel('UI x',{exact:true}).inputValue());const box=await page.getByRole('button',{name:'배치 stats',exact:true}).boundingBox();
 await page.mouse.move(box.x+box.width/2,box.y+box.height/2);await page.mouse.down();await page.mouse.move(box.x+box.width/2+22,box.y+box.height/2+12,{steps:4});await page.mouse.up();
 await page.waitForFunction(x=>Number(document.querySelector('input[aria-label="UI x"]').value)>x,originalX);
 const beforeWidth=Number(await page.getByLabel('UI width',{exact:true}).inputValue());const handle=await page.locator('.visual-handle.selected i').boundingBox();
 await page.mouse.move(handle.x+handle.width/2,handle.y+handle.height/2);await page.mouse.down();await page.mouse.move(handle.x+handle.width/2+15,handle.y+handle.height/2,{steps:4});await page.mouse.up();await page.waitForFunction(w=>Number(document.querySelector('input[aria-label="UI width"]').value)>w,beforeWidth);
 await page.getByRole('button',{name:'실행 취소',exact:true}).click();await page.waitForFunction(w=>Number(document.querySelector('input[aria-label="UI width"]').value)===w,beforeWidth);
 await page.getByRole('button',{name:'다시 실행',exact:true}).click();await page.waitForFunction(w=>Number(document.querySelector('input[aria-label="UI width"]').value)>w,beforeWidth);
 await page.getByLabel('편집 화면',{exact:true}).selectOption('story');await page.locator('.visual-layers button').filter({hasText:'portrait-right'}).click();await page.getByLabel('UI 등장 효과',{exact:true}).selectOption('slide-right');
 await page.getByRole('button',{name:'장면·연출',exact:true}).click();await page.getByLabel('연출 대상',{exact:true}).selectOption('activities/read.json');await page.getByLabel('장면 배경',{exact:true}).selectOption('visual-library');
 await page.getByLabel('장면 right 인물',{exact:true}).selectOption('mentor/visual-neutral');
 await page.getByRole('button',{name:'초안 저장',exact:true}).click();await page.waitForFunction(()=>document.querySelector('.workspace-status')?.textContent.includes('저장된 상태'));
 const project=JSON.parse(await readFile(resolve(root,'project.json'),'utf8'));assert.ok(project.visual.screens.find(s=>s.id==='schedule').elements.find(e=>e.id==='stats').x>originalX);assert.equal(project.visual.screens.find(s=>s.id==='story').elements.find(e=>e.id==='portrait-right').motion,'slide-right');
 await page.screenshot({path:`${output}/editor.png`});
 await page.getByRole('button',{name:'실제 플레이로 확인',exact:true}).click();await page.locator('.visual-stage[data-screen=title]').waitFor();await page.getByRole('button',{name:'새 게임 시작',exact:true}).click();await page.locator('.visual-stage[data-screen=schedule]').waitFor();
 await page.waitForFunction(()=>[...document.querySelectorAll('.visual-stage img')].every(i=>i.complete&&i.naturalWidth>0));await page.screenshot({path:`${output}/player.png`});
 for(const period of ['상순','중순','하순'])await page.getByLabel(`${period} 활동`,{exact:true}).selectOption('rest');
 await page.getByRole('button',{name:'일정 확정 / 활동 시작',exact:true}).click();await page.getByRole('button',{name:'일정 확정 / 활동 시작',exact:true}).click();await page.getByRole('button',{name:'다음으로',exact:true}).waitFor();
    await page.getByRole('button',{name:'저장',exact:true}).click();await page.locator('[role=status]').filter({hasText:'저장'}).first().waitFor();
 const saved=await page.evaluate(async()=>JSON.stringify((await window.player.view()).view.state));await page.getByRole('button',{name:'다음으로',exact:true}).click();
 await page.getByRole('button',{name:'불러오기',exact:true}).click();await page.waitForFunction(async expected=>JSON.stringify((await window.player.view()).view.state)===expected,saved);
 let sawChoice=false,sawStory=false;
 for(let i=0;i<230;i++){
  const v=await page.evaluate(async()=>(await window.player.view()).view);if(v.state.phase==='finished')break;
  if(v.state.phase==='narrative'){
    sawStory=true;if(v.narrative.node.type==='choice'){sawChoice=true;await page.locator('.visual-choices button:enabled').first().click();}
    else await page.getByRole('button',{name:'계속',exact:true}).click();
  } else if(['planning','ready'].includes(v.state.phase)){
    for(const [j,period] of ['상순','중순','하순'].entries())if(j>=v.time.periodInMonth)await page.getByLabel(`${period} 활동`,{exact:true}).selectOption('rest');
    await page.getByRole('button',{name:'일정 확정 / 활동 시작',exact:true}).click();
  } else await page.getByRole('button',{name:v.state.phase==='activity-result'?'다음으로':'일정 확정 / 활동 시작',exact:true}).click();
 }
 const final=await page.evaluate(async()=>(await window.player.view()).view);assert.equal(final.state.phase,'finished');assert.ok(sawStory);assert.ok(sawChoice);await page.locator('.visual-stage[data-screen=ending]').waitFor();await page.screenshot({path:`${output}/ending.png`});assert.deepEqual(errors,[]);
 await app.evaluate(({BrowserWindow})=>BrowserWindow.getAllWindows()[0].setSize(1000,700));await page.waitForTimeout(200);const stage=await page.locator('.visual-stage').boundingBox();const viewport=page.viewportSize()??await page.evaluate(()=>({width:innerWidth,height:innerHeight}));assert.ok(stage.x>=-1&&stage.y>=-1&&stage.x+stage.width<=viewport.width+1&&stage.y+stage.height<=viewport.height+1);await page.screenshot({path:`${output}/minimum.png`});
});
