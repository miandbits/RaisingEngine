import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile,copyFile} from 'node:fs/promises';
import {existsSync} from 'node:fs';
import {resolve,join,dirname} from 'node:path';
import {spawnSync} from 'node:child_process';
import {phaseForecastFixture} from '../fixtures/phase-forecast-ui.mjs';
import {saveStorageGuidanceText} from '../../dist/packages/player-service/save-storage-guidance.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {desktopTarget,gameDirectory,playerExecutable} from '../../dist/packages/export-service/targets.js';

test('native storage guidance agrees with Web templates for local files, genuine portable availability and previews',{timeout:90000},async()=>{
  await mkdir('.cache/save-storage-guidance',{recursive:true});
  const out=await mkdtemp(resolve('.cache/save-storage-guidance/parity-')),source=join(out,'probe.cpp'),exe=join(out,process.platform==='win32'?'probe.exe':'probe');
  await writeFile(source,`#include <iostream>
#include <string>
struct Probe {
  std::string locale; bool portable_available=false,save_storage_preview=false;
  std::string settings_words(const char* ko,const char* en,const char* ja) const {return locale.rfind("en",0)==0?en:locale.rfind("ja",0)==0?ja:ko;}
#include "save-storage-guidance.inc"
};
int main(int argc,char** argv){if(argc!=5)return 2;Probe p;p.locale=argv[1];p.portable_available=std::string(argv[2])=="1";p.save_storage_preview=std::string(argv[3])=="1";std::cout<<p.save_storage_guidance(std::string(argv[4])=="1");}
`);
  const mingw=join(process.env.QT_COMPILER??'C:/Program Files/mingw64/bin','g++.exe');
  const compiler=process.env.CXX??(process.platform==='win32'?(existsSync(mingw)?mingw:'g++'):'c++');
  const build=spawnSync(compiler,['-std=c++17','-finput-charset=UTF-8','-I',resolve('native/player'),source,'-o',exe],{encoding:'utf8',timeout:60000});assert.equal(build.status,0,build.stderr||build.error?.message);
  for(const locale of ['ko','en','ja','en-US'])for(const portable of [false,true])for(const preview of [false,true])for(const controls of [false,true]){
    const run=spawnSync(exe,[locale,portable?'1':'0',preview?'1':'0',controls?'1':'0'],{encoding:'utf8',timeout:10000});assert.equal(run.status,0,run.stderr||run.error?.message);
    assert.equal(run.stdout,saveStorageGuidanceText({location:preview?'preview':'device',portableBackup:portable&&controls},locale));
  }
});

test('shipping native renderer resolves direct/shared storage guidance and rejects older runtime exports',{timeout:90000},async()=>{
  await mkdir('.cache/save-storage-guidance',{recursive:true});const out=await mkdtemp(resolve('.cache/save-storage-guidance/native-'));
  const f=await phaseForecastFixture(join(out,'project'),{opted:false}),content=structuredClone(f.content),visual=content.project.visual;
  const note={id:'note',kind:'text',x:40,y:40,width:1100,height:100,fontSize:20,binding:'save-storage-guidance',text:'BROWSER ONLY STALE COPY'};
  const title=visual.screens.find(s=>s.id==='title');title.elements=[note,{id:'shared',kind:'component',componentId:'footer',x:0,y:180,width:1180,height:140}];delete title.readableLayout;
  visual.components=[{id:'footer',label:'Footer',width:1180,height:140,elements:[note]}];
  const target=desktopTarget(),runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app'),destination=join(out,'game');
  await exportGame({content,projectRoot:f.root,target,destination,runtimes:{windows:runtime,macos:runtime,web:runtime}});
  for(const locale of ['en','ko','ja']){
    const script=join(out,locale+'.json'),report=join(out,locale+'-report.json');await writeFile(script,JSON.stringify([{checkpoint:'storage-guidance'}]));
    const run=spawnSync(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--save',join(out,locale+'-save.json'),'--locale',locale,'--hidden','--automation',script,'--report',report,'--report-checkpoints-only'],{encoding:'utf8',timeout:30000});assert.equal(run.status,0,run.stderr||run.error?.message);
    const data=JSON.parse(await readFile(report,'utf8'));assert.equal(data.ok,true,data.error);
    const expected=saveStorageGuidanceText({location:'device',portableBackup:false},locale);
    assert.equal(data.final.texts.note,expected);assert.equal(data.final.texts.shared__note,expected);
    assert.equal(data.final.state,null);assert.deepEqual(data.final.commands,[]);
  }
  const old=join(out,'old-runtime');await mkdir(dirname(playerExecutable(target,old)),{recursive:true});await copyFile(playerExecutable(target,runtime),playerExecutable(target,old));const caps=JSON.parse(await readFile(join(runtime,'capabilities.json'),'utf8'));caps.visualFeatures=caps.visualFeatures.filter(f=>f!=='save-storage-guidance-v1');await writeFile(join(old,'capabilities.json'),JSON.stringify(caps));
  await assert.rejects(exportGame({content,projectRoot:f.root,target,destination:join(out,'rejected'),runtimes:{windows:old,macos:old,web:old}}),/save-storage-guidance-v1/);
  console.log(JSON.stringify({evidence:out}));
});
