import test from 'node:test';
import assert from 'node:assert/strict';
import { anchoredElement, logicalViewport, resolveScreenElements } from '../../dist/packages/visual/components.js';
import { extractMessages, localizeContent } from '../../dist/packages/localization/catalog.js';

const button={id:'buy',kind:'button',x:20,y:90,width:160,height:40,text:'Buy',action:'navigate',screenId:'confirm',anchorX:'stretch',anchorY:'bottom'};
const instance=(id,x=0)=>({id,kind:'component',componentId:'card',x,y:20,width:200,height:150});
function fixture(){return {width:800,height:600,components:[{id:'card',label:'Product card',width:200,height:150,elements:[{id:'name',kind:'text',x:20,y:10,width:160,height:50,text:'Tea'},structuredClone(button)]}],screens:[{id:'home',label:'Home',elements:[instance('one'),instance('two',220)]},{id:'shop',label:'Shop',elements:[instance('three')]}]};}

test('shared definition edits propagate across screens and keep per-instance actions isolated',()=>{
 const design=fixture();
 design.screens[0].elements[1].overrides=[{elementId:'name',values:{text:'Apple'}},{elementId:'buy',values:{screenId:'apple-confirm'}}];
 const original=structuredClone(design),home=resolveScreenElements(design,'home');
 assert.deepEqual(home.map(e=>e.id),['one__name','one__buy','two__name','two__buy']);
 assert.equal(home[2].text,'Apple');assert.equal(home[3].screenId,'apple-confirm');assert.equal(home[1].screenId,'confirm');
 assert.equal(home[2].x,240);assert.deepEqual(design,original);
 design.components[0].elements[0].text='Fresh tea';design.components[0].elements[1].text='Purchase';
 assert.equal(resolveScreenElements(design,'home')[0].text,'Fresh tea');
 assert.equal(resolveScreenElements(design,'home')[2].text,'Apple');
 assert.equal(resolveScreenElements(design,'shop')[0].text,'Fresh tea');
 assert.equal(resolveScreenElements(design,'home')[3].text,'Purchase');
 design.screens[0].elements[1].overrides=[];
 assert.equal(resolveScreenElements(design,'home')[2].text,'Fresh tea');
 design.screens[0].elements[0].opacity=0.5;design.components[0].elements[0].opacity=0.4;
 assert.equal(resolveScreenElements(design,'home')[0].opacity,0.2);
 assert.equal(resolveScreenElements(design,'home')[1].opacity,0.5);
 assert.equal(resolveScreenElements(design,'home')[2].opacity,0.4);
});

test('responsive viewport adds logical room without changing the reference canvas or fit behavior',()=>{
 const design=fixture();assert.deepEqual(logicalViewport(design,1600,600),{width:800,height:600});
 design.viewportMode='responsive';
 assert.deepEqual(logicalViewport(design,1600,600),{width:1600,height:600});
 assert.deepEqual(logicalViewport(design,800,1200),{width:800,height:1200});
 assert.deepEqual(logicalViewport(design,1201,600),{width:1201,height:600});
 assert.deepEqual(logicalViewport(design,901,700),{width:800,height:622});
 for(const [w,h] of [[0,0],[-5,600],[NaN,600],[800,Infinity]])assert.deepEqual(logicalViewport(design,w,h),{width:800,height:600});
 assert.equal(design.width,800);assert.equal(design.height,600);
});

test('anchors retain edges, center offsets and margins, including nested component parent dimensions',()=>{
 const source={...button,x:30,y:40,width:100,height:80};
 const reference={width:800,height:600},actual={width:1200,height:900};
 const values=[['left','top',30,40,100,80],['center','center',230,190,100,80],['right','bottom',430,340,100,80],['stretch','stretch',30,40,500,380]];
 for(const [anchorX,anchorY,x,y,width,height] of values){const e=anchoredElement({...source,anchorX,anchorY},reference,actual);assert.deepEqual([e.x,e.y,e.width,e.height],[x,y,width,height]);}
 const collapsed=anchoredElement({...source,anchorX:'stretch',anchorY:'stretch'},reference,{width:1,height:1});assert.equal(collapsed.width,16);assert.equal(collapsed.height,16);
 const design=fixture();design.screens[0].elements=[{...instance('wide',30),width:300,height:200,anchorX:'stretch',anchorY:'bottom'}];
 const [name,buy]=resolveScreenElements(design,'home',actual);
 assert.deepEqual([name.x,name.y,name.width,name.height],[50,330,160,50]);
 assert.deepEqual([buy.x,buy.y,buy.width,buy.height],[50,460,660,40]);
 assert.equal(buy.action,'navigate');assert.equal(buy.screenId,'confirm');
});

test('instance visibility cannot reveal a hidden or phase-gated child and unresolved drafts stay bounded',()=>{
 const design=fixture();design.screens[0].elements=[{...instance('one'),visibleWhen:'planning'}];
 design.components[0].elements[0].hidden=true;design.components[0].elements[1].visibleWhen='dialogue';
 let elements=resolveScreenElements(design,'home');assert.ok(elements.every(e=>e.hidden));
 delete design.components[0].elements[1].visibleWhen;
 elements=resolveScreenElements(design,'home');assert.equal(elements[1].visibleWhen,'planning');
 design.screens[0].elements[0].hidden=true;assert.ok(resolveScreenElements(design,'home').every(e=>e.hidden));
 design.screens[0].elements[0].componentId='missing';assert.deepEqual(resolveScreenElements(design,'home'),[]);
 assert.deepEqual(resolveScreenElements(design,'missing'),[]);
});

test('component definition text and locale fonts translate once and flow into every instance',()=>{
 const content={project:{visual:fixture()}},key='/project/visual/components/@card/elements/@buy/text';
 assert.ok(extractMessages(content).some(e=>e.key===key&&e.source==='Buy'));
 const config={defaultLocale:'ko',fallbackLocale:'ko',locales:[{id:'ko',label:'한국어',fontAssetId:'korean-font'}],entries:[{key,source:'Buy',translations:{ko:'구매'}}]};
 const translated=localizeContent(content,config,'ko');
 const elements=resolveScreenElements(translated.project.visual,'home');
 assert.equal(elements[1].text,'구매');assert.equal(elements[3].text,'구매');assert.ok(elements.every(e=>e.fontAssetId==='korean-font'));
 assert.equal(content.project.visual.components[0].elements[1].text,'Buy');assert.equal(content.project.visual.components[0].elements[1].fontAssetId,undefined);
});
