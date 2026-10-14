# Windows 배포와 확대 검증 — 8단계

Windows x64용 **Raising Studio 설치 파일**과 **Raising Player 설치 파일**을 제공합니다. Studio 설치판에는 로컬 MCP 서버와 Node 런타임을 함께 넣어 사용자의 Node.js·npm·개발 저장소가 필요하지 않습니다. Player는 Electron에 포함된 런타임으로 실행합니다.

## 설치 결과물

| 파일 | 용도 |
| --- | --- |
| `release/installers/Raising-Studio-Setup-0.1.0-x64.exe` | 인간용 편집기, 2·12개월 프로젝트 템플릿, 독립 MCP 런타임 |
| `release/installers/Raising-Player-Setup-0.1.0-x64.exe` | 독립 플레이어, 2·12개월 내장 샘플 |
| `release/installers/SHA256SUMS.txt` | 설치 파일의 SHA-256 |
| `release/installers/manifest.json` | 버전·플랫폼·파일 크기·해시·서명 여부 |

두 설치 파일은 NSIS의 현재 사용자 설치 방식이며 관리자 권한 상승을 요청하지 않도록 구성했습니다. 설치 폴더 선택, 시작 메뉴 바로가기, Windows 제거 프로그램 등록을 지원합니다. 설치 직후 앱을 자동 실행하지 않습니다. 기본 Electron 아이콘을 사용합니다.

기존 포터블 폴더 `release/Raising Studio-win32-x64`, `release/Raising Player-win32-x64`도 유지합니다. 포터블은 폴더 전체를 복사해야 합니다. Studio의 `resources/mcp`까지 포함하세요.

**이번 결과물은 코드 서명되지 않았습니다.** 실제 인증서 서명·공개 배포·자동 업데이트는 수행하지 않았습니다. SHA-256은 다운로드/복사 무결성을 확인하기 위한 값이며 발행자 서명을 대신하지 않습니다. 서명 상태는 Windows `Get-AuthenticodeSignature`의 `NotSigned`로 확인했습니다.

## 설치 후 사용

1. Studio를 실행합니다. 기존 **새 프로젝트**는 2개월 샘플을 복사합니다. **12개월 새 프로젝트**는 `견습생의 사계절`을 선택한 빈 폴더에 복사합니다.
2. 설치 폴더 밖의 별도 작업 폴더에 프로젝트를 두고 편집·저장하세요. 설치/제거와 작업 데이터를 분리합니다.
3. Player의 **12개월 샘플**로 내장 게임을 열거나 **프로젝트 열기**로 편집한 게임 폴더를 선택합니다.
4. MCP 호스트에는 아래의 설치 경로를 등록합니다. 호스트 전역 설정은 설치 프로그램이 변경하지 않습니다.

```json
{
  "mcpServers": {
    "raising-engine": {
      "command": "C:/선택한설치폴더/Raising Studio/resources/mcp/node.exe",
      "args": [
        "C:/선택한설치폴더/Raising Studio/resources/mcp/main/index.js",
        "--project",
        "D:/Games/MyRaisingGame"
      ]
    }
  }
}
```

`command`와 첫 번째 인수는 실제 Studio 설치 폴더로 바꿉니다. Node를 PATH에서 찾지 않고 설치된 실행 파일을 직접 사용합니다. 서버 로그는 stderr, MCP 프로토콜은 stdout에만 출력합니다. `--read-only`를 마지막 인수로 추가하면 수정을 거부합니다. 나머지 도구 계약·버전 충돌 처리법은 [MCP 문서](mcp.md)를 따릅니다.

일반 제거는 앱 파일과 제거 등록을 정리하며, 별도 폴더의 게임 프로젝트·세이브를 제거 대상으로 지정하지 않습니다. 앱의 사용자 데이터 자동 삭제도 비활성화했습니다. 이번 설치 테스트는 같은 컴퓨터의 새 설치 디렉터리를 사용하며, 기존 Raising 제품 설치가 감지되면 덮어쓰지 않고 중단합니다.

## 빌드 구조와 재현

개발 빌드에는 Node.js 22.12 이상과 의존성 설치가 필요합니다. 빌드 시 Electron·NSIS·공식 Node 배포본을 내려받기 위한 네트워크가 필요하며 완성된 앱의 동작에는 필요하지 않습니다.

```powershell
npm ci
npm run package:windows
npm test
npm run test:portable:year
npm run test:installed
```

`test:installed`는 설치 파일을 실제 실행하고 UI/MCP 통합 테스트 후 제거합니다. 현재 사용자/전체 사용자 레지스트리에 기존 Raising 제품이 있으면 중단하므로, 이미 사용하는 설치가 있다면 포터블 검사 또는 별도 Windows 사용자에서 실행하세요. 테스트 설치 폴더는 `.cache/installed-tests`, 생성한 게임·세이브·화면 캡처는 `.cache/release-tests`에 있습니다.

| 파일 | 역할 |
| --- | --- |
| `electron.mcp.config.ts` | MCP와 런타임 서비스를 독립 ESM 파일로 번들 |
| `scripts/package-mcp.mjs` | 공식 Node 22.23.3 x64 ZIP·SHA-256 확인, 런타임·스키마·라이선스 수집 |
| `scripts/package-studio.mjs` | Studio ASAR와 외부 `resources/mcp` 패키징 |
| `scripts/package-player.mjs` | Player와 두 샘플 패키징 |
| `scripts/build-installers.mjs` | electron-builder 26.15.3 NSIS 설치 파일과 해시 manifest 생성 |
| `scripts/test-installed.mjs` | 기존 설치 보호, 설치·제거·등록 확인, 통합 테스트 실행 |
| `tests/release/year.test.mjs` | 다섯 장기 시나리오·연도 전환·매 명령 저장/복원 검사 |
| `tests/release/installed.test.mjs` | 실제 설치판/포터블의 Studio·MCP·미리보기·Player 연결 검사 |

Node 런타임은 [공식 22.23.3 배포](https://nodejs.org/en/blog/release/v22.23.3)의 Windows x64 ZIP을 사용합니다. 공식 SHASUMS256과 ZIP 바이트가 일치해야 패키징을 계속하고, 확인한 버전·해시·출처를 `resources/mcp/runtime.json`에 기록합니다. Node 원문 라이선스와 MCP 번들에 포함한 9개 의존성의 라이선스도 함께 배포합니다. 설치 형식은 [electron-builder의 NSIS 설정](https://www.electron.build/docs/nsis/)을 사용합니다.

빌드 도구 의존성 감사에는 `sprintf-js` → `roarr` → `global-agent` → electron-builder 경로의 moderate 알림이 8개 보고됐습니다. 해당 코드들은 설치된 게임/MCP 번들에 포함하지 않습니다. 강제 버전 교체는 하지 않았으며 결과를 `.cache/stage8-audit.json`에 남겼습니다.

## 검증 범위

2026년 10월 11일의 최종 결과는 회귀 159개·실제 GUI 통합 5개 통과입니다. 연간 통합 사례는 포터블과 설치판에서 각각 통과했고, 설치판은 검사 후 실행 파일과 제거 등록을 정리했습니다. 중복 실행을 제외한 테스트 수는 164개입니다. 상세 근거는 [검증 기록](verification.md)을 따릅니다.

12개월 샘플은 활동 7개, 사건 19개, 엔딩 5개, 시나리오 5개입니다. 각 경로는 159개 코어 명령과 assertion 6개를 통과합니다. 매 명령 저장·복원 800회에서 전체 상태·난수·전이 기록이 기준 실행과 같습니다. 수작업 기대값은 [확장 샘플 문서](../examples/academy-year/README.md)에 있습니다.

통합 테스트는 실제 앱을 소스 저장소 밖 작업 디렉터리에서 실행하고, 자식 프로세스 PATH에서 개발용 Node·npm 경로를 제거합니다. 다음을 하나의 흐름으로 검사합니다.

1. Studio UI로 내장 12개월 템플릿을 새 프로젝트에 복사하고 활동 이름을 편집·저장합니다.
2. 설치된 Node와 MCP 서버로 연결해 오래된 수정 요청이 거부되는지 확인합니다.
3. MCP에서 검술 성장량 8→9와 시나리오 기대값 106→118을 같은 묶음으로 저장합니다.
4. 같은 프로젝트의 다섯 시나리오를 MCP로 검증하고, Studio가 갱신된 값을 불러오는지 검사합니다.
5. 실제 미리보기와 Player에서 12개월을 진행해 전체 상태가 MCP 결과와 같은지 확인합니다.
6. 6개월 뒤 파일 저장·불러오기, 앱 프로세스 종료 후 이어하기와 엔딩 도달을 검사합니다.
7. 탐사 성공/실패 경로도 전체 12개월을 플레이하고 난수 상태를 포함한 최종 저장·복원을 비교합니다.

**검증 한계:** 이 환경에서 테스트한 것은 Windows 10.0.26200 x64 개발 PC의 새 설치 폴더와 개발 PATH를 제거한 앱 실행입니다. 별도의 초기화된 PC/VM에서 검증한 것으로 간주하지 않습니다. 실제 깨끗한 PC·다른 Windows 버전·코드 서명·업데이트 검증은 남아 있습니다.

새 PC 인수 검사는 개발 도구가 없는 Windows x64에서 두 설치 파일만 복사해 설치, Studio 새 프로젝트 생성·편집·저장, Player 실행·파일 세이브·재실행 복원, 설치 Node를 사용하는 MCP 호스트 연결, 제거 후 프로젝트·세이브 보존을 확인하는 순서입니다. 검사한 OS·설치 위치·MCP 호스트 버전과 결과를 추가로 기록해야 최종 배포 승인을 판단할 수 있습니다.
