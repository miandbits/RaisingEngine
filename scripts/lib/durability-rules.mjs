import {readFile} from 'node:fs/promises';

// Synthetic stress content. It must never be presented as designer-authored content.
export async function durabilityRules(project) {
  const template=JSON.parse(await readFile('examples/rule-extension-lab/dojo.raiserules','utf8'));
  const packages=Array.from({length:16},(_,i)=>{
    const id=`durable-pack-${i}`, pack=JSON.parse(JSON.stringify(template).replaceAll('"dojo"',JSON.stringify(id)));
    pack.id=id;pack.name=`내구성 규칙 ${i}`;return pack;
  });
  const progressions=Array.from({length:32},(_,i)=>({id:`durable-flow-${i}`,name:`내구성 진행 ${i}`,packageId:packages[i%16].id,version:'1.0.0',ruleId:'training-cycle',scope:'shared',args:{stat:'skill',gain:1}}));
  project.ruleExtensions={apiVersion:1,packages,progressions};
  return {packages:packages.length,progressions:progressions.length,linkedEvents:0,conditionCalls:0,effectCalls:0};
}

export function connectDurabilityEvent(data,index,coverage) {
  if(index>=256)return;
  const packageId=`durable-pack-${index%16}`;
  data.requirements={op:'all',conditions:[data.requirements,{op:'ruleCondition',packageId,version:'1.0.0',ruleId:'threshold',args:{stat:'skill',minimum:10}}]};
  const reward=data.nodes.find(n=>n.type==='effect');
  if(reward){reward.effects.push({op:'ruleEffect',packageId,version:'1.0.0',ruleId:'practice',args:{stat:'skill',gain:1}});coverage.effectCalls++;}
  coverage.linkedEvents++;coverage.conditionCalls++;
}
