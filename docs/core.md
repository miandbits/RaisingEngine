# 화면 없는 코어 — 3·4단계

`packages/core`는 React·Electron·파일·네트워크·시스템 시각에 의존하지 않는 TypeScript 라이브러리입니다. 콘텐츠 스냅샷과 현재 상태, 명령을 받아 새 상태와 처리 기록을 반환합니다. 외부 패키지 런타임 의존성도 없습니다. 활동과 사건·대본·엔딩을 구현합니다. 세션·저장·시나리오 API는 [런타임 문서](runtime.md)를 따릅니다.

## 모듈과 API

| 파일 | 책임 |
| --- | --- |
| project-service/snapshot.ts | 구조·의미·에셋 검사, 콘텐츠 고정, canonical JSON·SHA-256 |
| core/model.ts | 읽기 전용 상태·명령·전이·오류 타입, coreVersion |
| core/run.ts | 초기화, 명령 수락·거부, 원자적 활동 실행, 일정·기간 전이 |
| core/evaluation.ts | 달력, 정수 수식, 조건, 순차 효과·변경 기록 |
| core/random.ts | 버전 고정 PRNG와 편향 없는 정수 추출 |
| core/narrative.ts | 사건 후보·엔딩 판정, 대본 노드·선택·표현 상태 |
| core/index.ts | 호스트가 사용하는 공개 API |

`buildSnapshot(loaded)`는 비동기이며 `{ok:true, snapshot, diagnostics}` 또는 `{ok:false, diagnostics}`를 반환합니다. 오류가 있는 초안을 직접 코어에 넘기지 않습니다.

`createRun(snapshot, seed, {mode:'full'|'activities-only', initialOverrides?})`는 `{ok:true, state}` 또는 `{ok:false, error}`를 반환합니다. 정의의 initial → 프로젝트 initialState → 실행 덮어쓰기 순서로 합칩니다. 덮어쓰기는 attributes·flags·relationships 세 객체를 모두 포함하며, 존재하지 않는 ID·범위 초과·잘못된 타입은 거부합니다. 초기값은 clamp하지 않습니다.

`dispatchCommand(snapshot, state, command, expectedRevision)`은 동기 함수입니다. 성공하면 `{ok:true, state, transition}`, 거부하면 `{ok:false, state, error}`를 반환합니다. 거부된 state는 입력과 같은 객체입니다. error에는 안정된 code, 한국어 message, 논리적인 JSON Pointer path가 있습니다. 예: `/activities/study/costs/0`. 실제 파일의 줄·열은 로더·검증기의 진단에서 제공합니다.

공개 상태와 스냅샷은 깊게 freeze합니다. API는 buildSnapshot과 createRun/dispatchCommand 또는 runtime-service의 검증된 복원이 만든 객체를 입력받습니다. 임의 JSON을 바로 상태로 넘기지 않고 decodeSave/loadRun으로 검증합니다. 명령 자체와 seed·덮어쓰기·예상 버전은 런타임에서도 검사합니다. 예상되는 실행 오류는 구조화해서 반환하고 프로그래밍 오류는 예외로 남깁니다.

## 상태와 명령

상태는 JSON으로 직렬화할 수 있습니다. 난수의 BigInt 내부 값은 10진수 문자열로 저장합니다. `coreVersion:'0.2.0'`, `contentFingerprint`, `mode`, `stateRevision`, `periodIndex`, `phase`, `schedule`, `attributes`, `flags`, `relationships`, `rng`, `completedEvents`, `lastActivityResult`, `activeNarrative`, `endingId`, `presentation`을 보존합니다. activities-only에서는 완료 사건이 비어 있고 endingId는 null입니다.

| 명령 | 허용 phase | 결과 |
| --- | --- | --- |
| `{type:'setSchedule', activityIds:[a,b,c]}` | planning, ready | 세 활동의 존재 확인, 일정 저장, ready |
| `{type:'executeActivity'}` | ready | 현재 슬롯 실행, activity-result |
| `{type:'acknowledgeResult'}` | activity-result | period-end |
| `{type:'completePeriodWithoutEvents'}` | period-end | 다음 슬롯 ready, 다음 달 planning 또는 finished |
| `{type:'resolvePeriodEnd'}` | period-end, full 전용 | 사건 시작 또는 다음 기간·엔딩으로 이동 |
| `{type:'advanceNarrative'}` | narrative, choice 제외 | 현재 대본 노드 하나 처리 |
| `{type:'choose', choiceId}` | narrative의 choice | 조건 재검사·선택 효과·커서 이동 |

completePeriodWithoutEvents는 activities-only 전용입니다. full 모드에서 사건을 건너뛰는 용도로 사용할 수 없습니다.

월초에는 세 슬롯 전체를 설정합니다. 월중에는 완료한 슬롯이 기존 일정과 같아야 하고 현재·미래 슬롯만 바꿀 수 있습니다. 미래의 지불 가능성은 일정 저장 시 확정하지 않습니다. 실행 직전 조건과 비용을 다시 검사하며 부족하면 ready를 유지해 재편성할 수 있습니다. 결과 확인·기간 종료를 건너뛰거나 끝난 회차를 계속 실행할 수 없습니다.

수락한 명령마다 revision이 정확히 1 증가합니다. 같은 일정을 다시 제출하는 것도 수락된 편집이므로 증가합니다. 기대 버전이 다르면 `REVISION_CONFLICT`; 같은 기대 버전으로 명령을 재전송해도 부분 효과를 다시 적용하지 않습니다. 서비스가 상태 보관·명령 직렬화를 담당해야 하며 코어가 전역 세션을 관리하지는 않습니다.

시간의 원본은 periodIndex입니다. calendarTime이 year·month·periodInMonth를 유도합니다. 활동·결과 확인·사건 중에는 현재 기간을 유지하고 사건 없음 또는 사건 end를 처리한 뒤 증가합니다. 마지막 기간은 인덱스를 넘기지 않고 엔딩 대본까지 처리한 뒤 finished가 됩니다. activities-only 모드는 기간 종료 명령에서 직접 이동합니다.

## 트랜잭션과 결과 기록

1. 콘텐츠·코어 버전, 예상 상태 버전, 명령 형태와 phase를 검사합니다.
2. 실행 직전 상태에서 조건·모든 비용·성공 확률을 평가합니다.
3. 임시 상태의 난수를 추출하고 비용 → baseEffects → 성공/실패 effects 순으로 적용합니다.
4. 전체가 유효한 경우에만 임시 상태와 난수, 결과 기록, 새 revision을 함께 확정합니다.

비용은 대상 min 이상을 유지할 수 있어야 합니다. 두 번째 비용이 부족해도 첫 번째 비용을 차감하지 않습니다. 효과와 달리 지불 비용을 clamp하지 않습니다. 확률은 비용·baseEffects 전 값을 읽고, 효과는 앞선 비용·효과를 반영한 임시 값을 읽습니다.

수치 연산의 모든 중간 값은 int32여야 합니다. 오버플로를 바깥 clamp로 숨길 수 없습니다. 유효한 효과 결과는 대상 min/max로 제한하고, 수치 변경마다 before·operand·requested·after·delta·clamped와 원인 경로를 남깁니다. delta는 설명용 차이이며 양 끝이 int32여도 int32보다 클 수 있습니다. 플래그 변경도 전후 값을 기록합니다.

활동 결과에는 활동 ID·시점·변경 목록·성공 판정을 보존합니다. 판정은 원래 확률, 제한한 확률, roll, passed, 소비한 원시 난수 개수입니다. 판정 없는 활동은 success:null이며 난수를 소비하지 않습니다. transition은 명령, 전후 revision·phase, 새 상태의 시점, 이번 활동 결과와 대본 처리 narrative·사건 후보 eventDecisions·새 엔딩 endingSelected·soundCues를 포함합니다. runtime-service는 수락된 명령을 누적하고 전이 기록은 호출자에게 반환합니다.

주요 오류는 INVALID_COMMAND, WRONG_PHASE, UNKNOWN_ACTIVITY, COMPLETED_SLOT, REQUIREMENTS_NOT_MET, INSUFFICIENT_COST, INTEGER_OVERFLOW, INVALID_SEED, INITIAL_RANGE, REVISION_CONFLICT, CONTENT_MISMATCH, INCOMPATIBLE_STATE입니다.

## 난수와 재현성

알고리즘 ID는 `pcg32-xsh-rr-v1`입니다. PCG32 XSH-RR의 64비트 LCG multiplier는 6364136223846793005, 고정 stream은 54, increment는 109입니다. 내부 산술은 mod 2^64입니다. seed는 0..4294967295의 정수로 제한합니다.

초기 상태 0에서 LCG를 한 번 진행하고 seed를 더한 뒤 다시 한 번 진행합니다. 이 초기화 두 번은 draws에 포함하지 않습니다. 추출은 이전 상태에서 `x = (((old >> 18) XOR old) >> 27) mod 2^32`, `rotation = old >> 59`를 얻고 x를 오른쪽 회전한 uint32입니다. 다음 내부 상태와 draws도 함께 반환합니다.

seed 42의 원시 출력은 `a15c02b7 7b47f409 ba1d3330 83d2f293 bfa4784b cbed606e`입니다. [PCG 공식 기본 구현 사용 문서](https://www.pcg-random.org/using-pcg-c-basic.html)의 표준 seed 42/stream 54 벡터로 검증합니다.

성공 판정은 0..9999를 균등 추출합니다. 원시 uint32가 4294960000 이상이면 버리고 다시 추출한 후 `% 10000`을 취합니다. 일반적인 PCG bounded helper와 달리 **상단 구간 거부 방식**으로 고정합니다. 확률 0·10000도 한 번의 bounded draw를 요청하고, 거부 추출이 있으면 원시 draws는 둘 이상 증가할 수 있습니다. seed 42의 첫 판정값은 1783입니다. `roll < chance`일 때 성공합니다.

재현에는 동일 콘텐츠 fingerprint·coreVersion·난수 알고리즘 버전·초기 덮어쓰기·seed·명령 순서·예상 revision이 필요합니다. 서로 다른 seed가 매번 다른 성공/실패를 보장하지는 않습니다. 테스트는 전체 상태와 전이 기록, 실제 roll을 비교합니다. 다른 프로세스·다른 TZ에서도 같은 입력은 JSON 바이트가 일치합니다. Node에서 검증했으며 Electron·브라우저 통합 테스트는 후속 단계입니다.

## 콘텐츠 스냅샷

snapshotVersion은 `1`입니다. project·attributes·flags·characters·activities·events·endings·assets와 assetHashes를 묶습니다. 에셋 경로는 프로젝트 안의 실재 파일이어야 하고 바이트의 SHA-256을 포함합니다. 에셋 자체의 배포 패키징은 후속 단계이며, 현재 코어는 에셋 파일을 읽지 않습니다.

캐릭터·활동·사건·엔딩 문서 목록은 ID의 코드 단위 순서로 정렬합니다. 문서 내부 배열 순서는 보존합니다. 객체 키는 JavaScript UTF-16 코드 단위 오름차순으로 정렬하고 각 키·문자열·숫자는 JSON.stringify 표현을 씁니다. 공백 없는 정규 JSON의 UTF-8 SHA-256 앞에 `sha256:`을 붙입니다. 정규화한 객체 자체를 반환해 원본 키 순서가 실행 기록으로 새어 나오지 않게 합니다.

소스 절대 경로·위치 맵·파일 버전·문서 최상위 $schema·시나리오는 제외합니다. 설명·표시 이름 등 콘텐츠 필드 변경은 보수적으로 fingerprint를 바꿉니다. 스냅샷은 검증 전에 입력 문서부터 복제하므로 호출 후 문서 변경이 섞이지 않습니다. 자산 디스크 동시 수정에 대한 파일 잠금·트랜잭션 저장은 아직 구현하지 않았습니다.

정규화 벡터: `{z:2,a:{b:true,a:null},list:[3,1]}` → `{"a":{"a":null,"b":true},"list":[3,1],"z":2}`. `{}`의 SHA-256은 `44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a`입니다.

## 실행과 후속 경계

```text
npm test
npm run demo:core
```

스크립트는 검증된 academy-demo를 로드해 `study/work/rest`, `study/rest/study`를 실행합니다. 최종 수치는 skill 30, money 27, stamina 60, stress 5이고 periodIndex 5, revision 20, phase finished입니다. 판정 없는 샘플이므로 draws는 0입니다. 난수 포함 활동은 별도 테스트 fixture에서 검증합니다.

demo:core는 3단계 활동 전용 예제를 유지합니다. 4단계 전체 게임 실행은 full 모드의 resolvePeriodEnd와 대본 명령을 사용합니다. `npm run scenarios -- examples/academy-demo --roundtrip`으로 기존 두 경로의 사건·엔딩과 저장 후 재현까지 검사할 수 있습니다. 편집기와 플레이어는 만들지 않았습니다.
