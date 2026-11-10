import {fileURLToPath} from 'node:url';
import path from 'node:path';
import {readEditorIdentity} from '../apps/qt-studio/editor-identity.mjs';
const args=process.argv.slice(2);
if(args.length&&!(args.length===2&&args[0]==='--root'))throw Error('Usage: node scripts/editor-identity.mjs [--root /path/to/engine]');
const root=args.length?path.resolve(args[1]):fileURLToPath(new URL('..',import.meta.url));
console.log(JSON.stringify(await readEditorIdentity(root),null,2));
