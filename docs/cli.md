# 프로젝트 검증 CLI

검증 CLI는 JSON 파일을 읽고 구조·의미 오류를 보고합니다. 프로젝트 파일을 수정하거나 게임을 실행하지 않습니다. 편집기·플레이어 없이 사용할 수 있습니다.

## 실행

엔진 루트에서 한 번 빌드합니다. 소스를 수정한 후에는 다시 빌드합니다.

```text
npm ci --ignore-scripts
npm run build
npm run validate -- examples/academy-demo
```

다른 프로젝트에는 폴더의 절대 경로 또는 현재 작업 폴더 기준 상대 경로를 지정합니다. 경로에 공백이 있으면 따옴표로 감쌉니다. npm의 안내 문구 없이 기계가 읽을 JSON을 얻으려면 다음처럼 직접 실행합니다.

```text
node dist/apps/tools/cli/main.js validate examples/academy-demo --json
node dist/apps/tools/cli/main.js --help
```

정상 샘플 결과는 `PASS: 15개 문서, 오류 0개, 경고 0개`입니다. 오류가 있으면 `파일:줄:열 severity CODE /json/pointer` 다음 줄에 원인을 표시합니다. 중복 ID에는 첫 선언 위치도 붙입니다.

## JSON 보고서와 종료 코드

보고서는 reportVersion, projectPath, valid, summary, diagnostics를 포함합니다. summary는 파싱된 문서 수·구조 통과 문서 수·오류 수·경고 수를 구분합니다. 진단은 code, severity, file, pointer, line, column, relatedIds, message를 포함하며 관련 선언이 있으면 relatedLocations도 반환합니다.

파일 경로는 프로젝트 루트 기준이며 슬래시를 사용합니다. JSON Pointer는 RFC 6901 방식으로 `~`와 `/`를 이스케이프합니다. 루트는 빈 문자열입니다. 줄·열은 1부터 시작하며 열은 UTF-16 코드 단위 기준입니다. 파일 읽기 오류는 루트 위치 1:1, 누락 필드는 상위 객체 위치를 사용합니다.

| 종료 코드 | 의미 |
| --- | --- |
| 0 | 오류 없음. 경고가 있을 수 있음 |
| 1 | 프로젝트 읽기·구조·의미 검증 오류 |
| 2 | 명령 사용법 오류 또는 검증 도구 내부 오류 |

검증 보고서는 stdout, 사용법·텍스트 내부 오류는 stderr로 출력합니다. `--json` 내부 오류는 internalError를 가진 JSON과 종료 코드 2를 반환합니다. 사용법 오류에는 JSON 보고서 대신 stderr 사용법을 반환합니다.

## 검사 범위

| 분류 | 진단 코드와 검사 |
| --- | --- |
| 프로젝트 읽기 | PROJECT_DIRECTORY, DOCUMENT_MISSING, DOCUMENT_READ, DIRECTORY_READ, DOCUMENT_NOT_FILE |
| 읽기 제한 | PATH_OUTSIDE_PROJECT, DOCUMENT_SIZE_LIMIT, PROJECT_SIZE_LIMIT, JSON_DEPTH_LIMIT, JSON_PARSE_LIMIT |
| JSON | JSON_SYNTAX, JSON_DUPLICATE_KEY, JSON_ENCODING |
| 형식 | FORMAT_VERSION, SCHEMA_INVALID, FILENAME_ID_MISMATCH |
| ID와 참조 | ID_DUPLICATE, REFERENCE_MISSING, REFERENCE_AMBIGUOUS, REFERENCE_KIND |
| 수치 | RANGE_ORDER, INITIAL_OUT_OF_RANGE, COST_DUPLICATE, COST_KIND, EXPRESSION_DEPTH |
| 인물과 표현 | PROTAGONIST_RELATIONSHIP, PORTRAIT_POSITION_DUPLICATE |
| 기간과 엔딩 | EVENT_WINDOW_ORDER, EVENT_WINDOW_BOUNDS, DEFAULT_ENDING_CONDITION, DEFAULT_ENDING_PRIORITY |
| 대본 | NARRATIVE_CYCLE, NARRATIVE_NO_END, NARRATIVE_UNREACHABLE |
| 에셋 파일 | ASSET_MISSING, ASSET_READ, ASSET_NOT_FILE, ASSET_OUTSIDE_PROJECT |

NARRATIVE_UNREACHABLE은 경고이며 나머지는 오류입니다. 순환 검사는 진입점에서 닿지 않는 대본 부분도 포함합니다. 분기 조건의 참·거짓과 관계없이 모든 연결을 검사합니다.

참조 검사는 능력치·플래그·인물·관계·화자·사건 완료 조건·기본 엔딩·일정 활동·시나리오 선택·이미지와 음향까지 포함합니다. 시나리오의 선택 주소가 해당 사건 안의 실제 선택 노드와 선택지를 가리키는지 확인합니다.

구조 오류가 있는 문서는 의미 색인에서 제외하며 나머지 문서를 계속 검사합니다. 따라서 한 파일의 구조 오류와 그 정의를 참조하는 다른 파일의 누락 참조 오류가 함께 나올 수 있습니다. 원인 파일의 구조 오류를 먼저 수정하세요.

## 아직 검사하지 않는 것

실제 활동 실행, 사건 조건 충족 가능성, 엔딩 도달 여부, 플레이 중 모든 선택 비활성 여부, 세이브 재현은 시뮬레이션 코어가 필요합니다. 에셋은 경로·존재·일반 파일 여부와 선언된 image/audio 종류까지만 검사하고 실제 디코딩은 하지 않습니다.

현재 파일 로더는 문서당 2 MiB, JSON 중첩 128단계의 한도가 있습니다. 외부 파일 시스템 링크가 프로젝트 밖으로 나가면 거부합니다. 운영체제가 realpath 접근을 EPERM 등으로 거부하면 해당 원인을 보고하고 검증을 실패 처리합니다.
# 시나리오 CLI (4단계 추가)

빌드 후 `npm run scenarios -- examples/academy-demo --roundtrip`으로 두 경로를 실행합니다. 특정 경로는 프로젝트 뒤에 ID를 추가합니다. `--roundtrip`은 초기화와 매 명령 뒤 세이브 직렬화·복원을 비교합니다. 파일을 수정하지 않습니다.

기계가 읽을 JSON은 `node dist/apps/tools/cli/scenarios.js examples/academy-demo --json --roundtrip`으로 출력합니다. 종료 코드는 성공 0, 콘텐츠·실행 실패 1, 사용법 오류 2입니다. 실제 파일 저장·불러오기 API와 오류 코드는 [런타임 문서](runtime.md)에 있습니다. 아래의 validate CLI는 기존 정적 검사 전용입니다.

