import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { resolve } from 'node:path';
const roots={macos:process.env.RAISING_MACOS_PLAYER_RUNTIME||(process.platform==='darwin'?'dist/native-player-app':'dist/macos-player-app'),windows:process.env.RAISING_NATIVE_PLAYER_RUNTIME||'dist/native-player-authoring-preview',web:process.env.WEB_OUT||'dist/web'};
const report={version:1,generatedAt:new Date().toISOString(),meaning:'Declared capabilities are export gates, not hardware certification. This command reads manifests only; documented test evidence is in release-durability.md.',runtimes:{},platforms:{
  windows:{stage:'capabilities-present',verification:'native integration tests and Qt authoring tests; see release-durability.md'},
  web:{stage:'capabilities-present',verification:'Chromium Wasm play, download/upload, touch emulation; other browsers unverified'},
  android:{stage:'source-preparation-tested',verification:'Packaging tests use a fake SDK; no APK/device claim'},
  ios:{stage:'simulator-tested',verification:'Source export, simulator touch routes/save restoration and unsigned arm64 device build; see platform-support.md. Physical devices and signing unverified'},
  macos:{stage:'apple-silicon-tested',verification:'Qt GUI journey, standalone dependency audit, exported routes and saves; see platform-support.md. Intel and notarization unverified'},
  linux:{stage:'unverified',verification:'Standalone native distribution not tested in this session'},
  switch:{stage:'blocked-sdk',verification:'No approved SDK or development hardware'},
  playstation:{stage:'blocked-sdk',verification:'No approved SDK or development hardware'},
  xbox:{stage:'blocked-sdk',verification:'No approved SDK or development hardware'}
}};
for(const [target,root] of Object.entries(roots)){
  try{const bytes=await readFile(resolve(root,'capabilities.json'));const capabilities=JSON.parse(bytes);report.runtimes[target]={root:resolve(root),sha256:createHash('sha256').update(bytes).digest('hex'),runtime:capabilities.runtime,declaredFeatures:capabilities.visualFeatures??[],toolFeatures:capabilities.toolFeatures??[]};}
  catch(error){report.runtimes[target]={root:resolve(root),status:'unavailable',error:error.code??error.message};report.platforms[target].stage='runtime-unavailable';}
}
const features=[...new Set(Object.values(report.runtimes).flatMap(r=>r.declaredFeatures??[]))].sort();
report.featureMatrix=features.map(feature=>({feature,...Object.fromEntries(Object.entries(report.runtimes).map(([target,r])=>[target,r.declaredFeatures?r.declaredFeatures.includes(feature):null]))}));
const output=resolve(process.env.PLATFORM_REPORT||'.cache/durability/platform-support.json');await mkdir(resolve(output,'..'),{recursive:true});await writeFile(output,JSON.stringify(report,null,2));
console.log(JSON.stringify({output,features:features.length,platforms:report.platforms}));
