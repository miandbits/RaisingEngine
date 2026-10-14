# 사건·대본·엔딩과 저장 — 4단계

실행 규칙은 `coreVersion:0.2.0`, 콘텐츠는 `formatVersion:0.1.0`, 세이브는 `saveVersion:1.0.0`입니다. UI·파일 I/O 없는 코어에 사건·대본·엔딩을 추가하고, `runtime-service`에서 실행 세션·저장·시나리오를 관리합니다. 편집기·플레이어는 없습니다.

## 사건과 엔딩의 순서

활동 결과를 확인하면 period-end입니다. full 모드의 resolvePeriodEnd는 현재 기간의 미완료 사건 중 기간·조건을 만족하는 후보를 priority 내림차순, 같은 priority이면 ID의 ASCII 오름차순으로 정렬해 하나만 시작합니다. 기간의 양 끝을 포함합니다. 선택된 사건은 이후 최초 조건이 달라져도 계속 진행합니다.

사건의 end 노드를 처리할 때 completedEvents에 추가한 뒤 다음 기간 또는 마지막 엔딩으로 이동합니다. 같은 기간에 두 번째 사건을 고르지 않습니다. 선택되지 않은 사건은 다음 기간에 다시 평가하고 유효 기간이 지나면 만료됩니다. inspectEvents와 전이의 eventDecisions는 selected·completed·before-window·expired·requirements·priority를 구분합니다. 조건 수식 오류도 명령 전체를 거부합니다.

마지막 기간의 사건이 완료되거나 사건이 없음을 확인한 뒤 엔딩을 선택합니다. 기본 엔딩을 제외한 후보에 같은 정렬 규칙을 쓰고, 후보가 없으면 defaultEndingId를 사용합니다. endingId를 고정한 뒤 엔딩 대본을 시작하며 대본 효과로 엔딩을 다시 고르지 않습니다. 엔딩의 end에서 finished가 됩니다. 마지막 periodIndex를 유지합니다.

## 대본 실행

activeNarrative는 `{kind:'event'|'ending', id, nodeId}`입니다. `getNarrativeView(content,state)`는 현재 노드·커서·선택지별 enabled 값을 반환하고 대본 밖에서는 null입니다. 조회는 상태를 바꾸지 않으며 수식 계산 실패는 CoreFault로 보고합니다.

| 노드 | 입력과 처리 |
| --- | --- |
| scene | advanceNarrative: 생략 필드 유지, 배경·음악 null 해제, portraits 전체 교체 |
| dialogue | 진행 입력 advanceNarrative를 기다리고 next로 이동 |
| choice | choose(choiceId)를 기다리고 조건 재검사 → 효과 → next |
| branch | advanceNarrative: 조건을 평가해 then/else로 이동 |
| effect | advanceNarrative: 전체 효과와 next 이동을 함께 확정 |
| end | advanceNarrative: 사건 완료·기간 이동 또는 엔딩 종료 |

자동 노드도 한 번의 advanceNarrative에 하나만 처리합니다. 호스트·시나리오 실행기는 자동 노드를 계속 진행하고 대사·선택에서 입력을 기다립니다. UI가 없는 이번 단계에서는 시나리오가 대사 진행 입력도 제공합니다. 따라서 어떤 노드 직전·직후든 저장할 수 있고 stepLimit을 실제 실행 단위로 셀 수 있습니다.

선택 ID 누락·알 수 없는 선택·비활성 선택을 자동 대체하지 않습니다. 선택 노드에 진입했는데 전부 비활성이면 NO_AVAILABLE_CHOICE로 해당 명령을 롤백합니다. 효과 오버플로도 같은 방식입니다. 모든 성공 명령은 revision을 한 번만 증가시키고 거부는 상태·커서·난수·명령 기록을 보존합니다.

presentation에는 현재 배경·음악·인물 배치를 저장합니다. scene 노드와 activity.presentation의 사운드는 전이의 soundCues에 한 번만 남깁니다. 실제 재생은 하지 않습니다. 복원은 현재 presentation만 돌려주고 과거 사운드 큐를 다시 전달하지 않습니다.

## 실행 세션 API

`packages/runtime-service/index.ts`가 진입점입니다. 코어에 직접 명령을 보내는 API도 유지하지만 저장할 실행에는 세션 API를 사용합니다.

| 함수 | 결과 |
| --- | --- |
| createSession(content, seed, {mode?, initialOverrides?}) | `{ok:true,session}` 또는 `{ok:false,error}`. mode 기본값 full |
| dispatchSession(content, session, command, expectedRevision) | 성공 시 새 session·transition, 거부 시 원래 session·error |
| encodeSave(content, session) | 검증된 JSON text 또는 오류 |
| decodeSave(content, text) | 불변 복원 session 또는 오류 |
| saveRun(file, content, session) | 파일 저장 결과 또는 오류 |
| loadRun(file, content) | 파일에서 읽은 복원 session 또는 오류 |
| runScenario(content, scenario, options?) | 최종 session, steps, 전이·assertion 기록, 성공 또는 오류 |

RunSession은 origin(seed·mode·초기 덮어쓰기), 수락된 commands, 현재 state를 보존합니다. 거부된 명령은 journal에 추가하지 않습니다. 세션·상태·입력 기록은 깊게 freeze합니다. 저수준 명령과 초기 옵션은 [코어 문서](core.md)를 따릅니다. 세션 최대 명령 수는 100,000입니다.

```ts
const created = createSession(snapshot, 42); // full
if (!created.ok) throw new Error(created.error.message);
const planned = dispatchSession(snapshot, created.session,
  { type: 'setSchedule', activityIds: ['study', 'work', 'rest'] }, 0);
if (!planned.ok) throw new Error(planned.error.message);
const saved = await saveRun(savePath, snapshot, planned.session);
if (!saved.ok) throw new Error(saved.error.message);
const restored = await loadRun(savePath, snapshot);
// restored.session의 현재 revision으로 다음 명령을 보냅니다.
```

## 세이브 계약과 검증

스키마는 [save.schema.json](../packages/runtime-service/save.schema.json)입니다. 최상위에는 saveVersion·coreVersion·contentFingerprint·origin·commands·state를 저장합니다. 시계·파일 경로는 세이브 본문에 넣지 않으며 정규 JSON으로 직렬화합니다. 세이브 용량 한도는 16 MiB입니다.

불러오기 순서는 엄격한 UTF-8·JSON 검사(중복 키·주석·후행 쉼표 거부) → 세이브/코어 버전 → 스키마 → 콘텐츠 fingerprint → 초기값부터 명령 재현 → 저장 state 전체와 정규 JSON 비교입니다. 저장 cursor·revision·RNG·완료 사건·엔딩·능력치·표현 상태를 임의로 신뢰하지 않습니다. 스키마에서 객체로 받는 lastActivityResult의 내부 구조도 전체 재현 비교로 검사합니다.

재현이 성공하면 저장된 객체 대신 새로 재현한 불변 상태를 반환합니다. 재현 중 전이는 검증 내부에서 버리므로 선택 효과·사운드가 재적용되지 않습니다. 저장도 같은 검증을 통과해야 파일을 교체합니다. 이 검사는 일관성 검사이며 서명·치트 방지 기능은 아닙니다.

미지원 saveVersion은 SAVE_VERSION, 다른 실행 규칙은 SAVE_CORE_VERSION, 다른 콘텐츠는 CONTENT_MISMATCH입니다. 구조는 SAVE_SCHEMA, 잘못된 명령 기록은 SAVE_REPLAY, 재현 결과가 다른 상태는 SAVE_STATE_MISMATCH입니다. 이전 단계에는 정식 세이브가 없었으므로 이번 버전은 마이그레이션을 추측하지 않고 거부합니다. 향후 변환은 명시적 버전별 절차가 필요합니다.

saveRun은 기존 부모 디렉터리 안의 임시 파일에 쓰고 flush·close한 뒤 rename으로 교체합니다. 검증·쓰기·교체 실패 시 원본을 보존하고 임시 파일을 정리합니다. 이는 단일 파일 교체이며 여러 프로세스의 동시 저장 조정이나 전원 장애에 대한 디렉터리 내구성 보장은 포함하지 않습니다. 파일 저장 시간의 임시 UUID는 게임 상태나 난수 규약에 영향을 주지 않습니다.

명령 재현 비용은 저장 이력에 비례합니다. 초기 MVP는 빠른 중간 체크포인트나 journal 압축 없이 전체 재현을 사용합니다. 무결성 검사를 우회하는 빠른 로드 경로는 두지 않았습니다.

## 시나리오와 CLI

planMonth는 월초 세 활동 설정으로 확장합니다. runPeriod는 활동 → 결과 확인 → 사건 판정 → 대본 입력/자동 노드 → 다음 기간까지 진행하며 마지막 기간에는 엔딩 대본까지 마칩니다. choices는 지정된 순서를 정확히 소비하고 누락·잉여·다른 사건/노드 주소는 실패합니다. 자동으로 첫 선택을 고르지 않습니다.

runPeriod의 assertions는 다음 기간 경계 또는 최종 종료에서, 최상위 assertions는 모든 명령 뒤 검사합니다. 실패 기록에는 JSON Pointer와 assertion을 남깁니다. stepLimit은 확장된 코어 명령 수입니다. 자동 노드도 별도 명령 하나이므로 정확히 1회 셉니다.

현재 시나리오 계약의 선택 주소는 eventId·nodeId·choiceId입니다. 엔딩 선택은 코어에서 지원하지만 시나리오에서 만나면 SCENARIO_ENDING_CHOICE로 실패합니다. 엔딩 선택 주소 확장은 콘텐츠 계약의 별도 변경입니다.

```text
npm run build
npm run scenarios -- examples/academy-demo
npm run scenarios -- examples/academy-demo certified-route --roundtrip
node dist/apps/tools/cli/scenarios.js examples/academy-demo --json --roundtrip
```

기본값은 프로젝트 내 모든 시나리오입니다. `--roundtrip`은 초기화와 매 명령 뒤 encodeSave·decodeSave를 적용하고 동일 세션임을 검사합니다. 프로젝트 파일을 수정하지 않습니다. JSON 보고서는 성공 여부, 시나리오 ID, steps, assertions, 최종 상태, 오류를 포함합니다. 종료 코드는 통과 0, 콘텐츠·실행 실패 1, 사용법 오류 2입니다.

API의 options.checkpoint는 비동기 세션 저장·복원 함수를 주입할 수 있습니다. 테스트는 이 지점에 실제 saveRun·loadRun을 연결해 모든 중간 상태를 디스크로 왕복합니다. 체크포인트가 다른 세션을 반환하면 CHECKPOINT_MISMATCH입니다.

## 확인된 샘플 결과

| 경로 | 엔딩 | skill | money | stamina | stress | mentor.trust | 명령/기대값 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| certified-route | certified | 35 | 22 | 60 | 5 | 1 | 28 / 14개 통과 |
| ordinary-route | ordinary | 10 | 61 | 60 | 8 | 0 | 25 / 9개 통과 |

두 경로 모두 periodIndex 5에서 finished입니다. 수료 경로는 두 사건을 완료하고 일반 경로는 초대 사건만 완료합니다. 기존 수작업 기대값을 변경하지 않았습니다. 매 명령 뒤 실제 파일 왕복을 넣어도 최종 상태와 모든 전이 JSON 바이트가 원래 실행과 같습니다. 선택 직전 세이브를 다른 프로세스·시간대에서 이어가는 테스트와 확률 활동의 난수 연속성 테스트도 포함합니다.
