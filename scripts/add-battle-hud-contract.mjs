import fs from 'node:fs';
const file='packages/schema/visual.schema.json',s=JSON.parse(fs.readFileSync(file,'utf8'));
const actions=['selectBattleAction','selectBattleTarget','confirmBattleAction','cancelBattleAction'];
for(const d of [s.$defs.element,s.$defs.componentChild]){
 if(!d.properties.kind.enum.includes('meter'))d.properties.kind.enum.push('meter');
 for(const action of actions)if(!d.properties.action.enum.includes(action))d.properties.action.enum.push(action);
 d.properties.value={type:'number'};d.properties.maxValue={type:'number',exclusiveMinimum:0};d.properties.battleTarget={enum:['self','enemy']};
 for(const [action,field]of [['selectBattleAction','battleAction'],['selectBattleTarget','battleTarget']])if(!d.allOf.some(x=>x.if?.properties?.action?.const===action))d.allOf.push({if:{properties:{action:{const:action}},required:['action']},then:{properties:{[field]:{}},required:[field]}});
 if(!d.allOf.some(x=>x.if?.properties?.action?.enum?.includes('selectBattleAction')))d.allOf.push({if:{properties:{action:{enum:['battleAction','selectBattleAction']},battleAction:{const:'useItem'}},required:['action','battleAction']},then:{properties:{itemId:{}},required:['itemId']}});
}
for(const kind of ['battleActors','battleActions','battleTargets','battleLog','battleRewards'])if(!s.$defs.element.properties.source.properties.type.enum.includes(kind))s.$defs.element.properties.source.properties.type.enum.push(kind);
for(const property of ['value','maxValue','battleAction','battleTarget'])if(!s.$defs.element.properties.bindings.items.properties.property.enum.includes(property))s.$defs.element.properties.bindings.items.properties.property.enum.push(property);
const overrides=s.$defs.element.properties.overrides.items.properties.values.properties;
for(const property of ['value','maxValue','battleAction','battleTarget'])overrides[property]=structuredClone(s.$defs.element.properties[property]);
for(const action of actions)if(!overrides.action.enum.includes(action))overrides.action.enum.push(action);
fs.writeFileSync(file,JSON.stringify(s,null,2)+'\n');
const saveFile='packages/runtime-service/save.schema.json',save=JSON.parse(fs.readFileSync(saveFile,'utf8'));
save.$defs.battleSelection={type:'object',additionalProperties:false,properties:{stateRevision:{$ref:'#/$defs/revision'},battleAction:{enum:['attack','guard','flee','useItem']},itemId:{$ref:'#/$defs/id'},target:{enum:['self','enemy']}},required:['stateRevision','battleAction']};
save.properties.playerPresentation.properties.battleSelection={anyOf:[{$ref:'#/$defs/battleSelection'},{type:'null'}]};
fs.writeFileSync(saveFile,JSON.stringify(save,null,2)+'\n');
