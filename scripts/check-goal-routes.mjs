import path from 'node:path';
import {loadProject} from '../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../dist/packages/project-service/snapshot.js';
import {checkGoalRoutes} from '../apps/qt-studio/goal-routes.mjs';

try {
  if (!process.argv[2]) throw Error('사용법: node scripts/check-goal-routes.mjs 프로젝트폴더 [경로ID ...]');
  const root=path.resolve(process.argv[2]),built=await buildSnapshot(await loadProject(root));
  if(!built.ok)throw Error(JSON.stringify(built.diagnostics));
  const report=await checkGoalRoutes(root,built.snapshot,process.argv.slice(3));
  for(const r of report.reports)console.log(`${r.ok?'PASS':'FAIL'} ${r.id??r.name} · ${r.executed??0}/${r.commands??0} 명령 · 저장 ${r.saves??0}회${r.failure?` · #${r.failure.step} ${r.failure.code}: ${r.failure.message}`:''}`);
  console.log(report.file);process.exitCode=report.ok?0:1;
}catch(error){console.error(error.message);process.exitCode=2;}
