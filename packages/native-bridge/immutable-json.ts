/** Only deeply frozen JSON can be reused by identity across runtime requests. */
const immutableJsonObjects=new WeakSet<object>();
export function immutableJson(value:unknown,active=new Set<object>()):boolean {
  if(value===null||typeof value==='string'||typeof value==='boolean'||typeof value==='number')return true;
  if(typeof value!=='object'||active.has(value))return false;
  if(immutableJsonObjects.has(value))return true;
  if(!Object.isFrozen(value))return false;
  const prototype=Object.getPrototypeOf(value);
  if(prototype!==Object.prototype&&prototype!==Array.prototype&&prototype!==null)return false;
  active.add(value);
  for(const descriptor of Object.values(Object.getOwnPropertyDescriptors(value))){
    if(!('value' in descriptor)||!immutableJson(descriptor.value,active)){active.delete(value);return false;}
  }
  active.delete(value);immutableJsonObjects.add(value);return true;
}
