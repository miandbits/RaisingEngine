# 5단계 최소 플레이어

Electron + React + TypeScript 앱에서 일정 기반 게임을 엔딩까지 실행합니다. 기본 프로젝트는 `examples/academy-demo`입니다. 콘텐츠 0.1.0, 코어 0.2.0, 세이브 1.0.0 계약을 유지합니다.

## 실행

Windows 포터블: `release/Raising Player-win32-x64/Raising Player.exe`. 런타임·스키마·기본 샘플이 들어 있어 개발 서버와 Node.js 없이 실행합니다. 이동 시 해당 폴더 전체가 필요합니다.

개발 환경: Node.js 22.12 이상. 프로젝트 루트에서 실행합니다.

```text
npm ci --ignore-scripts
node node_modules/electron/install.js
npm run player
```

의존성 설치의 스크립트를 생략한 경우 위 Electron 설치 명령이 필요합니다. Electron 바이너리는 설치 시 다운로드하며, 게임 실행은 오프라인입니다. `player`는 타입 검사·번들·리소스 복사 후 Electron을 실행합니다. 이미 빌드했다면 `npx --no-install electron .`로 시작합니다.

```text
npm test
npm run test:player
npm run package:player
```

`npm test`는 6단계 기준 회귀 139개, `test:player`는 컨트롤러 5개와 실제 Electron 통합 2개를 실행합니다. UI 테스트에는 그래픽 데스크톱 세션이 필요합니다. Chromium 별도 설치 없이 Electron 자체를 Playwright로 조작합니다. `package:player`는 Windows x64 포터블 폴더를 재생성합니다. 설치 프로그램·코드 서명은 8단계입니다.

패키지에 같은 UI 테스트를 적용하는 PowerShell 명령:

```powershell
$env:PLAYER_EXECUTABLE = 'release/Raising Player-win32-x64/Raising Player.exe'
node --test tests/player/electron.test.mjs
Remove-Item Env:PLAYER_EXECUTABLE
```

## 플레이 흐름

1. **새 게임 시작**을 누르고 상순·중순·하순 활동을 고른 뒤 **일정 확정**을 누릅니다.
2. **활동 시작**으로 한 기간을 실행합니다. 결과 화면에서 비용·효과·성공 판정을 확인하고 **다음으로** 진행합니다.
3. 사건이 발생하면 **계속** 또는 선택지로 진행합니다. 조건이 부족한 선택은 비활성으로 표시합니다.
4. 이미 지난 일정은 고정하고 아직 실행하지 않은 일정은 다시 고를 수 있습니다. 비용 부족 시 상태를 보존하므로 일정을 바꿔 다시 시작합니다.
5. 마지막 기간의 사건과 엔딩 대본을 마치면 결말 화면에 도착합니다.

**저장하기**는 파일 위치를 선택해 현재 실행 세션을 저장합니다. **불러오기**는 해당 프로젝트와 호환되는 저장 파일을 복원합니다. 파일 대화상자 취소·검증 오류는 현재 게임을 바꾸지 않습니다. 다른 프로젝트를 플레이하려면 **프로젝트 열기**로 `project.json`이 있는 폴더를 선택합니다. 새 게임·불러오기·프로젝트 교체·창 닫기는 게임 진행 중일 때 확인을 요청합니다. 자동 저장은 없습니다.

새 게임의 기본 seed는 42입니다. 현재 플레이어에는 seed·초기 상태 편집 UI가 없고 시나리오/CLI에서 설정합니다. 실행 중 원본 파일의 변경은 새 프로젝트를 열 때 반영합니다.

샘플 두 경로:

| 경로 | 1월 | 시험 초대 | 2월 | 엔딩 | 검술 / 소지금 / 체력 / 스트레스 |
| --- | --- | --- | --- | --- | --- |
| 기사 | 수업·아르바이트·휴식 | 참가 | 수업·휴식·수업 | 정식 견습 기사 | 35 / 22 / 60 / 5 |
| 일상 | 수업·아르바이트·휴식 | 불참 | 아르바이트·휴식·아르바이트 | 새로운 출발 | 10 / 61 / 60 / 8 |

## 구현 경계

| 파일/모듈 | 책임 |
| --- | --- |
| packages/player-service/controller.ts | 세션 소유, 명령 변환, 자동 노드 진행, 조회, 저장 서비스 호출 |
| legacy/player/main/index.ts | BrowserWindow, IPC 요청 직렬화·발신자 확인, 파일 대화상자, 로컬 에셋 |
| legacy/player/preload/index.ts | 고정된 여섯 브리지 메서드만 노출 |
| apps/player/renderer/ | React 화면·일정 초안·미디어 재생; 게임 규칙 계산 없음 |
| legacy/electron.vite.config.ts | main ESM·preload 단일 CJS·renderer 번들 |
| scripts/copy-player-resources.mjs | 번들 기준 상대 위치로 JSON Schema 복사 |
| scripts/package-player.mjs | 코드·스키마·샘플을 ASAR로 묶는 Windows 포터블 패키징 |

브리지는 `view`, `newRun`, `act`, `save`, `load`, `openProject`입니다. renderer는 파일 경로·임의 IPC 채널을 넘길 수 없습니다. 실행 요청은 `{ generation, revision }`을 포함합니다. `revision`은 기존 코어 상태 버전이고 `generation`은 프로젝트 교체·새 게임·복원마다 증가합니다. 이전 게임과 우연히 revision이 같은 요청도 거부합니다. main의 단일 큐와 UI의 동기 잠금으로 파일 대화상자 중의 실행 및 중복 클릭을 직렬화합니다.

`act`의 화면 명령은 `schedule`, `execute`, `continue`, `choose`입니다. 규칙은 `dispatchSession`을 통해 기존 코어에서 처리합니다. 기간 종료와 scene/branch/effect/end는 자동으로 진행하고 planning/ready/activity-result/dialogue/choice/finished에서 멈춥니다. 자동 진행 상한은 10,000명령입니다. 실패한 코어 명령은 롤백하되 앞서 성공한 자동 명령은 유지하고 최신 상태와 오류를 반환합니다. 오류 화면에서 저장·재시도할 수 있습니다.

응답 `PlayerReply`는 `{ view, error, notice, soundCues }`입니다. `PlayerView`에는 표시용 정의, 전체 읽기 전용 실행 상태, 달력, 현재 대본·선택 활성 상태, 엔딩 이름이 있습니다. React가 상태를 수정해도 main 세션에는 영향을 주지 않습니다.

플레이어는 full 모드 세이브를 받습니다. 헤드리스 명령 사이의 자동 노드 지점에서 저장된 파일은 복원 검증 후 다음 표시 지점까지 진행합니다. 이 정규화가 실패하면 불러오기 전 세션을 유지합니다. 이미 소비한 사운드 큐는 복원 시 다시 보내지 않습니다.

## 표현과 로컬 파일 경계

배경, 좌·중·우 인물, 배경음과 효과음을 기존 presentation 상태에 연결합니다. 샘플에는 아트·음원이 없으므로 기본 화면은 텍스트와 CSS로 구성됩니다. 소리는 기본 음소거이고 메뉴에서 켤 수 있습니다. PNG/JPEG/WebP/GIF, WAV/MP3/OGG를 지원하며 실제 코덱 지원은 Electron에 따릅니다.

renderer는 `raising-asset://project/<assetId>?g=<generation>`을 사용합니다. main이 매니페스트 ID, 현재 세대, realpath의 프로젝트 내부 여부, 확장자/종류, 스냅샷의 SHA-256을 확인한 뒤 바이트를 반환합니다. 실행 중 파일을 바꾸면 바뀐 에셋을 기존 게임에 섞지 않고 거부합니다. 파일을 다시 열면 새 스냅샷을 만듭니다.

BrowserWindow는 `sandbox: true`, `contextIsolation: true`, `nodeIntegration: false`입니다. preload는 Electron API를 제한된 브리지로 감쌉니다. CSP·외부 요청 차단·새 창과 탐색 차단·권한 거부를 적용합니다. 콘텐츠 대사는 React 텍스트로 렌더링하며 HTML을 실행하지 않습니다. IPC는 해당 창의 mainFrame과 정확한 renderer URL을 확인합니다. 구현 근거는 [Electron 보안 지침](https://www.electronjs.org/docs/latest/tutorial/security), [sandbox](https://www.electronjs.org/docs/latest/tutorial/sandbox), [electron-vite 의존성 처리](https://electron-vite.org/guide/dependency-handling)입니다.

## 현재 범위

최소 창 크기는 900×680이며 좁거나 내용이 긴 화면은 세로 스크롤합니다. 버튼·선택에는 키보드 포커스와 레이블을 제공합니다. 자동 저장·슬롯 관리·텍스트 속도·백로그·사용자 seed 설정·키 설정·볼륨 세분화는 후속 작업입니다. 파일 검증과 시뮬레이션은 현재 main에서 실행하므로 큰 콘텐츠의 작업 분리는 이후 측정에 따라 진행합니다.

Windows x64 현재 개발 PC에서 빌드와 포터블 실행을 검증했습니다. 다른 OS, 개발 도구가 전혀 없는 새 PC, 접근성 전체 감사, 장시간 실행, 설치·업데이트·서명은 아직 검증하지 않았습니다. 인간 편집기는 다음 6단계입니다.
