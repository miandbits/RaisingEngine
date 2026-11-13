import {open,realpath} from 'node:fs/promises';
import path from 'node:path';
import {PNG_LIMITS,validatedPngDimensions,type PngDimensions} from '../media/png.js';
import {surfaceReferences,surfaceInsetsProblem} from '../visual/surfaces.js';
import {diagnostic,type Diagnostic,type LoadedProject,type ProjectDocument,type JsonObject} from './model.js';
/** Validate the bytes used by every declared surface, not merely the initially visible screen. */
export async function validateSurfaceAssets(project:LoadedProject,documents:ProjectDocument[]):Promise<Diagnostic[]> {
  const diagnostics:Diagnostic[]=[],manifest=documents.find(d=>d.kind==='project');if(!manifest)return diagnostics;
  const refs=surfaceReferences((manifest.data as JsonObject).visual);if(!refs.length)return diagnostics;
  const assets=documents.filter(d=>d.kind==='assets').flatMap(d=>((d.data as JsonObject).assets??[]) as JsonObject[]);
  const decoded=new Map<string,Promise<PngDimensions>>();
  const dimensions=(id:string,file:string):Promise<PngDimensions>=>{
    let result=decoded.get(id);if(result)return result;
    result=(async()=>{const root=await realpath(project.root),resolved=await realpath(path.resolve(root,file)),relative=path.relative(root,resolved);
      if(relative==='..'||relative.startsWith('..'+path.sep)||path.isAbsolute(relative))throw Error('Surface image must remain inside the project.');
      const handle=await open(resolved,'r');try{const info=await handle.stat();if(!info.isFile()||info.size>PNG_LIMITS.maxBytes)throw Error('Surface PNG must be a regular file no larger than 20 MiB.');return validatedPngDimensions(await handle.readFile());}finally{await handle.close();}
    })();decoded.set(id,result);return result;
  };
  for(const {surface,pointer} of refs){
    if(surface.mode==='none')continue;
    const asset=assets.find(a=>a.id===surface.imageAssetId);
    // The common semantic walker already diagnoses missing IDs and incorrect asset kinds.
    if(!asset||asset.kind!=='image')continue;
    if(typeof asset.path!=='string'||path.extname(asset.path).toLowerCase()!=='.png'){diagnostics.push(diagnostic(manifest,pointer+'/imageAssetId','UI_SURFACE_PNG_REQUIRED','UI surfaces require a whole PNG image.',[surface.imageAssetId]));continue;}
    try{const problem=surfaceInsetsProblem(surface,await dimensions(surface.imageAssetId,asset.path));if(problem)diagnostics.push(diagnostic(manifest,pointer+'/insets','UI_SURFACE_INSETS',problem,[surface.imageAssetId]));}
    catch(error){diagnostics.push(diagnostic(manifest,pointer+'/imageAssetId','UI_SURFACE_PNG_INVALID',`Surface PNG cannot be decoded: ${error instanceof Error?error.message:'unknown error'}`,[surface.imageAssetId]));}
  }
  return diagnostics;
}
