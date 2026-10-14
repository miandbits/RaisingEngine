import fs from 'node:fs';
const file='packages/schema/visual.schema.json',s=JSON.parse(fs.readFileSync(file,'utf8'));
for(const d of [s.$defs.element,s.$defs.componentChild]){
 if(!d.properties.kind.enum.includes('group'))d.properties.kind.enum.push('group');
 d.properties.parentId={$ref:'common.schema.json#/$defs/id'};d.properties.clipChildren={type:'boolean'};
 d.properties.x.minimum=-3840;d.properties.y.minimum=-2160;
}
s.$defs.element.properties.overrides.items.properties.values.properties.clipChildren={type:'boolean'};
fs.writeFileSync(file,JSON.stringify(s,null,2)+'\n');
