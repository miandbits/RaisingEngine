param([string]$Manifest = 'packaging/winget/manifests/m/miandbits/RaisingEngine/Preview/0.1.0-preview.3')
$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -ne 'true') { throw 'Run this lifecycle test in its disposable GitHub Windows VM.' }
$evidence = Join-Path $env:GITHUB_WORKSPACE '.cache/winget-validation'
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$install = Join-Path $env:RUNNER_TEMP 'RaisingEngine Winget Preview'
$registry = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\RaisingEngine-0.1.0-preview.3'
if ((Test-Path -LiteralPath $install) -or (Test-Path -LiteralPath $registry)) { throw 'Refusing to replace an existing installation.' }
$before = winget settings export | ConvertFrom-Json
$localWasEnabled = $before.adminSettings.LocalManifestFiles
$app = $null
$success = $false
function Invoke-WinGetStep([string]$Name, [string[]]$Arguments) {
  Write-Host "Starting WinGet $Name (180 second limit)"
  $start = [Diagnostics.ProcessStartInfo]::new((Get-Command winget).Source)
  $start.UseShellExecute = $false
  $start.CreateNoWindow = $true
  $start.RedirectStandardOutput = $true
  $start.RedirectStandardError = $true
  foreach ($argument in $Arguments) { $start.ArgumentList.Add($argument) }
  $process = [Diagnostics.Process]::Start($start)
  $stdout = $process.StandardOutput.ReadToEndAsync()
  $stderr = $process.StandardError.ReadToEndAsync()
  $finished = $process.WaitForExit(180000)
  if (!$finished) {
    Get-CimInstance Win32_Process | Where-Object { $_.Name -match '^(winget|RaisingEngine.*|Uninstall)\.exe$' } |
      Select-Object Name,ProcessId,ParentProcessId,CommandLine | ConvertTo-Json | Set-Content "$evidence/$Name-processes.json"
    Get-Process | Where-Object { $_.ProcessName -match 'winget|RaisingEngine|Uninstall' } |
      Select-Object Id,ProcessName,MainWindowTitle,Responding | ConvertTo-Json | Set-Content "$evidence/$Name-windows.json"
    if (Test-Path -LiteralPath $registry) { Get-ItemProperty -LiteralPath $registry | ConvertTo-Json | Set-Content "$evidence/$Name-registry.json" }
    if (Test-Path -LiteralPath $install) { Get-ChildItem -LiteralPath $install | Select-Object Name,Length | ConvertTo-Json | Set-Content "$evidence/$Name-files.json" }
    & taskkill /PID $process.Id /T /F | Out-Null
    $process.WaitForExit(10000) | Out-Null
  }
  $stdout.GetAwaiter().GetResult() | Set-Content "$evidence/$Name.log"
  $stderr.GetAwaiter().GetResult() | Set-Content "$evidence/$Name-stderr.log"
  if (!$finished) { throw "WinGet $Name timed out; process and diagnostic logs retained" }
  if ($process.ExitCode -ne 0) { throw "WinGet $Name failed: $($process.ExitCode); see $Name.log" }
  Write-Host "WinGet $Name passed"
}
try {
  winget settings --enable LocalManifestFiles
  if ($LASTEXITCODE -ne 0) { throw 'Cannot enable local manifest testing in VM' }
  winget validate --manifest $Manifest --disable-interactivity 2>&1 | Tee-Object "$evidence/validate.log"
  if ($LASTEXITCODE -ne 0) { throw 'Manifest validation failed' }
  Invoke-WinGetStep 'install' @('install','--manifest',$Manifest,'--silent','--scope','user','--location',$install,'--accept-package-agreements','--accept-source-agreements','--disable-interactivity','--verbose-logs')
  $entry = Get-ItemProperty -LiteralPath $registry
  if ($entry.InstallLocation -ne $install -or $entry.DisplayVersion -ne '0.1.0-preview.3') { throw 'Installed app registration mismatch' }
  $exe = Join-Path $install 'RaisingStudioQt.exe'
  if (!(Test-Path -LiteralPath $exe)) { throw 'Editor executable missing' }
  $shortcut = Join-Path ([Environment]::GetFolderPath('Programs')) 'RaisingEngine 0.1.0-preview.3\Raising Studio.lnk'
  if (!(Test-Path -LiteralPath $shortcut)) { throw 'Start menu shortcut missing' }
  $oldPath = $env:PATH
  try {
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    $app = Start-Process -FilePath $exe -WorkingDirectory $install -ArgumentList '--ui-language','en' -WindowStyle Hidden -PassThru -RedirectStandardError "$evidence/editor-stderr.log"
    if (!$app.WaitForInputIdle(15000)) { throw 'Editor did not initialize its message loop' }
    if ($app.WaitForExit(3000)) { throw "Editor exited at startup: $($app.ExitCode)" }
    Add-Type @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class EditorWindows {
  public delegate bool Callback(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] static extern bool EnumWindows(Callback callback, IntPtr p);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint process);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetWindowText(IntPtr h, StringBuilder text, int size);
  [DllImport("user32.dll")] static extern bool PostMessage(IntPtr h, uint message, IntPtr w, IntPtr l);
  public static string[] Titles(uint process, bool close) {
    var titles = new List<string>();
    EnumWindows((h,p) => { uint id; GetWindowThreadProcessId(h,out id); if(id==process){var b=new StringBuilder(512); GetWindowText(h,b,512); if(b.Length>0){titles.Add(b.ToString()); if(close)PostMessage(h,0x10,IntPtr.Zero,IntPtr.Zero);}} return true; },IntPtr.Zero);
    return titles.ToArray();
  }
}
'@
    $titles = [EditorWindows]::Titles($app.Id, $false)
    if (!($titles -match 'Raising')) { throw 'No initialized Raising Studio window found' }
    [EditorWindows]::Titles($app.Id, $true) | Out-Null
    if (!$app.WaitForExit(10000)) { throw 'Editor did not close normally' }
    if ($app.ExitCode -ne 0) { throw "Editor close exit code: $($app.ExitCode)" }
  } finally { $env:PATH = $oldPath }
  # Prove uninstall keeps both unknown in-folder files and external project/save data.
  $sentinel = Join-Path $install 'user-preservation-check.txt'
  Set-Content -LiteralPath $sentinel -Value 'preserve this user file'
  $external = Join-Path $env:RUNNER_TEMP 'RaisingEngine user data'
  New-Item -ItemType Directory -Path $external | Out-Null
  Set-Content -LiteralPath (Join-Path $external 'save.json') -Value '{"sentinel":"preserve"}'
  Invoke-WinGetStep 'uninstall' @('uninstall','--manifest',$Manifest,'--silent','--accept-source-agreements','--disable-interactivity','--verbose-logs')
  if ((Test-Path -LiteralPath $exe) -or (Test-Path -LiteralPath $registry) -or (Test-Path -LiteralPath $shortcut)) { throw 'Installed app remains after uninstall' }
  if ((Get-Content -LiteralPath $sentinel -Raw).Trim() -ne 'preserve this user file') { throw 'Uninstall changed user file' }
  if ((Get-Content -LiteralPath (Join-Path $external 'save.json') -Raw).Trim() -ne '{"sentinel":"preserve"}') { throw 'Uninstall changed external save' }
  $success = $true
  @{ok=$true;package='miandbits.RaisingEngine.Preview';version='0.1.0-preview.3';winget=(winget --version);installedViaWinget=$true;customLocation=$install;registryMatched=$true;startMenuPresent=$true;editorWindowTitles=$titles;editorNormalExit=$true;systemRuntimePathsRemoved=$true;uninstalledViaWinget=$true;appRegistryShortcutRemoved=$true;userFilesPreserved=$true;environment=(Get-ComputerInfo | Select-Object WindowsProductName,OsBuildNumber)} | ConvertTo-Json -Depth 5 | Set-Content "$evidence/result.json"
} finally {
  $diagnostics = Join-Path $env:LOCALAPPDATA 'Packages/Microsoft.DesktopAppInstaller_8wekyb3d8bbwe/LocalState/DiagOutputDir'
  if (Test-Path -LiteralPath $diagnostics) { Copy-Item -Path "$diagnostics/WinGet-*.log" -Destination $evidence -ErrorAction Continue }
  if ($app -and !$app.HasExited) { Stop-Process -Id $app.Id -ErrorAction SilentlyContinue }
  if (!$localWasEnabled) {
    winget settings --disable LocalManifestFiles
    if ($LASTEXITCODE -ne 0) { throw 'Failed restoring local manifest setting' }
  }
  winget settings export | Set-Content "$evidence/settings-after.json"
  if (!$success) { Write-Warning 'Lifecycle failed; disposable VM evidence retained.' }
}
