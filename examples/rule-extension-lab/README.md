# Portable rule extension example

Open `game` with RaisingStudioQt. The example is based on the public two-month academy
fixture and adds the original `dojo.raiserules` package. It contains no assets from
commercial games.

In **디자인 → 규칙 확장 패키지 · 진행 방식…**, inspect **막내의 돌파 수련**.
The progression is 준비 → 수련 → 돌파, with an optional explicit restart loop.
On the schedule screen:

1. **수련 준비** costs 2 money. Cancelling the confirmation changes nothing.
2. **호흡 수련** costs 3 money, adds 12 skill and 2 stress.
3. **경지 돌파** requires at least 10 skill and changes the progression state.
4. **돌파의 깨달음** is a normal action whose condition/effect bindings were authored
   in the Qt extension forms: skill ≥12, then skill +4 and stress +2.
5. Continue the existing academy schedule/story to an ending. Save/load is available
   before, during and after this sequence.

The progression's generated actions and states are not copied into the authored
gameplay arrays: they are compiled on validation/export. Change the source package
or instance parameters to change them. The original `.raiserules` file is for reuse
in another project; this game's pinned copy lives in `project.json`.

The integration test uses real Qt widgets without displaying a window, then the shared
save service, hidden Windows player and headless Web/Wasm player. The basic scene and
action scaffold is test data; this is not an independent human authoring trial.
See [the contract and verification](../../docs/rule-extensions.md).
## 시각적 작성 예제 추가

`visual-authoring.raiserules`는 빈 패키지부터 실제 Qt 제작실 위젯으로 만든 뒤 파일 대화상자로 내보낸 규칙이다. 조건·효과·입력 기본값과 범위·준비/수련/돌파 진행·비용을 포함한다. 자동 생성된 식별자를 직접 입력할 필요는 없다.

디자인 → 규칙 확장 패키지 · 진행 방식에서 파일을 가져오고 진행 인스턴스를 추가한 뒤 행동 목록을 화면에 배치한다. 예제의 능력 참조는 이 프로젝트의 검술·소지금·스트레스를 사용한다.

자세한 제작·버전 갱신 절차와 시험 한계는 [제작실 문서](../../docs/rule-package-workshop.md)에 있다.
