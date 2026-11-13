import test from 'node:test';
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {createHash} from 'node:crypto';
import {fetchVerifiedBytes} from '../../scripts/lib/verified-download.mjs';

const bytes=Buffer.from('Pinned corresponding source archive'),sha=createHash('sha256').update(bytes).digest('hex');
async function server(t,handler) {
  let calls=0;
  const server=createServer((req,res)=>handler(++calls,req,res));
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  t.after(()=>new Promise(resolve=>{server.closeAllConnections();server.close(resolve);}));
  return {url:`http://127.0.0.1:${server.address().port}/archive`,calls:()=>calls};
}
test('verified download retries a closed socket, a truncated body and temporary HTTP failures',async t=>{
  for(const failure of ['socket','body','http'])await t.test(failure,async t=>{
    const source=await server(t,(call,req,res)=>{
      if(call>1){res.end(bytes);return;}
      if(failure==='socket'){req.socket.destroy();return;}
      if(failure==='http'){res.writeHead(503);res.end('Temporary outage');return;}
      res.writeHead(200,{'Content-Length':bytes.length+100});res.end(bytes.subarray(0,5));
    });
    const retries=[];
    assert.deepEqual(await fetchVerifiedBytes(source.url,sha,{retryDelayMs:1,timeoutMs:1500,onRetry:r=>retries.push(r.attempt)}),bytes);
    assert.equal(source.calls(),2);assert.deepEqual(retries,[1]);
  });
});
test('verified download bounds retries and never retries invalid bytes or permanent HTTP errors',async t=>{
  for(const failure of ['outage','integrity','missing'])await t.test(failure,async t=>{
    const source=await server(t,(_,req,res)=>{if(failure==='integrity')res.end('wrong source');else {res.writeHead(failure==='missing'?404:503);res.end();}});
    await assert.rejects(fetchVerifiedBytes(source.url,sha,{retryDelayMs:1}),failure==='integrity'?/hash mismatch/:/HTTP/);
    assert.equal(source.calls(),failure==='outage'?3:1);
  });
});
