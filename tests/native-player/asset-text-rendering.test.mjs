import test from 'node:test';
import assert from 'node:assert/strict';
import { cp, mkdir, mkdtemp, readFile, writeFile } from 'node:fs/promises';
import { join, resolve } from 'node:path';
import { spawnSync } from 'node:child_process';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { exportGame } from '../../dist/packages/export-service/export.js';
import { desktopTarget, gameDirectory, playerExecutable } from '../../dist/packages/export-service/targets.js';
import { decodePng, encodeRgbaPng } from '../../dist/packages/studio-service/png.js';

const colour = [33, 207, 91, 255];
const canvas = { width: 640, height: 360 };
const textBox = { x: 120, y: 20, width: 480, height: 320 };

function pixel(image, x, y) {
  const start = (Math.floor(y) * image.width + Math.floor(x)) * 4;
  return [...image.pixels.subarray(start, start + 3)];
}

// Repeated capitals give every naturally wrapped line the same glyph height.
// Look at rasterized ink, not layout boxes: the original bug preserved boxes.
function inkBands(image, scale, box = textBox, gap = 1) {
  const rows = [];
  for (let y = Math.ceil(box.y * scale); y < Math.floor((box.y + box.height) * scale); y++) {
    let ink = 0;
    for (let x = Math.ceil(box.x * scale); x < Math.floor((box.x + box.width) * scale); x++) {
      const at = (y * image.width + x) * 4;
      if (image.pixels[at] > 210 && image.pixels[at + 1] > 210 && image.pixels[at + 2] > 210) ink++;
    }
    if (ink >= Math.max(2, Math.floor(scale * 2))) rows.push({ y, ink });
  }
  const bands = [];
  for (const { y, ink } of rows) {
    const previous = bands.at(-1);
    if (previous && y - previous.end <= Math.max(1, Math.ceil(gap * scale))) { previous.end = y; previous.ink += ink; }
    else bands.push({ start: y, end: y, ink });
  }
  return bands.map(({ start, end, ink }) => ({ top: start / scale, height: (end - start + 1) / scale, ink: ink / (scale * scale) }));
}

test('exported native assets and naturally wrapped text survive Unicode paths and 1x/2x/3x density', { timeout: 180000 }, async t => {
  const evidenceRoot = resolve('.cache/native-asset-text');
  await mkdir(evidenceRoot, { recursive: true });
  const out = await mkdtemp(join(evidenceRoot, '렌더 proof '));
  const projectRoot = join(out, 'project with spaces 한글');
  await cp(resolve('examples/my-first-raising-game'), projectRoot, { recursive: true });
  const assetPath = 'assets/solid-image.png';
  await writeFile(join(projectRoot, assetPath), encodeRgbaPng({ width: 4, height: 4, pixels: Buffer.from(Array(16).fill(colour).flat()) }));
  const manifestPath = join(projectRoot, 'assets/manifest.json');
  const manifest = JSON.parse(await readFile(manifestPath, 'utf8'));
  manifest.assets.push({ id: 'render-fixture', kind: 'image', path: assetPath });
  const musicPath = 'assets/render-music.wav'; // The exported directory supplies the Unicode path.
  await cp(resolve('tests/native-player/fixtures/tone.wav'), join(projectRoot, musicPath));
  manifest.assets.push({ id: 'render-music', kind: 'audio', path: musicPath });
  await writeFile(manifestPath, JSON.stringify(manifest));
  const projectPath = join(projectRoot, 'project.json');
  const project = JSON.parse(await readFile(projectPath, 'utf8'));
  const wrapped = Array(40).fill('MMMM').join(' '); // No explicit line breaks.
  for (const [id, description] of [['rest', Array(6).fill('MMMM').join(' ')], ['work', '한'.repeat(30)]]) {
    const file = join(projectRoot, `activities/${id}.json`);
    const activity = JSON.parse(await readFile(file, 'utf8'));
    activity.description = description;
    await writeFile(file, JSON.stringify(activity));
  }
  project.visual = {
    ...canvas, viewportMode: 'fit', defaultScene: { musicAssetId: 'render-music' },
    screens: [
      { id: 'title', label: 'Rendering proof', elements: [
        { id: 'background', kind: 'panel', x: 0, y: 0, ...canvas, fill: '#0c0e12' },
        { id: 'start', kind: 'button', action: 'start', x: 16, y: 110, width: 80, height: 50, text: 'Start' },
        { id: 'image', kind: 'image', x: 16, y: 16, width: 64, height: 64, imageAssetId: 'render-fixture', fit: 'contain' },
        { id: 'wrapped', kind: 'text', ...textBox, fontSize: 24, color: '#ffffff', text: wrapped },
      ] },
      ...['home', 'schedule'].map(id => ({ id, label: id, elements: [
        { id: 'background', kind: 'panel', x: 0, y: 0, ...canvas, fill: '#0c0e12' },
        { id: 'sound', kind: 'button', action: 'sound', x: 10, y: 280, width: 80, height: 50, text: 'Sound' },
        { id: 'plan', kind: 'schedule', scheduleLayout: 'columns', showActivityDescription: true, x: 10, y: 0, width: 540, height: 340, color: '#ffffff', fontSize: 20 },
      ] })),
      ...['story', 'ending'].map(id => ({ id, label: id, elements: [] })),
    ],
    routing: { defaultScreenId: 'home', useLocationScreens: false, phases: [{ phase: 'title', screenId: 'title' }, { phase: 'planning', screenId: 'home' }] },
  };
  await writeFile(projectPath, JSON.stringify(project));
  const built = await buildSnapshot(await loadProject(projectRoot));
  assert.equal(built.ok, true, JSON.stringify(built.diagnostics));
  const target = desktopTarget();
  const runtime = resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME ?? process.env.RAISING_NATIVE_PLAYER_RUNTIME ?? 'dist/native-player-app');
  const destination = join(out, 'export with spaces 여행');
  await exportGame({ projectRoot, content: built.snapshot, target, destination, runtimes: { windows: runtime, macos: runtime, web: runtime } });
  const executable = playerExecutable(target, destination), directory = gameDirectory(target, destination);
  async function play(name, steps) {
    const script = join(out, `${name}.json`), reportFile = join(out, `${name}-report.json`);
    await writeFile(script, JSON.stringify(steps));
    const run = spawnSync(executable, ['--bundle', directory, '--save', join(out, '기록 save.json'), '--automation', script, '--report', reportFile, '--hidden'], { encoding: 'utf8', windowsHide: true, timeout: 90000 });
    await writeFile(join(out, `${name}-process.txt`), (run.stdout ?? '') + '\n' + (run.stderr ?? ''));
    assert.equal(run.status, 0, run.error?.message ?? run.stderr);
    const report = JSON.parse(await readFile(reportFile, 'utf8'));
    assert.equal(report.ok, true, JSON.stringify(report));
    return report;
  }
  // SDL window units differ from output pixels on Retina and Windows scaling.
  const probe = await play('probe', [{ resize: canvas }, { waitFrames: 12 }, { checkpoint: 'probe' }]);
  const pixelRatio = probe.final.presentation.pixelWidth / canvas.width;
  assert.ok(pixelRatio > 0 && pixelRatio <= 4, `Unexpected display scale ${pixelRatio}`);
  const densities = [1, 2, 3, 2, 1];
  const steps = densities.flatMap((density, index) => [
    { resize: { width: Math.round(canvas.width * density / pixelRatio), height: Math.round(canvas.height * density / pixelRatio) } },
    { waitFrames: 12 },
    { checkpoint: `density-${index}`, screenshot: join(out, `density-${index}-${density}x.png`) },
  ]);
  steps.push({ click: 'start' }, { select: 'plan-0', value: 'rest' }, { select: 'plan-1', value: 'work' }, { waitFrames: 12 },
    { checkpoint: 'word-wrap', screenshot: join(out, 'word-wrap.png') }, { click: 'sound' }, { checkpoint: 'muted' }, { click: 'sound' }, { checkpoint: 'unmuted' }, { key: 'F5' }, { checkpoint: 'saved' }, { key: 'F9' }, { checkpoint: 'loaded' });
  const report = await play('densities', steps);
  const samples = [];
  for (const [index, density] of densities.entries()) {
    const ui = report.steps.find(step => step.command.checkpoint === `density-${index}`).ui;
    assert.equal(ui.presentation.fontRasterDensity, density, `Requested ${density}x output must exercise that atlas density`);
    const image = decodePng(await readFile(join(out, `density-${index}-${density}x.png`)));
    const scale = image.width / canvas.width;
    assert.ok(Math.abs(scale - density) < 0.02, `Screenshot must capture the ${density}x game viewport`);
    samples.push({ density, scale, imagePixel: pixel(image, 40 * scale, 40 * scale), bands: inkBands(image, scale) });
  }
  await writeFile(join(out, 'measurements.json'), JSON.stringify({ pixelRatio, samples }, null, 2));
  await writeFile(join(evidenceRoot, 'latest.json'), JSON.stringify({ out, destination, target, pixelRatio, samples }, null, 2));
  await t.test('English activity words stay whole and long CJK text still wraps', async () => {
    const ui = report.steps.find(step => step.command.checkpoint === 'word-wrap').ui;
    assert.ok(ui.audio.initialized && ui.audio.musicPlaying && ui.audio.loadedSounds === 1, 'Unicode music paths must reach the live decoder and audio engine');
    const screenshot = decodePng(await readFile(join(out, 'word-wrap.png')));
    const bandsFor = (id, gap = 1) => {
      const control = ui.controls.find(control => control.id === id);
      return inkBands(screenshot, screenshot.width / canvas.width, { x: control.x, y: control.y + control.height + 10, width: control.width, height: 115 }, gap);
    };
    // Hangul syllables contain separated components within one glyph.
    const english = bandsFor('plan-0'), cjk = bandsFor('plan-1', 5);
    assert.equal(english.length, 3, 'Six whole words must wrap onto three equal lines');
    assert.ok(Math.max(...english.map(line => line.ink)) - Math.min(...english.map(line => line.ink)) < 8, `Every English line must have two complete words: ${JSON.stringify(english)}`);
    assert.ok(cjk.length >= 3, 'An unspaced Korean token must wrap within the card');
    assert.equal(cjk.length, 3, `The thirty Korean glyphs must fit on three lines: ${JSON.stringify(cjk)}`);
    assert.ok(cjk.every(line => line.height >= 12), `CJK fallback must retain readable full-height glyphs: ${JSON.stringify(cjk)}`);
    assert.equal(report.steps.find(step => step.command.checkpoint === 'muted').ui.message, 'Sound muted.');
    assert.equal(report.steps.find(step => step.command.checkpoint === 'unmuted').ui.message, 'Sound on.');
    assert.equal(report.steps.find(step => step.command.checkpoint === 'saved').ui.message, 'Game saved.');
    assert.equal(report.steps.find(step => step.command.checkpoint === 'loaded').ui.message, 'Continued from your saved game.');
  });
  await t.test('exported PNG reaches the renderer from an absolute path', () => {
    for (const sample of samples) assert.deepEqual(sample.imagePixel, colour.slice(0, 3), `${sample.density}x fixture colour must be visible`);
  });
  await t.test('wrapped glyphs keep their height and baseline after later lines and density changes', () => {
    const baseline = samples[0].bands;
    assert.ok(baseline.length >= 6, `Expected multiple natural wraps, got ${baseline.length}`);
    assert.ok(baseline.every(band => band.height >= 12), 'Every baseline line must contain full-height capital glyphs');
    for (const sample of samples) {
      assert.equal(sample.bands.length, baseline.length, `${sample.density}x must retain every wrapped line`);
      for (const [line, band] of sample.bands.entries()) {
        assert.ok(Math.abs(band.height - baseline[line].height) <= 2, `${sample.density}x line ${line}: height ${band.height} vs ${baseline[line].height}`);
        assert.ok(Math.abs(band.top - baseline[line].top) <= 2, `${sample.density}x line ${line}: baseline drift ${band.top} vs ${baseline[line].top}`);
      }
      assert.ok(Math.max(...sample.bands.map(band => band.height)) - Math.min(...sample.bands.map(band => band.height)) <= 2, `${sample.density}x earlier lines must not shrink as later lines append glyphs`);
    }
  });
});

test('isolated CJK labels populate a new density atlas before their first paint, including reading options', {timeout:120000}, async()=>{
  const root=resolve('.cache/native-cold-glyphs');await mkdir(root,{recursive:true});const out=await mkdtemp(join(root,'run-')),projectRoot=join(out,'project');await cp(resolve('examples/my-first-raising-game'),projectRoot,{recursive:true});
  const file=join(projectRoot,'project.json'),project=JSON.parse(await readFile(file,'utf8'));
  project.localization.locales.push({id:'ko',label:'한국어'},{id:'ja',label:'日本語'});
  const boxes=[{id:'korean',text:'읽기 설정',fontSize:21,x:40,y:30,width:500,height:60},{id:'japanese',text:'戻る',fontSize:27,x:40,y:110,width:500,height:70},{id:'mixed',text:'確認・적용',fontSize:35,x:40,y:200,width:500,height:90}];
  project.visual={...canvas,readableLayout:{background:'#172c29'},routing:{defaultScreenId:'title',useLocationScreens:false,phases:[{phase:'title',screenId:'title'}]},screens:[
    {id:'title',label:'Title',elements:[{id:'start',kind:'button',action:'start',text:'Start',x:20,y:20,width:200,height:60},{id:'inspect',kind:'button',action:'navigate',screenId:'glyphs',text:'Inspect',x:20,y:100,width:200,height:60}]},
    {id:'glyphs',label:'Cold glyphs',elements:[{id:'backdrop',kind:'panel',x:0,y:0,...canvas,fill:'#0c0e12'},...boxes.map(b=>({...b,kind:'text',color:'#ffffff'}))]},
    {id:'reading',label:'Reading',elements:[{id:'main',kind:'text',text:'今日の予定・오늘의 계획',x:20,y:20,width:500,height:100}],readableLayout:{items:[{elementId:'main',fontSize:24}]}},
    ...['schedule','story','ending'].map(id=>({id,label:id,elements:[]}))]};
  await writeFile(file,JSON.stringify(project));const built=await buildSnapshot(await loadProject(projectRoot));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  const target=desktopTarget(),runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??'dist/native-player-app'),destination=join(out,'game');await exportGame({projectRoot,content:built.snapshot,target,destination,runtimes:{windows:runtime,macos:runtime,web:runtime}});
  async function run(name,steps,extra=[]){const script=join(out,name+'.json'),report=join(out,name+'-report.json');await writeFile(script,JSON.stringify(steps));const p=spawnSync(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--save',join(out,name+'-save.json'),'--automation',script,'--report',report,'--report-checkpoints-only','--hidden',...extra],{encoding:'utf8',timeout:30000,windowsHide:true});await writeFile(join(out,name+'-process.txt'),(p.stdout??'')+'\n'+(p.stderr??''));assert.equal(p.status,0,p.error?.message??p.stderr);return JSON.parse(await readFile(report,'utf8'));}
  const probe=await run('probe',[{resize:canvas},{waitFrames:8}]),ratio=probe.final.presentation.pixelWidth/canvas.width,measurements=[];
  // Each process starts with ASCII only, then introduces each CJK string at a
  // different, previously unused font size. Waiting cannot repair missing ink.
  for(const density of [1,2,3]){const imageFile=join(out,'cold-'+density+'.png'),report=await run('cold-'+density,[{resize:{width:Math.round(canvas.width*density/ratio),height:Math.round(canvas.height*density/ratio)}},{waitFrames:8},{click:'inspect'},{waitFrames:12},{screenshot:imageFile}]);assert.equal(report.final.presentation.fontRasterDensity,density);const image=decodePng(await readFile(imageFile)),scale=image.width/canvas.width;for(const box of boxes){const bands=inkBands(image,scale,box,5);assert.ok(bands.reduce((sum,b)=>sum+b.ink,0)>100,`${box.id} must render actual strokes on its first isolated ${density}x atlas: ${JSON.stringify(bands)}`);measurements.push({density,id:box.id,bands});}}
  for(const locale of ['en','ko','ja']){const name='footer-'+locale,imageFile=join(out,name+'.png'),report=await run(name,[{resize:{width:390,height:844}},{waitFrames:12},{screenshot:imageFile}],['--preview-screen','reading','--preview-reading-layout','auto','--preview-text-scale','2','--locale',locale]);const ui=report.final,control=ui.readingLayout.controls.find(c=>c.id==='engine-reading-options');assert.ok(control&&control.height>=44);assert.equal(control.fontSize,'36px');assert.deepEqual(ui.commands,[]);const image=decodePng(await readFile(imageFile)),scale=image.width/390;let ink=0;for(let y=Math.ceil((control.y+8)*scale);y<Math.floor((control.y+control.height-8)*scale);y++)for(let x=Math.ceil((control.x+8)*scale);x<Math.floor((control.x+control.width-8)*scale);x++){const i=(y*image.width+x)*4;if(image.pixels[i]<100&&image.pixels[i+1]<100&&image.pixels[i+2]<100)ink++;}assert.ok(ink/(scale*scale)>120,`${locale} Reading options must contain visible label ink, not just a layout box`);measurements.push({locale,footerInk:ink/(scale*scale),control});}
  await writeFile(join(root,'latest.json'),JSON.stringify({ok:true,out,measurements},null,2));console.log('Native cold glyph evidence: '+out);
});
