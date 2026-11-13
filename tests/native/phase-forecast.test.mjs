import test from 'node:test';
import assert from 'node:assert/strict';
import { content as fixtureContent } from '../runtime/helpers.mjs';
import { nativeRequest, closeNativeRuntimes } from '../../dist/packages/native-bridge/index.js';
import { WasmRuntime } from '../../dist/packages/native-bridge/wasm.js';
import { existsSync } from 'node:fs';
import { requestPhaseForecast } from '../../dist/packages/player-service/phase-forecast.js';

const base = await fixtureContent();
const wasmAvailable = existsSync(new URL('../../dist/wasm/raising-wasm.mjs',import.meta.url));
assert.ok(process.env.RAISING_REQUIRE_FORECAST_WASM !== '1' || wasmAvailable,'This validation run requires the WASM artifact.');
const wasm = wasmAvailable ? new WasmRuntime(await (await import('../../dist/wasm/raising-wasm.mjs')).default()) : null;
const literal = value => ({op:'literal',value});
const add = (attributeId,value) => ({op:'add',target:{kind:'attribute',attributeId},value:literal(value)});
const action = (id,effects=[],costs=[],flow=[]) => ({id,name:id,effects,costs,flow});
const rule = (id,type,actionId) => ({id,name:id,trigger:{type},actionId});
const event = (nodes,extra={}) => ({id:'secret-event-id',label:'SECRET NARRATIVE LABEL',priority:1,requirements:{op:'always'},window:{fromPeriod:0,throughPeriod:100},entryNodeId:nodes[0].id,nodes,...extra});
function fixture(count=3) {
  const c=structuredClone(base);
  c.project.durationMonths=2;c.project.periodsPerMonth=count;
  c.project.initialState.attributes={money:200,stamina:96,skill:0,stress:10};
  c.project.gameplay={startLocationId:'home',locations:[{id:'home',name:'Home',connections:[]}],items:[],shops:[],interactions:[],actions:[],rules:[]};
  c.events=[];
  c.activities=[
    {id:'train',label:'Training',requirements:{op:'always'},costs:[{attributeId:'money',amount:5},{attributeId:'stamina',amount:3}],baseEffects:[add('skill',5),add('stamina',1)]},
    {id:'rest',label:'Rest',requirements:{op:'always'},costs:[],baseEffects:[add('stamina',8)]},
  ];
  return c;
}
const native = input => nativeRequest(input);
function create(c,seed=42) {const r=native({operation:'create',content:c,seed,options:{mode:'full'}});assert.equal(r.ok,true,JSON.stringify(r));return r.state;}
function command(c,state,cmd,request=native) {const r=request({operation:'command',content:c,state,command:cmd,expectedRevision:state.stateRevision});assert.equal(r.ok,true,JSON.stringify(r));return r.state;}
function forecast(c,state,draft) {
  const before=JSON.stringify({c,state,draft});
  const a=requestPhaseForecast(c,state,draft,native);
  if(wasm)assert.deepEqual(a,requestPhaseForecast(c,state,draft,input=>wasm.request(input)),'native and WASM must expose identical forecast semantics');
  assert.equal(JSON.stringify({c,state,draft}),before,'forecast must not mutate content, draft, live state, RNG, history or revision');
  assert.equal(a.ok,true,JSON.stringify(a));
  assert.ok(Object.isFrozen(a)&&Object.isFrozen(a.forecast.attributes));
  for(const p of [a.forecast,a.baseline]) for(const row of p.attributes) {
    assert.equal(row.after-row.before,row.delta);
    assert.equal(row.costs+row.effects+row.automatic,row.delta,'the breakdown must conserve the actual clamped state change');
  }
  assert.equal('state' in a,false,'a projection must never expose a speculative save state');
  return a;
}
function completePeriod(c,state) {
  state=command(c,state,{type:'executeActivity'});
  if(state.phase==='ready'&&state.periodPreparation)state=command(c,state,{type:'executeActivity'});
  state=command(c,state,{type:'acknowledgeResult'});
  return command(c,state,{type:'resolvePeriodEnd'});
}
const values = projection => Object.fromEntries(projection.attributes.map(a=>[a.attributeId,a.after]));
test.after(()=>closeNativeRuntimes());

test('phase forecast conserves canonical costs, clamped recovery, selected policies, diet and the final shared upkeep',()=>{
  const c=fixture(4),g=c.project.gameplay;
  c.project.periodFlow={preparePolicies:true};
  g.actions=[action('recover',[add('stamina',2)]),action('upkeep',[add('stress',-1)],[{attributeId:'money',amount:2}]),action('phase-tax',[add('money',-7)]),action('meal',[add('stamina',1)],[{attributeId:'money',amount:3}])];
  g.rules=[rule('recover','periodStart','recover'),rule('upkeep','periodEnd','upkeep'),rule('phase-tax','monthEnd','phase-tax')];
  g.policyGroups=[{id:'food',name:'Food',trigger:{type:'beforeActivity',intervalPeriods:2},options:[{id:'meal',name:'Meal',actionId:'meal'}]}];
  g.diets=[{id:'diet',name:'Diet',locationId:'home',costs:[{attributeId:'money',amount:1}],effects:[add('stamina',1)]}];
  let state=create(c);
  state=command(c,state,{type:'selectPolicy',groupId:'food',optionId:'meal'});
  state=command(c,state,{type:'setDiet',dietId:'diet'});
  const draft=['train','rest','train','rest'],before=JSON.stringify(state),out=forecast(c,state,draft);
  assert.equal(out.forecast.status,'exact');assert.equal(out.forecast.complete,true);assert.equal(out.forecast.rows.length,4);
  assert.equal(out.baseline.status,'projected');assert.equal(out.baseline.complete,true);
  let real=command(c,state,{type:'setSchedule',activityIds:draft});
  for(let i=0;i<4;i++)real=completePeriod(c,real);
  assert.deepEqual(values(out.forecast),real.attributes);
  assert.deepEqual(values(out.baseline),real.attributes);
  assert.equal(real.attributes.money,165);
  assert.equal(out.forecast.attributes.find(a=>a.attributeId==='money').costs,-10);
  assert.equal(out.forecast.attributes.find(a=>a.attributeId==='money').automatic,-25);
  assert.equal(out.forecast.rows[3].boundaryChanges.find(a=>a.attributeId==='money').delta,-9,'last boundary includes period upkeep and month-end tax');
  assert.equal(JSON.stringify(state),before);
  assert.equal(real.periodIndex,4);
});

test('remaining-phase forecast preserves completed slots and does not charge prior activities twice',()=>{
  const c=fixture(4),draft=['train','train','rest','rest'];
  let state=command(c,create(c),{type:'setSchedule',activityIds:draft});
  state=completePeriod(c,state);
  const out=forecast(c,state,draft);
  assert.equal(out.forecast.fromPeriodIndex,1);assert.equal(out.forecast.throughPeriodIndex,3);
  assert.deepEqual(out.forecast.rows.map(r=>r.slot),[1,2,3]);
  assert.equal(out.forecast.attributes.find(a=>a.attributeId==='money').delta,-5);
  const rejected=forecast(c,state,['rest',...draft.slice(1)]);
  assert.equal(rejected.forecast.status,'blocked');assert.equal(rejected.forecast.issues[0].code,'COMPLETED_SLOT');
});

test('exact prefix includes deterministic pre-dialogue story upkeep without exposing narrative text; baseline excludes it',()=>{
  const c=fixture();
  c.events=[event([
    {id:'arrival',type:'scene',scene:{},next:'upkeep'},
    {id:'upkeep',type:'effect',effects:[add('money',-7),add('stamina',-6)],next:'branch'},
    {id:'branch',type:'branch',condition:{op:'always'},then:'dialogue',else:'end'},
    {id:'dialogue',type:'dialogue',speakerId:'mentor',text:'SECRET SPOILER',next:'choice'},
    {id:'choice',type:'choice',prompt:'SECRET CHOICE',choices:[{id:'yes',text:'Secret',requirements:{op:'always'},effects:[add('money',500)],next:'end'}]},
    {id:'end',type:'end'},
  ],{window:{fromPeriod:2,throughPeriod:2}})];
  const state=create(c),draft=['train','train','train'],out=forecast(c,state,draft);
  assert.equal(out.forecast.status,'partial');assert.equal(out.forecast.complete,false);
  assert.equal(out.forecast.issues[0].code,'FORECAST_NARRATIVE');assert.equal(out.forecast.rows.length,3);
  assert.equal(values(out.forecast).money,178);assert.equal(values(out.baseline).money,185);
  assert.equal(out.forecast.rows[2].boundaryChanges.find(a=>a.attributeId==='money').delta,-7);
  assert.equal(out.baseline.status,'projected');assert.equal(out.baseline.complete,true);
  assert.ok(out.baseline.assumptions.includes('EXCLUDES_NARRATIVE'));
  assert.doesNotMatch(JSON.stringify(out),/SECRET|secret-event-id|activeNarrative|completedEvents|"rng"/);
});

test('a story at the next phase start does not leak its effects into the current phase forecast',()=>{
  const c=fixture();
  c.events=[event([{id:'effect',type:'effect',effects:[add('money',-100)],next:'dialogue'},{id:'dialogue',type:'dialogue',text:'SECRET',next:'end'},{id:'end',type:'end'}],{trigger:'period-start',window:{fromPeriod:3,throughPeriod:3}})];
  const out=forecast(c,create(c),['train','train','train']);
  assert.equal(out.forecast.status,'exact');assert.equal(values(out.forecast).money,185);
});

test('intermissions remain an explicit uncertainty; the separate baseline excludes their entry effects and choices',()=>{
  const c=fixture();c.flags.push({id:'done',label:'Done',initial:false});
  c.project.periodFlow={intermission:{screenId:'home',completionFlagId:'done',entryEffects:[add('money',-1)]}};
  const state=create(c),draft=['train','train','train'],out=forecast(c,state,draft);
  assert.equal(out.forecast.status,'partial');assert.equal(out.forecast.issues[0].code,'FORECAST_INTERMISSION');
  assert.equal(values(out.forecast).money,194);assert.equal(values(out.baseline).money,185);
  assert.equal(out.baseline.complete,true);assert.ok(out.baseline.assumptions.includes('EXCLUDES_INTERMISSIONS'));
  let between=command(c,state,{type:'setSchedule',activityIds:draft});between=command(c,between,{type:'executeActivity'});between=command(c,between,{type:'acknowledgeResult'});
  const blocked=forecast(c,between,draft);assert.equal(blocked.forecast.status,'blocked');assert.equal(blocked.baseline.status,'blocked');
});

test('random activity and rule outcomes are stopped before any draw and leave subsequent real commands unchanged',()=>{
  for(const probability of [0,5000,10000]) {
    const c=fixture();c.activities[0].successCheck={chance:literal(probability)};c.activities[0].successEffects=[add('money',100)];c.activities[0].failureEffects=[];
    const state=create(c),draft=['train','rest','rest'],out=forecast(c,state,draft);
    assert.equal(out.forecast.status,'blocked');assert.equal(out.forecast.issues[0].code,'FORECAST_RANDOM');assert.equal(out.baseline.complete,false);
    const ready=command(c,state,{type:'setSchedule',activityIds:draft});
    const actual=command(c,ready,{type:'executeActivity'}),reference=command(c,ready,{type:'executeActivity'},wasm?input=>wasm.request(input):native);
    assert.deepEqual(actual,reference);assert.equal(actual.rng.draws,1,'forecast guard must restore normal RNG execution');
  }
  const c=fixture(),g=c.project.gameplay;
  g.actions=[action('risky',[add('money',-3)],[],[{id:'chance',chance:literal(5000),thenActionId:'prize'}]),action('prize',[add('money',100)])];
  g.rules=[rule('random-rule','periodEnd','risky')];
  const first=forecast(c,create(c,1),['train','rest','rest']),second=forecast(c,create(c,999),['train','rest','rest']);
  assert.deepEqual(first,second,'no outcome or hidden seed can influence a forecast');
  assert.equal(first.forecast.status,'partial');assert.equal(first.forecast.issues[0].code,'FORECAST_RANDOM');
  assert.equal(values(first.forecast).money,195,'the random transaction rolls back its preceding cost/effect too');
  assert.equal(first.forecast.rows[0].boundaryComplete,false);
});

test('invalid drafts, deferred affordability, unsupported modes and one/twelve-period bounds stay explicit',()=>{
  const c=fixture(),state=create(c);
  for(const draft of [[],['rest'],['rest','','rest'],['rest',null,'rest']]) {
    const out=forecast(c,state,draft);assert.equal(out.forecast.status,'blocked');assert.equal(out.forecast.issues[0].code,'FORECAST_INCOMPLETE');
    if(draft.length===3)for(const projection of [out.forecast,out.baseline]){assert.equal(projection.issues[0].periodIndex,1);assert.equal(projection.issues[0].path,'/activityIds/1');assert.equal(projection.rows.length,0);}
  }
  assert.equal(forecast(c,state,['missing','rest','rest']).forecast.issues[0].code,'UNKNOWN_ACTIVITY');
  const poor={...state,attributes:{...state.attributes,money:5}},out=forecast(c,poor,['train','train','rest']);
  assert.equal(out.forecast.status,'partial');assert.equal(out.forecast.rows.length,1);assert.equal(out.forecast.issues[0].code,'INSUFFICIENT_COST');
  assert.equal(values(out.forecast).money,0);
  for(const count of [1,12]) {const content=fixture(count),r=forecast(content,create(content),Array(count).fill('rest'));assert.equal(r.forecast.complete,true);assert.equal(r.forecast.rows.length,count);assert.equal(r.forecast.throughPeriodIndex,count-1);}
  const roster=fixture();roster.project.roster={};assert.equal(forecast(roster,state,['rest','rest','rest']).forecast.issues[0].code,'FORECAST_ROSTER');
  const timed=fixture();timed.project.progression={model:'unified-v2'};
  assert.equal(forecast(timed,state,['rest','rest','rest']).forecast.issues[0].code,'FORECAST_TIMED_ACTIVITY');
  const trip=fixture();trip.activities[0].expeditionId='trip';assert.equal(forecast(trip,state,['train','rest','rest']).forecast.issues[0].code,'FORECAST_EXPEDITION');
});

test('native and WASM forecast parity is explicitly exercised when the web artifact is present',{skip:wasm?false:'WASM artifact is unavailable in this headless-only build'},()=>{
  const c=fixture(4),state=create(c),draft=['train','rest','train','rest'];
  forecast(c,state,draft);
  c.activities[0].successCheck={chance:literal(5000)};c.activities[0].successEffects=[];c.activities[0].failureEffects=[];
  forecast(c,state,draft);
});

test('persisted paid preparation is not charged again, and future requirements use the projected earlier effects',()=>{
  const c=fixture(),g=c.project.gameplay;
  c.project.periodFlow={preparePolicies:true};
  g.actions=[action('meal',[add('stamina',2)],[{attributeId:'money',amount:3}])];
  g.policyGroups=[{id:'food',name:'Food',trigger:{type:'beforeActivity'},options:[{id:'meal',name:'Meal',actionId:'meal'}]}];
  c.flags.push({id:'trained',label:'Trained',initial:false});
  c.activities[0].baseEffects.push({op:'setFlag',flagId:'trained',value:true});
  c.activities[1].requirements={op:'flag',flagId:'trained',equals:true};
  let state=create(c);state=command(c,state,{type:'selectPolicy',groupId:'food',optionId:'meal'});
  assert.equal(forecast(c,state,['rest','rest','rest']).forecast.issues[0].code,'REQUIREMENTS_NOT_MET');
  const draft=['train','rest','rest'];state=command(c,state,{type:'setSchedule',activityIds:draft});state=command(c,state,{type:'executeActivity'});
  assert.ok(state.periodPreparation);assert.equal(state.attributes.money,197);
  const out=forecast(c,state,draft);
  assert.equal(out.forecast.complete,true);assert.equal(values(out.forecast).money,186,'only two remaining policy payments plus one activity cost');
  let actual=state;for(let i=0;i<3;i++)actual=completePeriod(c,actual);
  assert.deepEqual(values(out.forecast),actual.attributes);
});
