/** Historical publication policy is not evidence for freshly assembled bytes. */
export function previewArtifactMetadata(release,manifest,files){
  if(release.platform!=='windows'||release.architecture!=='x64'||release.channel!=='unsigned-preview')throw Error('Preview artifact policy must explicitly identify Windows x64 unsigned-preview.');
  return {version:release.version,sourceCommit:manifest.sourceCommit,sourceTreeDirty:manifest.sourceTreeDirty,
    ...(manifest.editorIdentity?{editorIdentity:manifest.editorIdentity}:{}),signed:false,publicReleaseReady:false,
    publicationStatus:'not-evaluated',remainingReleaseChecks:['Publication evidence for this exact artifact set has not been recorded.'],
    historicalPublicationPolicy:{version:release.version,channel:release.channel,platform:release.platform,architecture:release.architecture,
      publicReleaseReady:release.publicReleaseReady===true,remainingReleaseChecks:structuredClone(release.remainingReleaseChecks??[]),signingPolicy:structuredClone(release.signingPolicy??null)},
    files};
}
