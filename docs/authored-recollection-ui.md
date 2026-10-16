# 편집 가능한 회상 재생 조작

`프로젝트 설정 → 도감 → 편집 가능한 회상 조작 화면 만들기`로 기본 화면을 만듭니다. 적용 후 화면 목록의 `회상 · 재생 조작`을 열어 일반 캔버스와 속성 창에서 버튼·문구·위치·앵커·색·폰트를 편집합니다. 기존 화면을 직접 만들어 `회상 조작 화면`에 연결할 수도 있습니다.

이 화면은 기록 당시의 배경·캐릭터·대사 위에 겹치는 UI입니다. 화면 전체 배경을 바꾸려면 패널이나 이미지를 배치합니다. 원본 사건 화면을 바꾸거나 과거 기록을 다시 해금할 필요가 없습니다. 재사용 부품과 그룹도 사용할 수 있습니다.

## 데이터 계약

선택 설정: `project.gallery.replayControlsScreenId`. 미설정 프로젝트는 기존 고정 조작 막대를 사용합니다. 저장 형식 변경이나 기존 프로젝트 마이그레이션은 필요하지 않습니다.

| 버튼 행동 | 동작 |
| --- | --- |
| `recollectionPrev` | 이전에 방문한 기록 프레임. 첫 프레임에서는 비활성 |
| `recollectionNext` | 다음 기록 프레임. 마지막 프레임에서는 회상 종료 |
| `recollectionClose` | 회상을 종료하고 도감으로 복귀 |
| `recollectionSave` | 회상 위치·연출 상태를 별도 파일에 저장 |
| `recollectionLoad` | 같은 기록의 저장 위치·연출 상태 복원 |
| `recollectionAuto` | 자동 진행 켜기/멈추기 |
| `recollectionSpeed` | 1 → 2 → 4 → 1배속 순환 |
| `recollectionSkip` | 마지막 기록 장면으로 이동하고 자동 진행 정지 |

텍스트 연결: `replay-title`(항목 제목), `replay-position`(현재/전체 프레임), `replay-notice`(저장·불러오기 성공/실패 메시지).

자동 진행 상태는 `replay-mode`, 현재 배속은 `replay-speed`로 표시합니다. 새 조작 화면 생성기는 자동/멈춤·배속·끝으로 건너뛰기 버튼과 상태 표시를 함께 만듭니다. 기존 화면에는 일반 버튼을 추가해 위 행동을 선택하면 됩니다. 새 세 행동은 선택 사항이므로 기존 다섯 버튼 화면도 계속 유효합니다.

자동 진행은 장면당 기본 3초이며 배속을 적용합니다. 당시 선택한 경로에 기록된 프레임만 순서대로 재생하고, 새로운 선택이나 게임 명령을 실행하지 않습니다. 마지막 프레임의 표시 시간이 지나면 도감으로 돌아옵니다. 건너뛰기는 마지막 프레임을 보여주고 멈추므로 결과를 읽을 수 있습니다. 수동 이전/다음은 자동 진행을 멈추고 해당 프레임의 타이머를 초기화합니다. 자동 멈춤은 장면 이동만 멈추며 장식 애니메이션과 음악은 계속됩니다. 배속은 장면 간격과 캐릭터 애니메이션에 적용하고 음악의 재생 속도·음정은 변경하지 않습니다.

회상 조작 화면은 패널·텍스트·버튼·이미지·그룹·게이지와 이들로 구성한 부품을 지원합니다. 검증기는 다섯 행동의 누락, 없는 화면 참조, 본편 저장/게임 시작 등 잘못된 행동, 게임 진행 위젯을 진단합니다. 부품 덮어쓰기까지 검사합니다. 조건으로 모든 버튼을 숨기는 배치는 피하세요. Esc/F5/F9 단축키는 유지됩니다. 일반 본편 화면에 회상 전용 버튼을 배치하면 비활성화됩니다.

## 상태 격리와 호환성

- 원본 연출은 기록된 콘텐츠와 방문한 상태로 별도의 세션·애니메이션 호스트에서 재생합니다. 능력치·비용·보상·난수·해금 효과를 다시 실행하지 않습니다.
- 현재 프로젝트의 조작 화면만 원본 연출 위에 합성합니다. 요소 ID에는 별도 접두사를 붙여 원본 장면의 버튼과 충돌하지 않습니다.
- 회상 저장 파일은 `recollection-<gameId>.json`입니다. 본편 슬롯과 `collection-<gameId>.json` 해금 프로필은 쓰지 않습니다.
- 저장의 선택 필드 `playback: { autoplay, speed, elapsedMs }`로 자동 진행 여부·배속·프레임 경과 시간을 복원합니다. 이전 저장에 필드가 없으면 자동 꺼짐·1배속·경과 0으로 읽습니다. 잘못된 설정은 적용 전에 거부하고 자동 진행을 멈춥니다.
- Cutout 1.14.2부터 회상 저장은 정확한 연출 체크포인트를 요청합니다. 반복 클립의 누적 시간·표정·트랙 간 전환·물리 흔들림의 위치와 속도·계산 시계를 복원해 저장 시점부터 이어집니다. 1/2/4배속과 자동 진행 타이머도 함께 복원합니다. 일반 본편 저장의 기존 복원 정책은 바꾸지 않았습니다.
- 예전 회상 저장은 계속 읽지만, 당시 기록하지 않았던 반복 동작의 시점은 복구할 수 없습니다. 정확 복원은 새 플레이어와 Cutout 1.14.2로 새로 저장한 회상에 적용됩니다. 다른 애니메이션 플러그인은 해당 플러그인의 저장 계약을 따릅니다.
- 이전 형식의 대사 기록과 원본 에셋/플러그인 변경 시의 기존 동작을 유지합니다. 원본 미디어 구성이 바뀐 기록은 기존 정책대로 원본 연출 재생이 제한될 수 있습니다.
- 이번 실행 지원은 Windows 네이티브 플레이어입니다. `authored-recollection-v1` 기능이 없는 런타임으로 내보내기를 시도하면 거부합니다. 웹·모바일·콘솔에서 검증했다고 주장하지 않습니다.
- 자동 진행 행동/상태 표시를 쓴 프로젝트에는 `recollection-playback-v1`도 필요합니다. 부품 덮어쓰기에만 새 행동이 있는 경우에도 구형 런타임 내보내기를 거부합니다.

## 적용 및 검증 — 2026-10-15

### 회상 반복 애니메이션 정확 복원

- 공개 샘플 `last-grandson-polished`의 Cutout 잠금을 1.14.2로 갱신했습니다. 이전 설치 버전은 보존됩니다. 과거 기록의 원본 플러그인 일치 검사는 유지되므로 플러그인을 바꾼 새 내보내기에서 기존 원본 연출 기록을 무조건 재생하도록 완화하지 않습니다.
- `dist/native-player-replay-exact/RaisingPlayer.exe`: 기존 실행 파일을 덮어쓰지 않은 새 Windows 플레이어 빌드.
- 코어 1,161개 검사 통과: 전환 중 저장, 반복 트랙·이벤트 연속성, 물리 자세/속도/계산 시계 일치, 잘못된 체크포인트의 무변경 거부, 기존 저장 정책 호환.
- `tests/native-player/replay-auto.test.mjs`: 실제 플레이어로 두 선택 경로를 기록하고 각각 1/2/4배속·자동 재생 중 저장/불러오기. 저장 직후와 이어 재생한 뒤의 렌더링 행렬·클립 시간·표정·트랙 상태를 비교했습니다. 프로세스를 새로 띄운 복원도 검사합니다. 본편 상태/RNG·명령·애니메이션 및 본편 슬롯/도감 프로필의 불변을 확인합니다.
- 증거: `.cache/replay-auto/{survival,farewell}-exact-{1,2,4}-report.json`과 같은 이름의 PNG. `.cache/replay-auto/game-path.txt`와 `save-path.txt`에 테스트 게임/저장 경로를 남깁니다.
- `tests/plugins/cutout-checkpoint.test.mjs`: 새 Wasm 모듈로 루프와 물리 체크포인트를 복원해 이후 프레임까지 일치하는지 확인합니다. 웹 회상 UI 전체 지원을 뜻하지는 않습니다.
- 기존 `replay-ui`, `gallery-media`, `gallery-ui` 실제 플레이어 회귀 검사 3개도 통과했습니다.

재현: 플러그인 `tools/build.mjs win wasm studio test` 및 `tools/package.mjs` 실행 후 새 패키지를 로컬 스토어에 설치합니다. `RAISING_NATIVE_PLAYER_RUNTIME=dist/native-player-replay-exact`로 위 네이티브 테스트를 실행합니다.

### 기존 제작·자동 진행 검증

`examples/last-grandson-polished/project.json`에 실제 Qt 편집기에서 생성하고 `다음 기억`으로 편집한 화면을 반영했습니다. 화면을 파일에서 수작업 생성한 뒤 GUI 제작이라고 처리하지 않았습니다. 공개 샘플 반영은 GUI에서 저장한 화면 데이터 복사입니다.

- 실제 Qt `replayUiAuthoring`: 새 프로젝트 복사, 화면 생성, 버튼 문구·행동 편집, 실행 취소/재실행, 텍스트 연결 확인, 저장/재열기, 오류 0건.
- 네이티브 `replay-ui.test.mjs`: 2개월 진행 → 인증 엔딩, 중간 본편 저장/복원, 사건 회상 이전/다음/종료/마지막 프레임 종료, 회상 저장/복원, 프로세스 재시작, 과거 기록에 최신 UI 반영, 일반 화면의 회상 버튼 비활성, 손상된 회상 저장 처리.
- 회상 전후 본편 상태(난수 포함), 명령 기록, 해금 프로필, 살아 있는 애니메이션 인스턴스 비교. 본편 슬롯과 해금 프로필 파일 내용도 동일함을 확인.
- `gallery-media.test.mjs`, `gallery-ui.test.mjs`: 고정 회상 조작 막대, 편집형 카드/필터, 원본 연출·음악, 기존 기록 호환성 회귀 통과.
- JSON Schema/의미 검증 및 부품·데이터 UI·중첩 UI 계약 검사.

로컬 증거: `.cache/replay-ui/`의 `editor-authored-controls.jpg`, `replay-first.png`, `replay-loaded.png`, `replay-updated.png`, `*-report.json`. 테스트 보고서와 개인 저장 파일은 공개 커밋에서 제외합니다.

## 재현 명령

```powershell
$env:NATIVE_PLAYER_OUT='dist/native-player-replay-auto'
node scripts/build-native-player.mjs
$env:QT_STUDIO_OUT='dist/qt-studio-replay-auto'
node scripts/build-qt-studio.mjs
$env:QT_UI_TEST_LOG='.cache/replay-auto-qt-test.txt'
node scripts/test-qt-studio.mjs replayUiAuthoring
$env:RAISING_NATIVE_PLAYER_RUNTIME='dist/native-player-replay-auto'
node --test tests/native-player/replay-ui.test.mjs tests/native-player/replay-auto.test.mjs
node --test tests/validation/replay-ui.test.mjs
```

GUI 시험에서 만든 별도 프로젝트로 플레이를 확인하려면 `RAISING_REPLAY_UI_PROJECT`에 `.cache/replay-auto/editor-project.txt`의 경로를 지정합니다. 기본 플레이 시험은 공개 샘플을 사용하므로 GUI 산출물이 없는 CI에서도 건너뛰지 않습니다.

## 자동 진행 추가 검증

실제 Qt `replayUiAuthoring` 시험은 현재 `.cache/replay-auto/editor-project.txt`에 제작 프로젝트를 기록합니다. 해당 시험에서 만든 새 버튼과 상태 표시를 기존 공개 샘플의 회상 화면에 반영했습니다. 기존 화면 ID는 보존했습니다.

`tests/native-player/replay-auto.test.mjs`는 서로 다른 선택·엔딩을 기록한 두 회차를 각각 플레이한 뒤 다음을 검사합니다.

- 수동·2배속 자동·4배속 자동·건너뛰기로 도달한 기록 프레임과 선택 경로 일치. 기록과 다른 선택은 비활성.
- 자동 진행 멈춤, 마지막 프레임 종료, 실제 벽시계 기반 자동 진행.
- 진행 도중 저장, 같은 프로세스/새 프로세스 복원, 나눠서 진행한 시간과 한 번에 진행한 시간의 결과 일치.
- 이전 저장 기본값, 잘못된 재생 설정 거부, 본편 상태·난수·명령 기록·해금 프로필·본편 애니메이션 인스턴스 보존.
- 본편 슬롯과 해금 프로필의 파일 내용 불변.

추가 실행: `RAISING_NATIVE_PLAYER_RUNTIME=dist/native-player-replay-auto`를 설정하고 `node --test tests/native-player/replay-auto.test.mjs`. GUI 제작본을 직접 쓰려면 `RAISING_REPLAY_AUTO_PROJECT`를 설정합니다. 증거는 `.cache/replay-auto/*-report.json`, `editor-authored-playback.png`, `survival-fast.png`, `farewell-fast.png`에 보관합니다.
