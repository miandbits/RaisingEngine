import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,mkdir,readFile,writeFile} from 'node:fs/promises';
import {join} from 'node:path';
import {createHash} from 'node:crypto';
import {spawnSync} from 'node:child_process';
import {samplePng,replaceReleaseMedia,auditReleaseMedia} from '../../scripts/release-sample-media.mjs';
test('release media is deterministic and fails closed on unknown or modified media',async()=>{
  await mkdir('.cache',{recursive:true});const root=await mkdtemp('.cache/release-media-');
  const path='engine/examples/sample/assets/character.png',source=Buffer.from('original pixels');
  const policy={replacements:[{path,originalSha256:createHash('sha256').update(source).digest('hex'),kind:'haru'}]};
  await mkdir(join(root,'engine/examples/sample/assets'),{recursive:true});await writeFile(join(root,path),source);
  await replaceReleaseMedia(root,policy);assert.deepEqual(await readFile(join(root,path)),samplePng('haru'));
  assert.equal((await auditReleaseMedia(root,policy)).images,1);
  await assert.rejects(replaceReleaseMedia(root,policy),/Unreviewed/);
  await writeFile(join(root,'engine/examples/sample/assets/extra.png'),samplePng('yuna'));
  await assert.rejects(auditReleaseMedia(root,policy),/Unapproved/);
});
test('generator rejects undeclared artwork types',()=>assert.throws(()=>samplePng('unknown'),/Unknown/));
test('portable packaging rejects files added after manifest review',async()=>{
  await mkdir('.cache',{recursive:true});const root=await mkdtemp('.cache/unreviewed-payload-');
  const release=JSON.parse(await readFile('config/preview-release.json','utf8'));
  await writeFile(join(root,'package-manifest.json'),JSON.stringify({releaseVersion:release.version,files:[]}));
  await writeFile(join(root,'private-history.txt'),'Must not be distributed');
  const result=spawnSync(process.execPath,['scripts/package-preview-artifacts.mjs'],{env:{...process.env,QT_PACKAGE_OUT:root},encoding:'utf8',windowsHide:true});
  assert.notEqual(result.status,0);assert.match(result.stderr,/Unreviewed extra payload/);
});
