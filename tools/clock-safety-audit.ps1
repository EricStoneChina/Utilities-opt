param(
  [string]$RootPath = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = "Stop"

$hardware = Get-Content -Raw -LiteralPath (Join-Path $RootPath "src\hardwareProvider.cpp")
$header = Get-Content -Raw -LiteralPath (Join-Path $RootPath "src\hardwareProvider.hpp")
$failures = New-Object System.Collections.Generic.List[string]

# CPU_change_freq is retained only as a source-compatibility symbol for old
# PicoC scripts.  The legacy MMIO sequence must never be reachable on CG50,
# emulators, or an unrecognised platform.
$function = [regex]::Match(
  $hardware,
  '(?s)void\s+CPU_change_freq\s*\(\s*int\s+mult\s*\)\s*\{(?<body>.*?)\n\s*\}'
)
if (-not $function.Success) {
  $failures.Add("CPU_change_freq definition was not found")
} else {
  $body = $function.Groups['body'].Value
  if ($body -notmatch 'getHardwareModel\s*\(\s*\)') {
    $failures.Add("CPU_change_freq does not identify the hardware model")
  }
  if ($body -notmatch 'if\s*\(\s*model\s*!=\s*1\s*&&\s*model\s*!=\s*2\s*\)\s*return\s*;') {
    $failures.Add("CPU_change_freq is not restricted to CG10/CG20")
  }
  $guard = $body.IndexOf('if (model != 1 && model != 2) return;')
  $asm = $body.IndexOf('__asm__')
  if ($guard -lt 0 -or $asm -lt 0 -or $guard -gt $asm) {
    $failures.Add("legacy MMIO clock writes are reachable before the platform guard")
  }
}

if ($header -notmatch 'Legacy CPU clocking constants for the fx-CG10/20 Prizm only') {
  $failures.Add("legacy clock constants are not documented as CG10/CG20-only")
}

if ($failures.Count -gt 0) {
  Write-Output "CLOCK_SAFETY_AUDIT=fail"
  $failures
  exit 1
}

Write-Output "CLOCK_SAFETY_AUDIT=pass"
exit 0
