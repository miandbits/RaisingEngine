import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {createVisual} from '../../dist/packages/visual/model.js';
import {verifyPresentationVisual,presentationVisualChanges} from '../../dist/packages/visual/presentation-update.js';
import {presentationPolicy} from '../../dist/packages/visual/presentation-policy.generated.js';

test('one checked-in policy drives native and browser/export allowlists',async()=>{
  const source=JSON.parse(await readFile('config/presentation-policy.json','utf8'));
  const native=await readFile('native/player/presentation-policy.generated.hpp','utf8');
  assert.deepEqual(presentationPolicy,source);
  assert.deepEqual(JSON.parse(native.match(/R"policy\((.+)\)policy"/)[1]),source);
});
test('paint, geometry and unbound UI text may change without changing semantic controls',()=>{
  const base=createVisual();base.styles=[{id:'button',fill:'#000000'}];
  const next=structuredClone(base),button=next.screens[0].elements.find(e=>e.kind==='button');
  next.width=1440;next.defaultStyle={fontSize:24,color:'#112233'};next.styles[0].fill='#ffffff';
  button.x+=16;button.width+=32;button.text='Begin your journey';button.fill='#243745';
  verifyPresentationVisual(base,next);
  assert.ok(presentationVisualChanges(base,next).some(p=>p.endsWith('/text')));
  assert.equal(base.screens[0].elements.find(e=>e.kind==='button').action,button.action);
});
test('functional and unknown changes cannot hide inside a visual update',()=>{
  const base=createVisual();
  for(const change of [
    v=>{v.screens[0].elements.find(e=>e.kind==='button').action='load';},
    v=>{v.screens[0].elements.find(e=>e.kind==='text').binding='description';},
    v=>{v.screens[0].elements.find(e=>e.kind==='text').text='bound text must not be overridden';},
    v=>{v.screens[0].elements[0].visibleWhen='planning';},
    v=>{v.screens[0].elements[0].hidden=true;},
    v=>{v.screens[0].elements[0].futureBehavior='unknown';},
    v=>{v.screens[0].elements.reverse();},
    v=>{v.defaultScene={musicAssetId:'changed'};},
    v=>{v.screens[0].elements[0].extension={plugin:'new'};},
  ]){const next=structuredClone(base);change(next);assert.throws(()=>verifyPresentationVisual(base,next),/PRESENTATION_CONTENT_CHANGED/);}
});
