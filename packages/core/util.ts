import type { CoreError, DeepReadonly } from './model.js';

export class CoreFault extends Error {
  constructor(readonly code: string, message: string, readonly path: string) { super(message); }
  diagnostic(): CoreError { return { code: this.code, message: this.message, path: this.path }; }
}
export function requireCore(condition: unknown, code: string, message: string, path: string): asserts condition {
  if (!condition) throw new CoreFault(code, message, path);
}
export function int32(value: number, path: string): number {
  requireCore(Number.isInteger(value) && value >= -2147483648 && value <= 2147483647,
    'INTEGER_OVERFLOW', '계산 결과가 int32 범위를 벗어났습니다.', path);
  return value;
}
export function freeze<T>(value: T): DeepReadonly<T> {
  if (value !== null && typeof value === 'object' && !Object.isFrozen(value)) {
    for (const child of Object.values(value)) freeze(child);
    Object.freeze(value);
  }
  return value as DeepReadonly<T>;
}
export function object(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}
