param([string]$Manifest = 'packaging/winget/manifests/m/miandbits/RaisingEngine/Preview/0.1.0-preview.3')
$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -ne 'true') { throw 'Run this lifecycle test in its disposable GitHub Windows VM.' }
$evidence = Join-Path $env:GITHUB_WORKSPACE '.cache/winget-validation'
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$install = Join-Path $env:RUNNER_TEMP 'RaisingEngine Winget Preview'
$registry = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\miandbits.RaisingEngine.Preview'
$alias = Join-Path $env:LOCALAPPDATA 'Microsoft/WinGet/Links/raisingstudio-preview.exe'
if ((Test-Path -LiteralPath $install) -or (Test-Path -LiteralPath $alias)) { throw 'Refusing to replace an existing installation.' }
$before = winget settings export | ConvertFrom-Json
$localWasEnabled = $before.adminSettings.LocalManifestFiles
$app = $null
$success = $false
Add-Type @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class WinGetWindows {
  public delegate bool Callback(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] static extern bool EnumWindows(Callback callback, IntPtr p);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint process);
  public static IntPtr[] Handles(uint process) {
    var handles = new List<IntPtr>();
    EnumWindows((h,p) => { uint id; GetWindowThreadProcessId(h,out id); if(id==process)handles.Add(h); return true; },IntPtr.Zero);
    return handles.ToArray();
  }
}
'@
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
    Add-Type -AssemblyName UIAutomationClient
    Add-Type -AssemblyName UIAutomationTypes
    $dialogDetails = foreach ($windowProcess in (Get-Process | Where-Object { $_.ProcessName -match 'winget|RaisingEngine|Uninstall|smartscreen' })) {
      foreach ($handle in [WinGetWindows]::Handles($windowProcess.Id)) {
        $root = [System.Windows.Automation.AutomationElement]::FromHandle($handle)
        $elements = $root.FindAll([System.Windows.Automation.TreeScope]::Subtree, [System.Windows.Automation.Condition]::TrueCondition)
        foreach ($element in $elements) { @{process=$windowProcess.ProcessName;name=$element.Current.Name;type=$element.Current.ControlType.ProgrammaticName;id=$element.Current.AutomationId} }
      }
    }
    $dialogDetails | ConvertTo-Json | Set-Content "$evidence/$Name-dialogs.json"
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
  $entries = @(Get-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*' | Where-Object { $_.DisplayName -eq 'RaisingEngine Preview' })
  if ($entries.Count -ne 1 -or $entries[0].DisplayVersion -ne '0.1.0-preview.3') { throw 'Installed app registration mismatch' }
  $registry = $entries[0].PSPath
  $exe = Join-Path $install 'RaisingStudio/RaisingStudioQt.exe'
  if (!(Test-Path -LiteralPath $exe)) { throw 'Editor executable missing' }
  if (!(Test-Path -LiteralPath $alias)) { throw 'WinGet command alias missing' }
  $oldPath = $env:PATH
  try {
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    $app = Start-Process -FilePath $alias -WorkingDirectory $env:RUNNER_TEMP -ArgumentList '--ui-language','en' -WindowStyle Hidden -PassThru -RedirectStandardError "$evidence/editor-stderr.log"
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
  # Portable installations are owned by WinGet. User projects must live outside them.
  $external = Join-Path $env:RUNNER_TEMP 'RaisingEngine user data'
  New-Item -ItemType Directory -Path $external | Out-Null
  Set-Content -LiteralPath (Join-Path $external 'save.json') -Value '{"sentinel":"preserve"}'
  Invoke-WinGetStep 'uninstall' @('uninstall','--manifest',$Manifest,'--silent','--accept-source-agreements','--disable-interactivity','--verbose-logs')
  if ((Test-Path -LiteralPath $exe) -or (Test-Path -LiteralPath $registry) -or (Test-Path -LiteralPath $alias)) { throw 'Installed app remains after uninstall' }
  if ((Get-Content -LiteralPath (Join-Path $external 'save.json') -Raw).Trim() -ne '{"sentinel":"preserve"}') { throw 'Uninstall changed external save' }
  $success = $true
  @{ok=$true;package='miandbits.RaisingEngine.Preview';version='0.1.0-preview.3';winget=(winget --version);installerType='zip/portable';installedViaWinget=$true;customLocation=$install;registryMatched=$true;commandAliasPresent=$true;editorLaunchedViaAlias=$true;editorWindowTitles=$titles;editorNormalExit=$true;systemRuntimePathsRemoved=$true;uninstalledViaWinget=$true;appRegistryAliasRemoved=$true;externalUserFilesPreserved=$true;environment=(Get-ComputerInfo | Select-Object WindowsProductName,OsBuildNumber)} | ConvertTo-Json -Depth 5 | Set-Content "$evidence/result.json"
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
