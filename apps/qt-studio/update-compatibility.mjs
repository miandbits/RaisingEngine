import {randomUUID,createHash} from 'node:crypto';
import {open,realpath} from 'node:fs/promises';
import path from 'node:path';
import {loadProject,buildSnapshot} from '../../dist/packages/project-service/index.js';
import {decodeSave,MAX_SAVE_BYTES} from '../../dist/packages/runtime-service/save.js';

const digest=bytes=>createHash('sha256').update(bytes).digest('hex');
async function savedSnapshot(root) {
  const loaded=await loadProject(root),built=await buildSnapshot(loaded);
  if(!built.ok)throw new Error('UPDATE_PROJECT_INVALID: '+built.diagnostics.filter(d=>d.severity==='error').slice(0,5).map(d=>`${d.code}: ${d.message}`).join('\n'));
  return {snapshot:built.snapshot,documentVersion:digest(JSON.stringify(loaded.documents.map(d=>[d.file,d.version]).sort((a,b)=>a[0].localeCompare(b[0]))))};
}
// Read at most the limit plus one byte even if the selected file grows after stat.
export async function readSampleSaveBytes(handle) {
  const check=info=>{if(!info.isFile()||info.size>MAX_SAVE_BYTES)throw Error('UPDATE_SAVE_SIZE: Select a regular save file of at most 16 MiB.');};
  check(await handle.stat());
  const buffer=Buffer.alloc(MAX_SAVE_BYTES+1);let length=0;
  while(length<buffer.length){const {bytesRead}=await handle.read(buffer,length,buffer.length-length,length);if(!bytesRead)break;length+=bytesRead;}
  if(length>MAX_SAVE_BYTES)throw Error('UPDATE_SAVE_SIZE: Select a regular save file of at most 16 MiB.');
  check(await handle.stat());
  return buffer.subarray(0,length);
}
async function sampleSave(file,content) {
  const handle=await open(file,'r');let bytes;
  try {bytes=await readSampleSaveBytes(handle);} finally {await handle.close();}
  const text=new TextDecoder('utf-8',{fatal:true}).decode(bytes),loaded=await decodeSave(content,text);
  return {file,sha256:digest(bytes),valid:loaded.ok,...(loaded.ok?{commands:loaded.session.commands.length,hasPresentation:!!loaded.playerPresentation}:{error:loaded.error})};
}
/** Release guidance only. Ordinary loading and explicit migration remain strict. */
export class UpdateCompatibility {
  checks=new Map();
  async inspect(editor,params) {
    editor.current(params.revision);
    if(!editor.root)throw Error('UPDATE_NO_PROJECT: Open the current project first.');
    if(editor.view({shared:true}).dirty)throw Error('UPDATE_SAVE_FIRST: Save the current project before checking existing saves.');
    if(typeof params.previousProject!=='string'||!path.isAbsolute(params.previousProject))throw Error('UPDATE_PREVIOUS_PROJECT: Select a saved previous project backup folder.');
    if(params.sampleSave!==undefined&&(typeof params.sampleSave!=='string'||!path.isAbsolute(params.sampleSave)))throw Error('UPDATE_SAMPLE_SAVE: Select an original save file.');
    await editor.assertDiskUnchanged();
    const previousProject=await realpath(params.previousProject),currentProject=editor.root;
    const [previousSaved,currentSaved,draft]=await Promise.all([savedSnapshot(previousProject),savedSnapshot(currentProject),editor.snapshot(params.revision)]);
    const previous=previousSaved.snapshot,current=currentSaved.snapshot;
    if(!draft.ok)throw Error('UPDATE_PROJECT_INVALID: Fix current project validation errors before checking existing saves.');
    if(draft.snapshot.contentFingerprint!==current.contentFingerprint)throw Error('UPDATE_SAVE_FIRST: Save the current project before checking existing saves.');
    const sample=params.sampleSave?await sampleSave(await realpath(params.sampleSave),previous):undefined;
    await editor.assertDiskUnchanged();editor.current(params.revision);
    const classification=previous.project.gameId!==current.project.gameId?'different-game':previous.contentFingerprint===current.contentFingerprint?'same-content':'changed-content';
    return {version:1,checkedEditorRevision:params.revision,previousProject,currentProject,previousDocumentVersion:previousSaved.documentVersion,previousTitle:previous.project.title,currentTitle:current.project.title,previousGameId:previous.project.gameId,currentGameId:current.project.gameId,previousFingerprint:previous.contentFingerprint,currentFingerprint:current.contentFingerprint,classification,...(sample?{sample}:{})};
  }
  async check(editor,params) {
    // A failed recheck must not leave an older successful badge usable for export.
    this.checks.clear();const report=await this.inspect(editor,params),checkId=randomUUID();
    this.checks.set(checkId,{params:{revision:params.revision,previousProject:report.previousProject,...(report.sample?{sampleSave:report.sample.file}:{})},report});
    return {...report,checkId};
  }
  async verify(editor,checkId) {
    const checked=this.checks.get(checkId);
    if(!checked)throw Error('UPDATE_CHECK_STALE: Check existing saves again before exporting this checked update.');
    const current=await this.inspect(editor,checked.params);
    if(JSON.stringify(current)!==JSON.stringify(checked.report))throw Error('UPDATE_CHECK_STALE: The project backup or sample save changed. Check existing saves again.');
    return current;
  }
}
