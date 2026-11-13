/** A notification is only a wake-up hint: an earlier response may notify after
 * its status was consumed and reset for the next request. Wait on the predicate
 * with one total deadline so that a late/spurious wake cannot expose stale bytes.
 */
export function waitForResponse(control:Int32Array,timeoutMs:number):boolean {
  const deadline=performance.now()+timeoutMs;
  while(Atomics.load(control,0)===0){
    const remaining=deadline-performance.now();
    if(remaining<=0)return false;
    Atomics.wait(control,0,0,remaining);
  }
  return true;
}
