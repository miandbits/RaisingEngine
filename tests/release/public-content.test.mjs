import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,mkdir,mkdtemp,writeFile,readdir} from 'node:fs/promises';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import path from 'node:path';
import {inspectPublicContent} from '../../scripts/check-public-content.mjs';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
const root=path.resolve('.'),policy=JSON.parse(await readFile('config/repository-content.json','utf8'));

test('tracked public tree excludes local work and known reference assets, even under renamed files',async()=>{
  const files=execFileSync('git',['ls-files','-z'],{encoding:'utf8',maxBuffer:16*1024*1024}).split('\0').filter(Boolean);
  assert.deepEqual(await inspectPublicContent(root,files,policy),[]);
  await mkdir('.cache',{recursive:true});const work=await mkdtemp(path.resolve('.cache/public-boundary-'));
  const bytes=Buffer.from('synthetic excluded bytes');await writeFile(path.join(work,'renamed.dat'),bytes);
  const synthetic={...policy,excludedAssetSha256:[createHash('sha256').update(bytes).digest('hex')]};
  assert.equal((await inspectPublicContent(work,['renamed.dat'],synthetic))[0].reason,'matches excluded reference-game asset');
  assert.equal((await inspectPublicContent(work,['projects/private/anything.json'],synthetic))[0].reason,'local-only path is tracked');
});

test('public examples and isolated regression projects load without private projects',async()=>{
  const roots=[...policy.publicSamples.filter(x=>!x.endsWith('/genre-suite'))];
  for(const dir of await readdir('examples/genre-suite',{withFileTypes:true}))if(dir.isDirectory())roots.push('examples/genre-suite/'+dir.name);
  for(const dir of await readdir(policy.testFixtureRoot,{withFileTypes:true}))if(dir.isDirectory())roots.push(policy.testFixtureRoot+'/'+dir.name);
  for(const project of roots){
    const built=await buildSnapshot(await loadProject(path.resolve(project)));
    assert.equal(built.ok,true,project+': '+JSON.stringify(built.diagnostics));
  }
  assert.ok(roots.length>=25);
});
