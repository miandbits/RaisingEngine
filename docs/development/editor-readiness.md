# 육성게임 범용 편집기 준비도

2026-10-22 소스 기준의 평가다. 기능 존재, 확인한 제작 흐름, 상용 제작의 안정성을 구분한다. 게시된 설치 파일이 최신 소스 기능을 모두 포함한다는 뜻은 아니다.

현재 위치는 **육성게임에 필요한 범용 규칙·화면·검증 기반을 갖춘 프리뷰이며, 처음 쓰는 제작자의 작업 흐름을 안정시키는 단계**다. 한 게임의 고정 메뉴와 규칙을 바꾸는 도구를 넘어섰지만, 장편 상용 프로젝트를 여러 사람이 끝까지 제작·유지하는 환경의 완성도는 아직 입증하지 못했다.

## 구현과 근거

| 영역 | 지금 가능한 것 | 남은 경계 |
| --- | --- | --- |
| 육성 규칙 | 개인·공용 능력, 자원, 관계, 소지품, 활동과 대상 지정, 조건·비용·효과, 사용자 상태 | 새 규칙 패키지는 기존 기본 연산을 조합한다. 임의 게임 로직 전체를 실행하는 스크립트 VM은 아니다. |
| 이야기 연결 | 활동·장소·날짜·선택·약속에서 후속 사건 연결, 공용 기록의 개인별 전환 미리보기 | 복잡한 대본·표현에는 전용 도구 이동이 남아 있다. 모든 조합의 의미가 자동 증명되지는 않는다. |
| 화면 제작 | 자유 화면, 데이터 바인딩, 조건부 표시·전환, 계층·부품·앵커, 상황별 표현 | 도구의 발견성, 작은 창 배치, 편집 방식과 진입점의 일관성을 더 다듬어야 한다. |
| 연출 | 활동 타임라인, 대사·선택·결과 재생, 선택 설치하는 애니메이션 플러그인 | 플러그인의 런타임과 대상 플랫폼 지원을 별도로 확인해야 한다. 타임라인·데이터·화면을 오가는 비용도 남아 있다. |
| 검증 | 결정적 재생, 대량 플레이, 목표 탐색, 저장 복원, 기준 비교·부분 승인·CLI/CI 보고서 | 제한 탐색의 미발견은 도달 불가능의 증명이 아니다. 재미·연출·경제 밸런스와 장편 콘텐츠 품질은 제작자 시험이 필요하다. |
| 배포 | Windows 및 지원 기능 범위 내의 Web 내보내기 | Web의 일부 UI·연출·시간·회상 기능 차이가 남는다. 모바일·macOS·콘솔의 실기 배포는 완료로 보지 않는다. |

근거: [연결 작업실](../manual/connection-workspace.md), [규칙 확장 계약](../manual/rule-extensions.md), [여섯 유형 제작 시험과 한계](../manual/genre-authoring-assurance.md), [플랫폼 지원표](platform-support.md). 개인별 사건은 공개 `personal-story-web.test.mjs`에서 실제 브라우저의 선택·저장·복원과 네이티브 재생을 비교한다.

## 우선순위

1. **기본 조작의 완결성**: 창 크기와 언어에 관계없는 명령 접근, 키보드 이동, 선택·복사·되돌리기·미저장 경고의 일관성. 상단 도구 접근 문제도 이 범주다.
2. **한 흐름에서 제작**: 선택한 인물·요소의 데이터 → 활동 → 조건·효과 → 사건 → 표현을 따라가며 수정하고, 변경 영향과 오류 위치로 돌아온다. 전용 도구 전환 시 선택과 미적용 입력을 잃지 않아야 한다.
3. **독립 제작자의 완주**: 템플릿 선택부터 첫 게임 배포까지 도움 없이 진행하는 시험에서 막힌 지점·소요 시간·오류 복구를 기록한다. 내부 기능 검사 통과와 별개의 기준이다.
4. **상용 제작 내구성**: 장편 콘텐츠 업데이트, 기존 저장 이전·복구, 프로젝트와 플러그인 버전 고정, 팀의 변경 충돌, 장시간 편집·실행을 반복해서 확인한다.
5. **플랫폼 차이 축소와 배포**: 지원표의 차이부터 실제 런타임으로 해소하고 실기·입력·오디오·접근성 검증으로 이어간다. 지원 선언만으로 완료 처리하지 않는다.

완성도를 하나의 백분율로 표시하지 않는다. 새 기능의 개수보다, 실제 제작자가 반복 작업을 얼마나 적게 하고 실수에서 얼마나 쉽게 회복하는지를 다음 개선의 기준으로 삼는다.

## P0 → P1 creator workflow — 2026-10-23 implementation

| Roadmap area | Implementation and repeatable check | Remaining gate |
| --- | --- | --- |
| Complete starter and optional checklist | Three-month starter; actual edit/play/export evidence; stable completion after adding documents and reopening | Observe unfamiliar designers; internal automation is not the acceptance study |
| Safe authoring | Existing named forms, Connections, undo/redo and validation navigation; new per-session recovery with imported files, restored into a separate project | Unapplied form text is outside recovery; media-heavy long sessions need separate study |
| Explain and repeat playtests | Named routes and first-divergence comparison retained; event/ending inspector displays actual values, selection priority and timing limits | Bounded search does not prove unreachability |
| Release loop | Early target compatibility, release version, launch, local Web server, game/project ZIPs, hashed dependency manifest and save-update guidance | Public signing/notarization, clean machines, third-party license review and physical iOS devices |
| P1 regression and performance | `test:creator-smoke`, actual Qt form journey, native and Web route checks, standalone Mac package gate, CI definitions and `benchmark:creator` | [GitHub run 40079987580](https://github.com/miandbits/RaisingEngine/actions/runs/40079987580) passed Linux services, Mac packaging and Windows portable/installer checks at `bc70377a8fd7e2d5ca9facb15c2bb75ffbc9e124`; unfamiliar-designer acceptance remains separate |

Measured locally on Apple M5 Pro / macOS arm64 / Node 22.23.4: the starter's median edit including recovery was 4.7 ms; the synthetic 517-document project's was 32.5 ms (first 73.2 ms), opening 138.8 ms median, validation 22.7 ms median, save 218.4 ms. Five edit/view samples and three open/validation samples are a small investigation baseline, not a universal performance guarantee. Timings include service compatibility/recovery work, but exclude Qt painting and large imported media. The benchmark emits the full repeatable report under `.cache/creator-benchmark/`.

The independent acceptance gate remains **four of five unfamiliar designers customizing and exporting without intervention, plus another person playing the export**. No human study, App Store release or Developer ID notarization is claimed. On 2026-10-24, Windows Server 2022 CI passed source builds, the bundled starter without developer tools on PATH, portable exports, the NSIS installer, installed-editor exports, native game save/load and safe uninstall. The same source passed the [goal-route regression run](https://github.com/miandbits/RaisingEngine/actions/runs/40079987585). The `windows-p1-installer` artifact is a tested review build, not a signed public release. Technical smoke completion and external release acceptance are tracked separately.

## Windows CI repairs and acceptance evidence — 2026-10-24

The Windows run exposed path normalization, PowerShell argument forwarding and missing QtTest runtime deployment problems. It also exposed a timing defect: the editor published a newly opened project before recovery discovery finished, allowing that background request to swallow the first project-settings or export click. Opening now returns recovery information together with the project, and an explicit export request waits for an in-flight bridge request.

The accepted run is [Creator journey 40079987580](https://github.com/miandbits/RaisingEngine/actions/runs/40079987580), source `bc70377a8fd7e2d5ca9facb15c2bb75ffbc9e124`. Its evidence artifacts are `windows-creator-evidence`, `macos-creator-evidence` and `creator-service-evidence`; the tested installer is `windows-p1-installer` (artifact SHA-256 `2e01d1f69a6aab1786d14edea10d9922834c87bc25cd89f972c2eb7adeac81a7`). Later engine changes require their own CI run; this evidence is not automatically transferred to a newer revision.
