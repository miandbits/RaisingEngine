import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,writeFile,stat} from 'node:fs/promises';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {deflateSync,inflateSync} from 'node:zlib';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {decodePng,encodeRgbaPng} from '../../dist/packages/studio-service/png.js';
const sig=Buffer.from([137,80,78,71,13,10,26,10]);
// Fixture chunks use a simple bitwise CRC independent of the production lookup table.
function chunk(type,data){const b=Buffer.alloc(data.length+12);b.writeUInt32BE(data.length);b.write(type,4);data.copy(b,8);let c=0xffffffff;for(const byte of b.subarray(4,-4)){c^=byte;for(let i=0;i<8;i++)c=c&1?(c>>>1)^0xedb88320:c>>>1;}b.writeUInt32BE((c^0xffffffff)>>>0,b.length-4);return b;}
function png({width=2,height=2,depth=8,type=6,interlace=0,raw,palette,alpha}){const h=Buffer.alloc(13);h.writeUInt32BE(width);h.writeUInt32BE(height,4);h[8]=depth;h[9]=type;h[12]=interlace;return Buffer.concat([sig,chunk('IHDR',h),...(palette?[chunk('PLTE',Buffer.from(palette))]:[]),...(alpha?[chunk('tRNS',Buffer.from(alpha))]:[]),chunk('IDAT',deflateSync(Buffer.from(raw))),chunk('IEND',Buffer.alloc(0))]);}
const rgba=[0,255,0,255, 255,0,0,255, 3,250,4,120, 0,0,255,0];
const sourceBytes=png({raw:[0,...rgba.slice(0,8),0,...rgba.slice(8)]});
function rawRgba(bytes){const parts=[];for(let i=8;i<bytes.length;){const n=bytes.readUInt32BE(i),kind=bytes.toString('ascii',i+4,i+8);if(kind==='IDAT')parts.push(bytes.subarray(i+8,i+8+n));i+=n+12;}const raw=inflateSync(Buffer.concat(parts)),w=bytes.readUInt32BE(16),h=bytes.readUInt32BE(20);const out=[];for(let y=0;y<h;y++){assert.equal(raw[y*(w*4+1)],0);out.push(...raw.subarray(y*(w*4+1)+1,(y+1)*(w*4+1)));}return out;}
async function fixture(){await mkdir('.cache/sprite-import-tests',{recursive:true});const root=await mkdtemp(path.resolve('.cache/sprite-import-tests/project-'));await cp('examples/academy-adventure',root,{recursive:true});const source=path.join(root,'sheet.png');await writeFile(source,sourceBytes);const service=new QtStudioService();await service.handle({method:'open',params:{root}});return {root,source,service};}
const regions=[{x:0,y:0,width:1,height:2},{x:1,y:0,width:1,height:2}];
test('manual RGBA, palette4+tRNS, Adam7, grayscale and all filter oracle pixels decode exactly',()=>{
 assert.deepEqual([...decodePng(sourceBytes).pixels],rgba);
 assert.deepEqual([...decodePng(png({width:2,height:1,type:3,depth:4,raw:[0,0x01],palette:[0,255,0,250,10,20],alpha:[0,123]})).pixels],[0,255,0,0,250,10,20,123]);
 assert.deepEqual([...decodePng(png({interlace:1,raw:[0,...rgba.slice(0,4),0,...rgba.slice(4,8),0,...rgba.slice(8)]})).pixels],rgba);
 assert.deepEqual([...decodePng(png({width:2,height:1,type:0,depth:1,raw:[0,0x40],alpha:[0,0]})).pixels],[0,0,0,0,255,255,255,255]);
 assert.deepEqual([...decodePng(png({width:1,height:1,type:2,depth:16,raw:[0,255,255,128,128,0,0]})).pixels],[255,128,0,255]);
 for(let filter=0;filter<=4;filter++){
   const a=rgba.slice(0,8),b=rgba.slice(8),encoded=b.map((v,i)=>{const left=i>=4?b[i-4]:0,up=a[i],ul=i>=4?a[i-4]:0,n=left+up-ul;const pa=Math.abs(n-left),pb=Math.abs(n-up),pc=Math.abs(n-ul);const predictor=filter===0?0:filter===1?left:filter===2?up:filter===3?Math.floor((left+up)/2):pa<=pb&&pa<=pc?left:pb<=pc?up:ul;return(v-predictor+256)%256;});
   assert.deepEqual([...decodePng(png({raw:[0,...a,filter,...encoded]})).pixels],rgba);
 }
});
test('single manifest/history transaction preserves original, transparency, conversion provenance and save/reopen',async()=>{
 const {root,source,service}=await fixture(),revision=service.editor.revision,baseline=structuredClone(service.editor.documents);
 const params={revision,filePath:source,prefix:'job-frame',regions,colorKey:{color:'#00ff00',tolerance:5},expectedVersion:service.editor.diskVersion()};
 const reply=await service.handle({method:'importSprites',params}),result=reply.spriteImport;
 for(const id of [result.sourceAssetId,...result.assetIds])assert.ok(reply.assets.some(a=>a.id===id&&path.isAbsolute(a.absolutePath)));
 assert.deepEqual(result.assetIds,['job-frame-1','job-frame-2']);assert.equal(service.editor.revision,revision+1);
 assert.deepEqual(await readFile(source),sourceBytes);assert.deepEqual(await readFile(path.join(root,'assets',result.sourceAssetId+'.png')),sourceBytes);
 assert.deepEqual(rawRgba(await readFile(path.join(root,result.frames[0].path))),[0,255,0,0,3,250,4,0]);
 assert.deepEqual(rawRgba(await readFile(path.join(root,result.frames[1].path))),[255,0,0,255,0,0,255,0]);
 const provenance=JSON.parse(await readFile(path.join(root,result.recordPath),'utf8'));assert.deepEqual(provenance.colorKey,params.colorKey);assert.deepEqual(provenance.frames.map(x=>x.region),regions);assert.match(provenance.source.sha256,/^[a-f0-9]{64}$/);
 await service.handle({method:'edit',params:{revision:service.editor.revision,edit:{type:'undo'}}});assert.deepEqual(service.editor.documents,baseline);
 await service.handle({method:'edit',params:{revision:service.editor.revision,edit:{type:'redo'}}});const current=service.editor.revision;
 await service.handle({method:'importSprites',params:{...params,revision:current}});assert.equal(service.editor.revision,current);
 await service.handle({method:'save',params:{revision:current}});const reopened=new QtStudioService();await reopened.handle({method:'open',params:{root}});assert.ok(reopened.editor.documents.find(d=>d.kind==='assets').data.assets.some(a=>a.id==='job-frame-2'));
});
test('invalid input, CRC, bounds, palette indices, compressed size and conflicts never mutate documents',async()=>{
 const {root,source,service}=await fixture(),revision=service.editor.revision,baseline=structuredClone(service.editor.documents),base={revision,filePath:source,prefix:'test',regions};
 for(const change of [{revision:revision+1},{expectedVersion:'stale'},{prefix:'../escape'},{filePath:'relative.png'},{regions:[]},{regions:Array(257).fill(regions[0])},{regions:[{x:2,y:0,width:1,height:1}]},{regions:[{x:0,y:0,width:1.5,height:1}]},{colorKey:{color:'#00ff00',tolerance:256}}]){await assert.rejects(service.handle({method:'importSprites',params:{...base,...change}}));assert.deepEqual(service.editor.documents,baseline);assert.equal(service.editor.revision,revision);}
 const invalid=[Buffer.from(sourceBytes),png({width:1,height:1,raw:[0,...rgba]}),png({width:1,height:1,raw:[5,0,0,0,0]}),png({width:1,height:1,type:3,depth:4,raw:[0,0x10],palette:[0,0,0]})];invalid[0][45]^=1;
 for(const bytes of invalid)assert.throws(()=>decodePng(bytes),/PNG/);
 const tooLarge=png({width:8193,height:1,raw:[0]});assert.throws(()=>decodePng(tooLarge),/크기/);
 const first=await service.handle({method:'importSprites',params:base}),after=structuredClone(service.editor.documents);await assert.rejects(service.handle({method:'importSprites',params:{...base,revision:service.editor.revision,regions:[{x:1,y:0,width:1,height:1}]}}),/ID/);assert.deepEqual(service.editor.documents,after);
 const project=path.join(root,'project.json'),raw=await readFile(project,'utf8');await writeFile(project,raw+'\n');await assert.rejects(service.handle({method:'importSprites',params:{...base,prefix:'external',revision:service.editor.revision}}));assert.deepEqual(service.editor.documents,after);
 assert.ok(first.spriteImport.sourceAssetId);
});
test('draft changed while asynchronous import is pending survives without partial manifest insertion',async()=>{
 const {source,service}=await fixture(),revision=service.editor.revision,check=service.editor.assertDiskUnchanged.bind(service.editor);let calls=0;
 service.editor.assertDiskUnchanged=async()=>{await check();if(++calls===2){const doc=service.editor.documents.find(d=>d.kind==='project');service.editor.edit({type:'replace',file:doc.file,data:{...doc.data,title:'Concurrent designer edit'}},revision);}};
 await assert.rejects(service.handle({method:'importSprites',params:{revision,filePath:source,prefix:'raced',regions}}),/갱신/);
 assert.equal(service.editor.documents.find(d=>d.kind==='project').data.title,'Concurrent designer edit');assert.ok(!service.editor.documents.find(d=>d.kind==='assets').data.assets.some(a=>a.id==='raced-1'));
});
test('existing destination bytes are never overwritten when immutable storage collides',async()=>{
 const {root,source,service}=await fixture(),before=structuredClone(service.editor.documents),revision=service.editor.revision;
 const hash=createHash('sha256').update(sourceBytes).digest('hex').slice(0,24),destination=path.join(root,'assets',`sprite-source-${hash}.png`);await writeFile(destination,'existing protected bytes');
 await assert.rejects(service.handle({method:'importSprites',params:{revision,filePath:source,prefix:'collision',regions}}),/다른 파일/);
 assert.equal(await readFile(destination,'utf8'),'existing protected bytes');assert.deepEqual(service.editor.documents,before);assert.equal(service.editor.revision,revision);
});
test('actual user-selected PM2 sheet palette can be cropped without changing source',{skip:!process.env.RAISING_TEST_PM2_SPRITES},async()=>{
 const file=process.env.RAISING_TEST_PM2_SPRITES,bytes=await readFile(file),image=decodePng(bytes);assert.equal(image.width,520);assert.equal(image.height,160);assert.equal(image.pixels.length,520*160*4);
 const {root,service}=await fixture(),result=(await service.handle({method:'importSprites',params:{revision:service.editor.revision,filePath:file,prefix:'pm2-job',regions:[{x:0,y:0,width:40,height:80},{x:40,y:0,width:40,height:80}],colorKey:{color:'#00cf00',tolerance:0}}})).spriteImport;
 const actual=rawRgba(await readFile(path.join(root,result.frames[0].path))),expected=[];let transparent=0,opaque=0;
 for(let y=0;y<80;y++)for(let x=0;x<40;x++){const offset=(y*520+x)*4,p=[...image.pixels.subarray(offset,offset+4)];if(p[0]===0&&p[1]===207&&p[2]===0)p[3]=0;if(p[3]===0)transparent++;else opaque++;expected.push(...p);}
 assert.deepEqual(actual,expected);assert.ok(transparent>0&&opaque>0);assert.deepEqual(await readFile(file),bytes);
});
