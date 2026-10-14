# Raising Engine

제품 목표는 프린세스 메이커 계열과 현대 육성 시뮬레이션의 게임 규칙·화면·콘텐츠를 GUI와 LLM으로 제작하는 작업실입니다. 현재 구현은 초기 MVP와 비주얼 편집기 첫 버전이며, 상용 육성게임의 모든 시스템을 쉽게 제작하는 수준은 아직 아닙니다. 이후 범위와 인수 기준은 [제품 완성 기준](docs/implementation-plan.md#제품-완성-기준-상용-육성게임-제작-작업실)을 따릅니다.

> 2026-10-11 후속 구현: [C++ 런타임과 도시·선물 제작 흐름](docs/native-authoring.md)을 추가했습니다. 자유 화면·행동 연결, 육성 작업실, Windows 네이티브 게임과 웹 내보내기를 지원합니다. 실제 플랫폼별 검증 범위는 [독립 QA 기록](docs/native-journey-qa.md)과 [컴파일러 검사](native/PORTABILITY-AUDIT.md)를 확인하세요.

인간 디자이너와 LLM이 같은 데이터로 만드는 일정 기반 2D 육성 시뮬레이션 엔진입니다. 첫 타깃은 월초에 상순·중순·하순 일정을 예약하고, 능력·관계·사건을 거쳐 엔딩에 도달하는 게임입니다.

초기 8단계인 Windows 설치판·12개월 확장 샘플·통합 검증에 이어 C++ 런타임과 자유 화면·행동 연결을 구현했습니다. Raising Studio와 로컬 MCP가 같은 데이터·편집·검증·저장 서비스를 사용합니다. 독립 Player, 편집기 미리보기, CLI·MCP의 실행 결과를 비교하며 GUI/LLM의 오래된 수정 요청은 프로젝트 버전 충돌로 거부합니다. 설치판의 로컬 검증을 제공하고, 별도 깨끗한 PC 검증·코드 서명은 남겨 둡니다.

## 문서

- [C++ 전환과 집·일정·도시·상점·선물·관계 사건 제작](docs/native-authoring.md)
- [콘텐츠 탐색·흐름·자동 플레이 작업실](docs/analysis-workspace.md)
- [화면·연출 편집기와 비주얼 미리보기 실행](docs/visual-editor.md)

- [전체 설계와 기술 결정](docs/architecture.md)
- [데이터 계약 0.1.0](docs/data-format/contract.md)
- [모듈 인터페이스](docs/interfaces.md)
- [단계별 구현 계획](docs/implementation-plan.md)
- [콘텐츠 작성 규칙](docs/authoring.md)
- [검증 결과](docs/verification.md)
- [CLI 사용법과 진단 코드](docs/cli.md)
- [화면 없는 코어 API와 재현 규약](docs/core.md)
- [사건·대본·세이브·시나리오 실행](docs/runtime.md)
- [플레이어 실행·조작·구조](docs/player.md)
- [편집기 사용법·저장 충돌·복구](docs/studio.md)
- [LLM·MCP 연결·도구 계약·충돌 처리](docs/mcp.md)
- [Windows 설치·MCP 런타임·배포 검증](docs/distribution.md)
- [12개월 확장 샘플과 독립 기대값](examples/academy-year/README.md)
- [최소 샘플과 수작업으로 산출한 기대 결과](examples/academy-demo/README.md)

## 빌드와 검증

개발 환경은 Node.js 22.12 이상입니다. Windows 설치판·포터블 앱은 별도 Node.js 설치 없이 실행합니다. Studio에 포함된 MCP는 번들 Node 22.23.3을 직접 실행합니다.

자유 화면·육성 작업실·C++ 전환을 포함한 최신 미리보기는 [Raising Studio 포터블](<release/native-journey-preview-ui-retention/Raising Studio-win32-x64/Raising Studio.exe>)입니다. 화면 편집 중 미리보기와 작업 공간을 오가도 선택·연출 대상·스냅 설정을 유지하는 수정이 포함됩니다. 준비된 테스트 프로젝트는 `projects/journey-playtest-2026-10-11`이며, **도시·선물 샘플 새 프로젝트**로 별도 프로젝트를 만들 수도 있습니다. 아래의 기존 설치 파일은 초기 8단계 배포판입니다.

설치 파일은 [Raising Studio Setup](release/installers/Raising-Studio-Setup-0.1.0-x64.exe)와 [Raising Player Setup](release/installers/Raising-Player-Setup-0.1.0-x64.exe)입니다. 둘 다 Windows x64·현재 사용자 설치이며 서명되지 않았습니다. [SHA256SUMS](release/installers/SHA256SUMS.txt)도 함께 제공합니다. 새 PC에서의 별도 인수 검증은 아직 수행하지 않았습니다.

포터블 `release/Raising Player-win32-x64/Raising Player.exe`를 실행하면 기본 샘플이 열립니다. 다른 PC로 옮길 때는 실행 파일 하나가 아니라 **폴더 전체**를 복사합니다.

편집기는 `release/Raising Studio-win32-x64/Raising Studio.exe`입니다. **새 프로젝트** 또는 **12개월 새 프로젝트**에서 빈 폴더를 선택하면 샘플을 복사합니다. Player에는 **12개월 샘플** 버튼도 있습니다. 기본 샘플은 편집기에서 읽기 전용으로 보호합니다.

```text
npm ci --ignore-scripts
node node_modules/electron/install.js
npm test
npm run player
npm run test:player
npm run package:player
npm run studio
npm run test:studio
npm run package:studio
npm run package:windows
npm run test:portable:year
npm run test:installed
npm run validate -- examples/academy-demo
npm run demo:core
npm run scenarios -- examples/academy-demo --roundtrip
npm run scenarios -- examples/academy-year --roundtrip
```

`npm test`는 타입 생성과 TypeScript·C++ 코어·addon 빌드를 먼저 수행합니다. 네이티브 빌드 도구와 웹 도구 설치 경로는 [C++ 런타임 문서](native/README.md)를 따릅니다. 테스트 없이 빌드하려면 `npm run build`를 사용합니다. 빌드 후 `npm run validate -- <프로젝트 폴더>`로 임의의 계약 0.1.0 프로젝트를 검사할 수 있습니다. JSON 출력은 `node dist/apps/tools/cli/main.js validate <프로젝트 폴더> --json`으로 얻습니다.

정적 검증은 파일 읽기 → 중복 키·문법 검사 → 로컬 JSON Schema 검사 → 의미 검사 → 에셋 파일 확인 순서입니다. 파일·JSON Pointer·줄·열·안정된 오류 코드를 반환합니다. 별도 scenarios 명령은 지정된 경로를 실제 실행하고 기대값을 검사합니다. 모든 경로의 도달 가능성을 증명하지는 않습니다.

두 샘플 경로는 `certified`·`ordinary` 엔딩에 도달하며 기대값 14개·9개가 각각 통과합니다. `--roundtrip`은 초기 상태와 매 명령 뒤 세이브 직렬화·복원 결과를 비교합니다. 실제 파일 저장·불러오기와 새 프로세스의 이어하기도 자동 테스트에서 검증합니다. 세이브 형식은 1.0.0, 실행 규칙 coreVersion은 0.2.0이며 콘텐츠 계약은 0.1.0을 유지합니다.

`demo:core`는 샘플의 두 달 일정을 `activities-only` 모드로 실행합니다. 총 6기간 후 검술 30·소지금 27·체력 60·스트레스 5, 상태 버전 20으로 종료합니다. 사건을 실행하지 않으므로 기존 시나리오의 사건·엔딩 포함 기대값과 다릅니다.

편집기·로더·검증기·CLI는 TypeScript입니다. 기본 실행 규칙·난수·명령 처리·플레이 조회·저장 기록 재실행은 C++이며, TypeScript 코어는 결과 비교를 위한 명시적 참조 구현입니다. Studio와 MCP는 Node-API 연결을 사용하고, 연결 조건이 맞지 않으면 동일한 C++ 실행 파일을 사용합니다. 웹은 같은 코어를 WebAssembly로 실행합니다. `.mjs` 파일은 개발 스크립트와 테스트입니다. 콘텐츠 타입은 JSON Schema로부터 생성하고 직접 수정하지 않습니다.

## 현재 폴더

```text
apps/tools/cli/           텍스트·JSON 검증과 시나리오 CLI
apps/player/              Electron main·preload, React 화면과 CSS
apps/studio/              Electron 편집기, 스키마 기반 폼·비교표·진단·참조
packages/schema/          JSON Schema와 생성된 TypeScript 타입
packages/project-service/ 파일·JSON 로더, 검사, 불변 스냅샷과 fingerprint
packages/validation/      구조·참조·범위·대본 검사와 진단
packages/core/            순수 상태 전이, 표현식·효과·난수, 사건·대본·엔딩
native/                   C++ 규칙·조회·저장 재실행과 Node-API 연결
native/player/            SDL3·RmlUi Windows 독립 게임 플레이어
packages/native-bridge/   C++ 프로세스·Node-API·WebAssembly 연결
packages/export-service/  에셋 검증·플랫폼 런타임·게임 번들 내보내기
packages/web-player/      WebAssembly 플레이 세션과 저장 파일 교환
apps/web/                 브라우저 게임 화면
packages/runtime-service/ 실행 세션·명령 기록, 세이브 스키마·검증·파일 I/O, 시나리오
packages/player-service/  화면용 실행 제어·조회, 자동 대본 진행, 세션 세대 검사
packages/studio-service/  편집 초안·revision·undo/redo·검증·저장·미리보기
packages/mcp-service/     공통 서비스의 MCP 어댑터·도구 계약·사건 이유 조회
apps/tools/mcp/           프로젝트 경로에 고정된 로컬 stdio 서버
examples/academy-demo/    2개월·6기간의 최소 게임 데이터
examples/academy-journey/ 편집기 GUI로 제작한 도시·상점·선물·관계 사건 샘플
tests/contracts/          스키마와 샘플 계약 검증
tests/validation/         오류 프로젝트·로더·CLI 통합 테스트
tests/core/               재현성·트랜잭션·시간·일정·스냅샷 테스트
tests/runtime/            대본·엔딩·시나리오·저장 후 재현·손상 세이브 테스트
tests/player/             실행 제어 회귀, 실제 Electron UI·미디어 통합 테스트
tests/studio/             외부 변경·다중 파일 복구, 실제 GUI 제작·미리보기 테스트
tests/mcp/                버전 충돌·실제 stdio 프로세스·GUI와 교대 수정 테스트
scripts/                  타입 생성, 스키마 복사, 화면 없는 실행 예제
docs/                     설계·규칙·인터페이스·계획
```

초기 비주얼 편집기의 회귀 176개와 Electron 통합 검증 기록은 [비주얼 편집기 문서](docs/visual-editor.md)에 있습니다. 최신 C++ 전환·자유 화면·육성 작업실의 검증은 [독립 QA 기록](docs/native-journey-qa.md)을 확인하세요. 상위 TheSword 프로젝트와 빌드·의존성을 공유하지 않습니다.
