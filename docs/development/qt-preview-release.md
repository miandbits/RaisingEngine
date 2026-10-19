# Qt 검토판 패키징·설치 검증

새 공개 저장소의 첫 프리뷰 준비는 [0.1.0-preview.1](preview-0.1.0.md)을 참고하세요. 아래 내용은 이전 로컬 검토판의 검증 기록입니다.

2026-10-18. 직접 작성한 엔진·편집기 코드는 소유자 승인에 따라 MIT다. 제3자 라이브러리와 게임 에셋의 조건은 별도로 유지한다. 이 단계의 결과물은 **서명하지 않은 로컬 검토판**이며 공개 배포 승인을 의미하지 않는다.

## 결과물과 재현

`release/qt-preview-r3/RaisingStudio-Qt-Preview-0.1.0-x64.exe`와 같은 디렉터리의 `RaisingStudio/` 휴대용 패키지를 사용한다. 설치 후 `RaisingStudioQt.exe`를 실행하면 프로젝트 선택기가 열린다. 기존 설치 폴더를 덮어쓰지 않으며 업그레이드는 새 경로에 설치한다. 프로젝트와 저장 파일은 설치 폴더 밖에 둔다.

```powershell
$env:QT_STUDIO_OUT = 'dist/qt-studio-release-preview'
$env:QT_PACKAGE_OUT = 'release/qt-preview-r3/RaisingStudio'
$env:QT_INSTALLER_OUT = 'release/qt-preview-r3/RaisingStudio-Qt-Preview-0.1.0-x64.exe'
npm run build:qt-studio
npm run package:qt-studio
npm run installer:qt-studio
npm run test:qt-package
$env:RAISING_TEST_WEB_EXPORT = (Get-Content .cache/qt-release/latest.json | ConvertFrom-Json).web
node --test tests/journey/authored-adventure-web.test.mjs
```

빌드에는 Qt 6.10.1 / MinGW, Node 22.14.0, NSIS와 빌드된 공통 서비스·Windows/Web 런타임이 필요하다. `NSIS_COMPILER`, `QT_ROOT`, `QT_COMPILER`, `QT_DOCS`, `RAISING_NATIVE_PLAYER_RUNTIME`, `RAISING_WEB_PLAYER_RUNTIME`으로 위치를 선택할 수 있다. 패키지는 Node, 필요한 서비스 의존성, 다섯 공개 템플릿, 규칙 확장 예제, 플레이어, 폰트와 고지를 포함한다. 사용자는 Node·Qt 개발 환경이 필요하지 않다.

`package-manifest.json`은 파일별 SHA-256, 버전, 원본 커밋과 작업 트리 변경 여부를 기록한다. 설치 파일 옆 JSON은 바이너리 SHA-256을 기록한다. 패키징은 공개 템플릿 허용 목록과 알려진 비공개 에셋 해시를 검사한다. 설치 프로그램은 명시적인 파일 목록만 설치·제거하며 다른 파일과 외부 프로젝트를 보존한다. 새 패키징 경로를 사용해야 하므로 위 명령을 반복할 때 출력 경로도 바꾼다.

## 실제 검증 범위

- 개발 PC에서 시스템 Node·Qt 경로와 관련 환경 변수를 제거한 설치본을 사용했다. 새 Windows VM 검증은 아니다.
- 격리된 설정으로 실제 Qt 프로젝트 선택기에서 공백·한글 경로의 adventure 프로젝트를 생성하고, 활동 이름을 편집·저장·재열기했다. 실제 내보내기 대화상자에서 Windows와 Web을 각각 생성했다.
- 숨겨진 Windows 플레이어에서 집 → 도시 → 저장 → 상점 → 불러오기를 수행하고 전체 상태 일치를 검사했다.
- 설치본에서 내보낸 Web 게임을 headless Chromium에서 플레이했다. 의뢰 수락, 검 구매·장착·해제, 일정, 탐험 이동, 전투, 전투 중 저장 다운로드·파일 복원, 동일 전투 결과, 전리품, 귀환, 의뢰 보상과 관계 사건을 확인했다.
- 재설치는 기존 폴더를 거부한다. 제거 후 외부 프로젝트·저장과 설치 폴더에 별도로 만든 사용자 파일이 남는다.
- Qt 서비스·미리보기 회귀 13건과 공개 콘텐츠 경계 검사를 실행했다. 결과와 로컬 증거 위치는 저장소의 `docs/archive/qt-preview-verification.json`을 본다.

Qt 시험은 `offscreen` 실제 위젯 테스트다. 개발용 QtTest·offscreen 플러그인은 시험할 때만 임시로 넣고 제거한다. 마우스로 직접 수행한 사람의 설치·제작 시험이라고 주장하지 않는다. Windows 플레이어와 Chromium도 숨김 실행하여 사용자의 게임 창을 건드리지 않았다.

## 남은 공개 조건

1. 새 Windows VM에서 설치·실행·제거, 실제 사용자 제작 시험, 코드 서명과 업그레이드 정책.
2. Qt/MinGW 및 포함 라이브러리의 완전한 고지·대응 소스 제공 절차 확인, 샘플 이미지·음악의 배포 권리 검토. MIT 엔진 라이선스가 이를 대신하지 않는다. [제3자 고지](../../THIRD_PARTY_NOTICES.md), [에셋 경계](public-assets.md).
3. 전체 Git 이력의 비밀 정보·비공개 에셋 검사와 별도 공개 이력 정리. 현재 브랜치를 강제 푸시하거나 공개 릴리스를 게시하지 않았다.
4. [지원표](platform-support.md)에 명시한 Web 기능 차이 해소. 모바일 실기·macOS/iOS·콘솔 빌드와 인증은 별도다.
