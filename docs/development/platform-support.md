# Windows · Web 기능 지원표

2026-10-18 검토판. **런타임 기능 선언**, **이번 실행 검증**, **이전 기능별 검증**을 구분한다. 원시 선언과 SHA-256은 [capabilities 보고서](platform-capabilities.json), 설치·제작 시험은 [Qt 검토판](qt-preview-release.md)을 본다. 선언은 내보내기 허용 조건이며 모든 조합의 완전성·하드웨어 인증을 뜻하지 않는다.

| 기능 | Windows 플레이어 | Web / Chromium | 근거·제약 |
|---|---|---|---|
| 일정·능력치·비용·효과·사건·엔딩 | 지원 | 지원 | 공통 C++ 규칙. 공개 표본·경로 회귀 검사 |
| 인물별 관계·소지품·활동·대상 | 지원 | 지원 | `character-scoped-rules`, `roster-activities`, `component-context`; 별도 인물 연결 시험 기록 |
| 캐릭터별 선택·약속·사건 후속 연결 | 지원 | 지원 | 2026-10-21 `actor-story-links-v1` 추가. [동일 샘플·양쪽 순서·중간 저장 검증](../manual/web-actor-stories.md). 이전 프리뷰 배포본은 별도 갱신 필요 |
| 자유 화면·사용자 상태·조건부 표현·전환 | 지원 | 지원 | `free-screens`, `custom-states`, `presentation-variants`, `conditional-routing` |
| 재사용 부품·앵커·기본 반응형·목록 데이터 | 지원 | 지원 | 기본 기능. 더 새로운 중첩·스크롤·툴팁 구성은 아래 UI composition 제한 참조 |
| 상점·구매·소비·선물·관계 사건 | 지원 | 지원 | 공개 adventure/journey 및 이번 Web 구매·퀘스트·관계 사건 시험 |
| 탐험 이동·일반 전투 HUD·장비·전리품 | 지원 | 지원 | 이번 Web 실제 전투와 전투 중 저장·다운로드·복원·동일 재생 |
| 파티 전술 전투 UI (`party-tactics-v1`) | 지원 선언/별도 이전 시험 | 내보내기 제한 | 기본 전투 지원과 다르다 |
| 월 일정·활동 SD·결과 자동 진행 | 지원 | 지원 선언/이전 시험 | `monthly-schedule`, `inline-result-playback`, `activity-timeline`; 모든 연출 조합을 이번 단계에서 재검사하지 않음 |
| 통합 개인 시간·진행 세션 UI | 지원 | 내보내기 제한 | `unified-time-v2`, `progression-sessions-v1` |
| 연출 시퀀스 | 지원 | 내보내기 제한 | `direction-sequences-v1`; 단순 이미지·초상 전환과 다름 |
| 중첩 UI·자동 배치·스크롤·모달·툴팁 묶음 | 지원 | 내보내기 제한 | `ui-composition-v1` |
| 도감·CG·음악·사건·엔딩 회상·분기 회상 | 지원 | 내보내기 제한 | gallery/recollection 계열 6개 선언 차이 |
| 사용자 규칙 패키지 | 컴파일 후 실행 | 컴파일 후 실행 | `.raiserules`를 공통 서비스에서 검증·전개. 대상별 UI 제약은 유지. 임의 코드 VM 아님 |
| 애니메이션 플러그인 | 선택 설치 | 선택 설치 | element/mesh/atlas 선언. 플러그인이 양쪽 player 구현을 제공해야 하며 개별 라이선스 필요 |
| 저장·불러오기 | 파일 슬롯 | 다운로드·파일 선택 | 이번 설치본 내보내기와 양쪽 복원 확인. 브라우저 `file://` 실행은 지원하지 않음 |
| 콘텐츠 업데이트 후 저장 이전 | Qt 서비스에서 명시적 이전 | Qt 서비스로 이전한 콘텐츠/저장 검증 필요 | 규칙 버전 변경은 자동 호환으로 간주하지 않음. 임의 플러그인 상태 이전 미지원 |
| 화면/장면/저장하지 않은 초안의 렌더러 미리보기 | Qt + Windows | 도구 기능 제한 | `authoring-preview`, `renderer-preview`, `scene-preview` |
| GUI 편집기·프로젝트 선택기 | Qt 검토판 설치 경로 | 제공하지 않음 | Web은 플레이어 배포 대상 |
| 자동 플레이·목표 탐색·회귀·MCP | Node/C++ 서비스 | 브라우저 편집기로 제공하지 않음 | Linux CI는 헤드리스 서비스 검증. GitHub 보고서 연동은 별도 `gh` 인증 필요 |

위 2026-10-18 보고서에서 Windows의 43개 visual feature 중 Web은 29개를 선언한다. 이 기준의 미지원 14개를 사용하는 콘텐츠는 내보내기에서 거부한다. 기능을 선언 목록에 추가하는 것만으로 지원 범위를 넓히지 않는다.

2026-10-20 소스 빌드 추가 기능:

| 기능 | Windows 플레이어 | Web | 근거·제약 |
|---|---|---|---|
| 일정 보드·편성 요약 | 지원 | 내보내기 제한 | `schedule-board-v1`; 기존 일정 목록과 별도 표시 방식 |
| 번호가 있는 선택 카드 | 지원 | 내보내기 제한 | `narrative-cards-v1`; [제작 안내](../manual/narrative-choice-cards.md), 조건 비활성·선택 전후 저장 복원·해상도 검사 |
| 선택 효과 미리보기·실제 결과 표시 (2026-10-21) | 지원 | 내보내기 제한 | `choice-feedback-v1`; 선택별 후속 안내, 최대치 보정 후 실제 변화·저장 복원 검사 |

추가 기능은 이미 게시된 프리뷰 설치 파일에 소급 적용되지 않는다. 기존 선택지 목록은 이전 호환성을 유지한다.

이번 Web 검증 대상은 데스크톱 Chromium이다. Safari·Firefox, 실제 모바일 터치/오디오 정책·백그라운드 복귀는 별도 검증이 필요하다. Windows 검토판은 개발 PC에서 개발 도구 경로를 제거해 검증한 결과이며 새 VM·서명·SmartScreen 평판 검증을 대신하지 않는다.

Android는 소스/패키징 준비와 가짜 SDK 기반 검사까지다. iOS·macOS는 실제 빌드·실기 미검증이다. Linux는 서비스 CI만 검증했으며 Qt/플레이어 배포 지원을 뜻하지 않는다. Switch·PlayStation·Xbox는 공식 SDK·개발기 확보 전까지 미지원/미검증 상태다.
