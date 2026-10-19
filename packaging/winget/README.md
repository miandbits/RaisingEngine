# WinGet preview package

Package: `miandbits.RaisingEngine.Preview`, version `0.1.0-preview.3`.

WinGet manages the public unsigned Windows x64 portable ZIP, its installed-app entry, and the `RaisingStudioQt` launch command. All editor resources stay together inside the archive layout. This preview has a separate identifier from any future stable release. The EXE installer remains available as a direct download for users who prefer Start menu shortcuts.

## Availability

Submitted as [microsoft/winget-pkgs#456322](https://github.com/microsoft/winget-pkgs/pull/456322). Approval and source indexing are pending. Until then, use the [installer or portable ZIP](https://github.com/miandbits/RaisingEngine/releases/tag/v0.1.0-preview.3).

[Installation, real editor startup and uninstall passed](https://github.com/miandbits/RaisingEngine/actions/runs/39338285631) in an isolated Windows VM. See the [validation record](validation.md) for scope and limitations.

After registration is available, run in a normal user terminal:

```powershell
winget install --id miandbits.RaisingEngine.Preview --exact --source winget
```

Open a new terminal so it picks up the WinGet command path, then launch:

```powershell
RaisingStudioQt
```

```powershell
winget list --id miandbits.RaisingEngine.Preview --exact
winget uninstall --id miandbits.RaisingEngine.Preview --exact
```

Store game projects and saves **outside the installation folder**. WinGet owns the portable package folder and may remove it during uninstall or upgrade. This package does not create Start menu shortcuts. Windows publisher warnings or device-policy restrictions can still apply to the unsigned editor.

## Maintainer checks

The submitted set is under `manifests/m/miandbits/RaisingEngine/Preview/0.1.0-preview.3/`: installer, English default locale, Korean/Japanese locales and version. No binary is stored in the community repository.

```powershell
winget validate --manifest packaging/winget/manifests/m/miandbits/RaisingEngine/Preview/0.1.0-preview.3
```

The manual `WinGet preview lifecycle` workflow uses an isolated Windows Server 2025 VM, pinned official WinGet 1.29.380, and read-only repository permissions. It validates the manifest, installs to a path containing spaces, checks the registered version and launch command, starts the actual Qt editor through the installed command from an unrelated working folder with system Node/Qt paths removed, verifies its window and normal exit, uninstalls via WinGet, and checks removal and external save preservation. Local-manifest testing is enabled only in this disposable VM and restored afterward. Each install/uninstall command has a three-minute limit with scoped diagnostic logs on failure. The user's desktop settings are untouched.

The initial NSIS-manifest test stalled before installer startup with SmartScreen active in the headless VM, on both WinGet 1.11 and 1.29. No security policy was disabled. The final package uses WinGet's standard ZIP/portable handler instead. NSIS direct-download testing is separate from this WinGet lifecycle check.

The manifest uses `ArchiveBinariesDependOnPath: true` because launching the Qt executable through a symlink breaks DLL discovery. WinGet adds the actual editor folder to the user PATH and removes it on uninstall.

Schema 1.10 contains all required fields. Future releases need their own version, exact public archive hash and fresh lifecycle verification. Never overwrite a published download behind an existing manifest hash.
