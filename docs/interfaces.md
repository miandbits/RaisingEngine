# 모듈 인터페이스 계약

7단계까지 기존 서비스와 PlayerController·PlayerBridge, StudioEditor·StudioBridge·commitProject, McpWorkspace를 구현했습니다. schema를 공통 콘텐츠 계약으로 사용하고 프로젝트 변경과 실행 상태 변경을 분리합니다. 아래 applyProjectChanges에 대응하는 실제 진입점은 MCP의 apply_project_changes → McpWorkspace.apply → StudioEditor.edit/save입니다. IPC 계약은 [플레이어 문서](player.md)와 [편집기 문서](studio.md), MCP 인수·버전 규약은 [MCP 문서](mcp.md)를 따릅니다.

loadProject는 문서·위치 맵·파일 버전·읽기 진단을 반환합니다. validateProject는 읽기 결과를 받아 구조·의미 검사 결과와 구조 검사를 통과한 문서를 반환합니다. structuredDocuments의 타입은 생성된 스키마 타입이며 의미 검사 통과를 뜻하지는 않습니다. checkProject는 로드·검증·에셋 경로 확인을 묶어 CLI용 reportVersion 1.0.0 보고서를 반환합니다. buildSnapshot은 다시 검증하고 에셋 바이트를 해시한 후 오류 없는 결과만 불변 콘텐츠로 반환합니다.

## 서비스 입력과 출력

| 이름 | 입력 | 출력 |
| --- | --- | --- |
| loadProject | 프로젝트 경로 | 문서 묶음, 파일 버전, 진단 |
| validateProject | 문서 묶음 | 오류·경고 |
| buildSnapshot | 문서 묶음 | 불변 실행 콘텐츠, fingerprint 또는 오류 |
| applyProjectChanges (McpWorkspace.apply) | 변경 목록, 예상 프로젝트 바이트 버전, 이유, 초안 허용 여부 | 새 버전, 변경 파일·이유·진단 또는 충돌 |
| createRun | 콘텐츠, seed, 명시적 모드·상태 덮어쓰기 | 실행 상태 또는 오류 |
| dispatchCommand | 콘텐츠, 상태, 명령, 예상 stateRevision | 다음 상태, 기록, 입력 대기 또는 오류 |
| createSession / dispatchSession | 콘텐츠, seed·옵션 / 세션·명령·예상 버전 | 불변 실행 세션·명령 기록 / 전이 또는 오류 |
| inspectEvents | 콘텐츠, 상태 | 사건별 기간·완료·조건·우선순위 선택/탈락 이유 |
| runScenario | 콘텐츠, 시나리오 | 최종 상태, 기록, assertion 결과 |
| encodeSave / decodeSave | 콘텐츠·세션 / 콘텐츠·JSON 문자열 | 검증된 세이브 문자열 / 복원 세션 또는 오류 |
| saveRun / loadRun | 파일 경로·콘텐츠·세션 / 파일 경로·콘텐츠 | 파일 저장 결과 / 복원 세션 또는 오류 |
| buildGamePackage | 콘텐츠, 에셋, 플랫폼 | 플레이어 배포물과 결과 |

현재 스키마에 있는 시나리오 commands와 dispatchCommand의 코어 명령은 다릅니다. 시나리오 실행기는 planMonth·runPeriod를 코어 명령으로 확장합니다. UI도 같은 코어 명령만 사용합니다.

## 코어 명령과 상태

명령은 setSchedule·executeActivity·acknowledgeResult와 full 모드의 resolvePeriodEnd·advanceNarrative·choose입니다. activities-only 모드에는 completePeriodWithoutEvents를 유지합니다. 구체적인 입력과 phase는 [코어 API](core.md), 대본·세이브는 [런타임 API](runtime.md)에 정의합니다. 비용 부족은 ready 상태를 그대로 유지하고 오류를 반환하므로 남은 일정을 수정할 수 있습니다. 거부된 명령은 수치·시간·난수·revision을 바꾸지 않습니다. 처리한 기간을 소급 수정할 수 없습니다.

dispatchCommand의 네 번째 인수 expectedRevision은 필수입니다. 성공한 명령마다 stateRevision을 정확히 1 증가시키고 중복 클릭·오래된 요청은 버전 충돌로 반환합니다. 잘못된 명령의 예외가 부분 적용 상태를 남기면 안 됩니다.

현재 실행 상태에는 contentFingerprint, coreVersion, mode, stateRevision, periodIndex, phase, attributes, relationships, flags, completedEvents, schedule, rng, lastActivityResult, activeNarrative, endingId, presentation이 있습니다. 세이브의 구조는 packages/runtime-service/save.schema.json, 의미 무결성은 명령 기록 재현과 상태 비교로 검사합니다.

현재 phase는 planning·ready·activity-result·period-end·narrative·finished입니다. activeNarrative는 대본 종류·콘텐츠 ID·nodeId를 담고 getNarrativeView로 노드 종류와 활성 선택을 조회합니다. 자동 노드도 한 명령에 하나씩 처리하며 효과와 커서 이동을 함께 확정합니다. 복원 중 재현한 전이·사운드 큐는 새 입력으로 전달하지 않습니다.

## 진단과 처리 기록

| 결과 | 필수 정보 |
| --- | --- |
| Diagnostic | code, severity, file, JSON Pointer, relatedIds, message |
| ConditionEvaluation | 조건 위치, 대상, 실제 값, 비교 값, 충족 여부 |
| ChangeResult | 변경 파일, 이전·새 파일 버전, 변경 이유, 검증 진단 |
| TransitionRecord | 원인, 시점, 전후 값, 요청 효과, 실제 효과 |
| Conflict | 예상·실제 버전, 충돌 파일 |
| AwaitingInput | 입력 종류, 가능한 선택, 비활성 이유 |

코드는 언어와 독립적인 안정된 식별자이며 message는 인간에게 표시할 설명입니다. ID를 알 수 없는 경우 그 값을 자동으로 생성하거나 가까운 이름으로 대체하지 않습니다.

## 파일 변경과 실행 콘텐츠

초안 저장은 구조적으로 올바르면 허용하고 의미 오류를 함께 표시합니다. 실행 콘텐츠 생성은 실행을 막는 오류가 있으면 실패합니다. GUI와 LLM 도구가 같은 파일 버전 검사와 검증 함수를 사용합니다.

다중 파일 변경은 project-service/transaction.ts의 임시 파일과 .studio/pending-save.json 복구 기록으로 한 작업으로 취급합니다. 원본/목표 바이트가 확인되는 중단 작업은 다음 열기에서 원상 복구합니다. 알 수 없는 외부 변경이나 손상된 복구 기록은 덮어쓰지 않고 작업을 중단합니다. 운영체제의 단일 rename으로 여러 파일의 원자성을 보장하지 않습니다. 제약은 [편집기 저장 규약](studio.md#저장과-외부-변경)을 따릅니다.

fingerprint v1은 런타임 콘텐츠, 에셋 선언·바이트 해시, 계약·스냅샷 버전을 포함합니다. JSON 공백·객체 키 순서·소스 경로·파일 버전·문서의 $schema 힌트·시나리오·개인 UI 설정은 제외합니다. 객체 키를 정렬하고 배열 순서를 보존하는 정규 JSON의 UTF-8 SHA-256입니다. 반환 객체 자체도 같은 키 순서로 정규화합니다. [규약과 테스트 벡터](core.md#콘텐츠-스냅샷)를 고정했습니다.

현재 실행 세션은 생성 시 콘텐츠를 고정합니다. 파일을 고쳐도 실행 상태와 콘텐츠가 자동 혼합되지 않습니다. 재현은 같은 콘텐츠 식별값, 실행 규칙 버전, 초기 상태, seed, 명령 기록을 필요로 합니다.

## 배포 경계

Renderer는 프로젝트 경로 전체를 임의로 읽거나 명령을 실행하지 않습니다. Preload는 필요한 프로젝트·실행 작업만 노출합니다. Electron main은 파일·창·프로세스를 담당하며 게임 규칙은 코어에 위임합니다.

CLI와 MCP는 같은 서비스를 호출합니다. stdio MCP는 구조화된 결과를 반환하고 로그는 프로토콜 출력과 분리합니다. 모델 API와 채팅 기록은 MVP의 저장 대상이 아닙니다.
