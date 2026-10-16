import { open } from 'node:fs/promises';
import { buildSnapshot, loadProject } from '../../dist/packages/project-service/index.js';
import { MAX_SAVE_BYTES } from '../../dist/packages/runtime-service/save.js';
import { previewSaveMigration, publishMigratedSave } from '../../dist/packages/runtime-service/migration.js';

export async function saveMigration(method, params) {
  const snapshot = async root => {
    if (typeof root !== 'string' || !root) throw Error('업데이트 전후 프로젝트 폴더를 지정하세요.');
    const result = await buildSnapshot(await loadProject(root));
    if (!result.ok) throw Error(JSON.stringify(result.diagnostics.filter(d=>d.severity==='error').slice(0,5)));
    return result.snapshot;
  };
  const handle = await open(params.save,'r'); let bytes;
  try { const info=await handle.stat(); if(!info.isFile()||info.size>MAX_SAVE_BYTES)throw Error('저장은 일반 파일이며 16 MiB 이하여야 합니다.');bytes=await handle.readFile(); } finally { await handle.close(); }
  const text = new TextDecoder('utf-8',{fatal:true}).decode(bytes);
  const [source,target] = await Promise.all([snapshot(params.from),snapshot(params.to)]);
  const settings={resetPresentation:params.resetPresentation===true};
  const result = method==='saveMigrationPublish'
    ? await publishMigratedSave(params.output,source,target,text,params.approval,settings)
    : await previewSaveMigration(source,target,text,settings);
  return result.ok ? {error:null,migration:{report:result.report,...(result.file?{file:result.file}:{})}}
    : {error:`${result.error.code} · ${result.error.path}\n${result.error.message}`};
}
