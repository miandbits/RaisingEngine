import type { DeepReadonly, RandomState } from './model.js';
import { requireCore } from './util.js';

const MASK = (1n << 64n) - 1n;
const MULTIPLIER = 6364136223846793005n;
const INCREMENT = 109n; // PCG stream 54, fixed in algorithm version v1.
const advance = (state: bigint) => (state * MULTIPLIER + INCREMENT) & MASK;

export function seedRandom(seed: number): RandomState {
  requireCore(Number.isInteger(seed) && seed >= 0 && seed <= 0xffffffff,
    'INVALID_SEED', 'seed는 uint32 정수여야 합니다.', '/seed');
  return { algorithm: 'pcg32-xsh-rr-v1', seed, state: advance((advance(0n) + BigInt(seed)) & MASK).toString(), draws: 0 };
}

export function nextUint32(rng: DeepReadonly<RandomState>): { value: number; rng: RandomState } {
  requireCore(rng.algorithm === 'pcg32-xsh-rr-v1' && /^(0|[1-9][0-9]{0,19})$/.test(rng.state)
    && BigInt(rng.state) <= MASK && Number.isSafeInteger(rng.draws) && rng.draws >= 0 && rng.draws < Number.MAX_SAFE_INTEGER,
  'INVALID_RANDOM_STATE', '지원하지 않거나 손상된 난수 상태입니다.', '/rng');
  const old = BigInt(rng.state);
  const x = Number((((old >> 18n) ^ old) >> 27n) & 0xffffffffn);
  const rotation = Number(old >> 59n);
  return {
    value: ((x >>> rotation) | (x << ((-rotation) & 31))) >>> 0,
    rng: { ...rng, state: advance(old).toString(), draws: rng.draws + 1 },
  };
}

/** High-tail rejection avoids modulo bias; rejected draws remain part of RNG state. */
export function sampleBelow(rng: DeepReadonly<RandomState>, bound: number): { value: number; rng: RandomState } {
  requireCore(Number.isInteger(bound) && bound >= 1 && bound <= 0x100000000,
    'INVALID_RANDOM_BOUND', '난수 상한은 1..2^32 정수여야 합니다.', '/bound');
  const limit = Math.floor(0x100000000 / bound) * bound;
  let result = nextUint32(rng);
  while (result.value >= limit) result = nextUint32(result.rng);
  return { value: result.value % bound, rng: result.rng };
}
