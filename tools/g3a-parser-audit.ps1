param(
  [string]$RootPath = (Split-Path -Parent $PSScriptRoot),
  [string]$ReferenceFile = ""
)

$ErrorActionPreference = "Stop"

function Read-U32BE([byte[]]$Data, [int]$Offset) {
  return [uint32](
    ([uint32]$Data[$Offset] -shl 24) -bor
    ([uint32]$Data[$Offset + 1] -shl 16) -bor
    ([uint32]$Data[$Offset + 2] -shl 8) -bor
    [uint32]$Data[$Offset + 3]
  )
}

function Assert-Equal($Actual, $Expected, [string]$Message) {
  if ($Actual -ne $Expected) {
    throw "$Message (actual: $Actual, expected: $Expected)"
  }
}

function Test-G3AHeader([string]$Path) {
  $data = [IO.File]::ReadAllBytes($Path)
  if ($data.Length -lt 0x7004) {
    throw "$Path is shorter than the CG50 G3A header and checksum"
  }

  $magic = [byte[]](0xAA, 0xAC, 0xBD, 0xAF, 0x90, 0x88, 0x9A,
                    0x8D, 0xD3, 0xFF, 0xFE, 0xFF, 0xFE, 0xFF)
  for ($i = 0; $i -lt $magic.Length; $i++) {
    Assert-Equal $data[$i] $magic[$i] "$Path has invalid G3A magic"
  }
  Assert-Equal $data[0x24] 1 "$Path active flag is invalid"
  Assert-Equal $data[0x25] 1 "$Path validation flag is invalid"

  [uint64]$declaredSize = Read-U32BE $data 0x5C
  Assert-Equal $declaredSize ([uint64]$data.Length) "$Path size field is wrong"
  [uint64]$codeOffset = Read-U32BE $data 0x2E
  [uint64]$checksumOffset = 0x7000 + $codeOffset
  Assert-Equal ($checksumOffset + 4) ([uint64]$data.Length) `
    "$Path code offset does not point to the trailing checksum"

  [uint64]$calculated = 0
  for ($i = 0; $i -lt $data.Length; $i++) {
    if (($i -ge 0x20 -and $i -lt 0x24) -or
        $i -ge ($data.Length - 4)) {
      continue
    }
    $calculated += $data[$i]
  }
  $stored = Read-U32BE $data 0x20
  $tail = Read-U32BE $data ($data.Length - 4)
  Assert-Equal $stored ([uint32]$calculated) "$Path checksum is invalid"
  Assert-Equal $tail $stored "$Path trailing checksum is invalid"

  return [pscustomobject]@{
    Name = (Split-Path -Leaf $Path)
    Bytes = $data.Length
    CodeOffset = $codeOffset
  }
}

$parserPath = Join-Path $RootPath "src\appProvider.cpp"
$parser = [IO.File]::ReadAllText($parserPath)
if (-not $parser.Contains("Bfile_ReadFile_OS(handle, buffer, size, offset)")) {
  throw "G3A parser regressed to implicit current-pointer header reads"
}
if (-not $parser.Contains("int actualFileSize = Bfile_GetFileSize_OS(handle);")) {
  throw "G3A parser no longer trusts the opened handle's file size"
}
if (-not $parser.Contains("readResult = Bfile_ReadFile_OS(handle, raw, G3A_ICON_BYTES, 0x1000);")) {
  throw "G3A icon preview no longer reads the unselected icon slot explicitly"
}
if (-not $parser.Contains("(unsigned short)raw[i * 2] << 8")) {
  throw "G3A icon preview no longer converts big-endian RGB565 explicitly"
}

$artifact = Get-ChildItem -LiteralPath $RootPath -Filter "*.g3a" -File |
  Select-Object -First 1
if (-not $artifact) { throw "No built G3A artifact was found" }
$artifactResult = Test-G3AHeader $artifact.FullName

Write-Output "G3A_PARSER_AUDIT=pass"
Write-Output ("- artifact: {0} ({1} bytes, code offset {2})" -f
  $artifactResult.Name, $artifactResult.Bytes, $artifactResult.CodeOffset)
if ($ReferenceFile) {
  $referenceResult = Test-G3AHeader (Resolve-Path -LiteralPath $ReferenceFile)
  Write-Output ("- reference: {0} ({1} bytes, code offset {2})" -f
    $referenceResult.Name, $referenceResult.Bytes, $referenceResult.CodeOffset)
}
exit 0
