import {readFile} from 'node:fs/promises';
import {resolve} from 'node:path';

export async function windowsPreviewCandidate(){
  const candidate=JSON.parse(await readFile('config/preview-candidate.json','utf8'));
  const windows=candidate.platforms?.windows;
  if(!/^\d+\.\d+\.\d+-preview\.\d+$/.test(candidate.version)||windows?.architecture!=='x64'||windows?.channel!=='unsigned-preview'){
    throw Error('Preview candidate must explicitly identify a version and Windows x64 unsigned-preview.');
  }
  const stem=`RaisingEngine-${candidate.version}-windows-x64`;
  const out=`release/${candidate.version}`;
  return {version:candidate.version,channel:windows.channel,
    bundle:resolve(process.env.QT_PACKAGE_OUT||`${out}/RaisingStudio`),
    installer:resolve(process.env.QT_INSTALLER_OUT||`${out}/${stem}.exe`),portableName:`${stem}-portable.zip`};
}

/** Historical publication policy is not evidence for freshly assembled bytes. */
export function previewArtifactMetadata(release,manifest,files){
  if(release.platform!=='windows'||release.architecture!=='x64'||release.channel!=='unsigned-preview')throw Error('Preview artifact policy must explicitly identify Windows x64 unsigned-preview.');
  return {version:manifest.releaseVersion,platform:'windows',architecture:'x64',channel:'unsigned-preview',sourceCommit:manifest.sourceCommit,
    sourceTree:manifest.editorIdentity?.source?.tree??null,sourceTreeDirty:manifest.sourceTreeDirty,
    ...(manifest.editorIdentity?{editorIdentity:manifest.editorIdentity}:{}),signed:false,publicReleaseReady:false,
    publicationStatus:'not-evaluated',remainingReleaseChecks:['Publication evidence for this exact artifact set has not been recorded.'],
    historicalPublicationPolicy:{version:release.version,channel:release.channel,platform:release.platform,architecture:release.architecture,
      publicReleaseReady:release.publicReleaseReady===true,remainingReleaseChecks:structuredClone(release.remainingReleaseChecks??[]),signingPolicy:structuredClone(release.signingPolicy??null)},
    files};
}
