// Authoring source for the checked-in 12-month sample. Does not read simulation results.
import { mkdir, readFile, writeFile } from 'node:fs/promises';
const root = 'examples/academy-year';
const always = { op: 'always' };
const literal = value => ({ op: 'literal', value });
const target = attributeId => ({ kind: 'attribute', attributeId });
const add = (attributeId, value) => ({ op: 'add', target: target(attributeId), value: literal(value) });
const compare = (attributeId, value, comparator = 'gte') => ({ op: 'compare', comparator, left: { op: 'read', target: target(attributeId) }, right: literal(value) });
const flag = flagId => ({ op: 'flag', flagId, equals: true });
const setFlag = flagId => ({ op: 'setFlag', flagId, value: true });
const trust = value => ({ op: 'add', target: { kind: 'relationship', characterId: 'mentor', relationshipId: 'trust' }, value: literal(value) });
const end = { id: 'end', type: 'end' };
const dialogue = (text, next = 'end', id = 'intro') => ({ id, type: 'dialogue', speakerId: 'mentor', text, next });
const effects = (values, next = 'end', id = 'reward') => ({ id, type: 'effect', effects: values, next });
async function save(file, kind, data) {
  await mkdir(`${root}/${file.includes('/') ? file.slice(0, file.lastIndexOf('/')) : ''}`, { recursive: true });
  const depth = file.includes('/') ? '../../../' : '../../';
  await writeFile(`${root}/${file}`, JSON.stringify({ $schema: `${depth}packages/schema/${kind}.schema.json`, formatVersion: '0.1.0', ...data }, null, 2) + '\n');
}
const project = JSON.parse(await readFile('examples/academy-demo/project.json', 'utf8'));
delete project.$schema;
await save('project.json', 'project', { ...project, gameId: 'academy-year', title: '견습생의 사계절',
  description: '봄 입학부터 다음 해 봄까지 12개월·36기간. 기사·학자·장인·이웃·여행자의 다섯 길을 선택합니다.',
  startMonth: 4, durationMonths: 12, defaultEndingId: 'wanderer' });
await save('attributes.json', 'attributes', { attributes: [
  ...['skill','knowledge','craftsmanship'].map((id, i) => ({ id, label: ['검술','학문','제작'][i], kind: 'stat', min: 0, max: 300, initial: 0 })),
  { id: 'money', label: '소지금', kind: 'resource', min: 0, max: 10000, initial: 60 },
  { id: 'stamina', label: '체력', kind: 'condition', min: 0, max: 100, initial: 60 },
  { id: 'stress', label: '스트레스', kind: 'condition', min: 0, max: 100, initial: 0 },
  { id: 'reputation', label: '평판', kind: 'stat', min: 0, max: 100, initial: 0 },
] });
await save('flags.json', 'flags', { flags: ['knight-track','scholar-track','artisan-track','community','graduated'].map(id => ({ id,
  label: { 'knight-track':'기사 과정', 'scholar-track':'학자 과정', 'artisan-track':'장인 과정', community:'마을 지원', graduated:'졸업 인증' }[id], initial: false })) });
await save('assets/manifest.json', 'assets', { assets: [] });
for (const id of ['apprentice','mentor']) {
  const data = JSON.parse(await readFile(`examples/academy-demo/characters/${id}.json`, 'utf8')); delete data.$schema;
  await save(`characters/${id}.json`, 'character', data);
}
const activities = [
  ['study','검술 수업',5,10,[add('skill',8),add('stress',4)]],
  ['read','도서관 연구',4,8,[add('knowledge',8),add('stress',3)]],
  ['craft','공방 실습',3,8,[add('craftsmanship',8),add('stress',4)]],
  ['work','마을 일 돕기',0,10,[add('money',12),add('stress',6)]],
  ['rest','휴식',0,0,[add('stamina',30),add('stress',-20)]],
  ['socialize','이웃과 교류',1,5,[add('reputation',3),add('stress',-8),trust(2)]],
  ['expedition','숲 탐사',0,15,[add('skill',3),add('stress',5)]],
];
for (const [id,label,money,stamina,baseEffects] of activities) await save(`activities/${id}.json`, 'activity', {
  id,label,requirements: always,costs: [...(money ? [{attributeId:'money',amount:money}] : []),...(stamina ? [{attributeId:'stamina',amount:stamina}] : [])],baseEffects,
  ...(id === 'expedition' ? { description: '65% 확률로 추가 검술 4와 소지금 8. 실패하면 스트레스 3이 증가합니다.',
    successCheck: { chance: literal(6500) }, successEffects: [add('skill',4),add('money',8)], failureEffects: [add('stress',3)] } : {}),
});
const scenes = ['입학의 봄','첫 동료','여름의 점검','비 오는 도서관','공방의 불빛','가을 마을 축제','첫 서리','긴 밤의 약속','겨울의 점검','새해의 인사','마지막 준비','다시 찾아온 봄'];
const texts = ['무엇을 배우고 싶은지 천천히 골라 보자.','함께 일할 동료가 생겼구나.','작은 성취도 기록해 두렴.','비 오는 날에도 배우는 길은 많단다.','불빛 아래서 각자의 일을 이어 가는구나.','마을 축제를 준비할 사람이 필요하단다.','추워졌으니 무리하지 말고 쉬어 가렴.','길었던 하루도 돌아보면 배움이 남지.','계속해 온 노력이 드러나는 계절이구나.','새해에도 네 선택을 응원할게.','한 해의 마지막 한 달이 남았구나.','서로 다른 길에도 배운 것은 남는단다.'];
for (let month = 1; month <= 12; month++) {
  let nodes = [dialogue(texts[month-1], 'reward'), effects([trust(1)]), end];
  if (month === 1) nodes = [dialogue(texts[0], 'path'), { id:'path',type:'choice',prompt:'어떤 한 해를 보내고 싶나요?',choices:[
    ...['knight','scholar','artisan'].map((id,i) => ({id,text:['기사의 길','학자의 길','장인의 길'][i],requirements:always,effects:[setFlag(`${id}-track`)],next:'end'})),
    {id:'free',text:'아직 정하지 않고 자유롭게 지낸다.',requirements:always,effects:[],next:'end'},
  ]},end];
  if (month === 6) nodes = [dialogue(texts[5], 'festival'), {id:'festival',type:'choice',prompt:'축제 준비를 도울까요?',choices:[
    {id:'support',text:'소지금 10으로 축제를 돕는다.',requirements:compare('money',10),effects:[add('money',-10),add('reputation',5),trust(3),setFlag('community')],next:'end'},
    {id:'observe',text:'이번에는 구경하며 쉰다.',requirements:always,effects:[add('stress',-5)],next:'end'},
  ]},end];
  if (month === 10) nodes = [{id:'intro',type:'branch',condition:flag('community'),then:'thanks',else:'greeting'},
    dialogue('축제를 도와준 일을 모두 기억하고 있단다.', 'reward', 'thanks'),dialogue(texts[9], 'reward', 'greeting'),effects([trust(1)]),end];
  await save(`events/month-${String(month).padStart(2,'0')}.json`, 'event', { id:`month-${String(month).padStart(2,'0')}`,label:scenes[month-1],
    window:{fromPeriod:month*3-1,throughPeriod:month*3-1},priority:0,requirements:always,entryNodeId:'intro',nodes });
}
for (const month of [3,9]) for (const [track,attribute] of [['knight','skill'],['scholar','knowledge'],['artisan','craftsmanship']]) {
  const id = `${track}-review-${month}`;
  await save(`events/${id}.json`, 'event', {id,label:`${month}개월차 전공 발표`,window:{fromPeriod:month*3-1,throughPeriod:month*3-1},priority:50,
    requirements:{op:'all',conditions:[flag(`${track}-track`),compare(attribute,month === 3 ? 20 : 60)]},entryNodeId:'intro',
    nodes:[dialogue('꾸준히 연습한 결과가 보이는구나. 한 단계 더 나아가 보자.', 'reward'),effects([add(attribute,5),add('reputation',3)]),end]});
}
const graduate = {op:'any',conditions:['skill','knowledge','craftsmanship'].map(id=>compare(id,90)).concat(compare('reputation',30))};
await save('events/graduation.json','event',{id:'graduation',label:'졸업식',window:{fromPeriod:35,throughPeriod:35},priority:100,requirements:graduate,entryNodeId:'intro',
  nodes:[dialogue('한 해 동안 쌓아 온 배움을 이제 세상에 나눌 차례야.', 'reward'),effects([setFlag('graduated'),add('reputation',4)]),end]});
for (const [id,label,attribute] of [['knight','마을의 수호 기사','skill'],['scholar','새로운 기록을 쓰는 학자','knowledge'],['artisan','자신의 공방을 연 장인','craftsmanship'],['neighbor','마을을 잇는 이웃','reputation'],['wanderer','다음 길을 찾는 여행자',null]]) {
  await save(`endings/${id}.json`,'ending',{id,label,priority:id==='wanderer'?0:50,
    requirements:attribute?{op:'all',conditions:[flag('graduated'),compare(attribute,id==='neighbor'?30:100)]}:always,
    entryNodeId:'intro',nodes:[dialogue(`${label}. 사계절의 끝에서 또 다른 한 해가 시작됩니다.`),end]});
}
// Hand-derived route totals, independent of engine execution. Rest clamps explain stamina totals.
const routes = [
  ['knight','study','knight',134,106,'skill',15,100],['scholar','read','scholar',146,106,'knowledge',15,100],
  ['artisan','craft','artisan',158,106,'craftsmanship',15,100],['neighbor','socialize','free',192,0,'skill',40,95],
  ['wanderer','expedition','free',null,null,'skill',0,100],
];
for (const [ending,activity,choice,money,stat,attribute,reputation,stamina] of routes) {
  const commands=[];
  for(let month=1;month<=12;month++) {
    commands.push({type:'planMonth',activityIds:ending==='neighbor'?['work','rest','socialize']:[activity,'work','rest']});
    for(let period=0;period<3;period++) commands.push({type:'runPeriod',choices:period!==2?[]:month===1?[{eventId:'month-01',nodeId:'path',choiceId:choice}]:month===6?[{eventId:'month-06',nodeId:'festival',choiceId:['knight','scholar','artisan'].includes(ending)?'support':'observe'}]:[]});
  }
  const assertions=[{kind:'ending',endingId:ending},...[[attribute,stat],['money',money],['reputation',reputation],['stamina',stamina],['stress',0]].filter(([,v])=>v!==null).map(([id,v])=>({kind:'condition',condition:compare(id,v,'eq')}))];
  if (ending==='wanderer') assertions.push({kind:'condition',condition:compare('skill',36)},{kind:'condition',condition:compare('skill',84,'lte')});
  await save(`scenarios/${ending}-year.json`,'scenario',{id:`${ending}-year`,description:`12개월 ${ending} 경로. 1·6개월 선택과 매월 일정을 고정합니다.`,seed:42,
    initialOverrides:{attributes:{},flags:{},relationships:{}},commands,assertions,stepLimit:1000});
}
console.log('Authored academy-year: 12 months, 7 activities, 19 events, 5 endings and 5 scenarios.');
