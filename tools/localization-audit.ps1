param(
  [string]$RootPath = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = "Stop"
$stringsPath = Join-Path $RootPath "src\stringsProvider.cpp"
$strings = Get-Content -LiteralPath $stringsPath -Raw

# These are the fixed labels introduced by the CG50 control-center pages.
# Dynamic values and report-file contents are intentionally not translated.
$requiredKeys = @(
  "Device dashboard",
  "Interface diagnostics",
  "Application audit",
  "Application details",
  "Application icon",
  "Icon unavailable.",
  "Screen tools",
  "Performance lab",
  "Native system settings",
  "Configuration backup",
  "Showing first 64 entries",
  "OS version:",
  "OS build:",
  "Calculator model:",
  "Real hardware:",
  "Capabilities:",
  "Device ID:",
  "System Information",
  "User Information",
  "Running benchmarks...",
  "Arithmetic",
  "Memory copy",
  "VRAM draw",
  "Timer callbacks:",
  "Timer error:",
  "F1 run  F2 reset  F3 save",
  "Results use RTC ticks (1/128 s).",
  "Password",
  "Function key color unavailable.",
  "F1  Save screenshot",
  "F2  RGB565 color test",
  "F3  Pixel coordinates",
  "F4  Full fill/refresh",
  "F5  Frame rate",
  "RGB565 color test",
  "Pixel coordinates",
  "Full fill/refresh",
  "Frame rate",
  "Frames:",
  "Elapsed:",
  "Refresh:",
  "Press any key to return",
  "n/a",
  "F5 quarantine  EXIT close",
  "F3  Open native Memory",
  "Open native Memory?",
  "Choose BACKUP then SAVE.",
  "Back up and quarantine the",
  "Change the",
  "Selected Add-In state?",
  "Recent diagnostic events",
  "Newest event first",
  "No events recorded.",
  "EXIT return",
  "Storage access unavailable.",
  "Main memory access unavailable.",
  "Run-Mat entered",
  "Run-Mat returned",
  "Run-Mat error",
  "Timer started",
  "Timer stopped",
  "Key matrix sampled",
  "Storage guard",
  "Unknown event",
  "Back up this add-in",
  "and remove it from root?",
  "Confirm quarantine of",
  "the selected .g3a file?",
  "Backup saved; add-in removed.",
  "Backup exists; nothing removed.",
  "Backup saved; remove failed.",
  "Backup failed; nothing removed."
  ,"Restore quarantined Add-Ins"
  ,"No quarantined backups found."
  ,"Showing first 32 backups"
  ,"F5 restore backups"
  ,"Restore this quarantined"
  ,"Add-In to the root?"
  ,"Confirm restore; existing"
  ,"files will never be overwritten."
  ,"Add-In restored; backup kept."
  ,"Original exists; nothing restored."
  ,"Restore failed; backup kept."
)

$missing = @(
  foreach ($key in $requiredKeys) {
    $pattern = '"' + [regex]::Escape($key) + '"'
    if ($strings -notmatch $pattern) { $key }
  }
)

if ($missing.Count -gt 0) {
  Write-Output "LOCALIZATION_AUDIT=fail"
  $missing | ForEach-Object { Write-Output ("- missing: " + $_) }
  exit 1
}

# Check the byte literals for the strings that previously showed as malformed
# characters on the calculator.  Keeping this as an ASCII source check avoids
# making the audit itself depend on the host console code page.
$expectedLiterals = @{
  "Running benchmarks..." = '\xd5\xfd\xd4\xda\xd4\xcb\xd0\xd0\xd0\xd4\xc4\xdc\xb2\xe2\xca\xd4...'
  "Timer callbacks:" = '\xb6\xa8\xca\xb1\xc6\xf7\xbb\xd8\xb5\xf7\xa3\xba'
  "F1 run  F2 reset  F3 save" = 'F1  \xd4\xcb\xd0\xd0  F2  \xd6\xd8\xd6\xc3  F3  \xb1\xa3\xb4\xe6'
  "Save screenshot" = '\xb1\xa3\xb4\xe6\xbd\xd8\xcd\xbc'
  "Queue" = '\xb6\xd3\xc1\xd0'
  "Storage access unavailable." = '\xb4\xe6\xb4\xa2\xb7\xc3\xce\xca\xb2\xbb\xbf\xc9\xd3\xc3\xa1\xa3'
  "Main memory access unavailable." = '\xd6\xf7\xc4\xda\xb4\xe6\xb7\xc3\xce\xca\xb2\xbb\xbf\xc9\xd3\xc3\xa1\xa3'
}
$literalFailures = @(
  foreach ($key in $expectedLiterals.Keys) {
    $pattern = 'if \(!strcmp\(text, "' + [regex]::Escape($key) +
      '"\)\) return "([^"]*)";'
    $match = [regex]::Match($strings, $pattern)
    if (-not $match.Success) {
      "missing translation literal: $key"
    } elseif ($match.Groups[1].Value -ne $expectedLiterals[$key]) {
      "malformed translation literal: $key"
    }
  }
)
if ($literalFailures.Count -gt 0) {
  Write-Output "LOCALIZATION_AUDIT=fail"
  $literalFailures | ForEach-Object { Write-Output ("- " + $_) }
  exit 1
}

Write-Output ("LOCALIZATION_AUDIT=pass ({0} keys)" -f $requiredKeys.Count)
exit 0
