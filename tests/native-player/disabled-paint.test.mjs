import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {controlColorsFixture,controlPalettes} from '../fixtures/control-colors.mjs';
import {surfaceFixture,readRaster,pixel} from '../fixtures/surfaces.mjs';
import {decodePng,encodeRgbaPng} from '../../dist/packages/studio-service/png.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {loadProject,buildSnapshot} from '../../dist/packages/project-service/index.js';
import {desktopTarget,gameDirectory,playerExecutable} from '../../dist/packages/export-service/targets.js';

const at=(report,name)=>report.steps.find(step=>step.command.checkpoint===name).ui;
const node=(ui,id)=>ui.nodes.find(value=>value.id===id);
const touch=id=>({scrollIntoView:id,touchWindow:id,waitFrames:3});
const unchanged=(before,after)=>{assert.deepEqual(after.state,before.state);assert.deepEqual(after.commands,before.commands);};
const sameBox=(before,after)=>{for(const key of ['x','y','width','height','hitbox'])assert.deepEqual(after[key],before[key],key);};
const sample=(image,ui,x,y)=>pixel(image,x*image.width/ui.viewport.width,y*image.height/ui.viewport.height);
const partial={id:'controls-partial',label:'State colors only',hoverFill:'#abcdef',pressedFill:'#123456',disabledColor:'#cbbcaa'};

test('disabled generated and textured controls retain normal paint under hover and pressed states',{timeout:180000},async()=>{
  await mkdir('.cache/control-colors/native',{recursive:true});
  const out=await mkdtemp(resolve('.cache/control-colors/native/disabled-'));
  const target=desktopTarget(),runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??'dist/native-player-app'),cases=[];
  const fixture=await controlColorsFixture(join(out,'controls-project'));
  const exported=async(name,content,root)=>{const destination=join(out,name);await exportGame({content,projectRoot:root,target,destination,runtimes:{[target]:runtime}});return destination;};
  const run=async(name,destination,steps,args=[])=>{
    const input=join(out,name+'.json'),report=join(out,name+'-report.json');
    await writeFile(input,JSON.stringify(steps));
    const child=spawnSync(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--save',join(out,name+'-save.json'),'--automation',input,'--report',report,'--report-checkpoints-only','--hidden',...args],{encoding:'utf8',timeout:60000});
    await writeFile(join(out,name+'-stderr.log'),child.stderr??'');
    assert.equal(child.status,0,child.stderr||child.error?.message);const result=JSON.parse(await readFile(report));assert.equal(result.ok,true,result.error);return result;
  };
  for(const mode of ['light','partial']){
    const content=structuredClone(fixture.content),design=content.project.visual,palette=mode==='light'?controlPalettes.light:partial;
    design.styles.push(partial);for(const screen of design.screens)for(const element of screen.elements)if(element.controlStyleId)element.controlStyleId=palette.id;
    const destination=await exported(mode,content,fixture.root),id='records__slots-request';
    const result=await run(mode,destination,[touch('start'),touch('save'),{hover:'records__slots-manual'},{checkpoint:'enabled-hover'},touch('records__slots-auto'),{hover:'records__slots-manual'},{checkpoint:'disabled',screenshot:join(out,mode+'-disabled.png')},{hover:id},{checkpoint:'disabled-hover',screenshot:join(out,mode+'-disabled-hover.png')},{clickWindow:id},{checkpoint:'disabled-click'}]);
    const before=at(result,'disabled'),hover=at(result,'disabled-hover'),normal=node(before,id),hovered=node(hover,id);
    assert.equal(normal.enabled,false);assert.equal(hovered.backgroundColor,normal.backgroundColor,'Disabled '+mode+' save button must not gain enabled hover paint');assert.equal(hovered.color,normal.color);sameBox(normal,hovered);
    assert.equal(node(at(result,'enabled-hover'),'records__slots-manual').backgroundColor.toLowerCase(),palette.hoverFill,'Enabled hover remains functional');
    unchanged(before,hover);unchanged(before,at(result,'disabled-click'));assert.equal(at(result,'disabled-click').screenId,before.screenId);
    // Select display subparts have their own paint, so verify the actual raster too.
    const selectResult=await run(mode+'-select',destination,[touch('start'),...[0,1,2,3].map(index=>({select:'plan-'+index,value:'rest'})),touch('advance'),touch('advance'),{checkpoint:'disabled',screenshot:join(out,mode+'-select.png')},{hover:'plan-0'},{checkpoint:'hover',screenshot:join(out,mode+'-select-hover.png')},{clickWindow:'plan-0'},{checkpoint:'clicked'}]);
    const selectBefore=at(selectResult,'disabled'),selectHover=at(selectResult,'hover'),control=selectBefore.controls.find(value=>value.id==='plan-0');
    assert.equal(selectBefore.readingLayout.controls.find(value=>value.id==='plan-0').enabled,false);assert.deepEqual(selectHover.controls.find(value=>value.id==='plan-0'),control);
    const normalImage=await readRaster(join(out,mode+'-select.png')),hoverImage=await readRaster(join(out,mode+'-select-hover.png'));
    for(const [x,y] of [[control.x+8,control.y+8],[control.x+control.width-8,control.y+8]])assert.deepEqual(sample(hoverImage,selectHover,x,y),sample(normalImage,selectBefore,x,y),'Disabled selectvalue/arrow paint stays unchanged');
    unchanged(selectBefore,selectHover);unchanged(selectBefore,at(selectResult,'clicked'));
    cases.push({mode,generatedDisabledHover:true,disabledSelectRaster:true,disabledClicksPreserveState:true});
  }
  // The reading picker uses the global per-ID fallback rules, independently of
  // the widget scope. An empty menu slot makes Apply disabled until inspection.
  {
    const content=structuredClone(fixture.content),design=content.project.visual;design.styles.push(partial);design.readableLayout.controlStyleId=partial.id;
    for(const screen of design.screens)for(const element of screen.elements)delete element.controlStyleId;
    design.screens.find(screen=>screen.id==='schedule').elements.find(element=>element.id==='schedule').scheduleLayout='menu';
    const destination=await exported('reading-partial',content,fixture.root);
    const result=await run('reading-partial',destination,[{resize:{width:390,height:844}},touch('start'),touch('board-slot-0'),{hover:'board-cancel'},{checkpoint:'disabled',screenshot:join(out,'reading-disabled.png')},{hover:'board-apply'},{checkpoint:'hover',screenshot:join(out,'reading-disabled-hover.png')},{clickWindow:'board-apply'},{checkpoint:'clicked'},touch('board-activity-rest'),{hover:'board-apply'},{checkpoint:'enabled-hover',screenshot:join(out,'reading-enabled-hover.png')}],['--reading-layout','on','--text-scale','2']);
    const before=at(result,'disabled'),hover=at(result,'hover'),control=before.readingLayout.controls.find(value=>value.id==='board-apply');
    assert.equal(control.enabled,false);sameBox(control,hover.readingLayout.controls.find(value=>value.id==='board-apply'));
    const normalImage=await readRaster(join(out,'reading-disabled.png')),hoverImage=await readRaster(join(out,'reading-disabled-hover.png'));
    assert.deepEqual(sample(hoverImage,hover,control.x+8,control.y+8),sample(normalImage,before,control.x+8,control.y+8),'Global partial palette cannot recolor disabled Apply');
    unchanged(before,hover);unchanged(before,at(result,'clicked'));unchanged(before,at(result,'enabled-hover'));
    assert.deepEqual(at(result,'clicked').scheduleDraft,before.scheduleDraft);assert.equal(at(result,'clicked').readingLayout.pickerOpen,true);
    const enabled=at(result,'enabled-hover'),enabledControl=enabled.readingLayout.controls.find(value=>value.id==='board-apply');assert.equal(enabledControl.enabled,true);
    assert.deepEqual(sample(await readRaster(join(out,'reading-enabled-hover.png')),enabled,enabledControl.x+8,enabledControl.y+8),[171,205,239],'Global enabled hover remains functional');
    cases.push({mode:'reading-partial',textScale:2,disabledHoverRaster:true,reviewDoesNotApplyDraft:true});
  }
  const surfaces=await surfaceFixture(join(out,'surface-project'));
  // Transparent center proves fallback fill cannot leak through the frame.
  const texture=decodePng(surfaces.textures.nine);for(let y=12;y<24;y++)for(let x=12;x<24;x++)texture.pixels[(y*texture.width+x)*4+3]=0;
  await writeFile(join(surfaces.root,'assets/surface-nine.png'),encodeRgbaPng(texture));
  const surfaceSnapshot=await buildSnapshot(await loadProject(surfaces.root));assert.equal(surfaceSnapshot.ok,true,JSON.stringify(surfaceSnapshot.diagnostics));
  for(const disabled of [false,true])for(const reading of [false,true]){
    const content=structuredClone(surfaceSnapshot.snapshot),button=content.project.visual.screens[0].elements.find(element=>element.id==='open');if(disabled)button.enabledCondition={op:'not',condition:{op:'always'}};
    const name='surface-'+(disabled?'disabled':'enabled')+'-'+(reading?'reading':'canvas'),destination=await exported(name,content,surfaces.root),args=reading?['--reading-layout','on','--text-scale','2']:[];
    const result=await run(name,destination,[{resize:reading?{width:390,height:844}:{width:1280,height:800}},{hover:reading?'node-paper':'node-cleared'},{checkpoint:'normal',screenshot:join(out,name+'-normal.png')},{hover:'open'},{checkpoint:'hover',screenshot:join(out,name+'-hover.png')},{clickWindow:'open'},{checkpoint:'clicked'}],args);
    const pressed=at(await run(name+'-pressed',destination,[{resize:reading?{width:390,height:844}:{width:1280,height:800}},{waitFrames:4},{checkpoint:'pressed',screenshot:join(out,name+'-pressed.png')}],[...args,'--preview-screen','title','--preview-element','open','--preview-state','pressed']),'pressed');
    const before=at(result,'normal'),hover=at(result,'hover'),normal=node(before,'open'),hovered=node(hover,'open'),press=node(pressed,'open');assert.equal(normal.enabled,!disabled);sameBox(normal,hovered);sameBox(normal,press);
    if(disabled){
      assert.equal(hovered.backgroundColor,normal.backgroundColor,'Disabled authored hover must retain the transparent normal fill');assert.equal(press.backgroundColor,normal.backgroundColor,'Disabled authored pressed preview must retain normal fill');
      const normalImage=await readRaster(join(out,name+'-normal.png')),hoverImage=await readRaster(join(out,name+'-hover.png')),pressedImage=await readRaster(join(out,name+'-pressed.png'));
      for(const [x,y] of [[normal.x+10,normal.y+10],[normal.x+normal.width/2,normal.y+normal.height-20]]){assert.deepEqual(sample(hoverImage,hover,x,y),sample(normalImage,before,x,y),'Disabled nine-slice remains painted');assert.deepEqual(sample(pressedImage,pressed,x,y),sample(normalImage,before,x,y),'Disabled pressed paint retains frame and transparent center');}
      unchanged(before,at(result,'clicked'));assert.equal(at(result,'clicked').screenId,before.screenId);
    }else{
      assert.equal(hovered.backgroundColor.toLowerCase(),'#337744');assert.equal(press.backgroundColor.toLowerCase(),'#992244');assert.equal(at(result,'clicked').screenId,'detail');unchanged(before,at(result,'clicked'));
    }
    cases.push({mode:name,disabled,reading,hoverAndPressed:true,geometryUnchanged:true});
  }
  await writeFile(join(out,'proof.json'),JSON.stringify({ok:true,cases,pressedMethod:'Existing editor state preview; hover and disabled clicks use actual native pointer events.'},null,2));
  console.log(JSON.stringify({evidence:out}));
});
