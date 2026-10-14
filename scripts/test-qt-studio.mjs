import { spawnSync } from 'node:child_process';
import { mkdir, mkdtemp, readFile } from 'node:fs/promises';
import path from 'node:path';
import { QtStudioService } from '../apps/qt-studio/service.mjs';
const root=path.resolve('.');
await mkdir('.cache/qt-ui',{recursive:true});
const project=await mkdtemp(path.resolve('.cache/qt-ui/project-'));
await new QtStudioService().handle({method:'create',params:{root:project}});
const qt=process.env.QT_ROOT??'C:/Qt/6.10.1/mingw_64';
const result=spawnSync(path.resolve(process.env.QT_STUDIO_OUT??'dist/qt-studio','qt-studio-test.exe'),[...process.argv.slice(2),'-o','.cache/qt-ui-tests.txt,txt'],{
  env:{...process.env,QTEST_FUNCTION_TIMEOUT:process.env.QTEST_FUNCTION_TIMEOUT??(process.argv.includes('workbenchAnalysis5000')?'2400000':'300000'),RAISING_ENGINE_ROOT:root,RAISING_QT_PROJECT:project,RAISING_QT_TEST:'1',PATH:`${qt}/bin;${process.env.PATH}`},
  cwd:root,windowsHide:true,timeout:Number(process.env.QT_TEST_TIMEOUT_MS??(process.argv.includes('workbenchAnalysis5000')?'2400000':'120000')),encoding:'utf8'});
if(result.error)throw result.error;
console.log(await readFile('.cache/qt-ui-tests.txt','utf8'));
process.exitCode=result.status??1;
