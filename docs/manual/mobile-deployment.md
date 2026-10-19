# 모바일 배포 현황과 재현 방법

현재 확인된 산출물은 **Android API 35 에뮬레이터에서 실행한 x86_64 디버그 APK**다. Windows와 같은 C++ 시뮬레이션 코어 및 SDL3/RmlUi 플레이어를 사용한다. arm64 실기기, 스토어 배포, iOS, 콘솔 실행을 완료한 상태는 아니다. 확인 당시 APK 해시·콘텐츠 지문·경로는 [검증 기록](../evidence/android-journey-preview.json)에 보존되어 있다.

## Android 빌드

먼저 편집기에서 검증된 게임을 Windows용으로 내보낸다. Android 생성기는 그 번들의 `game.json`, 참조된 `assets/`, `fonts/`, `licenses/`만 사용하며 Windows 실행 파일을 APK에 복사하지 않는다. 내보낸 폴더의 원본 에셋 경로 이탈과 심볼릭 링크를 검사한다. 결과 디렉터리는 새 경로여야 한다.

필요한 도구는 Node.js, JDK 17 이상, Android SDK, Gradle이다. 이번 환경은 Gradle 8.13 및 Android Studio의 JBR을 사용했다. 생성 프로젝트는 AGP 8.9.1, compile/target SDK 35, min SDK 26, NDK 28.2.13676358, CMake 3.22.1을 지정한다. SDK 패키지 설치에는 해당 라이선스 승인과 다운로드가 필요할 수 있다. 네이티브 플레이어의 vendored 의존성도 준비되어 있어야 한다.

저장소 루트의 PowerShell 예시:

```powershell
$env:NODE_OPTIONS = ''
$env:ANDROID_HOME = 'C:\Users\Mi\AppData\Local\Android\Sdk'
$env:JAVA_HOME = 'C:\Program Files\Android\Android Studio\jbr'
# Gradle 8.13이 PATH에 있거나 GRADLE에 gradle.bat의 절대 경로를 지정한다.
node scripts/build-android.mjs `
  --bundle 'release/native-journey-preview/game-exports-V1twC2/windows' `
  --out 'dist/android-new-preview' `
  --abi x86_64 `
  --application-id org.raisingengine.sample
```

`--bundle`은 실제 새 내보내기 경로로 바꾼다. `--sdk`, `--java-home`, `--gradle`로 환경 변수 대신 명시할 수 있다. `--prepare-only`는 Gradle 프로젝트·번들·메타데이터를 준비하고 컴파일을 생략한다. `--abi`는 `x86_64`와 `arm64-v8a`를 받으며 기본값은 `arm64-v8a`다. 기본값이 있다는 사실은 해당 ABI의 실기기 검증을 뜻하지 않는다.

산출물:

- `project/`: Gradle Android 앱 프로젝트. 네이티브 CMake 소스는 현재 저장소 경로를 참조한다.
- `RaisingPlayer-debug.apk`: 디버그 서명 APK.
- `build.json`: 콘텐츠 지문, ABI, 빌드 ID, 진행 상태. `apk-built-not-device-verified`는 컴파일 성공만 뜻한다.

에뮬레이터가 이미 실행 중일 때 설치 예시:

```powershell
& "$env:ANDROID_HOME\platform-tools\adb.exe" -s emulator-5554 install -r 'dist/android-new-preview/RaisingPlayer-debug.apk'
& "$env:ANDROID_HOME\platform-tools\adb.exe" -s emulator-5554 shell am start -n org.raisingengine.sample/org.raisingengine.player.RaisingActivity
```

시리얼과 application ID는 실제 대상에 맞춘다. 기존 앱 데이터 삭제나 에뮬레이터 초기화는 필요하지 않다.

## 실행·저장 구조

Java Activity는 앱 내부 저장소에 콘텐츠 해시별 `game-<buildId>` 디렉터리를 만들고 에셋을 추출한 뒤 `--bundle`로 네이티브 플레이어에 전달한다. SDL3와 `libmain.so`를 로드하며 Android 글꼴은 해당 번들의 `fonts/`에서 읽는다. 캔버스 비율에 맞춘 가로/세로 방향 힌트를 SDL 초기화 전에 지정한다.

터치는 레터박스를 고려해 창 좌표에서 논리 화면 좌표로 변환한다. 합성 마우스 입력을 차단하여 한 번의 터치가 두 번 실행되지 않게 한다. 취소된 터치는 버튼 동작을 확정하지 않는다.

백그라운드 진입 시 SDL 이벤트 감시에서 이미 대기 중인 동작을 한 번 처리하고 확정 상태를 앱 전용 저장 경로의 `autosave.json`에 저장한다. 중복 lifecycle 알림은 추가 저장하지 않는다. 수동 저장 슬롯은 보존한다. 오디오는 일시 정지하고 전경 복귀 시 재개한다. 재시작은 자동 저장을 명령 재현으로 검증하고, 주 파일이 없거나 손상되면 `.bak` 백업을 검증해 복구한다. 검증에 실패한 상태는 적용하지 않는다.

자동 저장 검증에서는 HOME 키를 보낸 직후 파일을 읽거나 즉시 강제 종료하지 않는다. Android의 화면 전환이 끝나기 전에 pause 콜백이 아직 전달되지 않을 수 있다. 제한 시간을 두고 자동 저장의 **예상 콘텐츠 지문과 상태 버전**이 기록되었는지 확인한 다음 강제 종료·재시작·전체 상태 일치를 검사한다. 이전 콘텐츠의 파일 두 개가 우연히 같다는 결과는 복원 성공 증거가 아니다.

## 실제 확인한 범위

[검증 기록](../evidence/android-journey-preview.json)의 `dist/android-preview-01/RaisingPlayer-debug.apk`에서 한글과 실제 이미지, 새 게임 → 도시 → 상점 → 꽃 구매 → 수동 저장 → 선물 → 관계 사건을 에뮬레이터 터치로 확인했다. 백그라운드 → 강제 종료 → 재시작 후 저장 JSON이 정확히 일치했다. 방향 보정 후 가로 화면 스크린샷도 기록했다. 이 기록은 이후 추가된 탐험·전투 콘텐츠의 Android 검증을 대신하지 않는다.

별도 Windows 플레이어에서는 동일 터치 좌표 처리와 lifecycle 함수의 자동 검증, 자동 저장 백업 복구/거절, 기존 키보드·이미지·오디오 검증 총 10개가 통과했다. 이는 실제 Android 운영체제의 모든 중단 상황을 검증했다는 뜻은 아니다.

## 다음 플랫폼별 완료 조건

| 대상 | 남은 작업과 외부 조건 |
| --- | --- |
| Android 실기기 | arm64 APK 빌드와 물리 기기 실행, 고해상도/노치 안전 영역, 긴 목록의 터치 스크롤, 전화·오디오 포커스·저메모리 종료·회전 검사, 16KB 페이지 크기 호환 검증 |
| Android 스토어 | 릴리스 서명 키의 사용자 소유 관리, AAB/릴리스 파이프라인, 당시 스토어 정책과 target SDK 요구 확인, 라이선스/개인정보/등급 및 실제 배포 심사 |
| iOS | macOS와 Xcode 도구 체인, 실제 Apple SDK 빌드 타깃 및 앱 lifecycle/저장/입력 어댑터, 기기 테스트, 서명·프로비저닝과 배포 자격. 현재 빌드 또는 시뮬레이터 실행 증거 없음 |
| Switch·PlayStation·Xbox | 각 플랫폼 개발자 승인, 비공개 SDK·개발기와 계약, 플랫폼별 그래픽/입력/저장/사용자 프로필 어댑터 및 인증 검증. 공개 SDL 소스와 C++ 코어만으로 배포 지원을 선언할 수 없음 |

에뮬레이터 실행은 오디오를 비활성화했으므로 Android 소리 출력이나 전화 중 오디오 복귀는 아직 검증하지 않았다. 플랫폼별 승인이나 서명 자격을 코드에서 대체하거나 우회하지 않는다.
