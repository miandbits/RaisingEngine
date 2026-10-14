import test from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';

test('condition preview uses shared evaluator, does not edit or mutate debug state, rejects invalid overrides',async()=>{
 const service=new QtStudioService();const opened=await service.handle({method:'open',params:{root:path.resolve('projects/magical-academy-reference')}});
 const revision=opened.view.revision;const before=JSON.stringify(opened.view.documents);
 const first=await service.handle({method:'conditionPreview',params:{revision}});
 assert.ok(first.conditionPreview.hidden.home.includes('night'));assert.ok(first.conditionPreview.hidden.home.includes('free-panel'));assert.ok(!first.conditionPreview.hidden.home.includes('class-title'));
 const night=await service.handle({method:'conditionPreview',params:{revision,attributes:{'day-stage':2}}});
 assert.ok(night.conditionPreview.hidden.home.includes('class-title'));assert.ok(!night.conditionPreview.hidden.home.includes('night'));assert.ok(!night.conditionPreview.hidden.home.includes('sleep'));
 assert.equal(night.view.revision,revision);assert.equal(JSON.stringify(night.view.documents),before);assert.equal(service.debugSession,null);
 await assert.rejects(()=>service.handle({method:'conditionPreview',params:{revision,attributes:{'day-stage':999}}}));
 await assert.rejects(()=>service.handle({method:'conditionPreview',params:{revision:revision+1}}));
 const restored=await service.handle({method:'conditionPreview',params:{revision}});assert.deepEqual(restored.conditionPreview,first.conditionPreview);
});
