# CLI · GitHub Actions 경로 회귀 검증

편집기의 저장 비교와 같은 `checkGoalImpacts` / `reviewGoalItems` 서비스를 사용합니다. 원본 기준, 활성 기준, 부분 승인 기대값을 읽고 현재 콘텐츠를 실행합니다. CLI가 기준을 새로 만들거나 변화를 자동 승인하지 않습니다.

## 실행

Node.js 22.12 이상, CMake 3.20 이상, C++17 컴파일러가 필요합니다. Windows는 기존 MinGW 도구 체인을 PATH에 설정합니다. Qt·Electron·그래픽 플레이어는 필요하지 않습니다.

```sh
npm ci
npm run build:headless
node scripts/check-goal-regression.mjs projects/starlight-three-months --report-dir .cache/goal-regression
```

`npm run check:regression -- ...`도 같은 CLI입니다. PowerShell에서 npm 래퍼가 옵션을 소비하는 환경에서는 위의 `node` 직접 실행을 사용하세요.

다른 컴퓨터 또는 CI에서 빌드한 런타임으로 검사할 때:

```sh
node scripts/check-goal-regression.mjs projects/starlight-three-months --runtime-policy verify-replay --timeout-ms 300000 --report-dir .cache/ci-goal-regression
```

- 기본 `strict`: 실행 파일 SHA-256이 기준과 달라지면 기존 편집기와 같이 변경으로 판정합니다.
- `verify-replay`: 실행 파일이 다르면 기준 기록에 들어 있는 **원래 콘텐츠·seed·명령**을 현재 런타임에서 먼저 재생합니다. 원본의 단계별 상태·비용/효과/조건 관측·목표 도달 결과를 재현해야 해당 기준에 묶인 부분 승인을 적용합니다. 재현 실패는 검사 오류입니다. 다른 런타임이라는 사실과 검증 결과는 보고서에 남습니다.
- 이 정책은 기록된 경로에서의 호환성 검사입니다. 런타임 전체의 동등성 보장이 아닙니다. 편집기의 기본 런타임 정책은 바뀌지 않았습니다.
- 전체 시간 예산은 `--timeout-ms`로 지정합니다(기본 120000, 최대 3600000). 완료하지 못한 경로는 실패로 남습니다.

## 판정과 보고서

| 종료 코드 | 의미 |
| --- | --- |
| 0 | 모든 경로가 승인 기대값과 일치하고 목표 도달 및 중간 저장 복원 검사 통과 |
| 1 | 승인하지 않은 변화, 승인값의 재변경, 경로/런타임 변경, 명령 실패, 목표 엔딩/사건 미도달, 저장 복원 불일치 |
| 2 | 유효하지 않은 프로젝트·경로·승인 기록, 기준 누락/손상, 등록 경로 없음, 시간 초과, 런타임 원본 재현 실패, 검사 중 입력 변경 |

`--report-dir`에 다음 세 파일을 만듭니다. 실패 시에도 생성합니다.

- `summary.md`: 경로별 결과, 승인/남은 변화 수, 첫 차이 단계, 저장 복원 횟수와 오류. 남은 항목의 이전 기대값과 현재 값도 표시합니다.
- `report.json`: 전체 변경 항목·승인 기대값·발생 단계, 콘텐츠 및 경로 식별값, 런타임 재현 결과, 저장 복원 결과.
- `junit.xml`: 경로별 testcase와 failure/error. 외부 CI의 JUnit 수집기에 사용할 수 있습니다.

승인은 경로/항목을 무시하는 설정이 아닙니다. 원본 비용 2를 기대값 7로 승인했다면 현재 7은 통과하고 9나 2는 회귀입니다. 목표 미도달·명령 실패를 부분 승인으로 숨길 수 없습니다. 경로 삭제는 더 이상 등록 경로가 아니므로 일반 파일 리뷰에서 확인해야 합니다. 모든 경로가 없어지면 CLI는 오류로 종료합니다.

## Git에 포함할 파일

콘텐츠와 함께 `playtests/routes/*.json`, `playtests/baselines/*.json`, `playtests/baselines/.approvals`를 커밋합니다. 마지막 파일은 편집기에서 승인/복원한 활성 기준·부분 기대값·이력입니다. `.studio` 검사 캐시는 필요하지 않습니다. 기준이 없는 상태에서 CI가 현재 값을 기준으로 생성하도록 설정하면 회귀를 놓치므로 그렇게 구성하지 않습니다.

파일럿에는 기존 세 엔딩(`ordinary`, `relationship`, `success`)과 관계 사건(`auto-friendship`)의 원본 성공 기준 4개를 추가했습니다. 이번 초기 기준에는 부분 승인 항목이 없습니다. 실제 승인 이후 생성되는 `.approvals`도 Git에 포함해야 합니다.

## GitHub Actions

`.github/workflows/goal-regression.yml`은 push·pull request·수동 실행에서 Linux의 C++ 런타임을 빌드하고 CLI 통합 테스트와 파일럿 검사를 실행합니다. 저장된 Windows 기준은 `verify-replay`로 먼저 원본 재현을 확인합니다.

- 결과 요약: Actions 실행 페이지의 Summary.
- 다운로드: `goal-regression-reports` artifact(JSON/Markdown/JUnit, 실패 보고서도 포함, 14일 보관).
- 권한: `contents: read`, 별도 secret 없이 실행.
- CLI가 1 또는 2로 종료하면 작업이 실패합니다. 빌드 실패로 실행하지 못한 검사를 통과로 표시하지 않습니다.
- PR 병합을 강제 차단하려면 저장소 branch protection/ruleset에서 `regression` 작업을 필수 검사로 지정해야 합니다. 이 설정은 자동 변경하지 않았습니다.

## 검증

`npm run test:regression`은 실제 네이티브 실행 파일을 자식 프로세스로 호출합니다. 부분 승인 통과, 남은 회귀, 승인값 재변경/원복, 엔딩 미도달, 저장 복원, 런타임 해시 변경 및 원본 재현 실패, 기준 누락·손상, 빈 경로, 잘못된 입력, 시간 초과를 검사하며 보고서도 읽어 확인합니다.
