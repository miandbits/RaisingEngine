import {readFile,writeFile} from 'node:fs/promises';
const file='packages/schema/project.schema.json',s=JSON.parse(await readFile(file,'utf8'));
const locale={type:'string',pattern:'^[a-z]{2,3}(-[A-Za-z0-9]{2,8})*$',maxLength:40};
s.properties.localization={type:'object',additionalProperties:false,required:['defaultLocale','fallbackLocale','locales','entries'],properties:{
 defaultLocale:locale,fallbackLocale:locale,
 locales:{type:'array',minItems:1,maxItems:32,items:{type:'object',additionalProperties:false,required:['id','label'],properties:{id:locale,label:{type:'string',minLength:1,maxLength:80},fontAssetId:{$ref:'common.schema.json#/$defs/id'}}}},
 entries:{type:'array',maxItems:50000,items:{type:'object',additionalProperties:false,required:['key','source','translations'],properties:{key:{type:'string',minLength:1,maxLength:1000,pattern:'^/'},source:{type:'string',maxLength:20000},translations:{type:'object',maxProperties:32,propertyNames:locale,additionalProperties:{type:'string',maxLength:20000}}}}}
}};
await writeFile(file,JSON.stringify(s,null,2)+'\n');
