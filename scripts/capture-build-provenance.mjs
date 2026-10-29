import {execFileSync} from 'node:child_process';
import {mkdir,writeFile} from 'node:fs/promises';
import path from 'node:path';

// Diagnostics only. Keep the packager's independent sourceTreeDirty flag; these
// records explain checkout changes without claiming reproducible binaries.
const out=path.resolve('.cache/build-provenance');
const status=execFileSync('git',['status','--porcelain'],{encoding:'utf8'});
const diff=execFileSync('git',['diff','--stat'],{encoding:'utf8'});
await mkdir(out,{recursive:true});
await Promise.all([
  writeFile(path.join(out,'source-status.txt'),status),
  writeFile(path.join(out,'source-diff-stat.txt'),diff),
]);
console.log('Pre-package source status:\n'+(status||'(empty)\n'));
console.log('Pre-package source diff statistics:\n'+(diff||'(empty)\n'));
