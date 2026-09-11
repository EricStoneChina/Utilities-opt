param(
  [string]$RootPath = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = "Stop"

$sourceRoot = Join-Path $RootPath "src"
$sourceExtensions = @(".c", ".cpp", ".h", ".hpp")
$sourceFiles = @(
  Get-ChildItem -LiteralPath $sourceRoot -Recurse -File |
    Where-Object { $sourceExtensions -contains $_.Extension.ToLowerInvariant() }
)

function Get-LineCount([System.IO.FileInfo]$File) {
  return @(Get-Content -LiteralPath $File.FullName).Count
}

function Get-MatchCount([string]$Pattern) {
  $total = 0
  foreach ($file in $sourceFiles) {
    $matches = Select-String -LiteralPath $file.FullName -Pattern $Pattern -AllMatches
    foreach ($match in $matches) {
      $total += $match.Matches.Count
    }
  }
  return $total
}

$fileStats = @(
  foreach ($file in $sourceFiles) {
    [pscustomobject]@{
      Lines = Get-LineCount $file
      Path = $file.FullName.Substring($RootPath.Length + 1)
    }
  }
)

$gitDescription = (& git -C $RootPath describe --always --dirty 2>$null)
if (-not $gitDescription) { $gitDescription = "unavailable" }

$allocaCount = Get-MatchCount '\balloca\s*\('
$bfileCount = Get-MatchCount '\bBfile_[A-Za-z0-9_]+\s*\('
$mcsCount = Get-MatchCount '\bMCS[A-Za-z0-9_]+\s*\('
$clearVramCount = Get-MatchCount '\bBdisp_AllClr_VRAM\s*\('
$putDispCount = Get-MatchCount '\bBdisp_PutDisp_DD\s*\('
$keyCount = Get-MatchCount '\b(GetKey|GetKeyWait_OS)\s*\('
$sprintfCount = Get-MatchCount '\bsprintf\s*\('
$strcpyCount = Get-MatchCount '\bstrcpy\s*\('
$strcatCount = Get-MatchCount '\bstrcat\s*\('
$largeStackPattern = '\b(?:char|unsigned\s+char|unsigned\s+short|short|int|long|textElement|MenuItem|CalendarEvent|SimpleCalendarEvent|AddIn|Transaction|totp)\s+[A-Za-z_][A-Za-z0-9_]*\s*\[[^\]]*(?:[0-9]{4,}|TEXT_BUFFER_SIZE|MAX_EVENT_FILESIZE|numelements|count\s*\*)[^\]]*\]'
$largeStackArrays = @(
  foreach ($file in $sourceFiles) {
    Select-String -LiteralPath $file.FullName -Pattern $largeStackPattern |
      ForEach-Object {
        [pscustomobject]@{
          Path = $file.FullName.Substring($RootPath.Length + 1)
          Line = $_.LineNumber
          Text = $_.Line.Trim()
        }
      }
  }
)

Write-Output "# Utilities static performance audit"
Write-Output ""
Write-Output "- Generated: $([DateTime]::Now.ToString('s'))"
Write-Output "- Revision: $gitDescription"
Write-Output "- Source files: $($sourceFiles.Count)"
Write-Output "- Source lines: $(($fileStats | Measure-Object -Property Lines -Sum).Sum)"
Write-Output ""
Write-Output "## High-risk operation counts"
Write-Output ""
Write-Output "| Operation | Count |"
Write-Output "|---|---:|"
Write-Output "| alloca | $allocaCount |"
Write-Output "| Bfile_* | $bfileCount |"
Write-Output "| MCS* | $mcsCount |"
Write-Output "| Bdisp_AllClr_VRAM | $clearVramCount |"
Write-Output "| Bdisp_PutDisp_DD | $putDispCount |"
Write-Output "| GetKey/GetKeyWait | $keyCount |"
Write-Output "| sprintf | $sprintfCount |"
Write-Output "| strcpy | $strcpyCount |"
Write-Output "| strcat | $strcatCount |"
Write-Output "| large stack array candidates | $($largeStackArrays.Count) |"

Write-Output ""
Write-Output "## Largest source files"
Write-Output ""
Write-Output "| File | Lines |"
Write-Output "|---|---:|"
foreach ($item in ($fileStats | Sort-Object Lines -Descending | Select-Object -First 12)) {
  Write-Output "| ``$($item.Path)`` | $($item.Lines) |"
}

Write-Output ""
Write-Output "## Files with stack-sensitive operations"
Write-Output ""
foreach ($file in $sourceFiles) {
  $allocaMatches = Select-String -LiteralPath $file.FullName -Pattern '\balloca\s*\(' -AllMatches
  if ($allocaMatches) {
    $relativePath = $file.FullName.Substring($RootPath.Length + 1)
    $lineNumbers = ($allocaMatches | ForEach-Object { $_.LineNumber }) -join ", "
    Write-Output "- ``$relativePath``: lines $lineNumbers"
  }
}

Write-Output ""
Write-Output "## Large stack array candidates"
Write-Output ""
if ($largeStackArrays.Count -eq 0) {
  Write-Output "- none detected"
} else {
  foreach ($item in $largeStackArrays) {
    Write-Output "- ``$($item.Path):$($item.Line)``: $($item.Text)"
  }
}
