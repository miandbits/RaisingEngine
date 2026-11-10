import { spawn } from 'node:child_process';
import { existsSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import {sourceEditorLaunch} from '../apps/qt-studio/editor-identity.mjs';
const root=fileURLToPath(new URL('..',import.meta.url));
const launch=sourceEditorLaunch(root,{output:process.env.QT_STUDIO_OUT??'dist/qt-studio',args:process.argv.slice(2)});
const {executable}=launch;
if(!existsSync(executable)){
  console.error(`Qt editor not found: ${executable}\nRun npm run build:qt-studio with the same QT_STUDIO_OUT first.`);
  process.exit(1);
}
const child=spawn(executable,launch.args,{cwd:launch.cwd,stdio:'inherit',env:{...process.env,RAISING_NODE:process.env.RAISING_NODE??process.execPath}});
child.on('error',error=>{console.error(error.message);process.exitCode=1;});
child.on('exit',code=>{process.exitCode=code??1;});
