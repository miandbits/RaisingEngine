import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,mkdir,writeFile,readFile} from 'node:fs/promises';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {prepareFixtures} from '../../scripts/prepare-fixtures.mjs';
async function fixture(){
  await mkdir('.cache',{recursive:true});const root=await mkdtemp(path.resolve('.cache/fixture-media-'));
  const bytes=Buffer.from('synthetic fixture bytes');await writeFile(path.join(root,'original.dat'),bytes);
  return {root,manifest:{files:[{source:'original.dat',target:'tests/fixtures/projects/example/assets/image.dat',sha256:createHash('sha256').update(bytes).digest('hex')}]}};
}
test('fixture copies are reproducible and never overwrite local changes',async()=>{
  const {root,manifest}=await fixture();assert.equal((await prepareFixtures(root,manifest)).created,1);
  assert.equal((await prepareFixtures(root,manifest)).created,0);
  const target=path.join(root,manifest.files[0].target);await writeFile(target,'local edit');
  await assert.rejects(prepareFixtures(root,manifest),/local changes; preserved/);
  assert.equal(await readFile(target,'utf8'),'local edit');
});
test('fixture source hashes and repository boundaries are enforced',async()=>{
  const {root,manifest}=await fixture();manifest.files[0].sha256='0'.repeat(64);
  await assert.rejects(prepareFixtures(root,manifest),/hash mismatch/);
  manifest.files[0].target='tests/fixtures/projects/../../../../escape.dat';
  await assert.rejects(prepareFixtures(root,manifest),/escapes repository/);
});
