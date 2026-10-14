import { access,cp } from 'node:fs/promises';
import path from 'node:path';
const destination=path.resolve('projects/rules-lab');
try{await access(destination);throw new Error(`Refusing to overwrite ${destination}`);}catch(error){if(error.code!=='ENOENT')throw error;}
await cp('examples/academy-adventure',destination,{recursive:true,errorOnExist:true,force:false});
console.log(destination);
// Policies, actions, budgets, UI and navigation are authored by Qt widgets.
