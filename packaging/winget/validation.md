# Preview WinGet lifecycle validation

Validated 2026-10-19 for `miandbits.RaisingEngine.Preview` version `0.1.0-preview.3`.

- [Successful workflow and downloadable evidence](https://github.com/miandbits/RaisingEngine/actions/runs/39338285631)
- Engine repository test commit: `bcdc790`
- Windows Server 2025 Datacenter, build 26100; official WinGet 1.29.380.
- Public portable archive SHA-256: `a2e52293cdd1f1b96986b09ca430ee837e97e27aea58cb6041f8db5568ad7756`.

| Check | Result |
|---|---|
| `winget validate --manifest` | Passed without warnings |
| WinGet download, hash check, ZIP/portable installation | Passed |
| Custom installation path containing spaces | Passed |
| Installed-app registration and version | Matched |
| Actual executable directory added to user PATH | Passed |
| `RaisingStudioQt` command resolves to installed executable | Passed |
| Real editor startup from an unrelated working directory | `Raising Studio — Projects` window initialized |
| Node/Qt SDK directories absent from test process PATH | Passed; bundled runtime used |
| Normal editor window close | Exit code 0 |
| `winget uninstall --manifest` | Passed |
| Executable, installed-app record and command PATH removed | Passed |
| External save-file sentinel preserved | Passed |
| VM-only LocalManifestFiles setting restored | Disabled afterward |

The evidence artifact contains command logs, the lifecycle receipt and restored WinGet settings. Receipt fields containing `Alias` refer to the manifest's command entry; the final package uses a real directory on PATH, not a symlink.

Earlier NSIS local-manifest tests stalled before installer startup with SmartScreen active in the headless VM. The initial ZIP attempt installed successfully but its executable symlink could not locate Qt DLLs. The final manifest uses `ArchiveBinariesDependOnPath: true`, and the entire lifecycle above passed. No SmartScreen, antivirus or installer-hash override was used. No full desktop screenshots were collected or uploaded.

This verifies the submitted local manifest and actual public archive. Installation by package ID from the community source still requires PR acceptance and source indexing. Start menu shortcuts are provided by the separate EXE installer, not this portable WinGet package. User projects/saves must be kept outside the WinGet-owned installation folder; preservation of arbitrary files inside that folder is not promised. Windows 10/11 device-policy behavior is not inferred from this Server VM test.
