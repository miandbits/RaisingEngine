import {open,readFile,lstat,readdir,rm} from 'node:fs/promises';
import path from 'node:path';

const table=Array.from({length:256},(_,value)=>{for(let i=0;i<8;i++)value=(value>>>1)^((value&1)?0xedb88320:0);return value>>>0;});
function crc32(bytes){let crc=0xffffffff;for(const byte of bytes)crc=(crc>>>8)^table[(crc^byte)&255];return (crc^0xffffffff)>>>0;}
export async function treeFiles(root,prefix='') {
  const files=[];
  for(const entry of (await readdir(path.join(root,prefix),{withFileTypes:true})).sort((a,b)=>a.name.localeCompare(b.name))) {
    const name=prefix?prefix+'/'+entry.name:entry.name;
    if(entry.isSymbolicLink())throw Error('Sharing archives cannot contain symbolic links: '+name);
    if(entry.isDirectory())files.push(...await treeFiles(root,name));
    else if(entry.isFile())files.push(name);
    else throw Error('Unsupported archive entry: '+name);
  }
  return files;
}
/** ZIP32 with stored entries, Unicode names and Unix executable modes. No shell. */
export async function writeZip(root,files,destination) {
  if(files.length>65535)throw Error('Sharing archive exceeds the ZIP32 file limit.');
  const output=await open(destination,'wx'),central=[];let offset=0;const names=new Set();
  const put=async buffer=>{let written=0;while(written<buffer.length){const result=await output.write(buffer,written,buffer.length-written);written+=result.bytesWritten;}offset+=buffer.length;if(offset>0xffffffff)throw Error('Sharing archive exceeds 4 GiB.');};
  try {
    for(const name of files) {
      if(!name||name.startsWith('/')||name.includes('\\')||name.includes(':')||name.split('/').some(p=>!p||p==='.'||p==='..'))throw Error('Invalid archive path.');
      const key=name.normalize('NFC').toLowerCase();if(names.has(key))throw Error('Colliding archive filenames: '+name);names.add(key);
      const file=path.join(root,name),stat=await lstat(file);if(!stat.isFile()||stat.size>256*1024*1024)throw Error('Archive requires regular files of at most 256 MiB: '+name);
      const bytes=await readFile(file),filename=Buffer.from(name),crc=crc32(bytes),start=offset;
      if(filename.length>65535)throw Error('Archive filename is too long.');
      const local=Buffer.alloc(30);local.writeUInt32LE(0x04034b50);local.writeUInt16LE(20,4);local.writeUInt16LE(0x800,6);local.writeUInt16LE(33,12);
      local.writeUInt32LE(crc,14);local.writeUInt32LE(bytes.length,18);local.writeUInt32LE(bytes.length,22);local.writeUInt16LE(filename.length,26);
      await put(local);await put(filename);await put(bytes);
      const record=Buffer.alloc(46);record.writeUInt32LE(0x02014b50);record.writeUInt16LE(0x0314,4);record.writeUInt16LE(20,6);record.writeUInt16LE(0x800,8);record.writeUInt16LE(33,14);
      record.writeUInt32LE(crc,16);record.writeUInt32LE(bytes.length,20);record.writeUInt32LE(bytes.length,24);record.writeUInt16LE(filename.length,28);record.writeUInt32LE(((stat.mode&0xffff)<<16)>>>0,38);record.writeUInt32LE(start,42);
      central.push(Buffer.concat([record,filename]));
    }
    const start=offset;for(const record of central)await put(record);const length=offset-start;
    const end=Buffer.alloc(22);end.writeUInt32LE(0x06054b50);end.writeUInt16LE(files.length,8);end.writeUInt16LE(files.length,10);end.writeUInt32LE(length,12);end.writeUInt32LE(start,16);await put(end);await output.sync();
  }catch(error){await output.close();await rm(destination,{force:true});throw error;}
  await output.close();return destination;
}
