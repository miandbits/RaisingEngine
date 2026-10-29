import {randomUUID,createHash} from 'node:crypto';
import {open,realpath} from 'node:fs/promises';
import path from 'node:path';
import {preparePresentationRevision} from '../../dist/packages/export-service/presentation-revision.js';
import {decodeSave} from '../../dist/packages/runtime-service/save.js';
import {readSampleSaveBytes} from './update-compatibility.mjs';

const digest=bytes=>createHash('sha256').update(bytes).digest('hex');
const MAX_BUNDLE=64*1024*1024;
async function readBundle(file){
  const handle=await open(file,'r');
  try{
    const info=await handle.stat();if(!info.isFile()||info.size>MAX_BUNDLE)throw Error('PRESENTATION_BUNDLE_SIZE: Choose a game.json file of at most 64 MiB.');
    const bytes=Buffer.alloc(MAX_BUNDLE+1);let count=0;
    while(count<bytes.length){const {bytesRead}=await handle.read(bytes,count,bytes.length-count,count);if(!bytesRead)break;count+=bytesRead;}
    if(count>MAX_BUNDLE)throw Error('PRESENTATION_BUNDLE_SIZE: game.json grew beyond 64 MiB.');
    const data=bytes.subarray(0,count);return {base:JSON.parse(new TextDecoder('utf-8',{fatal:true}).decode(data)),sha256:digest(data)};
  }finally{await handle.close();}
}
/** Explicit export choice. Never changes project documents, saves or baseline content. */
export class PresentationUpdates {
  checks=new Map();
  async inspect(service,params){
    const editor=service.editor;editor.current(params.revision);
    if(!editor.root)throw Error('PRESENTATION_NO_PROJECT: Open the edited project first.');
    if(editor.view({shared:true}).dirty)throw Error('PRESENTATION_SAVE_FIRST: Save this project before checking a presentation update.');
    if(!['web','windows','macos','ios'].includes(params.target))throw Error('PRESENTATION_TARGET: Select an export platform.');
    if(typeof params.previousBundle!=='string'||!path.isAbsolute(params.previousBundle))throw Error('PRESENTATION_BASE: Select game.json from the previous published game.');
    if(params.sampleSave!==undefined&&(typeof params.sampleSave!=='string'||!path.isAbsolute(params.sampleSave)))throw Error('PRESENTATION_SAVE: Select an original save file.');
    await editor.assertDiskUnchanged();
    const previousBundle=await realpath(params.previousBundle),{base,sha256}=await readBundle(previousBundle),built=await editor.snapshot(params.revision);
    if(!built.ok)throw Error('PRESENTATION_PROJECT_INVALID: Fix validation errors before checking this update.');
    const extensions=await editor.exportExtensions(params.revision,built.snapshot,params.target,await service.playerFeatures(params.target));
    const result=preparePresentationRevision(built.snapshot,base,extensions?.game);
    let sample;
    if(params.sampleSave){
      const file=await realpath(params.sampleSave),handle=await open(file,'r');let bytes;
      try{bytes=await readSampleSaveBytes(handle);}finally{await handle.close();}
      const loaded=await decodeSave(result.content,new TextDecoder('utf-8',{fatal:true}).decode(bytes));
      if(!loaded.ok)throw Error('PRESENTATION_SAVE_INVALID: The selected save does not validate against the published baseline. '+loaded.error.code);
      sample={file,sha256:digest(bytes),valid:true,commands:loaded.session.commands.length};
    }
    await editor.assertDiskUnchanged();editor.current(params.revision);
    const current=await editor.snapshot(params.revision);
    if(!current.ok||current.snapshot.contentFingerprint!==built.snapshot.contentFingerprint)throw Error('PRESENTATION_CHECK_STALE: Project content changed during review. Check again.');
    const report={version:1,checkedEditorRevision:params.revision,target:params.target,previousBundle,currentProject:editor.root,
      previousBundleSha256:sha256,baselineFingerprint:result.content.contentFingerprint,sourceFingerprint:built.snapshot.contentFingerprint,
      classification:(base.presentationRevision?.sourceContentFingerprint??base.content.contentFingerprint)===built.snapshot.contentFingerprint?'unchanged':'presentation-only',
      changedPaths:result.presentationRevision.changedPaths??[],images:Object.keys(result.presentationRevision.images).sort(),...(sample?{sample}:{})};
    return {base,report};
  }
  async check(service,params){
    const {report}=await this.inspect(service,params),checkId=randomUUID();
    // A cancelled alternate review must not revoke the already selected update.
    // Every retained token still revalidates its exact inputs before publication.
    while(this.checks.size>=32)this.checks.delete(this.checks.keys().next().value);
    this.checks.set(checkId,{params:{revision:params.revision,target:params.target,previousBundle:report.previousBundle,...(report.sample?{sampleSave:report.sample.file}:{})},report});
    return {...report,checkId};
  }
  async verify(service,checkId,{revision,target}){
    const checked=this.checks.get(checkId);
    if(!checked||checked.params.revision!==revision||checked.params.target!==target)throw Error('PRESENTATION_CHECK_STALE: Review and choose this presentation update again.');
    const current=await this.inspect(service,checked.params);
    if(JSON.stringify(current.report)!==JSON.stringify(checked.report))throw Error('PRESENTATION_CHECK_STALE: The baseline, sample or project changed. Review again.');
    return current;
  }
}
