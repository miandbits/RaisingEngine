# LLM·MCP 연결 — 7단계

> 2026-10-11 후속 구현: [콘텐츠 탐색·흐름·자동 플레이 작업실](analysis-workspace.md)을 추가했습니다. 아래 단계별 기록과 구분해 최신 기능·검증 결과를 확인하세요.

LLM 호스트가 로컬 stdio MCP 서버를 실행해 Raising Studio와 같은 JSON 문서를 읽고 수정합니다. `McpWorkspace → StudioEditor → loadProject / validateProject / buildSnapshot / commitProject` 경로를 사용하며 게임 규칙은 기존 코어·시나리오 실행기에 위임합니다. 모델 API, API 키, 채팅 화면은 엔진에 포함하지 않습니다.

## 실행과 호스트 등록

8단계 Windows 설치판에는 Node 22.23.3과 번들 MCP가 포함됩니다. 별도 Node 설치 없이 연결하는 경로는 [설치판 MCP 설정](distribution.md#설치-후-사용)을 따릅니다. 아래는 소스 저장소에서 실행하는 개발 방식입니다.

Node.js 22.12 이상에서 프로젝트 루트의 `npm ci`, `npm run build`를 먼저 실행합니다. 실행 예:

```powershell
node dist/apps/tools/mcp/main.js --project "D:\Games\MyRaisingGame"
```

폴더는 `project.json`이 있는 **게임 프로젝트**입니다. Studio에서 새 프로젝트로 만든 폴더를 지정하세요. 기본 샘플을 조사만 하려면 다음처럼 실행합니다.

```powershell
node dist/apps/tools/mcp/main.js --project examples/academy-demo --read-only
```

MCP 호스트의 stdio 서버 설정 예입니다. 호스트별 설정 파일 위치와 바깥쪽 형식은 해당 호스트를 따릅니다. 기존 전역 설정은 자동으로 변경하지 않습니다.

```json
{
  "mcpServers": {
    "raising-engine": {
      "command": "node",
      "args": [
        "D:/Projects/TheSword/RaisingEngine/dist/apps/tools/mcp/main.js",
        "--project",
        "D:/Games/MyRaisingGame"
      ]
    }
  }
}
```

호스트에서 `node`를 찾지 못하면 설치된 Node 실행 파일의 절대 경로를 사용합니다. 상대 경로에 의존하지 않도록 서버·게임 경로를 절대 경로로 지정하세요. **호스트는 위 Node 진입점을 직접 실행**해야 합니다. 빌드 명령이나 일반 `npm run`의 안내문이 stdout에 섞이면 프로토콜을 깨뜨립니다. 서버 로그·시작 오류는 stderr, 프로토콜만 stdout을 사용합니다. 연결 후 입력을 기다리는 것은 정상입니다.

공식 SDK `@modelcontextprotocol/server`·`client` 2.3.1과 Zod 4.6.5를 lockfile에 고정했습니다. `serveStdio`로 2025 계열 초기화와 2026-07-28 연결을 지원하며 둘 다 실제 SDK 클라이언트로 검사했습니다. API 근거: [공식 stdio 시작 예제](https://ts.sdk.modelcontextprotocol.io/v2/), [프로토콜 버전 협상](https://ts.sdk.modelcontextprotocol.io/v2/protocol-versions).

## 도구 계약

모든 반환값은 JSON 텍스트와 `structuredContent`를 함께 제공합니다. 업무 실패는 `ok: false`, MCP `isError: true`입니다. SDK가 거부한 잘못된 도구 인수는 SDK 오류 형식을 따릅니다.

| 도구 | 입력 | 결과 |
| --- | --- | --- |
| `read_project` | 없음 | 프로젝트 version, 문서 파일·종류·ID·파일 버전, 진단, 읽기 전용 여부 |
| `read_document` | `file`, `expectedVersion` | 해당 버전의 문서 전체 JSON과 파일 버전 |
| `get_schema` | `name` | 편집기와 같은 원본 JSON Schema. common/project/attributes/flags/character/activity/event/ending/assets/scenario |
| `validate_project` | `expectedVersion` | 구조·의미·에셋 검증, file/pointer/line/column 진단, 성공 시 fingerprint |
| `apply_project_changes` | `expectedVersion`, `edits`, `reason`, 선택 `allowInvalidDraft` | 이전/새 프로젝트 버전, 바뀐 파일의 이전/새 SHA-256, 이유, 진단 |
| `run_scenario` | `expectedVersion`, `scenarioId` | seed가 고정된 기존 시나리오의 assertion·최종 상태·명령 수 |
| `explain_events` | `expectedVersion`, `scenarioId`, `afterCommands` | 해당 코어 명령 수까지 재현한 시점의 사건 선택/탈락 이유와 조건 실제값·비교값 |

`apply_project_changes.edits`는 1~100개, `reason`은 1~1000자입니다. 편집 명령은 GUI와 같습니다.

- `{"type":"replace","file":"activities/study.json","data":{...}}`: 문서 전체 교체. 새 버전을 조회한 후 변경할 필드만 수정하고 나머지 필드를 보존하세요.
- `{"type":"add","kind":"activity","id":"practice"}`: GUI와 같은 기본 템플릿 생성. kind는 activity/event/ending/character. 같은 묶음에 replace를 이어 넣어 내용을 완성할 수 있습니다.
- `{"type":"delete","file":"activities/practice.json"}`: GUI에서 삭제할 수 있는 콘텐츠 문서 삭제. 필수 루트 문서는 삭제할 수 없습니다.

전체 묶음을 별도 초안에 적용한 다음 검증·저장합니다. 중간 명령 실패나 구조 오류는 디스크에 일부만 쓰지 않습니다. 기본값은 의미 오류도 거부합니다. `allowInvalidDraft: true`를 명시하면 GUI 초안 저장과 동일하게 **의미 오류가 있는 구조적으로 올바른 문서**를 저장할 수 있습니다. 실행·미리보기 검증은 여전히 실패합니다. 오류 진단의 줄·열은 GUI처럼 2칸 들여쓰기 초안 기준이며 JSON Pointer가 안정된 위치 식별자입니다.

`reason`은 응답의 변경 요약에 돌려주며 별도의 영구 감사 이력으로 저장하지 않습니다. 도구 결과·사용자 승인 기록 보관은 호스트 책임입니다. 에셋 가져오기, 새 시나리오 파일 생성, JSON Patch·필드 자동 병합은 현재 도구 범위 밖입니다. 기존 시나리오는 replace로 편집할 수 있습니다.

## 안전한 편집 흐름과 충돌

1. `read_project`에서 version과 문서 목록을 받습니다.
2. 같은 version으로 필요한 `read_document`와 `get_schema`를 조회합니다.
3. 기존 필드를 보존한 변경 묶음을 `apply_project_changes`에 보냅니다.
4. 반환된 새 version으로 `validate_project`와 두 샘플의 `run_scenario`를 실행합니다.
5. `VERSION_CONFLICT`이면 **기존 변경을 새 토큰에 그대로 실어 재시도하지 않습니다.** 새 문서를 읽고 인간/다른 LLM의 변경을 검토한 후 수정 내용을 다시 구성합니다.

프로젝트 버전은 `project-v1:<SHA-256>`입니다. 실제 프로젝트 경로, 모든 로드된 콘텐츠/시나리오 문서의 상대 경로와 **원본 바이트 SHA-256**으로 만듭니다. 공백 변경, 파일 추가·삭제, 다른 문서 수정도 토큰을 무효화합니다. 프로세스를 재시작해도 같은 내용은 같은 토큰입니다. 카운터가 아니므로 내용을 완전히 원복하면 이전 토큰과 같아질 수 있습니다. 이는 동일 바이트에 대한 수정이므로 허용합니다. 실행용 콘텐츠 fingerprint와는 별개이며, 에셋 바이너리 바이트·`.studio` 기록은 편집 버전 대상이 아닙니다. 에셋은 실행 스냅샷 생성 시 따로 해시합니다.

GUI는 자신의 초안 revision과 디스크 기준 버전을 따로 관리합니다. MCP는 각 요청마다 새 StudioEditor 초안을 만들고 전체 디스크 버전을 검사합니다. 두 호스트 모두 `.studio/pending-save.json`의 배타적 생성 잠금을 사용하며, **잠금 획득 후 프로젝트 전체 읽기 집합을 다시 검사**합니다. 따라서 서로 다른 문서를 수정한 동시 요청도 오래된 프로젝트를 기반으로 모두 성공할 수 없습니다. 동일 MCP 연결의 요청은 순서대로 처리합니다.

| 코드 | 처리 |
| --- | --- |
| `VERSION_CONFLICT` | expectedVersion/actualVersion와 파일 버전 또는 충돌 파일을 검토하고 다시 조회 |
| `PROJECT_BUSY` | 다른 저장이 끝난 뒤 조회부터 다시 시작. 수정 성공을 가정하지 않음 |
| `VALIDATION_FAILED` | 위치별 진단을 수정. 초안 저장 opt-in은 의미 오류만 허용 |
| `CHANGE_REJECTED` | 명령·경로·구조 오류. 요청 전체가 반영되지 않음 |
| `READ_ONLY` | 읽기 전용 서버에는 수정 요청 불가 |
| `DOCUMENT_NOT_FOUND` / `SCENARIO_NOT_FOUND` | 조회한 목록의 정확한 파일·ID 사용 |
| `PROJECT_READ_ERROR` / `PROJECT_ERROR` | 읽기·파일 시스템 문제를 해결한 뒤 재조회 |

충돌 시 MCP 변경 묶음은 저장하지 않고, GUI가 가진 미저장 초안도 지우지 않습니다. GUI의 다시 불러오기는 기존 확인 절차를 따릅니다. 자동 강제 덮어쓰기·자동 병합은 없습니다.

저장은 기존 중단 복구 기록과 원본/목표 바이트 확인을 사용합니다. 쓰기 서버 시작 시 복구를 시도하고, 읽기 전용 서버는 복구하지 않습니다. 진행 중인 저장/복구 기록이 있으면 도구 읽기도 거부합니다. 알 수 없는 외부 바이트는 덮어쓰지 않습니다. 여러 파일을 하나의 OS rename으로 바꾸는 것은 아니며, 이 잠금을 따르지 않는 임의 외부 프로그램의 동시 쓰기까지 직렬화하지는 않습니다. 자세한 제약은 [편집기 저장 규약](studio.md#저장과-외부-변경)을 따릅니다.

## 사건 이유 조회

`afterCommands`는 시나리오의 planMonth/runPeriod 개수가 아니라, 실행기가 확장한 **수락된 코어 명령 개수**입니다. 0은 초기 상태입니다. `explain_events`는 실제 시나리오를 실행하고 그 명령 기록의 앞부분을 같은 seed·초기 상태로 재현합니다. 기간·완료 여부·조건·우선순위 판정에는 기존 `inspectEvents`와 코어 표현식 평가기를 사용합니다. 비교 조건은 실제 값·기준 값, 플래그 조건은 실제/기대 불리언을 제공합니다.

`decisionAppliesNow: true`는 phase가 period-end일 때만 반환합니다. 다른 phase의 결과는 조건 조사용이며 지금 사건이 발동했다는 뜻이 아닙니다. 이미 실행된 사건은 completed로 표시됩니다. 단락 평가 때문에 실행되지 않은 조건의 추가 설명에서 계산 오류가 생기면 그 하위 설명에 오류를 표시합니다. 조회는 게임 상태·세이브 파일을 변경하지 않습니다.

## 검증과 파일

```powershell
npm test
npm run test:mcp
npm run test:mcp:studio
```

서비스/stdio 15개와 실제 Electron↔stdio 통합 1개를 추가했습니다. 전체 headless 회귀는 154개이며 기존 Studio GUI 테스트도 통과했습니다. 샘플 원본은 수정하지 않고 `.cache/mcp-tests`에 복사해 검사합니다. GUI 충돌 캡처는 `.cache/mcp-tests/gui-mcp-conflict.png`, 로그는 `.cache/stage7-*.log`입니다.

- `packages/mcp-service/workspace.ts`: 공통 편집 서비스 어댑터·시나리오·이유 조회
- `packages/mcp-service/server.ts`: 도구 입력 계약·응답·요청 직렬화
- `apps/tools/mcp/main.ts`: 프로젝트에 고정된 stdio 실행 진입점
- `packages/project-service/version.ts`: 파일 바이트 기반 프로젝트 버전·충돌
- `packages/studio-service/editor.ts`, `packages/project-service/transaction.ts`: GUI/MCP 공통 저장 전 전체 버전 재검사

포트·원격 HTTP·임의 셸 도구는 제공하지 않습니다. 한 서버는 시작 인수의 프로젝트만 다룹니다. 호스트별 설치 UI와 모델별 자동 승인 정책은 검증 대상이 아니며, 실제 프로토콜 연결·도구 호출은 공식 SDK 클라이언트로 검증했습니다.
