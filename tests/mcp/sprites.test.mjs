import test from 'node:test';
import assert from 'node:assert/strict';
import {writeFile,readFile} from 'node:fs/promises';
import path from 'node:path';
import {connect,fixture} from './helpers.mjs';
import {encodeRgbaPng,decodePng} from '../../dist/packages/studio-service/png.js';
test('MCP imports sprites through shared service, persists one manifest and rejects stale/read-only writes',async t=>{
 const root=await fixture(),filePath=path.join(root,'sheet.png');await writeFile(filePath,encodeRgbaPng({width:2,height:1,pixels:Buffer.from([0,255,0,255,255,0,0,255])}));
 const mcp=await connect(t,root),project=await mcp.call('read_project'),input={expectedVersion:project.version,filePath,prefix:'mcp-sprite',regions:[{x:0,y:0,width:1,height:1}],colorKey:{color:'#00ff00',tolerance:0}};
 const reply=await mcp.call('import_sprites',input);assert.equal(reply.ok,true,JSON.stringify(reply));assert.equal(reply.changes.length,1);assert.equal(reply.changes[0].file,'assets/manifest.json');assert.deepEqual(reply.spriteImport.assetIds,['mcp-sprite-1']);
 assert.deepEqual([...decodePng(await readFile(path.join(root,reply.spriteImport.frames[0].path))).pixels],[0,255,0,0]);
 const manifest=await readFile(path.join(root,'assets/manifest.json'),'utf8'),stale=await mcp.call('import_sprites',{...input,prefix:'stale'});assert.equal(stale.ok,false);assert.equal(stale.error.code,'VERSION_CONFLICT');assert.equal(await readFile(path.join(root,'assets/manifest.json'),'utf8'),manifest);
 const readOnly=await connect(t,root,{readOnly:true}),denied=await readOnly.call('import_sprites',{...input,expectedVersion:reply.version,prefix:'readonly'});assert.equal(denied.ok,false);assert.equal(denied.error.code,'READ_ONLY');assert.equal(mcp.stderr(),'');
});
