param(
  [string]$RootPath = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = "Stop"

function Get-FunctionBody([string[]]$Lines, [int]$StartLine) {
  $depth = 0
  $body = New-Object System.Collections.Generic.List[string]
  $line = $StartLine
  do {
    $current = $Lines[$line]
    $body.Add($current)
    $depth += ([regex]::Matches($current, "\{")).Count
    $depth -= ([regex]::Matches($current, "\}")).Count
    $line++
  } while ($line -lt $Lines.Count -and $depth -gt 0)
  return $body.ToArray()
}

function Test-StorageCall([string]$Text) {
  return $Text -match "\b(?:Bfile_[A-Za-z0-9_]+|MCS[A-Za-z0-9_]*)\s*\("
}

function Test-ConversionOnly([string]$Text) {
  $relevant = $Text -replace "\bBfile_(?:NameToStr_ncpy|StrToName_ncpy|Name_MatchMask)\s*\(", ""
  $relevant = $relevant -replace "\bMCS_GetMainMemoryStart\s*\(", ""
  return -not (Test-StorageCall $relevant)
}

$functionPattern = '^\s*(?!(?:else\s+)?(?:if|for|while|switch|catch)\b)(?:(?:static|inline|extern\s+"C"|const|unsigned|signed|long|short)\s+)*(?:void|int|char|bool|float|double|size_t|unsigned|long|short|[A-Za-z_][A-Za-z0-9_:<>*&]*)\s+[A-Za-z_][A-Za-z0-9_:]*\s*\([^;]*\)\s*\{'
$violations = @()

$appProvider = Get-Content (Join-Path $RootPath 'src\appProvider.cpp') -Raw
$fileProvider = Get-Content (Join-Path $RootPath 'src\fileProvider.cpp') -Raw
if ($appProvider -notmatch 'static\s+int\s+writeFileChunk' -or
    ([regex]::Matches($appProvider, '!writeFileChunk\s*\(').Count -lt 2)) {
  $violations += 'src\appProvider.cpp: chunk writes do not validate the returned file position'
}

if ($fileProvider -match 'unsigned\s+char\s+copybuffer\s*\[\s*FILE_COPYBUFFER') {
  $violations += 'src\fileProvider.cpp: ordinary copy still places the 24 KB buffer on the add-in stack'
}
if ($fileProvider -notmatch '(?s)void\s+copyFile\s*\(.*?unsigned\s+char\s*\*\s*copybuffer\s*=\s*\(unsigned\s+char\s*\*\)malloc\s*\(\s*FILE_COPYBUFFER\s*\)') {
  $violations += 'src\fileProvider.cpp: ordinary copy does not use a checked heap buffer'
}
if ($fileProvider -notmatch '(?s)void\s+copyFile\s*\(.*?!storageWriteFile\(hNewFile,\s*copybuffer,\s*rwsize\)') {
  $violations += 'src\fileProvider.cpp: ordinary copy does not verify Bfile write progress'
}

foreach ($file in (Get-ChildItem -LiteralPath (Join-Path $RootPath "src") -Recurse -File -Filter *.cpp)) {
  $lines = Get-Content -LiteralPath $file.FullName
  for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -notmatch $functionPattern) { continue }
    $body = Get-FunctionBody $lines $i
    $bodyText = $body -join [Environment]::NewLine
    if ((Test-StorageCall $bodyText) -and
        -not (Test-ConversionOnly $bodyText) -and
        $bodyText -notmatch "StorageScope|storageOperationBegin") {
      $relative = $file.FullName.Substring($RootPath.Length + 1)
      $violations += "\${relative}:$($i + 1)"
    }
  }
}

# The PicoC bridge uses a macro-shaped function declaration, so check those
# wrappers separately. Conversion-only helpers do not touch the filesystem.
$bridge = Join-Path $RootPath "src\picoc\platform\library_prizm_cpp.cpp"
if (Test-Path -LiteralPath $bridge) {
  $lines = Get-Content -LiteralPath $bridge
  for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -notmatch '^\s*pcvoid\((?<name>(?:Bfile_|MCS)[A-Za-z0-9_]*)\)') {
      continue
    }
    $name = $Matches.name
    if ($name -in @("Bfile_NameToStr_ncpy", "Bfile_StrToName_ncpy",
                    "Bfile_Name_MatchMask", "MCS_GetMainMemoryStart")) {
      continue
    }
    $body = Get-FunctionBody $lines $i
    if (($body -join [Environment]::NewLine) -notmatch "StorageScope") {
      $violations += "src\picoc\platform\library_prizm_cpp.cpp:$($i + 1)"
    }
  }
}

if ($violations.Count -gt 0) {
  Write-Output "STORAGE_GUARD_AUDIT=fail"
  $violations | Sort-Object -Unique
  exit 1
}

Write-Output "STORAGE_GUARD_AUDIT=pass"
exit 0
