param(
  [string]$RootPath = (Get-Location)
)

$mainPath = Join-Path $RootPath "src\main.cpp"
$lockPath = Join-Path $RootPath "src\lockGUI.cpp"
if (-not (Test-Path -LiteralPath $mainPath) -or
    -not (Test-Path -LiteralPath $lockPath)) {
  throw "Run-Mat source files were not found"
}

$main = Get-Content -LiteralPath $mainPath -Raw
$lock = Get-Content -LiteralPath $lockPath -Raw
$checks = @(
  @{ Name = "setjmp landing point"; Text = $main; Pattern = "setjmp\(utilities_return\)" },
  @{ Name = "landing cleanup"; Text = $main; Pattern = "finalizeRunMatReturn\(\)" },
  @{ Name = "longjmp return"; Text = $lock; Pattern = "longjmp\(utilities_return,\s*1\)" },
  @{ Name = "timer stop"; Text = $lock; Pattern = "Timer_Stop" },
  @{ Name = "timer deinstall"; Text = $lock; Pattern = "Timer_Deinstall" },
  @{ Name = "keyboard flags"; Text = $main; Pattern = "Bkey_ClrAllFlags" },
  @{ Name = "cursor cleanup"; Text = $main; Pattern = "Cursor_SetFlashOff" },
  @{ Name = "F-key cleanup"; Text = $main; Pattern = "resetFunctionKeyMappings\(\)" },
  @{ Name = "Run-Mat call"; Text = $lock; Pattern = "APP_RUNMAT" },
  @{ Name = "pending return guard"; Text = $lock; Pattern = "runMatReturnPending" },
  @{ Name = "fast matrix sampling"; Text = $lock; Pattern = "Timer_Install\(0,\s*returnToUtilitiesHandler,\s*10\)" },
  @{ Name = "EXIT fallback"; Text = $lock; Pattern = "if \(exitPressed \|\| \(runMatShiftSeen" },
  @{ Name = "F6 compatibility sample"; Text = $lock; Pattern = "runMatF6Seen" },
  @{ Name = "MENU compatibility sample"; Text = $lock; Pattern = "runMatMenuSeen" },
  @{ Name = "F6 matrix read"; Text = $lock; Pattern = "platformIsKeyPressed\(KEY_PRGM_F6\)" },
  @{ Name = "MENU matrix read"; Text = $lock; Pattern = "platformIsKeyPressed\(KEY_PRGM_MENU\)" },
  @{ Name = "shift-state cleanup"; Text = $lock; Pattern = "SetSetupSetting\(.*0\)" },
  @{ Name = "normal-return UI cleanup"; Text = $lock; Pattern = "finalizeRunMatReturn\(\)" }
)

$failed = @()
foreach ($check in $checks) {
  if ($check.Text -notmatch $check.Pattern) {
    $failed += $check.Name
  }
}
if ($failed.Count -gt 0) {
  throw ("RUNMAT_AUDIT_FAIL: " + ($failed -join ", "))
}
Write-Output ("RUNMAT_AUDIT=pass ({0} invariants)" -f $checks.Count)
exit 0
