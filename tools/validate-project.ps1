$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path $PSScriptRoot -Parent
$configPath = Join-Path $projectRoot 'Config\HighFpsFixes.ini'
$header = Get-Content -LiteralPath $configPath -TotalCount 1
if ($header -notmatch '^# High FPS Fixes v(?<version>\d+\.\d+\.\d+)$') {
    throw "Invalid version header in $configPath"
}
$version = $Matches.version

$configurationSource = [IO.File]::ReadAllText(
    (Join-Path $projectRoot 'src\modules\configuration.inl'))
if (-not $configurationSource.Contains("# High FPS Fixes v$version\n")) {
    throw 'Embedded default INI version does not match the canonical INI.'
}

$readme = [IO.File]::ReadAllText((Join-Path $projectRoot 'README.md'))
if (-not $readme.Contains("# High FPS Fixes v$version")) {
    throw 'README configuration example does not match the canonical INI.'
}

$projectPath = Join-Path $projectRoot 'src\HighFpsFixes.vcxproj'
[xml]$project = Get-Content -LiteralPath $projectPath -Raw
$listedModules = @{}
foreach ($node in $project.SelectNodes("//*[local-name()='None']")) {
    $include = $node.Include
    if ($include -like 'modules\*.inl' -or $include -like 'modules\*\*.inl') {
        $listedModules[$include.ToLowerInvariant()] = $true
    }
}

$modulesRoot = Join-Path $projectRoot 'src\modules'
foreach ($file in Get-ChildItem -LiteralPath $modulesRoot -Filter '*.inl' -File -Recurse) {
    $relative = $file.FullName.Substring((Join-Path $projectRoot 'src').Length + 1)
    if (-not $listedModules.ContainsKey($relative.ToLowerInvariant())) {
        throw "$relative is not listed in HighFpsFixes.vcxproj."
    }
    $lineCount = ([IO.File]::ReadAllLines($file.FullName)).Length
    if ($lineCount -gt 1400) {
        throw "$relative has grown to $lineCount lines; split the module."
    }
}

Write-Host "Project validation passed for High FPS Fixes v$version."
