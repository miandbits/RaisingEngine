param(
  [Parameter(Mandatory)][string]$Editor,
  [Parameter(Mandatory)][string]$Evidence,
  [Parameter(Mandatory)][ValidateSet('en','ko')][string]$Locale
)

$ErrorActionPreference = 'Stop'
$env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
$env:QT_QPA_PLATFORM = 'windows'
$env:RAISING_PROJECT_HISTORY = Join-Path $Evidence 'launcher-history.ini'

Add-Type @'
using System;
using System.ComponentModel;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
public static class RaisingWindow {
  private delegate bool Callback(IntPtr handle, IntPtr data);
  [DllImport("user32.dll")] private static extern bool EnumWindows(Callback callback, IntPtr data);
  [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr handle);
  [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr handle, out uint process);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr handle, StringBuilder title, int capacity);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr handle, uint message, IntPtr wParam, IntPtr lParam);
  [DllImport("kernel32.dll", SetLastError=true)] private static extern bool GetExitCodeProcess(IntPtr handle, out uint exitCode);
  public static uint ExitCode(IntPtr handle) {
    uint code;
    if (!GetExitCodeProcess(handle, out code)) throw new Win32Exception(Marshal.GetLastWin32Error());
    return code;
  }
  public static string FileSha256(string file) {
    using (var input = File.OpenRead(file))
    using (var sha256 = SHA256.Create())
      return BitConverter.ToString(sha256.ComputeHash(input)).Replace("-", "").ToLowerInvariant();
  }
  public static IntPtr Find(uint process, string expected) {
    IntPtr window = IntPtr.Zero;
    EnumWindows((handle, data) => {
      uint owner;
      GetWindowThreadProcessId(handle, out owner);
      if (owner != process || !IsWindowVisible(handle)) return true;
      var title = new StringBuilder(512);
      GetWindowText(handle, title, title.Capacity);
      if (title.ToString() != expected) return true;
      window = handle;
      return false;
    }, IntPtr.Zero);
    return window;
  }
}
'@

# Keep the PowerShell 5.1 script ASCII and derive the two localized titles
# from the same source catalog used by the packaged launcher.
$catalog = Get-Content (Join-Path $PSScriptRoot '../native/studio/translations/editor.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$title = $null
foreach ($row in $catalog) {
  if ($row[1] -ceq ("Raising Studio " + [char]0x2014 + " Projects")) {
    $title = if ($Locale -eq 'en') { $row[1] } else { $row[0] }
    break
  }
}
if (!$title) { throw 'Missing project launcher title in the editor catalog' }

$app = Start-Process -FilePath $Editor -WorkingDirectory $Evidence -ArgumentList '--ui-language',$Locale -PassThru -RedirectStandardError (Join-Path $Evidence 'windows-launch-stderr.txt')
try {
  # Windows PowerShell Start-Process with stderr redirection can lose ExitCode
  # unless the process handle is retained while the child is still running.
  $handle = $app.Handle
  $clock = [System.Diagnostics.Stopwatch]::StartNew()
  do {
    if ($app.HasExited) { throw "Installed editor exited during startup ($([RaisingWindow]::ExitCode($handle)))" }
    $window = [RaisingWindow]::Find($app.Id, $title)
    if ($window -ne [IntPtr]::Zero) { break }
    Start-Sleep -Milliseconds 150
  } while ($clock.Elapsed.TotalSeconds -lt 30)
  if ($window -eq [IntPtr]::Zero) { throw "No visible project launcher in locale $Locale" }
  if (![RaisingWindow]::PostMessage($window, 0x10, [IntPtr]::Zero, [IntPtr]::Zero)) { throw 'Cannot close project launcher' }
  # StudioBridge::shutdown allows 10 seconds for EOF and 1.5 for cleanup.
  $closing = [System.Diagnostics.Stopwatch]::StartNew()
  if (!$app.WaitForExit(20000)) { throw 'Installed editor did not close within 20 seconds' }
  $exitCode = [RaisingWindow]::ExitCode($handle)
  if ($exitCode -ne 0) { throw "Installed editor closed with exit code $exitCode" }
  $receipt = @{
    ok = $true
    locale = $Locale
    title = $title
    editorSha256 = [RaisingWindow]::FileSha256($Editor)
    qpaPlatform = $env:QT_QPA_PLATFORM
    systemRuntimePathsRemoved = $true
    closeElapsedMilliseconds = $closing.ElapsedMilliseconds
    exitCode = $exitCode
  } | ConvertTo-Json
  [IO.File]::WriteAllText([IO.Path]::Combine($Evidence,'windows-launch.json'), $receipt)
} finally {
  if (!$app.HasExited) { Stop-Process -Id $app.Id -Force }
}
