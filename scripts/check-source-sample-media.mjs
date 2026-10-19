import {readFile} from 'node:fs/promises';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import assert from 'node:assert/strict';
import {samplePng} from './release-sample-media.mjs';
const policy=JSON.parse(await readFile('config/source-sample-media.json','utf8'));
const sha=bytes=>createHash('sha256').update(bytes).digest('hex');
for(const file of policy.files){assert.equal(sha(await readFile(file.path)),file.sha256,file.path);assert.equal(sha(samplePng(file.kind)),file.sha256,'Generator mismatch: '+file.path);}
const excluded=new Set(policy.files.map(f=>f.excludedOriginalSha256));
const tracked=execFileSync('git',['ls-files','-z'],{encoding:'utf8'}).split('\0').filter(f=>/\.(png|jpg|jpeg|webp|gif)$/i.test(f));
for(const file of tracked)assert.ok(!excluded.has(sha(await readFile(file))),'Retained original illustration in source archive: '+file);
console.log(JSON.stringify({ok:true,sourceImages:policy.files.length,trackedRasterFiles:tracked.length,license:policy.license,excludedIllustrationsAbsent:true,scope:'Current source tree, not past Git history'}));
