import { object, requireCore } from './util.js';

import type { UserFieldDefinition } from '../schema/generated/project.js';
export type { UserFieldDefinition };

/** Unicode scalar values; no normalization, trimming, coercion or truncation. */
export function userTextIssue(value: unknown, minLength: number, maxLength: number): string | null {
  if (typeof value !== 'string') return '텍스트 값을 입력하세요.';
  if (/[\u0000-\u001f\u007f-\u009f\u2028\u2029]/u.test(value)) return '줄바꿈과 제어 문자는 사용할 수 없습니다.';
  let length = 0;
  for (const scalar of value) {
    const point = scalar.codePointAt(0)!;
    if (point >= 0xd800 && point <= 0xdfff) return '올바른 Unicode 텍스트가 필요합니다.';
    length++;
  }
  return length < minLength || length > maxLength ? `${minLength}~${maxLength}자 텍스트가 필요합니다.` : null;
}

/** Omit the property altogether for old projects, preserving exact save replay. */
export function resolveUserValues(fields: readonly UserFieldDefinition[] | undefined, input: unknown): Record<string, string> | undefined {
  const supplied = input === undefined ? {} : input;
  requireCore(object(supplied), 'INVALID_USER_VALUES', '사용자 입력은 객체여야 합니다.', '/userValues');
  const definitions = fields ?? [];
  const ids = new Set(definitions.map(field => field.id));
  for (const key of Object.keys(supplied)) requireCore(ids.has(key), 'UNKNOWN_USER_FIELD', `알 수 없는 사용자 필드: ${key}`, `/userValues/${key.replaceAll('~', '~0').replaceAll('/', '~1')}`);
  if (!definitions.length) return undefined;
  const values: Record<string, string> = {};
  for (const field of definitions) {
    const value = Object.hasOwn(supplied, field.id) ? supplied[field.id] : field.initial;
    const problem = userTextIssue(value, field.minLength, field.maxLength);
    requireCore(problem === null, 'INVALID_USER_VALUE', `${field.label}: ${problem ?? ''}`, `/userValues/${field.id}`);
    values[field.id] = value as string;
  }
  return values;
}
