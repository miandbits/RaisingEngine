import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,readdir,writeFile} from 'node:fs/promises';
import path from 'node:path';
import {publishImmutableAsset} from '../../dist/packages/studio-service/immutable-asset.js';
async function directory(){await mkdir('.cache/immutable-asset-tests',{recursive:true});return mkdtemp(path.resolve('.cache/immutable-asset-tests/run-'));}
test('partial ENOSPC leaves no final or temporary file, and identical retry succeeds',async()=>{
 const dir=await directory(),name='sprite-frame.png',bytes=Buffer.from('the complete immutable image bytes');
 await assert.rejects(publishImmutableAsset(dir,name,bytes,async(file,data)=>{await file.writeFile(data.subarray(0,7));throw Object.assign(new Error('simulated partial disk write'),{code:'ENOSPC'});}),{code:'ENOSPC'});
 assert.deepEqual(await readdir(dir),[]);
 await publishImmutableAsset(dir,name,bytes);assert.deepEqual(await readFile(path.join(dir,name)),bytes);assert.deepEqual(await readdir(dir),[name]);
});
test('concurrent identical publication deduplicates and different existing bytes survive unchanged',async()=>{
 const dir=await directory(),name='sprite-frame.png',bytes=Buffer.from('complete image');
 await Promise.all(Array.from({length:4},()=>publishImmutableAsset(dir,name,bytes)));
 await assert.rejects(publishImmutableAsset(dir,name,Buffer.from('different image')),/다른 파일/);
 assert.deepEqual(await readFile(path.join(dir,name)),bytes);assert.deepEqual(await readdir(dir),[name]);
 const protectedFile=path.join(dir,'original.png');await writeFile(protectedFile,'original');
 await assert.rejects(publishImmutableAsset(dir,'../original.png',bytes),/파일 이름/);assert.equal(await readFile(protectedFile,'utf8'),'original');
});
