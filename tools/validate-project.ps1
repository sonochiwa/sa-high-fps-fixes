$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path $PSScriptRoot -Parent
$configPath = Join-Path $projectRoot 'Config\HighFpsFixes.ini'
$header = Get-Content -LiteralPath $configPath -TotalCount 1
if ($header -notmatch '^# High FPS Fixes v(?<version>\d+\.\d+\.\d+)$') {
    throw "Invalid version header in $configPath"
}
$version = $Matches.version

$versionHeader = [IO.File]::ReadAllText((Join-Path $projectRoot 'src\version.h'))
if (-not $versionHeader.Contains("#define PLUGIN_VERSION `"$version`"")) {
    throw 'src\version.h does not match the canonical INI version.'
}

$changelog = Get-Content -LiteralPath (Join-Path $projectRoot 'CHANGELOG.md')
if (-not ($changelog -contains "## $version")) {
    throw "CHANGELOG.md has no section for $version."
}

$projectPath = Join-Path $projectRoot 'src\HighFpsFixes.vcxproj'
[xml]$project = Get-Content -LiteralPath $projectPath -Raw
$listed = @{}
foreach ($node in $project.SelectNodes("//*[local-name()='ClCompile' or local-name()='ClInclude']")) {
    if ($node.Include) {
        $listed[$node.Include.ToLowerInvariant()] = $true
    }
}

$sourceRoot = Join-Path $projectRoot 'src'
foreach ($file in Get-ChildItem -LiteralPath $sourceRoot -File -Recurse | Where-Object { $_.Extension -in '.cpp', '.h' }) {
    $relative = $file.FullName.Substring($sourceRoot.Length + 1)
    if (-not $listed.ContainsKey($relative.ToLowerInvariant())) {
        throw "$relative is not listed in HighFpsFixes.vcxproj."
    }
    $lineCount = ([IO.File]::ReadAllLines($file.FullName)).Length
    $limit = if ($file.Extension -eq '.h') { 1200 } else { 500 }
    if ($lineCount -gt $limit) {
        throw "$relative has grown to $lineCount lines; split it."
    }
}

Write-Host "Project structure is valid for High FPS Fixes v$version."
