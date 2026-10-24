import { spawn } from 'node:child_process';
import { existsSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
const root=fileURLToPath(new URL('..',import.meta.url));
const output=path.resolve(root,process.env.QT_STUDIO_OUT??'dist/qt-studio');
const executable=path.join(output,process.platform==='darwin'?'RaisingStudioQt.app/Contents/MacOS/RaisingStudioQt':process.platform==='win32'?'RaisingStudioQt.exe':'RaisingStudioQt');
if(!existsSync(executable)){
  console.error(`Qt editor not found: ${executable}\nRun npm run build:qt-studio with the same QT_STUDIO_OUT first.`);
  process.exit(1);
}
const child=spawn(executable,['--engine-root',root,...process.argv.slice(2)],{cwd:root,stdio:'inherit',env:{...process.env,RAISING_NODE:process.env.RAISING_NODE??process.execPath}});
child.on('error',error=>{console.error(error.message);process.exitCode=1;});
child.on('exit',code=>{process.exitCode=code??1;});
