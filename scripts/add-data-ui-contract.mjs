import fs from 'node:fs';
const file='packages/schema/visual.schema.json',s=JSON.parse(fs.readFileSync(file,'utf8'));
const id={$ref:'common.schema.json#/$defs/id'},condition={$ref:'common.schema.json#/$defs/condition'},text={type:'string',maxLength:4000},primitive={oneOf:[{type:'string',maxLength:4000},{type:'number'},{type:'boolean'}]};
const object=(properties,required=[])=>({type:'object',additionalProperties:false,properties,required});
const field={type:'string',pattern:'^[A-Za-z][A-Za-z0-9_-]{0,63}$'};
s.properties.dataTables={type:'array',maxItems:100,items:object({id,name:text,columns:{type:'array',minItems:1,maxItems:100,items:object({id,name:text,type:{enum:['text','number','boolean']}},['id','name','type'])},rows:{type:'array',maxItems:1000,items:object({id,values:{type:'object',additionalProperties:primitive}},['id','values'])}},['id','name','columns','rows'])};
for(const d of [s.$defs.element,s.$defs.componentChild]){d.properties.visibleCondition=condition;d.properties.enabledCondition=condition;}
const d=s.$defs.element,p=d.properties;if(!p.kind.enum.includes('collection'))p.kind.enum.push('collection');
p.source=object({type:{enum:['shopOffers','policyOptions','relationships','table']},shopId:id,groupId:id,characterId:id,tableId:id},['type']);
p.layout={enum:['list','grid']};p.columns={type:'integer',minimum:1,maximum:12};p.gap={type:'integer',minimum:0,maximum:128};p.rowHeight={type:'integer',minimum:16,maximum:2160};
p.bindings={type:'array',maxItems:200,items:object({elementId:id,property:{enum:['text','label','imageAssetId','action','actionId','shopId','itemId','groupId','optionId','screenId','locationId','interactionId','quantity','visible','enabled']},field,format:{type:'string',maxLength:1000}},['elementId','property','field'])};
p.rowFilter=object({field,operator:{enum:['eq','ne','gt','gte','lt','lte']},value:primitive},['field','operator','value']);
if(!d.allOf.some(x=>x.if?.properties?.kind?.const==='collection'))d.allOf.push({if:{properties:{kind:{const:'collection'}},required:['kind']},then:{properties:{source:{},componentId:{}},required:['source','componentId']}});
fs.writeFileSync(file,JSON.stringify(s,null,2)+'\n');
