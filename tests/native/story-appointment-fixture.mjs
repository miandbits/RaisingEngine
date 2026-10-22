import assert from 'node:assert/strict';
import {readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {actorStoryFixture} from './actor-story-fixture.mjs';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';

export async function appointmentFixture(t,trigger,window={fromPeriod:0,throughPeriod:35}){
 const {root}=await actorStoryFixture(t),file=resolve(root,'events/personal-follow.json');
 const follow=JSON.parse(await readFile(file,'utf8'));
 Object.assign(follow,{trigger,window},trigger==='after-activity'?{activityId:'study'}:trigger==='location-enter'?{locationId:'market'}:{});
 await writeFile(file,JSON.stringify(follow));
 const built=await buildSnapshot(await loadProject(root));assert.ok(built.ok,JSON.stringify(built));return {root,content:built.snapshot};
}
