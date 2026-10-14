// Minimal ZIP reader/writer for *.raiseplugin packages (stored + deflate, no ZIP64,
// no encryption). The reader is the trust boundary for downloaded archives, so it
// rejects anything that could escape the staging folder or exhaust memory.
import { crc32, deflateRawSync, inflateRawSync } from 'node:zlib';

export const MAX_ZIP_ENTRIES = 2000;
export const MAX_UNPACKED_BYTES = 64 * 1024 * 1024;
export interface ZipEntry { name: string; data: Buffer }
export class PluginError extends Error {
  constructor(readonly code: string, message: string) { super(message); }
}
const fail = (code: string, message: string): never => { throw new PluginError(code, message); };

const RESERVED = /^(?:con|prn|aux|nul|com[0-9]|lpt[0-9])(?:\.|$)/i;
/** Portable relative path: forward slashes only, no drive/ADS colon, no `.`/`..`, safe on Windows. */
export function isSafeRelativePath(name: string): boolean {
  if (!name || name.length > 240 || /[\\:\0*?"<>|]/.test(name) || name.startsWith('/')) return false;
  return name.split('/').every(s => s !== '' && s !== '.' && s !== '..' && !/[ .]$/.test(s) && !RESERVED.test(s)
    && [...s].every(c => c.charCodeAt(0) >= 32));
}

export function readZip(bytes: Buffer): ZipEntry[] {
  let eocd = -1;
  for (let i = bytes.length - 22; i >= Math.max(0, bytes.length - 22 - 65535); i--) {
    if (bytes.readUInt32LE(i) === 0x06054b50) { eocd = i; break; }
  }
  if (eocd < 0) fail('PLUGIN_PACKAGE_FORMAT', 'ZIP 끝 레코드를 찾을 수 없습니다. 올바른 .raiseplugin 파일인지 확인하세요.');
  const count = bytes.readUInt16LE(eocd + 10), cdSize = bytes.readUInt32LE(eocd + 12), cdOffset = bytes.readUInt32LE(eocd + 16);
  if (bytes.readUInt16LE(eocd + 4) !== 0 || bytes.readUInt16LE(eocd + 6) !== 0 || bytes.readUInt16LE(eocd + 8) !== count)
    fail('PLUGIN_PACKAGE_FORMAT', '분할 ZIP은 지원하지 않습니다.');
  if (count === 0xffff || cdOffset === 0xffffffff) fail('PLUGIN_PACKAGE_FORMAT', 'ZIP64 패키지는 지원하지 않습니다.');
  if (count > MAX_ZIP_ENTRIES) fail('PLUGIN_PACKAGE_LIMIT', `패키지 항목은 ${MAX_ZIP_ENTRIES}개 이하여야 합니다.`);
  if (cdOffset + cdSize > eocd) fail('PLUGIN_PACKAGE_FORMAT', 'ZIP 중앙 디렉터리가 손상되었습니다.');
  const entries: ZipEntry[] = [], seen = new Set<string>();
  let offset = cdOffset, total = 0;
  for (let n = 0; n < count; n++) {
    if (offset + 46 > eocd || bytes.readUInt32LE(offset) !== 0x02014b50) fail('PLUGIN_PACKAGE_FORMAT', 'ZIP 중앙 디렉터리 항목이 손상되었습니다.');
    const madeBy = bytes.readUInt16LE(offset + 4), flags = bytes.readUInt16LE(offset + 8), method = bytes.readUInt16LE(offset + 10);
    const crc = bytes.readUInt32LE(offset + 16), packed = bytes.readUInt32LE(offset + 20), size = bytes.readUInt32LE(offset + 24);
    const nameLength = bytes.readUInt16LE(offset + 28), extra = bytes.readUInt16LE(offset + 30), comment = bytes.readUInt16LE(offset + 32);
    const external = bytes.readUInt32LE(offset + 38), local = bytes.readUInt32LE(offset + 42);
    const name = bytes.toString('utf8', offset + 46, offset + 46 + nameLength);
    offset += 46 + nameLength + extra + comment;
    if (offset > eocd) fail('PLUGIN_PACKAGE_FORMAT', 'ZIP 중앙 디렉터리 항목이 손상되었습니다.');
    if (flags & 0x41) fail('PLUGIN_PACKAGE_FORMAT', `암호화된 항목은 지원하지 않습니다: ${name}`);
    if (packed === 0xffffffff || size === 0xffffffff) fail('PLUGIN_PACKAGE_FORMAT', 'ZIP64 항목은 지원하지 않습니다.');
    // Unix "made by" (high byte 3) stores st_mode in the upper 16 bits; symlinks could redirect writes.
    const mode = (external >>> 16) & 0o170000;
    if ((madeBy >>> 8) === 3 && mode === 0o120000) fail('PLUGIN_PACKAGE_PATH', `심볼릭 링크는 패키지에 넣을 수 없습니다: ${name}`);
    if ((external & 0x400) !== 0) fail('PLUGIN_PACKAGE_PATH', `링크(재분석 지점) 항목은 패키지에 넣을 수 없습니다: ${name}`);
    const directory = name.endsWith('/');
    const clean = directory ? name.slice(0, -1) : name;
    if (!isSafeRelativePath(clean)) fail('PLUGIN_PACKAGE_PATH', `허용되지 않는 패키지 경로입니다(절대 경로·..·특수 문자): ${name}`);
    if (directory) { if (size !== 0) fail('PLUGIN_PACKAGE_FORMAT', `폴더 항목에 데이터가 있습니다: ${name}`); continue; }
    const key = clean.toLowerCase();
    if (seen.has(key)) fail('PLUGIN_PACKAGE_PATH', `중복된 패키지 경로입니다: ${name}`);
    seen.add(key);
    total += size;
    if (size > MAX_UNPACKED_BYTES || total > MAX_UNPACKED_BYTES) fail('PLUGIN_PACKAGE_LIMIT', '패키지 압축 해제 크기는 64 MiB 이하여야 합니다.');
    if (local + 30 > cdOffset || bytes.readUInt32LE(local) !== 0x04034b50) fail('PLUGIN_PACKAGE_FORMAT', `ZIP 로컬 헤더가 손상되었습니다: ${name}`);
    const start = local + 30 + bytes.readUInt16LE(local + 26) + bytes.readUInt16LE(local + 28);
    if (start + packed > cdOffset) fail('PLUGIN_PACKAGE_FORMAT', `ZIP 항목 데이터가 손상되었습니다: ${name}`);
    const raw = bytes.subarray(start, start + packed);
    let data: Buffer;
    if (method === 0) data = Buffer.from(raw);
    else if (method === 8) {
      // maxOutputLength bounds a lying size field (zip bomb) before allocation.
      try { data = inflateRawSync(raw, { maxOutputLength: Math.max(1, size) }); }
      catch { return fail('PLUGIN_PACKAGE_FORMAT', `압축 데이터가 손상되었거나 선언 크기보다 큽니다: ${name}`); }
    } else return fail('PLUGIN_PACKAGE_FORMAT', `지원하지 않는 압축 방식(${method})입니다: ${name}`);
    if (data.length !== size || (crc32(data) >>> 0) !== crc) fail('PLUGIN_PACKAGE_CRC', `CRC 또는 크기가 맞지 않습니다: ${name}`);
    entries.push({ name: clean, data });
  }
  return entries;
}

/** Deterministic archive (fixed timestamp, sorted input order kept by the caller). */
export function writeZip(entries: readonly ZipEntry[]): Buffer {
  if (entries.length > MAX_ZIP_ENTRIES) fail('PLUGIN_PACKAGE_LIMIT', `패키지 항목은 ${MAX_ZIP_ENTRIES}개 이하여야 합니다.`);
  const locals: Buffer[] = [], central: Buffer[] = [];
  let offset = 0;
  const seen = new Set<string>();
  for (const entry of entries) {
    if (!isSafeRelativePath(entry.name) || seen.has(entry.name.toLowerCase())) fail('PLUGIN_PACKAGE_PATH', `잘못되었거나 중복된 경로: ${entry.name}`);
    seen.add(entry.name.toLowerCase());
    const name = Buffer.from(entry.name, 'utf8'), crc = crc32(entry.data) >>> 0;
    const deflated = deflateRawSync(entry.data, { level: 9 });
    const method = deflated.length < entry.data.length ? 8 : 0, body = method === 8 ? deflated : entry.data;
    const header = Buffer.alloc(30);
    header.writeUInt32LE(0x04034b50, 0); header.writeUInt16LE(20, 4); header.writeUInt16LE(0x0800, 6); header.writeUInt16LE(method, 8);
    header.writeUInt16LE(0, 10); header.writeUInt16LE(0x21, 12); header.writeUInt32LE(crc, 14);
    header.writeUInt32LE(body.length, 18); header.writeUInt32LE(entry.data.length, 22); header.writeUInt16LE(name.length, 26);
    const record = Buffer.alloc(46);
    record.writeUInt32LE(0x02014b50, 0); record.writeUInt16LE(20, 4); record.writeUInt16LE(20, 6); record.writeUInt16LE(0x0800, 8);
    record.writeUInt16LE(method, 10); record.writeUInt16LE(0, 12); record.writeUInt16LE(0x21, 14); record.writeUInt32LE(crc, 16);
    record.writeUInt32LE(body.length, 20); record.writeUInt32LE(entry.data.length, 24); record.writeUInt16LE(name.length, 28);
    record.writeUInt32LE(offset, 42);
    locals.push(header, name, body); central.push(record, name);
    offset += header.length + name.length + body.length;
  }
  const directory = Buffer.concat(central), end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50, 0); end.writeUInt16LE(entries.length, 8); end.writeUInt16LE(entries.length, 10);
  end.writeUInt32LE(directory.length, 12); end.writeUInt32LE(offset, 16);
  return Buffer.concat([...locals, directory, end]);
}
