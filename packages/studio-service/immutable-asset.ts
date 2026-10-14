import {randomUUID} from 'node:crypto';
import {open,link,lstat,readFile,realpath,unlink,type FileHandle} from 'node:fs/promises';
import type {Stats} from 'node:fs';
import path from 'node:path';
/** The writer boundary permits deterministic disk-failure testing with a real temporary file. */
type AssetWriter=(file:FileHandle,bytes:Buffer)=>Promise<void>;
export async function publishImmutableAsset(directory:string,name:string,bytes:Buffer,write:AssetWriter=(file,data)=>file.writeFile(data)){
  if(path.basename(name)!==name||!/^[-a-zA-Z0-9_.]+$/.test(name)||name==='.'||name==='..')throw new Error('올바르지 않은 에셋 파일 이름입니다.');
  const canonical=await realpath(directory),destination=path.join(canonical,name),temporary=path.join(canonical,`.sprite-stage-${randomUUID()}.tmp`);
  let handle:FileHandle|undefined,owned:Stats|undefined;
  try{
    handle=await open(temporary,'wx');owned=await handle.stat();
    await write(handle,bytes);await handle.sync();await handle.close();handle=undefined;
    // link is an atomic, no-overwrite publication of an already complete file.
    // Unlike rename, it cannot replace another import's existing destination.
    try{await link(temporary,destination);}catch(error){
      if((error as NodeJS.ErrnoException).code!=='EEXIST')throw error;
      const actual=await realpath(destination);
      if(path.dirname(actual)!==canonical||!(await readFile(actual)).equals(bytes))throw new Error('같은 이름의 다른 파일 또는 외부 링크가 있습니다.');
    }
  }finally{
    await handle?.close();
    if(owned){
      try{
        const remaining=await lstat(temporary);
        // Delete only the exact regular temporary file created by this call.
        // Windows path lstat can report dev=0 while the open handle reports
        // its volume ID. The inode and resolved parent still identify our file.
        const sameDevice=remaining.dev===owned.dev||process.platform==='win32'&&remaining.dev===0;
        if(remaining.isFile()&&sameDevice&&remaining.ino===owned.ino&&await realpath(temporary)===temporary)await unlink(temporary);
      }catch(error){if((error as NodeJS.ErrnoException).code!=='ENOENT')throw error;}
    }
  }
}
