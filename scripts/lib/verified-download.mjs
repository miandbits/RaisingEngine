import {createHash} from 'node:crypto';
import {setTimeout as delay} from 'node:timers/promises';

// Retry transport failures and temporary HTTP responses, including a connection
// lost while reading the body. Integrity failures are never retried or accepted.
export async function fetchVerifiedBytes(url, sha256, {attempts=3, timeoutMs=300000, retryDelayMs=1000, onRetry=()=>{}}={}) {
  new URL(url);
  if(!Number.isInteger(attempts)||attempts<1||attempts>5)throw Error('Download attempts must be between 1 and 5');
  if(!/^[a-f0-9]{64}$/.test(sha256))throw Error('A pinned SHA-256 is required');
  for(let attempt=1;attempt<=attempts;attempt++) {
    let data;
    try {
      const response=await fetch(url,{signal:AbortSignal.timeout(timeoutMs)});
      if(!response.ok) {
        await response.body?.cancel();
        const error=Error(`Source download failed: HTTP ${response.status}`);
        error.retryable=response.status===408||response.status===429||response.status>=500;
        throw error;
      }
      data=Buffer.from(await response.arrayBuffer());
    } catch(error) {
      if(error.retryable===false||attempt===attempts)throw error;
      onRetry({attempt,attempts,error});
      await delay(retryDelayMs*attempt);
      continue;
    }
    if(createHash('sha256').update(data).digest('hex')!==sha256)throw Error('Corresponding-source hash mismatch');
    return data;
  }
}
