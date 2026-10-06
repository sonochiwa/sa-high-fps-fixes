# Runs the High FPS Fixes autotest and compares each measurement at a high
# frame rate with the same one at 30 FPS.
#
#   powershell -ExecutionPolicy Bypass -File tests\run.ps1 [-Scenarios timer,forklift]
#       [-Fps 30,300] [-Modes stock,fixed] [-Settings fireSpread=0,log=0]
#       [-TimeoutSeconds 240]
#
# Two test copies of the game, side by side in the folder given by -Copies or
# HFF_TEST_COPIES, hold an ASI loader, SilentPatch and Borderless Mode:
# "hff test stock" without the plugin and "hff test fixed" with
# build\HighFpsFixes.asi and Config\HighFpsFixes.ini (log=1), both deployed
# here. Each run starts one copy with HFF_AUTOTEST naming one scenario and
# HFF_AUTOTEST_FPS the virtual frame rate; build\HighFpsFixesTests.asi drives
# the game, writes its measurements and quits it. Results and plugin logs land
# in build\autotest\. The game is not started while another game or a
# full-screen application holds the screen.
param(
    [string[]]$Scenarios = @('timer', 'countdown', 'burning', 'explosion', 'teargas',
        'forklift', 'firetruck', 'parking', 'braking', 'cornering', 'sinking', 'hydraulics',
        'engine_revs', 'plane', 'plane_gentle', 'plane_damaged', 'heli', 'sniper', 'swim',
        'swim_speed'),
    [string[]]$Fps = @('30', '300'),
    [string[]]$Modes = @('stock', 'fixed'),
    [string]$Copies = $env:HFF_TEST_COPIES,
    [string]$Plugin = '',
    [string[]]$Settings = @(),
    [int]$TimeoutSeconds = 240,
    [double]$Tolerance = 0.1
)
$ErrorActionPreference = 'Stop'
if (-not $Copies) { throw 'Name the folder of the test copies with -Copies or HFF_TEST_COPIES.' }
# A list given on the command line arrives as one comma-separated string.
$Scenarios = @($Scenarios | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$Modes = @($Modes | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$Fps = @($Fps | ForEach-Object { $_ -split ',' } | Where-Object { $_ } | ForEach-Object { [int]$_ })
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root 'build'
if (-not $Plugin) { $Plugin = Join-Path $build 'HighFpsFixes.asi' }
$Plugin = (Resolve-Path $Plugin).Path
$out = Join-Path $build 'autotest'
New-Item -ItemType Directory -Force $out | Out-Null

Add-Type @'
using System.Runtime.InteropServices;
public static class HffUserState {
  [DllImport("shell32")] public static extern int SHQueryUserNotificationState(out int state);
}
'@

function Get-Copy([string]$mode) {
    $game = Join-Path $Copies "hff test $mode"
    return [pscustomobject]@{
        Game = $game
        Scripts = Join-Path $game 'scripts'
        Exe = Join-Path $game 'gta_sa.exe'
    }
}

function Get-TestGames {
    $paths = $Modes | ForEach-Object { (Get-Copy $_).Exe }
    Get-Process -Name gta_sa -ErrorAction SilentlyContinue | Where-Object { $paths -contains $_.Path }
}

# Why the game must not start now, or nothing. A test game that has just
# quit can leave the full-screen state set for a few seconds, so that state
# is waited out before it counts.
function Test-UserBusy {
    $others = Get-Process -Name gta_sa, gta-sa, gta3, gta-vc, gta5 -ErrorAction SilentlyContinue |
        Where-Object { $_.Id -notin @(Get-TestGames | ForEach-Object Id) }
    if ($others) { return 'another game is running' }
    for ($i = 0; $i -lt 30; $i++) {
        $state = 0
        [void][HffUserState]::SHQueryUserNotificationState([ref]$state)
        if ($state -notin 2, 3, 4) { return $null }
        Start-Sleep -Seconds 1
    }
    return 'a full-screen application is running'
}

function Stop-TestGames {
    Get-TestGames | ForEach-Object { Stop-Process -Id $_.Id -Force; $_.WaitForExit(10000) | Out-Null }
}

function Install-Builds([string]$mode) {
    $copy = Get-Copy $mode
    Copy-Item (Join-Path $build 'HighFpsFixesTests.asi') $copy.Scripts -Force
    $installed = Join-Path $copy.Scripts 'HighFpsFixes.asi'
    $ini = Join-Path $copy.Scripts 'HighFpsFixes.ini'
    Remove-Item $installed, $ini, (Join-Path $copy.Scripts 'HighFpsFixes.log') -ErrorAction SilentlyContinue
    if ($mode -eq 'fixed') {
        Copy-Item $Plugin $installed
        $text = [IO.File]::ReadAllText((Join-Path $root 'Config\HighFpsFixes.ini'))
        foreach ($setting in @('log=1') + $Settings) {
            $key, $value = $setting -split '=', 2
            $pattern = "(?m)^$([regex]::Escape($key))=[^\r\n]*"
            if (-not [regex]::IsMatch($text, $pattern)) { throw "Unknown setting $key." }
            $text = [regex]::Replace($text, $pattern, "$key=$value")
        }
        [IO.File]::WriteAllText($ini, $text)
    }
}

# One game run; returns the lines the autotest wrote.
function Invoke-Run([string]$scenario, [int]$fps, [string]$mode) {
    $copy = Get-Copy $mode
    $results = Join-Path $copy.Scripts 'HighFpsFixesTests.txt'
    Remove-Item $results -ErrorAction SilentlyContinue
    $env:HFF_AUTOTEST = $scenario
    $env:HFF_AUTOTEST_FPS = "$fps"
    $process = Start-Process -FilePath $copy.Exe -WorkingDirectory $copy.Game -PassThru
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $finished = $false
    while (-not $process.HasExited -and $watch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        if ((Test-Path $results) -and (Select-String -Path $results -Pattern '^# finished' -Quiet)) {
            $finished = $true
            break
        }
        Start-Sleep -Milliseconds 500
    }
    if (-not $process.WaitForExit(20000)) { Stop-TestGames }
    # The game can write its last line and quit between two polls.
    $finished = $finished -or ((Test-Path $results) -and
        (Select-String -Path $results -Pattern '^# finished' -Quiet))
    Remove-Item Env:HFF_AUTOTEST, Env:HFF_AUTOTEST_FPS -ErrorAction SilentlyContinue
    $name = "$scenario-$mode-$fps"
    $lines = @()
    if (Test-Path $results) {
        Copy-Item $results (Join-Path $out "$name.txt") -Force
        $lines = @(Get-Content $results)
    }
    $log = Join-Path $copy.Scripts 'HighFpsFixes.log'
    if (Test-Path $log) { Copy-Item $log (Join-Path $out "$name.log") -Force }
    if (-not $finished) {
        $why = if ($process.HasExited) { "exited with code $($process.ExitCode)" } else { 'timed out' }
        Write-Host "  run failed: $why"
        $lines += "$scenario run_failed 1"
    }
    return $lines
}

$busy = Test-UserBusy
if ($busy) { Write-Host "Not started: $busy."; exit 2 }
Stop-TestGames

$table = @{}
foreach ($mode in $Modes) {
    Install-Builds $mode
    foreach ($scenario in $Scenarios) {
        foreach ($rate in $Fps) {
            $busy = Test-UserBusy
            if ($busy) { Write-Host "Stopped: $busy."; exit 2 }
            Write-Host ("{0,-13} {1,-6} {2,4} FPS" -f $scenario, $mode, $rate)
            foreach ($line in Invoke-Run $scenario $rate $mode) {
                if ($line -match '^([^#\s]\S*) (\S+) (-?[0-9.]+)') {
                    $key = "$($Matches[1]) $($Matches[2])"
                    if (-not $table.ContainsKey($key)) { $table[$key] = @{} }
                    $table[$key]["$mode $rate"] = [double]$Matches[3]
                }
            }
        }
    }
}

# A measurement passes when the plugin gives the same value at every frame
# rate as at the lowest one, within the tolerance.
$base = $Fps[0]
$failed = 0
$report = foreach ($key in ($table.Keys | Sort-Object)) {
    $row = [ordered]@{ Measurement = $key }
    foreach ($mode in $Modes) {
        foreach ($rate in $Fps) {
            $value = $table[$key]["$mode $rate"]
            $row["$mode $rate"] = if ($null -eq $value) { '-' } else { '{0:0.###}' -f $value }
        }
    }
    $verdict = '-'
    if ($Modes -contains 'fixed') {
        $reference = $table[$key]["fixed $base"]
        $verdict = 'ok'
        foreach ($rate in $Fps) {
            $value = $table[$key]["fixed $rate"]
            if ($null -eq $value -or $null -eq $reference) { $verdict = 'missing'; break }
            $allowed = [Math]::Max([Math]::Abs($reference) * $Tolerance, 0.05)
            if ([Math]::Abs($value - $reference) -gt $allowed) { $verdict = 'DIFFERS'; break }
        }
        if ($verdict -ne 'ok') { $failed++ }
    }
    $row['Verdict'] = $verdict
    [pscustomobject]$row
}
$text = $report | Format-Table -AutoSize | Out-String -Width 200
$text
[IO.File]::WriteAllText((Join-Path $out 'report.txt'), $text)
exit ([int]($failed -gt 0))
