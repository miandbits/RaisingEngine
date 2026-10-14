// Plugin versions are plain MAJOR.MINOR.PATCH (no prerelease/build): store paths and
// lock pins must be unambiguous strings, and ordering must not depend on tags.
export type Version = readonly [number, number, number];
const VERSION = /^(0|[1-9]\d{0,8})\.(0|[1-9]\d{0,8})\.(0|[1-9]\d{0,8})$/;
export const VERSION_PATTERN = VERSION.source;
export const RANGE_PATTERN = /^[~^]?(0|[1-9]\d{0,8})\.(0|[1-9]\d{0,8})\.(0|[1-9]\d{0,8})$/.source;

export function parseVersion(text: string): Version | null {
  const m = VERSION.exec(text);
  return m ? [Number(m[1]), Number(m[2]), Number(m[3])] : null;
}
export function compareVersions(a: string, b: string): number {
  const x = parseVersion(a), y = parseVersion(b);
  if (!x || !y) throw new Error(`잘못된 버전: ${!x ? a : b}`);
  for (let i = 0; i < 3; i++) if (x[i] !== y[i]) return x[i]! < y[i]! ? -1 : 1;
  return 0;
}
/** `^1.2.3` (same major; for 0.x same minor; for 0.0.x exact), `~1.2.3` (same minor), or exact `1.2.3`. */
export function satisfies(version: string, range: string): boolean {
  const v = parseVersion(version);
  const op = range[0] === '^' || range[0] === '~' ? range[0] : '';
  const base = parseVersion(op ? range.slice(1) : range);
  if (!v || !base) return false;
  const cmp = compareVersions(version, base.join('.'));
  if (op === '') return cmp === 0;
  if (cmp < 0) return false;
  if (op === '~') return v[0] === base[0] && v[1] === base[1];
  if (base[0] > 0) return v[0] === base[0];
  if (base[1] > 0) return v[0] === 0 && v[1] === base[1];
  return v[0] === 0 && v[1] === 0 && v[2] === base[2];
}
