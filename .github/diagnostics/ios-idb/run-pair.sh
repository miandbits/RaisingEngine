#!/bin/bash
set -euo pipefail
test -z "${SDL_EVENT_LOGGING+x}${SIMCTL_CHILD_SDL_EVENT_LOGGING+x}"
test -z "${RAISING_INPUT_TRACE+x}${SIMCTL_CHILD_RAISING_INPUT_TRACE+x}"
test -z "${RAISING_IOS_HOST_CONTACT_TOOL+x}${RAISING_SYSTEM_UI_HOST_CONTACT+x}${TEST_RUNNER_RAISING_SYSTEM_UI_HOST_CONTACT+x}"
case "$RAISING_IDB_ORDER" in
  a-b) routes=(xctest idb) ;;
  b-a) routes=(idb xctest) ;;
  *) exit 2 ;;
esac

mkdir -p .cache/ios-system-ui
compatible=false
failed=0
for index in "${!routes[@]}"; do
  route="${routes[$index]}"
  before="$(cat .cache/ios-system-ui/latest.json 2>/dev/null || true)"
  status=0
  RAISING_IOS_CONTACT_ROUTE="$route" node scripts/test-ios-system-ui.mjs .cache/ios-simulator-ci/export \
    --project examples/my-first-raising-game --family iphone --test files-roundtrip --text-scale 2 || status=$?
  if [[ "$status" != 0 ]]; then failed=1; fi
  outcome="$(node --input-type=module - "$index" "$route" "$status" "$before" <<'JS'
import {readFileSync,writeFileSync} from 'node:fs';
import path from 'node:path';
const [index,backend,status,before]=process.argv.slice(2);
const base=path.resolve('.cache/ios-system-ui');
let directory,receipt;
try {
  const latest=readFileSync(path.join(base,'latest.json'),'utf8');
  const {out}=JSON.parse(latest);
  if(latest!==before&&path.dirname(out)===base&&path.basename(out).startsWith('run-')) {
    directory=path.basename(out);
    receipt=JSON.parse(readFileSync(path.join(out,'result.json'),'utf8'));
  }
} catch(error) {console.error(error.message);}
const compatibility=receipt?.contactExperiment?.noInputCompatibility??'not-observed';
const cleaned=!!receipt&&(!receipt.device?.id||receipt.deletedDevice===true)&&
  !receipt.contactExperiment?.companionCleanupError&&receipt.contactExperiment?.companionCleanup?.forced!==true;
writeFileSync(path.join(base,`arm-${Number(index)+1}.json`),JSON.stringify({
  index:Number(index)+1,backend,runnerExit:Number(status),directory,compatibility,cleaned,
  filesPassed:receipt?.actualFilesRoundTrip===true&&receipt?.ok===true&&Number(status)===0,
},null,2),{flag:'wx'});
process.stdout.write(`${compatibility} ${cleaned}`);
JS
)"
  read -r compatibility cleaned <<< "$outcome"
  reason=
  if [[ "$cleaned" != true ]]; then
    reason='The previous attempt did not establish owned device and companion cleanup.'
    compatible=false
  elif [[ "$route" == idb ]]; then
    if [[ "$compatibility" == passed ]]; then compatible=true
    else reason='The owned B no-input gate was not established.'
    fi
  fi
  if [[ -n "$reason" ]]; then
    if [[ "$index" == 0 ]]; then
      node --input-type=module - "${routes[1]}" "$reason" <<'JS'
import {writeFileSync} from 'node:fs';
writeFileSync('.cache/ios-system-ui/arm-2.json',JSON.stringify({
  index:2,backend:process.argv[2],attempted:false,reason:process.argv[3],
},null,2),{flag:'wx'});
JS
    fi
    echo "$reason Do not start another arm."
    failed=1
    break
  fi
done
echo "compatible=$compatible" >> "$GITHUB_OUTPUT"
exit "$failed"
