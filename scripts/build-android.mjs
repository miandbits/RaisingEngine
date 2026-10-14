import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { cp, mkdir, readFile, writeFile, readdir, realpath, stat } from 'node:fs/promises';
import { existsSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('..', import.meta.url));
const args = process.argv.slice(2);
function option(name, fallback) { const i = args.indexOf(name); if (i < 0) return fallback; if (!args[i + 1] || args[i + 1].startsWith('--')) throw new Error(`Missing ${name} value`); return args[i + 1]; }
const bundle = path.resolve(option('--bundle', 'dist/android-game'));
const output = path.resolve(option('--out', 'dist/android'));
const sdk = option('--sdk', process.env.ANDROID_HOME ?? process.env.ANDROID_SDK_ROOT);
const java = option('--java-home', process.env.JAVA_HOME);
const gradle = option('--gradle', process.env.GRADLE ?? 'gradle');
const applicationId = option('--application-id', 'org.raisingengine.sample');
const abi = option('--abi', 'arm64-v8a');
if (!/^([a-z][a-z0-9_]*\.)+[a-z][a-z0-9_]*$/.test(applicationId)) throw new Error('Invalid Android application ID');
if (!['arm64-v8a', 'x86_64'].includes(abi)) throw new Error('Supported --abi: arm64-v8a, x86_64');
if (!sdk || !existsSync(sdk)) throw new Error('Set ANDROID_HOME or --sdk to an installed Android SDK');
if (!java || !existsSync(path.join(java, 'bin', process.platform === 'win32' ? 'java.exe' : 'java'))) throw new Error('Set JAVA_HOME or --java-home to JDK 17 or newer');
if (existsSync(output)) throw new Error('Output already exists; choose a new --out directory');
const source = await realpath(bundle);
const game = JSON.parse(await readFile(path.join(source, 'game.json'), 'utf8'));
if (game.formatVersion !== '1' || !game.content || !game.assets) throw new Error('Expected a validated Raising export bundle containing game.json');
// Do not copy desktop executables or arbitrary bundle contents into the APK.
const files = ['game.json'];
for (const asset of Object.values(game.assets)) {
  if (typeof asset !== 'string' || !/^assets\/[a-zA-Z0-9_.-]+$/.test(asset)) throw new Error('Invalid exported asset path');
  files.push(asset);
}
async function collect(relative) {
  for (const entry of await readdir(path.join(source, relative), { withFileTypes: true })) {
    const child = `${relative}/${entry.name}`;
    if (entry.isSymbolicLink()) throw new Error(`Bundle symlink unsupported: ${child}`);
    if (entry.isDirectory()) await collect(child); else if (entry.isFile()) files.push(child);
  }
}
await collect('fonts');
await collect('licenses');
const stagedFiles = [];
const digest = createHash('sha256');
for (const file of [...new Set(files)].sort()) {
  const resolved = await realpath(path.join(source, file));
  const relative = path.relative(source, resolved);
  if (relative.startsWith(`..${path.sep}`) || relative === '..' || path.isAbsolute(relative)) throw new Error(`Asset escapes bundle: ${file}`);
  if (!(await stat(resolved)).isFile()) throw new Error(`Not a bundle file: ${file}`);
  const bytes = await readFile(resolved);
  digest.update(file).update('\0').update(bytes);
  stagedFiles.push({ file, bytes });
}
await mkdir(output, { recursive: true });
const project = path.join(output, 'project');
const assets = path.join(project, 'app/src/main/assets');
await mkdir(assets, { recursive: true });
for (const { file, bytes } of stagedFiles) {
  const target = path.join(assets, 'game', file); await mkdir(path.dirname(target), { recursive: true }); await writeFile(target, bytes);
}
const buildId = digest.digest('hex');
await writeFile(path.join(assets, 'raising-build-id.txt'), buildId);
const javaRoot = path.join(project, 'app/src/main/java');
await mkdir(path.join(javaRoot, 'org/raisingengine/player'), { recursive: true });
await cp(path.join(root, 'native/player/vendor/SDL/android-project/app/src/main/java/org'), path.join(javaRoot, 'org'), { recursive: true });
await cp(path.join(root, 'native/mobile/android/RaisingActivity.java'), path.join(javaRoot, 'org/raisingengine/player/RaisingActivity.java'));
await cp(path.join(root, 'native/mobile/android/AndroidManifest.xml'), path.join(project, 'app/src/main/AndroidManifest.xml'));
const slash = value => value.replaceAll('\\', '/');
const quoted = value => "'" + slash(value).replaceAll("'", "\\'") + "'";
await writeFile(path.join(project, 'settings.gradle'), "pluginManagement { repositories { google(); mavenCentral(); gradlePluginPortal() } }\ndependencyResolutionManagement { repositories { google(); mavenCentral() } }\nrootProject.name = 'RaisingAndroid'\ninclude ':app'\n");
await writeFile(path.join(project, 'build.gradle'), "plugins { id 'com.android.application' version '8.9.1' apply false }\n");
await writeFile(path.join(project, 'gradle.properties'), 'org.gradle.jvmargs=-Xmx3072m\norg.gradle.workers.max=4\nandroid.useAndroidX=false\n');
await writeFile(path.join(project, 'local.properties'), `sdk.dir=${slash(path.resolve(sdk))}\n`);
await writeFile(path.join(project, 'app/build.gradle'), `plugins { id 'com.android.application' }
android {
  namespace 'org.raisingengine.player'
  compileSdk 35
  ndkVersion '28.2.13676358'
  defaultConfig {
    applicationId '${applicationId}'
    minSdk 26
    targetSdk 35
    versionCode 1
    versionName '0.1.0'
    ndk { abiFilters '${abi}' }
    externalNativeBuild { cmake { arguments '-DCMAKE_BUILD_TYPE=Release'; targets 'raising-player', 'SDL3-shared' } }
  }
  externalNativeBuild { cmake { path ${quoted(path.join(root, 'native/player/CMakeLists.txt'))}; version '3.22.1' } }
  buildTypes { release { minifyEnabled false } }
}
`);
const metadata = { formatVersion: '1', target: 'android', applicationId, abi, buildId, contentFingerprint: game.content.contentFingerprint,
  status: 'source-prepared', distribution: 'debug-preview', targetSdk: 35, ndk: '28.2.13676358',
  note: 'A debug APK is not a store release. Device, lifecycle, page-size and release signing verification are separate requirements.' };
const report = path.join(output, 'build.json');
await writeFile(report, JSON.stringify(metadata, null, 2));
if (!args.includes('--prepare-only')) {
  const env = { ...process.env, JAVA_HOME: java, ANDROID_HOME: sdk, ANDROID_SDK_ROOT: sdk, NODE_OPTIONS: '' };
  let executable = gradle, buildArgs = ['--no-daemon', '--console=plain', 'assembleDebug'];
  if (process.platform === 'win32' && /\.bat$/i.test(gradle)) {
    const libraries = path.resolve(path.dirname(gradle), '../lib');
    const launcher = (await readdir(libraries)).find(name => /^gradle-gradle-cli-main-[\d.]+\.jar$/.test(name))
      ?? (await readdir(libraries)).find(name => /^gradle-launcher-[\d.]+\.jar$/.test(name));
    if (!launcher) throw new Error('Gradle launcher JAR not found beside --gradle');
    executable = path.join(java, 'bin/java.exe');
    buildArgs = ['-classpath', path.join(libraries, launcher), 'org.gradle.launcher.GradleMain', ...buildArgs];
  }
  const result = spawnSync(executable, buildArgs, { cwd: project, env, stdio: 'inherit', windowsHide: true });
  if (result.error || result.status !== 0) {
    metadata.status = 'build-failed'; await writeFile(report, JSON.stringify(metadata, null, 2));
    throw result.error ?? new Error(`Android build failed (${result.status}); prepared project retained at ${project}`);
  }
  const apk = path.join(output, 'RaisingPlayer-debug.apk');
  await cp(path.join(project, 'app/build/outputs/apk/debug/app-debug.apk'), apk);
  metadata.status = 'apk-built-not-device-verified';
  metadata.apkSha256 = createHash('sha256').update(await readFile(apk)).digest('hex');
  await writeFile(report, JSON.stringify(metadata, null, 2));
  console.log(apk);
} else console.log(`Android source prepared: ${project}`);
