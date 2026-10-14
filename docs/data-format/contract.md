# 데이터 계약 0.1.0

이 계약은 월 3기간 육성 게임의 콘텐츠 형식과 실행 의미를 규정합니다. 스키마·로더·정적 의미 검증기는 구현되어 있으며 실행 의미를 적용하는 코어는 후속 단계입니다. 실행에 관한 “해야 한다”는 향후 코어의 통과 기준입니다.

## 파일과 버전

모든 콘텐츠 문서는 `formatVersion: "0.1.0"`을 요구합니다. `$schema`는 선택적인 편집기 힌트이며 제품은 이 값을 따라 네트워크에 접근하지 않습니다. 문서 종류는 고정된 위치로 결정합니다.

| 위치 | 스키마 |
| --- | --- |
| project.json | project.schema.json |
| attributes.json | attributes.schema.json |
| flags.json | flags.schema.json |
| characters/*.json | character.schema.json |
| activities/*.json | activity.schema.json |
| events/*.json | event.schema.json |
| endings/*.json | ending.schema.json |
| assets/manifest.json | assets.schema.json |
| scenarios/*.json | scenario.schema.json |

`common.schema.json`은 공통 정의 라이브러리이며 직접 검증하는 콘텐츠 문서가 아닙니다. 스키마 `$id`의 `raising-engine.example`은 소유 도메인이나 다운로드 서버가 아닌 예약된 예시 네임스페이스입니다. 모든 `$ref`는 로컬 등록된 스키마로 해석합니다.

필수 파일은 project·attributes·flags·assets/manifest이며 각각 하나만 존재합니다. 빈 flags·assets 목록과 빈 portraits는 유효합니다. 텍스트 전용 샘플도 완전한 프로젝트로 취급합니다. JSON 주석과 중복 객체 키는 허용하지 않습니다. 로더는 중복 키를 조용히 마지막 값으로 덮어쓰지 않고 오류로 보고해야 합니다.

알 수 없는 필드는 거부합니다. 미지원 formatVersion은 읽기 전용으로 취급하며 저장하지 않습니다. 0.x도 명시적 버전 전환을 사용하고 호환성을 추정하지 않습니다. 기본값을 검증기가 자동 삽입하거나 입력을 변형하지 않습니다.

## ID와 이름

ID는 최대 64자, 소문자 ASCII 영문자로 시작하며 숫자와 단일 하이픈 구분을 사용할 수 있습니다. 표시 이름과 설명은 한국어를 포함한 UTF-8 텍스트입니다. 같은 종류의 최상위 ID는 프로젝트 안에서 유일해야 합니다. 다른 종류 사이에는 같은 ID가 있을 수 있습니다.

인물의 관계·표정 ID는 해당 인물 안에서, 대본 노드 ID는 해당 사건·엔딩 안에서, 선택지 ID는 해당 선택 노드 안에서 유일합니다. 파일명은 문서 ID와 일치해야 합니다. 참조는 경로가 아니라 해당 종류의 ID입니다.

## 시간

`startYear`는 1~9999, `startMonth`는 1~12, `durationMonths`는 1~1200입니다. `periodsPerMonth`는 3으로 고정합니다. `periodIndex`는 시작점 기준 0부터 `durationMonths * 3 - 1`까지입니다. `periodInMonth`는 0=상순, 1=중순, 2=하순입니다.

`monthOffset = floor(periodIndex / 3)`, `absoluteMonth = startMonth - 1 + monthOffset`입니다. 달은 `absoluteMonth % 12 + 1`, 연도는 `startYear + floor(absoluteMonth / 12)`입니다. 연말을 넘겨도 같은 계산을 적용합니다.

사건의 `window.fromPeriod`와 `throughPeriod`는 양끝 포함입니다. from ≤ through이고 둘 다 게임 기간 안이어야 합니다. 사건 조건은 해당 기간의 활동 결과가 적용된 뒤, 다음 기간으로 넘어가기 전에 평가합니다. 마지막 사건 중에도 periodIndex는 마지막 유효 기간을 유지합니다.

## 수치와 초기 상태

AttributeDefinition의 `kind`는 stat·resource·condition입니다. 관계도 min·max·initial을 가집니다. min ≤ initial ≤ max여야 합니다. 값과 중간 산술 결과는 부호 있는 32비트 정수 범위 안이어야 하며 넘치면 오류입니다. 부동소수점·나눗셈·시스템 시각을 사용하지 않습니다.

초기 상태는 정의의 initial → project.initialState → 시나리오 initialOverrides 순서로 덮어씁니다. 세 객체의 attributes·flags·relationships는 필수이지만 각각 빈 객체일 수 있습니다. 알 수 없는 ID, 초기 범위를 벗어나는 값, 육성 대상 자신에게 선언한 관계는 의미 오류입니다.

일반 add·set 효과는 계산 후 대상 범위로 제한합니다. 요청값·적용 전후 값·실제 변화량·범위 제한 여부를 기록합니다. 범위 제한은 정수 오버플로를 복구하는 수단이 아닙니다.

## 조건과 표현식

조건의 op는 always·all·any·not·compare·flag·eventCompleted입니다. all·any는 한 개 이상의 조건을 요구합니다. compare의 comparator는 eq·ne·gt·gte·lt·lte입니다. flag는 불리언 일치, eventCompleted는 종료까지 마친 사건만 검사합니다.

숫자 표현식은 literal·read·time·add·subtract·multiply·clamp입니다. read는 attribute 또는 characterId와 relationshipId의 쌍을 대상으로 합니다. time은 periodIndex·year·month·periodInMonth만 읽습니다. clamp의 min ≤ max여야 합니다.

평가는 부작용이 없어야 합니다. 구조화된 진단에는 조건의 데이터 위치, 대상, 실제 값, 비교 값, 결과를 포함합니다. 정상 평가에서는 단락 평가를 사용할 수 있지만 설명 요청에서는 모든 하위 조건을 평가할 수 있어야 합니다. 중첩 깊이는 실행용 묶음 생성 시 32단계 이내로 제한합니다.

## 활동과 효과

한 활동은 한 기간을 사용합니다. requirements와 costs는 실행 직전에 검사합니다. costs.amount는 0 이상의 상수 정수이고 동일 attributeId를 중복 선언하지 않습니다. 자원과 컨디션만 비용 대상이 될 수 있습니다. 비용 차감 결과가 대상 min 미만이면 진행을 멈추고 일정을 수정하게 합니다.

거부된 활동은 시간·난수·수치를 바꾸지 않습니다. 앞선 사건 때문에 월초 예약 활동이 불가능해져도 자동 대체하지 않습니다.

실행 순서는 다음과 같습니다.

1. 활동 시작 상태에서 requirements·비용·성공 확률을 평가합니다.
2. successCheck가 있으면 chance를 0~10000으로 제한해 판정합니다. 0은 항상 실패, 10000은 항상 성공입니다.
3. 임시 상태에서 비용을 차감하고 baseEffects를 순서대로 적용합니다.
4. 성공 여부에 맞는 successEffects 또는 failureEffects를 순서대로 적용합니다.
5. 전체가 유효하면 상태와 난수 상태를 함께 확정합니다.

successCheck·successEffects·failureEffects는 모두 존재하거나 모두 생략합니다. 생략하면 baseEffects만 적용하고 난수를 소비하지 않습니다. 판정이 있으면 확률 0·10000을 포함해 판정용 난수를 한 번 요청합니다. 3단계 규약은 PCG32 XSH-RR·고정 stream 54·uint32 seed·상단 구간 거부 추출입니다. 원시 난수 소비량과 64비트 내부 상태를 명시적으로 보존합니다. [정확한 규약과 벡터](../core.md#난수와-재현성)를 따릅니다. 기존 콘텐츠 샘플은 확률을 사용하지 않으며 별도 코어 테스트에서 판정을 검사합니다.

효과 목록은 순서가 의미를 가지며 뒤 효과는 앞 효과가 적용된 임시 상태를 읽습니다. 중간 실패 시 전체 작업을 취소합니다. 효과는 add·set·setFlag만 지원하며 시간 이동이나 사건 호출은 하지 않습니다.

선택지에 소지금 차감 같은 효과를 넣을 때 작성자가 requirements로 비용 조건을 명시해야 합니다. 효과의 음수 증감은 활동 비용과 다르게 하한으로 제한되므로, 자동 결제 검사로 해석하면 안 됩니다.

## 사건과 엔딩

활동 결과 이후 미완료 사건 중 기간·조건을 만족한 것을 후보로 합니다. priority 내림차순, 같은 값이면 ID의 ASCII 오름차순으로 하나를 선택합니다. 선택되지 않은 사건은 다음 기간에 다시 평가하며 유효 기간이 끝나면 만료됩니다. 이번 기간에 사건이 끝났다고 다음 후보를 추가 실행하지 않습니다.

사건 완료 기록은 end에 도달한 순간 추가합니다. 이미 선택된 사건은 선택 효과로 최초 조건이 달라져도 중단하지 않습니다. 미발생 설명은 기간 미충족·완료됨·조건 불충족·우선순위 탈락을 구분합니다.

마지막 기간의 사건이 end에 도달하거나 사건이 없음을 확정한 뒤 엔딩을 선택합니다. defaultEndingId를 제외한 엔딩의 조건을 평가하고 같은 우선순위 규칙으로 선택합니다. 후보가 없으면 기본 엔딩을 사용합니다. 기본 엔딩의 조건은 always, priority는 0이어야 합니다. 선택된 엔딩은 대본 효과로 재판정하지 않습니다.

## 대본과 표현

nodes는 순서에 의존하지 않고 entryNodeId와 연결 ID로 실행합니다. scene·dialogue·choice·branch·effect·end 여섯 종류입니다. dialogue.speakerId가 null이면 내레이션입니다.

scene의 생략된 속성은 유지, backgroundAssetId·musicAssetId의 null은 해제입니다. portraits가 있으면 전체 교체하며 빈 배열은 모두 지웁니다. 동일 position을 중복 점유하지 않습니다. soundAssetId는 진입 시 한 번 재생하며 시뮬레이션 결과를 바꾸지 않습니다.

dialogue는 진행 입력을 기다립니다. choice는 requirements가 참인 선택지만 선택 가능하며 입력 시 다시 검사합니다. 모든 선택지가 비활성이면 콘텐츠 오류로 중단하고 설명합니다. branch는 condition에 따라 then 또는 else로 이동합니다. effect는 효과 전체 적용과 커서 이동을 함께 확정합니다.

모든 next·then·else·선택 next는 같은 대본에 존재해야 합니다. 대본은 순환 없는 유향 그래프이며 모든 진입 경로가 end에 도달해야 합니다. 도달 불가능한 노드는 경고 대상입니다. 실행 단계에서 명령 버전과 커서로 효과의 중복 적용을 막습니다.

## 에셋

manifest의 경로는 프로젝트 루트 기준이며 assets/ 아래의 ASCII 안전 경로로 제한합니다. 절대 경로·역슬래시·상위 이동·URL을 허용하지 않습니다. 로더는 심볼릭 링크 해석 후에도 루트 안인지 확인해야 합니다. 이미지·음향 종류, 파일 존재와 디코딩 가능 여부는 후속 검사입니다.

표정은 portraitId → image assetId 매핑입니다. scene과 activity.presentation에 표시한 모든 인물·표정·에셋 참조를 검사해야 합니다. 데이터 계약 샘플은 빈 manifest와 빈 portraits를 사용하므로 별도 아트가 필요하지 않습니다.

## 시나리오

시나리오는 4단계 실행기의 입력 형식입니다. commands는 코어 명령보다 높은 수준의 테스트 절차이며 runScenario가 확장해 실행합니다.

- planMonth: 월초에 정확히 세 활동을 예약합니다.
- runPeriod: 한 활동, 결과 확인, 자동 노드, 대사 진행, 지정한 선택을 처리하여 다음 기간의 경계까지 진행합니다. 마지막 기간에는 엔딩 대본까지 마칩니다.
- choices: eventId·nodeId·choiceId를 순서대로 지정합니다. 선택이 부족하거나 남거나 대상이 다르면 테스트 실패입니다. 자동으로 첫 선택지를 고르지 않습니다.
- 단계별 assertions: 해당 runPeriod 종료 상태에서 평가합니다. 최상위 assertions는 전체 명령 종료 뒤 평가합니다.
- stepLimit: 고수준 명령 개수가 아니라 확장된 코어 명령의 총 실행 한도입니다. 자동 노드도 advanceNarrative 명령 하나로 처리하므로 노드마다 1회를 셉니다. 같은 전환을 두 번 세지 않습니다.

현재 선택 주소는 사건만 대상으로 합니다. 시나리오 실행 도중 엔딩 choice에 도달하면 SCENARIO_ENDING_CHOICE로 실패합니다. core는 엔딩 choice도 지원하며 별도 테스트로 검증합니다. 시나리오의 엔딩 선택 주소는 이후 계약 확장 항목입니다.

## 구조 검사와 의미 검사의 경계

스키마가 보장하는 것은 필수 필드, 타입, 버전, ID 문법, 열거형, 정수 범위, 알 수 없는 필드 거부, 대본 종류별 모양, 성공 판정 필드의 동시 존재, 일정 슬롯 수입니다.

스키마가 보장하지 않는 것은 ID 유일성·참조 존재, min/max 상호 관계, 초기값 범위, 비용 대상 종류, 사건 기간의 상호 관계, 엔딩 기본값 규칙, 대본 순환·도달성, 실행 중 모든 선택 비활성, 에셋 실체, 시나리오 성공입니다.

의미 검증기는 ID·참조·범위·기간·기본 엔딩·대본 연결과 순환을 검사합니다. 전체 프로젝트 검사는 에셋 파일의 존재·종류 선언·경로 범위를 추가 확인합니다. 4단계 실행기는 실제 경로의 모든 선택 비활성 오류와 시나리오 기대값을 검사합니다. 이미지·음향 디코딩 및 모든 경로의 동적 도달성 증명은 구현 범위 밖입니다.

1단계의 샘플 전용 검사도 회귀 테스트로 유지합니다. 제품 의미 검증기의 진단은 파일 경로·JSON Pointer·1부터 시작하는 줄·열을 사용합니다. 열은 JavaScript 문자열의 UTF-16 코드 단위 기준입니다. 누락 필드는 가장 가까운 상위 객체의 위치를 사용합니다.

로더는 문서당 2 MiB와 JSON 중첩 128단계 제한을 두고, 조건·표현식은 의미 검사에서 32단계를 제한합니다. 구조 검사를 통과하지 못한 문서는 의미 검사의 정의 색인에서 제외합니다. 다른 문서는 계속 검사하며 참조 대상이 제외되었다면 누락 참조 진단이 함께 나올 수 있습니다.
