import { createRun, dispatchCommand } from '../../dist/packages/core/run.js';

/** Deterministic, bounded hostile API shapes. This is not gameplay randomness. */
export function malformedCorpus(content) {
  const state=createRun(content,42,{mode:'full'}).state;
  const ready=dispatchCommand(content,state,{type:'setSchedule',activityIds:['rest','work','study']},0).state;
  const base={
    create:{operation:'create',content,seed:42,options:{mode:'full'}},
    view:{operation:'view',content,state},
    command:{operation:'command',content,state,command:{type:'setSchedule',activityIds:['rest','rest','rest']},expectedRevision:0},
    execute:{operation:'command',content,state:ready,command:{type:'executeActivity'},expectedRevision:1},
    replay:{operation:'replay',content,origin:{seed:42,mode:'full'},commands:[]},
    evaluate:{operation:'evaluate',content,state,expression:{op:'literal',value:1}},
  };
  const corpus=[];
  const add=(label,value)=>corpus.push({label,raw:JSON.stringify(value)});
  for(const raw of ['{','[','null','true','42','"text"','{} extra','{"operation":','{"operation":"info","x":1e400}',
    '{"operation":"info","x":"\\ud800"}','{"operation":"info","x":"\\u0000"}']) corpus.push({label:'raw:'+raw.slice(0,50),raw});
  let nested=null; for(let i=0;i<24;i++)nested={nested};
  const values=[null,false,true,0,-1,0.5,2147483648,-2147483649,4294967296,Number.MAX_SAFE_INTEGER,1e100,
    '', 'missing', '__proto__','constructor','한글🌷\u0000',[],[null],[1,2,3],{}, {id:'missing'}, nested];
  const paths={
    create:[['seed'],['options'],['options','mode'],['options','initialOverrides'],['options','initialOverrides','attributes'],['options','initialOverrides','attributes','money']],
    view:[['state'],['state','phase'],['state','periodIndex'],['state','stateRevision'],['state','activeNarrative'],['state','activeNarrative','kind'],['state','activeNarrative','id'],['state','gameplay']],
    command:[['state'],['state','phase'],['state','stateRevision'],['state','rng'],['state','attributes'],['command'],['command','type'],['command','activityIds'],['command','activityIds',0],['expectedRevision']],
    execute:[['state','rng'],['state','rng','state'],['state','rng','increment'],['state','periodIndex'],['state','slotIndex'],['state','schedule'],['state','attributes'],['state','attributes','money'],['expectedRevision']],
    replay:[['origin'],['origin','seed'],['origin','mode'],['commands'],['commands',0]],
    evaluate:[['state'],['expression'],['expression','op'],['expression','value'],['condition']],
  };
  // Common envelope and content contract boundaries, in addition to command data.
  for(const operation of Object.keys(base)) {
    for(const path of [['operation'],['content'],['content','project'],['content','attributes'],['content','events'],...paths[operation]]) {
      for(const value of values) {
        const input=structuredClone(base[operation]); let target=input;
        for(let i=0;i<path.length-1;i++) {
          const key=path[i]; if(!target[key]||typeof target[key]!=='object')target[key]={}; target=target[key];
        }
        target[path.at(-1)]=structuredClone(value);
        add(`${operation}/${path.join('/')}/${values.indexOf(value)}`,input);
      }
      const missing=structuredClone(base[operation]); let target=missing;
      for(const key of path.slice(0,-1)) { if(!target[key]||typeof target[key]!=='object')target[key]={}; target=target[key]; }
      delete target[path.at(-1)]; add(`${operation}/${path.join('/')}/missing`,missing);
    }
  }
  // Interleave operations/paths using a reproducible shuffle before bounding.
  const raw=corpus.splice(0,11); let rng=0x0badc0de;
  for(let i=corpus.length-1;i>0;i--) { rng=(Math.imul(rng,1664525)+1013904223)>>>0;const j=rng%(i+1);[corpus[i],corpus[j]]=[corpus[j],corpus[i]]; }
  return [...raw,...corpus.slice(0,1000-raw.length)];
}
