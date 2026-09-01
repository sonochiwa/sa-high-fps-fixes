param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateNotNullOrEmpty()]
    [string]$Path
)

$ErrorActionPreference = 'Stop'
$resolved = (Resolve-Path -LiteralPath $Path).Path
$bytes = [System.IO.File]::ReadAllBytes($resolved)

function Read-U16([int]$Offset) {
    if ($Offset -lt 0 -or $Offset + 2 -gt $bytes.Length) {
        throw "Offset 0x$($Offset.ToString('X')) is outside the executable."
    }
    return [BitConverter]::ToUInt16($bytes, $Offset)
}

function Read-U32([int]$Offset) {
    if ($Offset -lt 0 -or $Offset + 4 -gt $bytes.Length) {
        throw "Offset 0x$($Offset.ToString('X')) is outside the executable."
    }
    return [BitConverter]::ToUInt32($bytes, $Offset)
}

if ((Read-U16 0) -ne 0x5A4D) { throw 'Not a PE executable: MZ header missing.' }
$pe = [int](Read-U32 0x3C)
if ((Read-U32 $pe) -ne 0x00004550) { throw 'Not a PE executable: PE header missing.' }
$sectionCount = Read-U16 ($pe + 6)
$optionalSize = Read-U16 ($pe + 20)
$optional = $pe + 24
if ((Read-U16 $optional) -ne 0x10B) { throw 'Expected a 32-bit PE executable.' }
$imageBase = [uint32](Read-U32 ($optional + 28))
$sectionTable = $optional + $optionalSize

$sections = for ($i = 0; $i -lt $sectionCount; ++$i) {
    $offset = $sectionTable + $i * 40
    [pscustomobject]@{
        VirtualSize = [uint32](Read-U32 ($offset + 8))
        VirtualAddress = [uint32](Read-U32 ($offset + 12))
        RawSize = [uint32](Read-U32 ($offset + 16))
        RawAddress = [uint32](Read-U32 ($offset + 20))
    }
}

function Get-FileOffset([uint32]$Address, [int]$Length) {
    if ($Address -lt $imageBase) { throw 'Virtual address is below the image base.' }
    $rva = [uint32]($Address - $imageBase)
    foreach ($section in $sections) {
        $span = [Math]::Max($section.VirtualSize, $section.RawSize)
        if ($rva -ge $section.VirtualAddress -and
            [uint64]$rva + $Length -le [uint64]$section.VirtualAddress + $span) {
            $delta = $rva - $section.VirtualAddress
            if ([uint64]$delta + $Length -gt $section.RawSize) {
                throw "Address 0x$($Address.ToString('X8')) has no file-backed bytes."
            }
            return [int]($section.RawAddress + $delta)
        }
    }
    throw "Address 0x$($Address.ToString('X8')) is not mapped by a PE section."
}

function Read-VirtualBytes([uint32]$Address, [int]$Length) {
    $offset = Get-FileOffset $Address $Length
    return [byte[]]$bytes[$offset..($offset + $Length - 1)]
}

function Test-BytesEqual([byte[]]$Left, [byte[]]$Right) {
    if ($Left.Length -ne $Right.Length) { return $false }
    for ($i = 0; $i -lt $Left.Length; ++$i) {
        if ($Left[$i] -ne $Right[$i]) { return $false }
    }
    return $true
}

$profiles = @(
    @{ Name = 'GTA SA 1.0 US Compact'; Signature = [byte[]](0x55,0x8B,0xEC,0x53) },
    @{ Name = 'GTA SA 1.0 US Hoodlum'; Signature = [byte[]](0xE9,0x7B,0x19,0x16) }
)

$entry = Read-VirtualBytes 0x00401000 4
$profile = $profiles | Where-Object {
    Test-BytesEqual $entry $_.Signature
} | Select-Object -First 1
if (-not $profile) {
    throw "Unsupported GTA executable. Entry signature: $([BitConverter]::ToString($entry))."
}

# Representative probes span player, vehicle and world code. Runtime still
# validates the complete expected byte sequence for every enabled patch before
# modifying memory.
$probes = @(
    @{ Name='aiming rifle walk'; Address=[uint32]0x0061E0CA; Bytes=[byte[]](0xD8,0x0D,0xA8,0x8C,0x85,0x00) },
    @{ Name='bloody footprint countdown'; Address=[uint32]0x005E5877; Bytes=[byte[]](0x49,0x85,0xC9,0x89,0x8E,0x50,0x07,0x00,0x00) },
    @{ Name='bloody footstep side'; Address=[uint32]0x005E5E64; Bytes=[byte[]](0xE8,0x17,0xF5,0xFF,0xFF) },
    @{ Name='bloody footprint shadow'; Address=[uint32]0x005E54C1; Bytes=[byte[]](0xE8,0x9A,0x1A,0x12,0x00) },
    @{ Name='wheel friction'; Address=[uint32]0x006D6E69; Bytes=[byte[]](0xD9,0x05,0xCC,0xB9,0xC2,0x00) },
    @{ Name='burnout'; Address=[uint32]0x006A4FE6; Bytes=[byte[]](0xD9,0x05,0x94,0x9A,0x85,0x00) },
    @{ Name='turn air resistance'; Address=[uint32]0x00544D29; Bytes=[byte[]](0xD9,0x46,0x50,0xD8,0x0D,0xD0) }
)

$failed = @()
foreach ($probe in $probes) {
    $actual = Read-VirtualBytes $probe.Address $probe.Bytes.Length
    if (-not (Test-BytesEqual $actual $probe.Bytes)) {
        $failed += "$($probe.Name) at 0x$($probe.Address.ToString('X8')): expected $([BitConverter]::ToString($probe.Bytes)), got $([BitConverter]::ToString($actual))"
    }
}

Write-Host "Profile: $($profile.Name)"
Write-Host "Image base: 0x$($imageBase.ToString('X8'))"
Write-Host "SHA-256: $((Get-FileHash -LiteralPath $resolved -Algorithm SHA256).Hash)"
if ($failed.Count -ne 0) {
    $failed | ForEach-Object { Write-Error $_ }
    exit 1
}
Write-Host "Compatibility probes passed: $($probes.Count)."
exit 0
