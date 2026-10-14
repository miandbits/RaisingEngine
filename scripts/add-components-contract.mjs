import {readFile,writeFile} from 'node:fs/promises';
const file='packages/schema/visual.schema.json',schema=JSON.parse(await readFile(file,'utf8'));
const element=schema.$defs.element;
element.properties.anchorX={enum:['left','center','right','stretch']};
element.properties.anchorY={enum:['top','center','bottom','stretch']};
const child=structuredClone(element);
child.properties.kind.enum=['panel','text','image','button'];
delete child.properties.componentId;delete child.properties.overrides;
child.allOf=child.allOf.filter(rule=>rule.if?.properties?.kind?.const!=='component');
schema.$defs.componentChild=child;
schema.$defs.component={type:'object',additionalProperties:false,required:['id','label','width','height','elements'],properties:{
 id:{$ref:'common.schema.json#/$defs/id'},label:{type:'string',minLength:1,maxLength:100},
 width:{type:'integer',minimum:16,maximum:3840},height:{type:'integer',minimum:16,maximum:2160},
 elements:{type:'array',maxItems:100,items:{$ref:'#/$defs/componentChild'}}
}};
schema.properties.viewportMode={enum:['fit','responsive']};
schema.properties.components={type:'array',maxItems:256,items:{$ref:'#/$defs/component'}};
if(!element.properties.kind.enum.includes('component'))element.properties.kind.enum.push('component');
element.properties.componentId={$ref:'common.schema.json#/$defs/id'};
const fields=['text','label','imageAssetId','action','screenId','locationId','shopId','itemId','interactionId','quantity','styleId','fill','color','fontSize'];
element.properties.overrides={type:'array',maxItems:100,items:{type:'object',additionalProperties:false,required:['elementId','values'],properties:{
 elementId:{$ref:'common.schema.json#/$defs/id'},values:{type:'object',additionalProperties:false,properties:Object.fromEntries(fields.map(field=>[field,structuredClone(element.properties[field])]))}
}}};
element.allOf=element.allOf.filter(rule=>rule.if?.properties?.kind?.const!=='component');
element.allOf.push({if:{properties:{kind:{const:'component'}},required:['kind']},then:{properties:{componentId:{}},required:['componentId']}});
await writeFile(file,JSON.stringify(schema,null,2)+'\n');
