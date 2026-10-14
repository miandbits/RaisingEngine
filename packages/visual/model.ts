import type { VisualDesign as GeneratedVisual } from '../schema/generated/visual.js';
export type VisualDesign = Omit<GeneratedVisual,'screens'> & {screens:Screen[]};
export type ReadonlyDesign = {readonly [K in keyof VisualDesign]: DeepReadonly<VisualDesign[K]>};
type DeepReadonly<T> = T extends object ? {readonly [K in keyof T]:DeepReadonly<T[K]>}:T;
export type Screen = GeneratedVisual['screens'][number];
export type Element = Screen['elements'][number];
export type ScreenId = Screen['id'];
export const screenNames = { title:'타이틀',schedule:'일정·활동 결과',story:'대화·선택지',ending:'엔딩' };
export const elementNames: Record<Element['kind'],string> = {plugin:'애니메이션 플러그인',input:'사용자 입력란',group:'그룹',meter:'수치 게이지',collection:'데이터 목록',actions:'행동 목록',policies:'정책 선택',diets:'식단 선택',services:'병원·서비스',sell:'물품 판매',component:'재사용 UI',panel:'패널',text:'텍스트',image:'이미지',stats:'능력·자원',schedule:'월간 일정',dialogue:'대사',choices:'선택지',result:'활동 결과',button:'버튼',locations:'이동할 장소',shop:'상점 상품',inventory:'소지품',interactions:'대화·선물',relationships:'인물 관계',equipment:'장비',quests:'퀘스트',expedition:'탐험 이동',battle:'턴제 전투','relationship-stages':'관계 단계'};
export function createVisual(): VisualDesign {
  const element=(id:string,kind:Element['kind'],x:number,y:number,width:number,height:number,extra:Partial<Element>={}):Element=>({id,kind,x,y,width,height,...extra});
  const background=()=>element('background','image',0,0,1280,720,{imageSource:'background',fit:'cover',locked:true,motion:'fade',durationMs:600});
  const panel=(id:string,x:number,y:number,w:number,h:number)=>element(id,'panel',x,y,w,h,{fill:'#fff9e8ed',radius:20});
  const text=(id:string,binding:NonNullable<Element['binding']>,x:number,y:number,w:number,h:number,size=24)=>element(id,'text',x,y,w,h,{binding,fontSize:size,color:'#304d46'});
  const button=(id:string,label:string,action:NonNullable<Element['action']>,x:number,y:number,w=160)=>element(id,'button',x,y,w,48,{text:label,action,fill:'#355c4c',color:'#ffffff',radius:10,fontSize:18});
  const portraits=()=>['left','center','right'].map((side,i)=>element(`portrait-${side}`,'image',300+i*225,80,360,610,{imageSource:`portrait-${side}` as NonNullable<Element['imageSource']>,fit:'contain',motion:i===0?'slide-left':'fade',durationMs:450}));
  const menu=()=>[button('save','저장','save',920,24,100),button('load','불러오기','load',1030,24,110),button('sound','소리','sound',1150,24,100)];
  return {width:1280,height:720,screens:[
    {id:'title',label:screenNames.title,elements:[background(),...portraits(),panel('title-panel',80,180,600,380),text('title','title',115,225,530,110,48),text('description','description',115,350,530,100,20),button('start','새 게임 시작','start',115,476,230),button('load','불러오기','load',370,476,180)]},
    {id:'schedule',label:screenNames.schedule,elements:[background(),...portraits(),panel('stats-panel',24,24,266,672),text('title','title',42,42,230,74,22),text('calendar','calendar',42,120,230,45,22),element('stats','stats',42,185,230,485,{fontSize:18}),...menu(),panel('schedule-panel',320,404,936,292),element('schedule','schedule',344,424,888,125,{visibleWhen:'planning',fontSize:18}),element('result','result',344,424,888,190,{visibleWhen:'result',fontSize:20}),button('advance','일정 확정 / 활동 시작','advance',910,625,320)]},
    {id:'story',label:screenNames.story,elements:[background(),...portraits(),...menu(),panel('dialogue-panel',44,465,1192,230),text('speaker','speaker',72,480,450,44,22),element('dialogue','dialogue',72,525,1100,90,{fontSize:26}),element('choices','choices',580,145,630,292,{visibleWhen:'choice',fontSize:21,fill:'#fff9e8ee',color:'#304d46',radius:12}),button('advance','계속','advance',1030,630,175)]},
    {id:'ending',label:screenNames.ending,elements:[background(),...portraits(),panel('ending-panel',260,150,760,430),text('ending','ending',300,210,680,125,48),element('thanks','text',300,355,680,80,{text:'당신의 선택으로 완성한 한 해',fontSize:25,align:'center'}),button('start','새로운 이야기 시작','start',450,480,380),...menu()]}
  ]};
}

