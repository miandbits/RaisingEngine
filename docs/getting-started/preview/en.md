# Install and make your first game

## Install

1. Use the Windows x64 installer from the reviewed preview distribution. Compare its SHA-256 with the accompanying `.exe.json` or `SHA256SUMS.txt` before running it. Do not use an installer from an unknown mirror.
2. Select English, Korean or Japanese in the installer. Choose a **new** installation folder. This preview refuses to overwrite an existing folder; install upgrades side by side.
3. Run `RaisingStudioQt.exe` from that folder. Node.js and Qt SDKs are bundled; you do not need development tools. Windows may warn that this preview is unsigned. This preview has not been certified on a clean Windows VM.
4. In the project manager, choose **Editor language → English**. Close and reopen the editor to apply it. Inside the editor the same setting is at **View → 언어 / Language / 言語…**. This never changes a project's text or saved games.

## Make and play a small game

1. In **New project**, select **Exploration and combat**. Enter an empty folder outside the installation, such as `Documents/Raising Studio Projects/first-game`, and click **Create project**. This copies the sample; the original is preserved.
2. Open **File → Project settings…** and change the game title. Keep the initial calendar and stat ranges for this first test. Apply and save.
3. In the content search, find the activity `휴식` (rest). Select **Edit selected content…**, change its name and one numeric effect, then apply. IDs are stable connections: keep them unchanged for this first edit. Advanced tools and catalogued editor errors are translated. Sample names and story text remain designer content.
4. Press **Save project**, then **Validate**. Double-click an error to visit its source. Validation checks data and references; it does not prove every ending is reachable.
5. Select a screen and add a text or button element. Use **Inspector** to set its text, position and size. For a button, choose an action under **Action · On click** and a valid destination. Save and validate again.
6. Click **▶ Play from start**. Start the sample, choose activities, visit town and a shop, and follow dialogue. Save partway through using the game's save button or **F5** in the Windows player. Make another move, then load using the load button or **F9** and check the restored state. Saving the project and saving a play session are separate operations.

## Translate your game

Open **Design → Localization workspace…**, add `en` and `ja`, and enter translations for the selected messages. Set a default and fallback language. Blank translations use fallback text; changed source text needs review before an old translation is reused. `%1` and `{value}`-style placeholders must retain their meaning. Import a redistributable font with the required glyphs, then assign it to the language. The canvas language picker previews game content independently of the editor language. Save, validate and test both languages; the editor does not machine-translate your story.

## Export and test

1. Choose **File → Export game…**. Select Windows or Web, an existing parent folder and a **new** output folder name. Unsaved edits are included in the export but are not automatically saved back to the project.
2. After success, open the output folder. Windows: run `RaisingPlayer.exe`. Web: upload the **whole folder** to an HTTP(S) static host, including `wasm/`, fonts and assets. Opening `index.html` through `file://` is unsupported. For a local test with Python installed: `python -m http.server 8080 --directory "path/to/web-export"`, then open `http://localhost:8080`.
3. Repeat play, save and load in the exported game. On Web, download a save and restore it through the file input; keep a copy outside browser storage.
4. If export fails, read the error, correct the content and retry. Reopen the export dialog after changing the project. Existing output directories remain protected.

Keep an older project copy before content updates. Use **File → Content updates · Migrate / recover saves…** to inspect migration rather than assuming old saves remain compatible. Uninstall with `Uninstall.exe`; user projects outside the installation remain untouched. Mobile and console distribution are not validated by this Windows preview.

## Download and update

Use the versioned assets on [GitHub Releases](https://github.com/miandbits/RaisingEngine/releases): the Windows x64 `.exe` for installation, or `-portable.zip` to extract and run without installation. Both contain the runtime and third-party notices. Compare `SHA256SUMS.txt`. The installer adds a versioned Start menu shortcut and an entry in Windows Installed apps. Updates install alongside older previews; keep projects outside both folders. ZIP users run `RaisingStudioQt.exe` after extracting the entire archive. GitHub Packages is not needed to install this desktop application. Draft release assets are for maintainer review until publication checks pass.
