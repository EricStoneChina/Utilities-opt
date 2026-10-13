param(
  [string]$RootPath = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = "Stop"
$performancePath = Join-Path $RootPath "src\performanceGUI.cpp"
$stringsPath = Join-Path $RootPath "src\stringsProvider.cpp"
$performance = Get-Content -LiteralPath $performancePath -Raw
$strings = Get-Content -LiteralPath $stringsPath -Raw
$failures = New-Object System.Collections.Generic.List[string]

$requiredSourcePatterns = @(
  "SCREEN_FILL_REFRESH_PASSES",
  "SCREEN_FRAMERATE_PASSES",
  "drawFullFillRefreshTest",
  "drawFrameRateTest",
  "KEY_CTRL_F4",
  "KEY_CTRL_F5",
  "Bdisp_PutDisp_DD",
  "tickDifferenceToMilliseconds"
)
foreach ($pattern in $requiredSourcePatterns) {
  if ($performance -notmatch [regex]::Escape($pattern)) {
    $failures.Add("screen tools missing: $pattern")
  }
}

$requiredKeys = @(
  "F4  Full fill/refresh",
  "F5  Frame rate",
  "Full fill/refresh",
  "Frame rate",
  "Frames:",
  "Elapsed:",
  "Refresh:"
)
foreach ($key in $requiredKeys) {
  $pattern = '"' + [regex]::Escape($key) + '"'
  if ($strings -notmatch $pattern) {
    $failures.Add("screen tool localization missing: $key")
  }
}

if ($failures.Count -gt 0) {
  Write-Output "SCREEN_TOOLS_AUDIT=fail"
  $failures
  exit 1
}

Write-Output "SCREEN_TOOLS_AUDIT=pass"
exit 0
