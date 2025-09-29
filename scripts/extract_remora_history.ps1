<#!
.SYNOPSIS
  Reconstruct latest Remora full firmware source snapshot from .history timestamped files.
.DESCRIPTION
  The project keeps time-versioned Remora sources under:
    .history/Remora-OS6/Firmware/FirmwareSource/Remora-OS6
  Each file has suffix _YYYYMMDDHHMMSS before its extension. This script picks, per logical file
  (path with the timestamp removed), the most recent timestamped variant and copies it into the
  canonical production tree expected by CMake:
    Remora-OS6/Firmware/FirmwareSource/Remora-OS6
  After running, re-configure with -DFULL_FIRMWARE=ON.
.PARAMETER DryRun
  If set, only prints the operations.
#>
[CmdletBinding()]
param(
  [switch]$DryRun
)

$ErrorActionPreference = 'Stop'

function Timestamp-ToSortableInt($ts){
  # ts already sortable lexicographically (YYYYMMDDHHMMSS) but keep helper if we add validation later
  return [int64]$ts
}

$projRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).ProviderPath
$historyRoot = Join-Path $projRoot '.history/Remora-OS6/Firmware/FirmwareSource/Remora-OS6'
if(-not (Test-Path $historyRoot)){
  Write-Error "History root not found: $historyRoot"
}
$destRoot = Join-Path $projRoot 'Remora-OS6/Firmware/FirmwareSource/Remora-OS6'
Write-Host "[INFO] History root: $historyRoot"
Write-Host "[INFO] Destination : $destRoot"

# Collect timestamped files (*.c|*.h|*.cpp) that contain _YYYYMMDDHHMMSS before extension
$regex = [regex]'^(?<base>.+?)_(?<ts>\d{14})(?<ext>\.(c|h|cpp))$'
$all = Get-ChildItem -Recurse -File $historyRoot | Where-Object { $regex.IsMatch($_.Name) }
if(-not $all){
  Write-Warning 'No timestamped files found.'
  return
}

# Map: relativeCleanPath -> (FileInfo with newest ts)
$latestMap = @{}
foreach($f in $all){
  $rel = $f.FullName.Substring($historyRoot.Length)
  $rel = $rel -replace '^[\\/]',''
  $m = $regex.Match($f.Name)
  if(-not $m.Success){ continue }
  $cleanName = ($m.Groups['base'].Value + $m.Groups['ext'].Value)
  $relDir = Split-Path $rel -Parent
  if([string]::IsNullOrEmpty($relDir) -or $relDir -eq '.') { $cleanRel = $cleanName } else { $cleanRel = Join-Path $relDir $cleanName }
  $ts = $m.Groups['ts'].Value
  if($latestMap.ContainsKey($cleanRel)){
    $prev = $latestMap[$cleanRel]
    if($f.Name -gt $prev.Name){
      $latestMap[$cleanRel] = $f
    }
  } else {
    $latestMap[$cleanRel] = $f
  }
}

Write-Host "[INFO] Files to materialize: $($latestMap.Count)"
if(-not $DryRun){ New-Item -ItemType Directory -Force -Path $destRoot | Out-Null }

$copied = 0
foreach($key in ($latestMap.Keys | Sort-Object)){
  $src = $latestMap[$key]
  $dest = Join-Path $destRoot $key
  $destDir = Split-Path $dest -Parent
  if(-not $DryRun -and -not (Test-Path $destDir)){
    New-Item -ItemType Directory -Force -Path $destDir | Out-Null
  }
  if(-not $DryRun){ Copy-Item -LiteralPath $src.FullName -Destination $dest -Force }
  $srcRel = $src.FullName.Substring($historyRoot.Length) -replace '^[\\/]',''
  Write-Host ("[COPY] {0}" -f $srcRel) -ForegroundColor Cyan
  $copied++
}
Write-Host "[INFO] Copied $copied latest files." -ForegroundColor Green

# Copy any non-timestamp static support files (if they exist)
$staticCandidates = @('drivers','usb','st','lwip') | ForEach-Object { Join-Path $historyRoot $_ }
foreach($cand in $staticCandidates){
  if(Test-Path $cand){
    Write-Host "[INFO] Ensuring directory tree for $(Split-Path $cand -Leaf)" -ForegroundColor Yellow
    if(-not $DryRun){ Copy-Item -Recurse -Force -Container:$true $cand (Join-Path $destRoot (Split-Path $cand -Leaf)) }
  }
}

Write-Host "[DONE] Remora history extraction complete. Re-run CMake with -DFULL_FIRMWARE=ON." -ForegroundColor Green
