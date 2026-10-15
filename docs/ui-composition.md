# 5단계: UI 조합 제작

검증 환경: Windows, Qt 6.10.1 편집기, C++/SDL/RmlUi 플레이어.

## 열기

저장소 루트의 `start-ui-composition.cmd`를 실행한다. 편집기는 `dist/qt-studio-ui5/RaisingStudioQt.exe`, 플레이어는 `dist/native-player-ui5/RaisingPlayer.exe`, 샘플은 `projects/ui-composition-lab`이다. 기존 편집기·플레이어 출력 폴더는 교체하지 않았다.

디자인 메뉴의 **UI 조합 · 중첩 / 배치 / 선택 / 모달**을 연다. 화면과 부품을 같은 작업 창에서 선택하고 요소를 추가·삭제·수정할 수 있다. 부품 생성·복제, 모달 화면 생성, 부모 그룹, 앵커, 배치 방향과 간격, 스크롤, 목록 필터·정렬, 선택 필드와 행동 매개변수, 툴팁을 편집한다. 저장 버튼은 프로젝트 초안에 적용하며, 디스크 기록은 메인 편집기의 프로젝트 저장을 사용한다. 실행 취소/다시 실행은 전체 편집 변경을 하나의 작업으로 다룬다.

## 샘플 확인 순서

1. 플레이 → 새 게임 → 집 화면의 데이터 서비스 목록 버튼(`button-6`).
2. 이름 오름차순으로 Prayer service가 먼저 나온다. `show=false`인 Hidden service는 나오지 않는다. 아래로 스크롤하면 Rest service가 보인다.
3. 카드 버튼에 마우스를 올리면 공유 부품의 툴팁이 나온다. 카드를 누르면 해당 행의 서비스명으로 상세 모달이 열린다.
4. 상세 모달의 실행 버튼을 누르면 기존 범용 행동의 비용·효과 확인창이 열린다. 취소하면 게임 상태가 바뀌지 않는다. 확인하면 Prayer는 평판 +2, Rest는 선택 행의 수량 2가 전달되어 체력 +20이 적용된다.
5. 모달 아래 버튼은 그룹 내부 스크롤로 확인한다. Tab은 모달 안의 버튼으로 이동하며 배경 화면의 입력은 차단된다. Esc는 가장 위의 확인창 또는 상세 모달을 닫는다.
6. F5 저장 → F9 불러오기. 게임 상태는 복원되고 임시 UI 선택과 모달은 초기화된다. 새 프로세스에서 불러오기까지 검사했다.

## 데이터 계약

- 부품 안에 `kind: component`를 배치한다. 내부 요소 경로는 `인스턴스__요소`로 참조한다. 중첩 최대 8단계, 확장 요소 한도 10,000개를 적용하고 순환과 누락 참조를 진단한다.
- 그룹의 `flowLayout`은 `row`, `column`, `grid` 및 `gap`, `padding`, `columns`를 사용한다. 수동 자식 좌표 대신 배열 순서로 배치한다. `overflow`는 `visible`, `hidden`, `scroll`이다.
- 목록은 기존 `rowFilter`에 `sort: {field, direction}`을 더한다. 필터 후 안정 정렬하며 동률은 원본 순서를 보존한다. 누락 값은 뒤로 간다.
- 목록의 `selectionKey`와 반복 버튼의 `selectRow`가 행 ID를 저장한다. `screenId`를 지정하면 모달도 연다.
- 문구는 `binding: selected-field`, `selectionKey`, `selectionField`, `valueFormat`으로 선택 값을 표시한다.
- 버튼의 `selectionBindings`는 `{property, field}` 배열이다. 예: `actionId ← target`, `quantity ← quantity`, `actorId ← 캐릭터 필드`. 실행 직전에 현재 행을 다시 조회하므로 원본에서 없어진 행이나 실행 불가 행은 실행하지 않는다. 실제 명령의 검증은 기존 코어가 담당한다.
- 화면의 `modal: true`, 버튼의 `openModal`/`closeModal`, 요소의 `tooltip`을 사용한다. 모달 요소 ID는 화면 접두사로 분리해 배경의 동일 ID와 충돌하지 않는다. 확인창이 중첩되면 최상단 창만 입력을 받는다.
- 새 속성은 선택 사항이다. 기존 프로젝트와 저장 형식을 바꾸지 않는다. UI 선택·스크롤·모달은 게임 저장 데이터에 포함하지 않는다.

## 검증 증거

- `uiCompositionAuthoringRoundtrip`: 기존 `data-ui-lab`을 별도 폴더에 복사한 뒤 실제 Qt 위젯으로 중첩 부품·목록 연결·모달을 제작. 저장·재열기·실행 취소/다시 실행 확인. 최초 폴더 복사는 파일 작업이며 GUI 프로젝트 생성으로 주장하지 않는다.
- `uiCompositionPlayIntegration`: 실제 Qt 편집기 플레이 버튼으로 실행, 선택 행동의 평판 증가와 저장 복원 확인.
- `tests/native-player/ui-composition.test.mjs`: 1280×720, 1920×1080, 800×1200에서 정렬·필터·선택·툴팁·스크롤·모달·확인/취소·효과·저장·새 프로세스 복원 검사. hover/focus만으로 게임 명령이 실행되지 않는지도 검사.
- 기존 네이티브 계층 및 데이터 목록 테스트 2개 통과.
- 시각 요소·부품·계층·데이터 목록 단위 및 의미 검증 21개 통과. 최종 샘플 검증 오류/경고 0개.
- 스크린샷과 실제 플레이 보고서: `docs/evidence/ui-composition/`. 주요 파일은 `verification.json`, `editor-play-report.json`, `play-1280x720-report.json`, `play-1920x1080-report.json`, `play-800x1200-report.json`이다. 빌드/테스트 로그는 `.cache/ui5-*.log`에 있다.

## 현재 범위

신규 기능은 `ui-composition-v1` 런타임 기능으로 식별한다. 미지원 런타임으로 내보내기는 거절한다. 이번 구현의 실제 플레이 검증 범위는 Windows 네이티브이며 웹 플레이어는 이 기능을 아직 지원하지 않는다. 모바일·콘솔 실기기 검증, 해상도별 별도 디자인을 자동 전환하는 브레이크포인트, 전체 접근성 감사를 완료한 것은 아니다.

자동 배치는 줄바꿈 없는 row/column과 고정 열 grid이다. 앵커와 반응형 캔버스는 기존 규격을 사용한다. 모달은 하나의 사용자 모달 위에 기존 거래 확인창을 올리는 구조이며 임의 깊이의 모달 스택은 제공하지 않는다. 필드·중첩 요소 경로는 현재 표에서 직접 입력하며 오류는 공통 검증기에 표시된다. 원본 데이터와 전문 위젯 생성은 기존 데이터/UI 제작 도구를 함께 사용한다.
