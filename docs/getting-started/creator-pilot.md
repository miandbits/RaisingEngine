# Raising Studio 제작자 파일럿 / Creator pilot

## 한국어

**도움말 → Raising Studio 정보…**에서 배포 버전 `0.1.0-preview.4`와 제공받은 파일의 SHA-256을 확인합니다. Windows x64는 설치 프로그램으로 새 폴더에 설치하거나 portable ZIP 전체를 풉니다. Windows 설치 프로그램에는 코드 서명이 없습니다. macOS는 Apple Silicon용 아카이브를 풀어 `RaisingStudio.app`을 실행합니다. macOS 후보는 ad-hoc 서명 개발 빌드이며 공증을 받지 않았습니다. 기기 정책이 실행을 차단하면 지원 담당자에게 알려주세요. 프로젝트와 플레이 저장은 설치 폴더 밖에 보관합니다.

1. 프로젝트 선택기에서 편집기 언어를 고른 후 **My first raising game · recommended** 스타터를 새 프로젝트 폴더에 만듭니다. 체크리스트에서 처음부터 플레이합니다.
2. **디자인 → 활동 · 조건 · 연출 함께 편집…**을 엽니다. 활동 하나의 이름과 능력 보상 하나를 바꾸고 적용합니다. 저장한 프로젝트를 다시 열어 두 변경을 확인합니다.
3. **디자인 → 육성 게임 화면 흐름 만들기…**에서 거처·계획·연출·성장 화면을 미리 보고 적용합니다. 권한이 있는 이미지나 공개 예제 이미지를 한 장 가져옵니다. 변경 후 게임을 끝까지 플레이합니다. 필요하면 기록한 경로를 같은 입력으로 다시 실행하여 수정 전후를 비교합니다.
4. **파일 → 릴리스 센터…**에서 게임을 내보냅니다. Windows에서는 Windows와 Web, macOS에서는 macOS와 Web을 선택합니다. 새 출력 폴더를 지정하고 게임 ZIP과 편집 가능한 프로젝트 백업을 생성합니다. 게임 버전에는 예를 들어 `0.1.0`을 사용합니다. 내보낸 게임을 별도 프로세스에서 실행하고, 플레이를 저장한 후 게임을 종료하고 다시 열어 불러옵니다. Web은 **내보낸 게임 플레이**를 사용해 로컬 HTTP 주소에서 확인합니다.
5. `Project-backup.zip`을 원래 프로젝트와 다른 새 폴더에 복구합니다. 복구본을 열어 바꾼 활동과 가져온 이미지가 있는지 확인합니다. `Game.zip`은 플레이용이고 `Project-backup.zip`은 제작용입니다.

## English

Check distribution version `0.1.0-preview.4` in **Help → About Raising Studio…** and verify the supplied archive or installer against its SHA-256 checksum. On Windows x64, install to a fresh folder or extract the entire portable ZIP. The Windows installer is unsigned. On an Apple Silicon Mac, extract the archive and open `RaisingStudio.app`. The Mac candidate is an ad-hoc signed development build without notarization. Tell your support contact if your device policy blocks launch. Keep projects and play saves outside the installation folder.

1. Choose the editor language in the project picker. Select **My first raising game · recommended**, create it in a fresh project folder, and play from the beginning using the checklist.
2. Open **Design → Edit activity, conditions and presentation together…**. Rename an activity and change one stat reward. Apply, save, and reopen the project to check both changes.
3. Open **Design → Create raising game screen flow…**, then preview and apply the flow for home, planning, presentation, and growth. Import an image you have permission to use or a public sample image. Play to an ending. To compare versions, replay the same inputs in the route tools.
4. Open **File → Release center…** and export your game. Use Windows and Web on Windows, or macOS and Web on a Mac. Choose a fresh output folder and create the game ZIP and editable project backup. Use a game version such as `0.1.0`. Run the exported game, save during play, close it, then relaunch and load. Use **Play exported game** to open the Web export at a local HTTP address.
5. Restore `Project-backup.zip` to a fresh folder outside the original project. Open the copy and confirm the activity edits and imported image. Use `Game.zip` for play and `Project-backup.zip` to resume editing.
