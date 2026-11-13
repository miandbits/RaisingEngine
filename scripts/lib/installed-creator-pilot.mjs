import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {readFile, writeFile} from 'node:fs/promises';
import {join} from 'node:path';
import {pathToFileURL} from 'node:url';
import {chromium} from 'playwright';

const activityLabel = 'Lantern practice';
const presentationText = 'Lanterns at dusk — Lantern practice';
const click = clickWindow => ({clickWindow});
const checkpoint = name => ({checkpoint:name});
const screen = expectScreen => ({expectScreen});

export async function sha256(file) {
  return createHash('sha256').update(await readFile(file)).digest('hex');
}

async function readJson(file) {
  return JSON.parse(await readFile(file, 'utf8'));
}

function checkGrowth(state) {
  assert.equal(state.phase, 'activity-result');
  const skill = state.lastActivityResult.changes.find(change => change.target?.attributeId === 'skill');
  assert.ok(skill, 'Lantern practice must change Skill');
  assert.equal(skill.after - skill.before, 12);
}

function nativePlan(activities) {
  return [
    screen('raising-home'), click('plan'), screen('raising-planner'),
    ...activities.flatMap((activity, i) => [
      {click:`board__board-slot-${i}`},
      {click:`board__board-activity-${activity}`},
      {click:'board__board-apply'},
    ]),
    click('advance'), screen('raising-growth'),
  ];
}

async function playNative(run, evidence, target) {
  const game = join(evidence, target);
  const bundle = target === 'macos' ? join(game, 'RaisingPlayer.app/Contents/Resources') : game;
  const player = join(game, target === 'macos' ? 'RaisingPlayer.app/Contents/MacOS/RaisingPlayer' : 'RaisingPlayer.exe');
  const save = join(evidence, 'play-save.json');
  const session = async (name, steps) => {
    const automation = join(evidence, name+'-steps.json');
    const report = join(evidence, name+'-report.json');
    await writeFile(automation, JSON.stringify(steps));
    run(player, ['--bundle',bundle,'--save',save,'--hidden','--locale','en','--automation',automation,'--report',report,'--report-checkpoints-only']);
    const result = await readJson(report);
    assert.ok(result.ok, JSON.stringify(result));
    return name => {
      const step = result.steps.find(step => step.command.checkpoint === name);
      assert.ok(step, 'Missing native checkpoint '+name);
      return step.ui;
    };
  };

  const first = await session('native-first', [
    click('start'), ...nativePlan(['study','rest','rest']),
    {checkpoint:'presentation', screenshot:join(evidence,'native-presentation.png')},
    {timelineAdvanceMs:3500}, checkpoint('growth'), {key:'F5'},
  ]);
  const clip = first('presentation').timeline.plan.clips[0];
  assert.equal(clip.text, presentationText);
  assert.ok(clip.imageAssetId);
  const saved = first('growth').state;
  checkGrowth(saved);
  await readFile(save);

  const rest = [
    {key:'F9'}, checkpoint('loaded'), screen('raising-growth'), click('advance'),
  ];
  for (let month = 0; month < 3; ++month) {
    if (month > 0) rest.push(...nativePlan(['rest','rest','rest']), click('advance'));
    for (let period = 1; period < 3; ++period) {
      rest.push(screen('raising-home'), click('continue'), screen('raising-growth'), click('advance'));
    }
  }
  rest.push(screen('story'), click('advance'), screen('ending'),
    {checkpoint:'ending', screenshot:join(evidence,'native-ending.png')});
  const resumed = await session('native-fresh-process', rest);
  assert.deepEqual(resumed('loaded').state, saved);
  assert.equal(resumed('ending').state.phase, 'finished');
  assert.equal(resumed('ending').state.endingId, 'own-path');
  return {playerSha256:await sha256(player), saveSha256:await sha256(save), freshProcess:true, ending:'own-path', assetId:clip.imageAssetId};
}

async function playWeb(engine, evidence) {
  const {GameReleases} = await import(pathToFileURL(join(engine,'apps/qt-studio/releases.mjs')).href);
  const releases = new GameReleases();
  let browser;
  try {
    const directory = join(evidence,'web');
    const {url} = await releases.launch(releases.register({target:'web', directory}));
    browser = await chromium.launch({headless:true, channel:process.env.RAISING_BROWSER_CHANNEL || 'chromium'});
    const page = await browser.newPage({viewport:{width:1280,height:800}});
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    page.setDefaultTimeout(15000);
    const action = id => page.locator(`[data-element-id="${id}"] button`);
    const select = id => page.locator('#board__board-'+id);
    await page.goto(url);
    await action('start').click();
    await action('plan').click();
    await page.locator('[data-reading-id="engine-planning-guide-skip"]').click();
    for (const [slot,id] of ['study','rest','rest'].entries()) {
      await select('slot-'+slot).click();
      const activity = select('activity-'+id);
      if (id === 'study') assert.match(await activity.innerText(), /Lantern practice/);
      await activity.click();
      await select('apply').click();
    }
    await action('advance').click();
    const presentation = page.locator('#timeline-text');
    try {
      await presentation.waitFor();
    } catch (error) {
      const view = await page.evaluate(async () => (await window.player.view()).view);
      await writeFile(join(evidence,'web-failed-view.json'),JSON.stringify({view,errors},null,2));
      await page.screenshot({path:join(evidence,'web-failed.png')});
      throw error;
    }
    assert.equal(await presentation.innerText(), presentationText);
    const image = page.locator('#timeline-image');
    await image.waitFor();
    await page.waitForFunction(() => document.querySelector('#timeline-image')?.naturalWidth === 320);
    await page.screenshot({path:join(evidence,'web-presentation.png')});
    await presentation.waitFor({state:'hidden'});
    const view = await page.evaluate(async () => (await window.player.view()).view);
    assert.equal(view.runtimeBackend, 'wasm');
    checkGrowth(view.state);
    await page.screenshot({path:join(evidence,'web-growth.png')});
    assert.deepEqual(errors, []);
    return {backend:view.runtimeBackend, presentation:presentationText, skillGain:12};
  } finally {
    await browser?.close();
    await releases.close();
  }
}

export async function verifyInstalledCreatorPilot({run, evidence, workflow, target='windows'}) {
  const releases = {};
  for (const name of [target, 'web']) {
    const directory = join(evidence,name);
    const manifest = await readJson(join(directory,'RELEASE.json'));
    assert.equal(manifest.version, '0.1.0');
    for (const item of [manifest.archives.game, manifest.archives.project]) {
      assert.equal(await sha256(join(directory,item.file)), item.sha256);
    }
    releases[name] = manifest;
  }
  const activity = await readJson(join(workflow.project,'activities/study.json'));
  assert.equal(activity.label, activityLabel);
  assert.deepEqual(await readJson(join(workflow.restoredProject,'activities/study.json')), activity);
  const native = await playNative(run,evidence,target);
  assert.equal(activity.presentation.backgroundAssetId, native.assetId);
  const web = await playWeb(workflow.engine,evidence);
  const proof = {releases,native,web};
  await writeFile(join(evidence,'creator-pilot-play.json'), JSON.stringify(proof,null,2));
  return proof;
}
