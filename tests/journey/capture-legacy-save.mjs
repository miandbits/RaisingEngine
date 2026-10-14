// Historical fixture capture utility. Refuses to overwrite the original fixture.
import { mkdir, writeFile } from 'node:fs/promises';
import { content, start, send } from '../runtime/helpers.mjs';
import { encodeSave } from '../../dist/packages/runtime-service/save.js';
const snapshot = await content();
let session = start(snapshot);
session = send(snapshot, session, { type: 'setSchedule', activityIds: ['study', 'work', 'rest'] });
session = send(snapshot, session, { type: 'executeActivity' });
const encoded = await encodeSave(snapshot, session);
if (!encoded.ok) throw Error(JSON.stringify(encoded));
await mkdir(new URL('./fixtures/', import.meta.url), { recursive: true });
await writeFile(new URL('./fixtures/pre-migration-save.json', import.meta.url), `${encoded.text}\n`, { flag: 'wx' });
console.log(`Captured pre-migration ${session.state.phase} save; fingerprint ${snapshot.contentFingerprint}`);
