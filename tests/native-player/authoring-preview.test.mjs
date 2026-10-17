import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdtemp,mkdir,cp} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';

test('Windows authoring inspection renders scoped state and ignores gameplay/save actions',{skip:!process.env.RAISING_NATIVE_PLAYER_RUNTIME,timeout:90000},async()=>{
  await mkdir('.cache/authoring-preview-native',{recursive:true});const out=await mkdtemp(resolve('.cache/authoring-preview-native/run-'));
  const root=join(out,'project');await cp('tests/fixtures/projects/component-context',root,{recursive:true});
  const service=new QtStudioService();await service.handle({method:'open',params:{root}});
  const response=await service.handle({method:'preview',params:{revision:service.editor.revision,authoringContext:{screenId:'screen-1',elementId:'instance-1',actorId:'trainee-2',attributes:{skill:25}}}});
  const p=response.play,inspection=JSON.parse(await readFile(p.inspection,'utf8'));
  const script=join(out,'steps.json'),report=join(out,'report.json');
  await writeFile(script,JSON.stringify([{checkpoint:'before'},{click:'instance-1__element-5'},{checkpoint:'after'},{screenshot:join(out,'preview.png')}]));
  const args=['--bundle',p.directory,'--save',p.save,'--hidden','--preview-screen',p.screenId,'--preview-inspection',p.inspection,'--automation',script,'--report',report];
  const result=spawnSync(p.executable,args,{encoding:'utf8',windowsHide:true,timeout:60000});
  assert.equal(result.status,0,result.stderr);const actual=JSON.parse(await readFile(report,'utf8'));assert.equal(actual.ok,true,JSON.stringify(actual));
  const checkpoints=actual.steps.filter(s=>s.command.checkpoint);for(const row of checkpoints){assert.equal(row.ui.preview.stateSource,'authoring');assert.deepEqual(row.ui.state,inspection.state);assert.equal(row.ui.texts['instance-1__element-3'],'검술: 25');assert.equal(row.ui.texts['instance-2__element-3'],'검술: 25');}
  // Two cards intentionally read the same selected actor here; the authored first actor is still untouched.
  assert.equal(inspection.state.roster.attributes.apprentice.skill,10);await assert.rejects(readFile(p.save));
  const hidden=inspection.visual.screens.find(s=>s.id==='screen-1').elements.find(e=>e.action==='start');
  const hiddenResult=spawnSync(p.executable,[...args,'--preview-element',hidden.id],{encoding:'utf8',windowsHide:true,timeout:60000});assert.equal(hiddenResult.status,0,hiddenResult.stderr);
  const invalid={...inspection,contentFingerprint:'wrong'};await writeFile(p.inspection,JSON.stringify(invalid));
  const rejected=spawnSync(p.executable,args,{encoding:'utf8',windowsHide:true,timeout:60000});assert.notEqual(rejected.status,0);
  const withoutPreview=spawnSync(p.executable,['--bundle',p.directory,'--preview-inspection',p.inspection],{encoding:'utf8',windowsHide:true,timeout:10000});assert.notEqual(withoutPreview.status,0);
});
