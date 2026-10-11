# Starts the original Catalyst mission in a fresh test game for manual play.
param(
    [string]$Copies = $env:HFF_TEST_COPIES,
    [ValidateRange(0, 300)][int]$Fps = 300,
    [switch]$WithoutFix,
    [switch]$Stock
)
$ErrorActionPreference = 'Stop'
if (-not $Copies) { throw 'Name the folder of the test copies with -Copies or HFF_TEST_COPIES.' }
if (Get-Process gta_sa, gta-sa, gta3, gta-vc, gta5 -ErrorAction SilentlyContinue) {
    throw 'Another game is running.'
}
Add-Type @'
using System.Runtime.InteropServices;
public static class HffPreviewUserState {
    [DllImport("shell32")] public static extern int SHQueryUserNotificationState(out int state);
    [DllImport("user32")] public static extern bool ShowWindowAsync(System.IntPtr window, int command);
    [DllImport("user32")] public static extern bool SetForegroundWindow(System.IntPtr window);
}
'@
$state = 0
[void][HffPreviewUserState]::SHQueryUserNotificationState([ref]$state)
if ($state -in 2, 3, 4) { throw 'A full-screen application is running.' }
$root = Split-Path -Parent $PSScriptRoot
$copyName = if ($Stock) { 'hff test stock' } else { 'hff test fixed' }
if ($Stock -and $Fps -ne 30) { throw 'The stock preview uses the original 30 FPS limiter; specify -Fps 30.' }
$game = (Resolve-Path (Join-Path $Copies $copyName)).Path
$scripts = Join-Path $game 'scripts'
if ($Stock) {
    if (Test-Path (Join-Path $scripts 'HighFpsFixes.asi')) {
        throw 'The stock copy contains HighFpsFixes.asi; remove it before the stock preview.'
    }
} else {
    Copy-Item (Join-Path $root 'build\HighFpsFixes.asi') $scripts -Force
}
Copy-Item (Join-Path $root 'build\HighFpsFixesTests.asi') $scripts -Force
$ini = [IO.File]::ReadAllText((Join-Path $root 'Config\HighFpsFixes.ini'))
$missionFix = if ($WithoutFix) { 0 } else { 1 }
$ini = [regex]::Replace($ini, '(?m)^missionScripts=[^\r\n]*', "missionScripts=$missionFix")
$ini = [regex]::Replace($ini, '(?m)^fpsLimit=[^\r\n]*', "fpsLimit=$Fps")
if ($Fps -eq 0) {
    $ini = [regex]::Replace($ini, '(?m)^(forSchools|forMissions|forMinigames|forCutscenes|forScriptedCutscenes|forPauseMenu)=[^\r\n]*', '$1=0')
}
$ini = [regex]::Replace($ini, '(?m)^log=[^\r\n]*', 'log=1')
if (-not $Stock) { [IO.File]::WriteAllText((Join-Path $scripts 'HighFpsFixes.ini'), $ini) }
$env:HFF_AUTOTEST = 'catalyst_mission'
$env:HFF_AUTOTEST_FPS = '30'
$env:HFF_AUTOTEST_INTERACTIVE = '1'
$env:HFF_AUTOTEST_STOCK = if ($Stock) { '1' } else { '0' }
try {
    $process = Start-Process -FilePath (Join-Path $game 'gta_sa.exe') -WorkingDirectory $game -WindowStyle Normal -PassThru
} finally {
    Remove-Item Env:HFF_AUTOTEST, Env:HFF_AUTOTEST_FPS, Env:HFF_AUTOTEST_INTERACTIVE, Env:HFF_AUTOTEST_STOCK
}
for ($i = 0; $i -lt 30; ++$i) {
    $process.Refresh()
    if ($process.HasExited) { throw "Test game exited with code $($process.ExitCode)." }
    if ($process.MainWindowHandle -ne [IntPtr]::Zero) { break }
    Start-Sleep -Seconds 1
}
if ($process.MainWindowHandle -eq [IntPtr]::Zero) {
    throw 'The test game did not create a window within 30 seconds. Check its scripts folder logs.'
}
[void][HffPreviewUserState]::ShowWindowAsync($process.MainWindowHandle, 9)
[void][HffPreviewUserState]::SetForegroundWindow($process.MainWindowHandle)
$fixState = if ($Stock) { 'High FPS Fixes absent' } else { "missionScripts=$missionFix" }
Write-Host "Catalyst launched: $Fps FPS, $fixState. Close the game before another run."
