import path from 'node:path';

/** Reject accidental authoring metadata and signing secrets in a player runtime.
 * This is a packaging boundary, not a scanner or a confidentiality guarantee.
 */
export function assertDistributableRuntimePath(relative:string):void {
  const parts=relative.split(/[\\/]/).map(part=>part.toLowerCase());
  const name=parts.at(-1)??'';
  if(parts.some(part=>['.git','.studio','.cache','node_modules'].includes(part))||
      name==='.env'||name.startsWith('.env.')||
      /^project-backup(?:[-.].*)?\.zip$/.test(name)||
      ['.key','.pem','.p12','.pfx'].includes(path.extname(name))) {
    throw new Error(`Player runtime contains a non-distributable file: ${relative}. Remove authoring backups, local metadata and private keys from the runtime before exporting.`);
  }
}
