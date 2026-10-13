param(
  [string]$RootPath = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = "Stop"

$platform = Get-Content -Raw -LiteralPath (Join-Path $RootPath "src\platformProvider.cpp")
$memory = Get-Content -Raw -LiteralPath (Join-Path $RootPath "src\memsysGUI.cpp")
$failures = New-Object System.Collections.Generic.List[string]

if ($platform -notmatch "platformInfo\.capabilities\s*=\s*0") {
  $failures.Add("platform capability mask is not reset before model gating")
}
if ($platform -notmatch "if\s*\(platformInfo\.model\s*!=") {
  $failures.Add("capabilities are not gated by a recognised model")
}
if ($platform -notmatch "PLATFORM_CAP_BFILE" -or
    $platform -notmatch "PLATFORM_CAP_MCS" -or
    $platform -notmatch "PLATFORM_CAP_RUNMAT" -or
    $platform -notmatch "PLATFORM_CAP_SYSTEM_LANGUAGE" -or
    $platform -notmatch "PLATFORM_CAP_LINK") {
  $failures.Add("recognised-model capability set is incomplete")
}
if ($platform -notmatch "LocalizeMessage1" -or
    $platform -notmatch "platformGetSystemLanguageCode") {
  $failures.Add("system-language capability does not use the platform probe")
}
if ($memory -notmatch "canReadStorage\s*=\s*platformHasCapability\(PLATFORM_CAP_BFILE\)") {
  $failures.Add("memory page does not gate Bfile access")
}
if ($memory -notmatch "canReadMainMemory\s*=\s*platformHasCapability\(PLATFORM_CAP_MCS\)") {
  $failures.Add("memory page does not gate MCS access")
}
if ($memory -notmatch "canReadStorage\s*\?\s*Bfile_GetMediaFree_OS") {
  $failures.Add("Bfile media query is not capability guarded")
}
if ($memory -notmatch "canReadMainMemory\s*\?\s*MCS_GetState") {
  $failures.Add("MCS state query is not capability guarded")
}
if ($memory -notmatch "PLATFORM_CAP_SYSTEM_APPS") {
  $failures.Add("device power settings are not capability guarded")
}
$diagnostics = Get-Content -Raw -LiteralPath (Join-Path $RootPath "src\diagnosticsGUI.cpp")
if ($diagnostics -notmatch "platformHasCapability\(PLATFORM_CAP_TIMER\)") {
  $failures.Add("diagnostics timer is not capability guarded")
}
$application = Get-Content -Raw -LiteralPath (Join-Path $RootPath "src\appGUI.cpp")
if ($application -notmatch "platformHasCapability\(PLATFORM_CAP_BFILE\)") {
  $failures.Add("application audit is not capability guarded")
}
$performance = Get-Content -Raw -LiteralPath (Join-Path $RootPath "src\performanceGUI.cpp")
if ($performance -notmatch "platformHasCapability\(PLATFORM_CAP_BFILE\)" -or
    $performance -notmatch "platformHasCapability\(PLATFORM_CAP_TIMER\)") {
  $failures.Add("performance lab storage/timer paths are not capability guarded")
}
$link = Get-Content -Raw -LiteralPath (Join-Path $RootPath "src\linkProvider.cpp")
if ($link -notmatch "PLATFORM_CAP_LINK" -or
    $link -notmatch "APP_LINK_transmit_select_dialog" -or
    $link -notmatch "App_InitDlgDescriptor") {
  $failures.Add("Link center does not use the guarded native transfer handoff")
}

if ($failures.Count -gt 0) {
  Write-Output "PLATFORM_AUDIT=fail"
  $failures
  exit 1
}

Write-Output "PLATFORM_AUDIT=pass"
exit 0
