param(
  [string]$RootPath = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = "Stop"

$parserPath = Join-Path $RootPath "src/appProvider.cpp"
if (-not (Test-Path -LiteralPath $parserPath -PathType Leaf)) {
  throw "G3A parser source is missing: $parserPath"
}
$parserText = [IO.File]::ReadAllText($parserPath)
if (-not $parserText.Contains("Bfile_ReadFile_OS(handle, buffer, size, offset)")) {
  throw "G3A parser must use absolute-position reads for independent header fields"
}
if (-not $parserText.Contains("int actualFileSize = Bfile_GetFileSize_OS(handle);")) {
  throw "G3A parser must validate the size reported by the opened file handle"
}

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

$g3a = Get-ChildItem -LiteralPath $RootPath -Filter "*.g3a" -File |
  Select-Object -First 1
if (-not $g3a) { throw "No .g3a artifact was found" }

Add-Type -AssemblyName System.Drawing

$data = [IO.File]::ReadAllBytes($g3a.FullName)
if ($data.Length -lt 0x7004) {
  throw "G3A is shorter than the 0x7000-byte CG50 header"
}

$magic = [byte[]](0xAA, 0xAC, 0xBD, 0xAF, 0x90, 0x88, 0x9A,
                  0x8D, 0xD3, 0xFF, 0xFE, 0xFF, 0xFE, 0xFF)
for ($i = 0; $i -lt $magic.Length; $i++) {
  Assert-Equal $data[$i] $magic[$i] "Unexpected G3A magic at byte $i"
}

Assert-Equal $data[0x24] 1 "G3A active flag is invalid"
Assert-Equal $data[0x25] 1 "G3A validation flag is invalid"

$declaredSize = Read-U32BE $data 0x5C
Assert-Equal $declaredSize ([uint32]$data.Length) "G3A file size field is wrong"

$codeOffset = Read-U32BE $data 0x2E
$checksumOffset = 0x7000 + $codeOffset
Assert-Equal ($checksumOffset + 4) $data.Length "G3A code offset does not point to the trailing checksum"

$storedChecksum = Read-U32BE $data 0x20
$tailChecksum = Read-U32BE $data ($data.Length - 4)
[uint64]$calculatedChecksum = 0
for ($i = 0; $i -lt $data.Length; $i++) {
  if (($i -ge 0x20 -and $i -lt 0x24) -or
      $i -ge ($data.Length - 4)) {
    continue
  }
  $calculatedChecksum += $data[$i]
}
Assert-Equal $storedChecksum ([uint32]$calculatedChecksum) "G3A checksum does not match its contents"
Assert-Equal $tailChecksum $storedChecksum "G3A trailing checksum does not match the header checksum"

$iconSlotSize = 0x2E00
$unselectedIconOffset = 0x1000
$selectedIconOffset = 0x4000
if ($unselectedIconOffset + $iconSlotSize -gt $data.Length -or
    $selectedIconOffset + $iconSlotSize -gt $data.Length) {
  throw "G3A icon slots extend beyond the artifact"
}
$unselectedIcon = $data[$unselectedIconOffset..(
  $unselectedIconOffset + $iconSlotSize - 1)]
$selectedIcon = $data[$selectedIconOffset..(
  $selectedIconOffset + $iconSlotSize - 1)]
$unselectedNonZero = @($unselectedIcon | Where-Object { $_ -ne 0 }).Count
$selectedNonZero = @($selectedIcon | Where-Object { $_ -ne 0 }).Count
if ($unselectedNonZero -lt 100 -or $selectedNonZero -lt 100) {
  throw "G3A icon slot appears empty"
}
$differentIconBytes = 0
for ($i = 0; $i -lt $iconSlotSize; $i++) {
  if ($unselectedIcon[$i] -ne $selectedIcon[$i]) {
    $differentIconBytes++
  }
}
if ($differentIconBytes -lt 100) {
  throw "Selected and unselected G3A icon slots are indistinguishable"
}

function Read-RGB565Pixel([byte[]]$Icon, [int]$X, [int]$Y) {
  $offset = 2 * ($Y * 92 + $X)
  $value = ([int]$Icon[$offset] -shl 8) -bor [int]$Icon[$offset + 1]
  $red = (($value -shr 11) -band 0x1F) * 255 / 31
  $green = (($value -shr 5) -band 0x3F) * 255 / 63
  $blue = ($value -band 0x1F) * 255 / 31
  return @($red, $green, $blue)
}

$embeddedSelectedPixel = Read-RGB565Pixel $selectedIcon 46 20
if ($embeddedSelectedPixel[2] -le $embeddedSelectedPixel[0] -or
    $embeddedSelectedPixel[2] -le $embeddedSelectedPixel[1]) {
  throw "Embedded selected G3A icon tile is not blue"
}

function Test-CG50Icon([string]$Path) {
  $png = [IO.File]::ReadAllBytes($Path)
  if ($png.Length -lt 33) { throw "$Path is not a complete PNG" }
  $signature = [byte[]](137, 80, 78, 71, 13, 10, 26, 10)
  for ($i = 0; $i -lt $signature.Length; $i++) {
    Assert-Equal $png[$i] $signature[$i] "$Path has an invalid PNG signature"
  }
  Assert-Equal (Read-U32BE $png 8) 13 "$Path has an invalid IHDR length"
  Assert-Equal ([Text.Encoding]::ASCII.GetString($png, 12, 4)) "IHDR" "$Path has no IHDR chunk"
  Assert-Equal (Read-U32BE $png 16) 92 "$Path width is not 92 pixels"
  Assert-Equal (Read-U32BE $png 20) 64 "$Path height is not 64 pixels"
  Assert-Equal $png[24] 8 "$Path must use 8-bit PNG channels"
  Assert-Equal $png[25] 2 "$Path must be RGB PNG without alpha or palette"

  $bitmap = New-Object System.Drawing.Bitmap($Path)
  try {
    Assert-Equal $bitmap.PixelFormat `
      ([Drawing.Imaging.PixelFormat]::Format24bppRgb) `
      "$Path must decode as 24bpp RGB"
  } finally {
    $bitmap.Dispose()
  }
}

$selectedPath = Join-Path $RootPath "assets/icons/selected.png"
$unselectedPath = Join-Path $RootPath "assets/icons/unselected.png"
if (-not (Test-Path -LiteralPath $selectedPath -PathType Leaf) -or
    -not (Test-Path -LiteralPath $unselectedPath -PathType Leaf)) {
  throw "CG50 PNG icon sources are missing from assets/icons"
}
Test-CG50Icon $selectedPath
Test-CG50Icon $unselectedPath

function Test-CG50IconLayout([string]$SelectedPath, [string]$UnselectedPath) {
  $selected = New-Object System.Drawing.Bitmap($SelectedPath)
  $unselected = New-Object System.Drawing.Bitmap($UnselectedPath)
  try {
    # The OS owns the lower title strip; it must remain a white canvas.
    # The unselected icon reserves a white lower title strip. The official
    # selected icon instead carries a blue rounded selection backplate behind
    # that strip, with white only at the outer corners.
    for ($y = 43; $y -lt 64; $y++) {
      for ($x = 0; $x -lt 92; $x++) {
        $pixel = $unselected.GetPixel($x, $y)
        if ($pixel.R -ne 255 -or $pixel.G -ne 255 -or $pixel.B -ne 255) {
          throw "unselected.png has artwork in the OS title strip at ($x,$y)"
        }
      }
    }
    for ($x = 0; $x -lt 92; $x++) {
      foreach ($y in @(0, 1, 2, 3, 4, 43, 63)) {
        $pixel = $unselected.GetPixel($x, $y)
        if ($pixel.R -ne 255 -or $pixel.G -ne 255 -or $pixel.B -ne 255) {
          throw "unselected.png outer canvas is not white at ($x,$y)"
        }
      }
    }

    $tilePixel = $unselected.GetPixel(46, 20)
    if ($tilePixel.R -eq 255 -and $tilePixel.G -eq 255 -and
        $tilePixel.B -eq 255) {
      throw "unselected.png has no colored tile at its center"
    }
    $selectedTilePixel = $selected.GetPixel(46, 20)
    if ($selectedTilePixel.R -eq 255 -and $selectedTilePixel.G -eq 255 -and
        $selectedTilePixel.B -eq 255) {
      throw "selected.png has no colored tile at its center"
    }
    if ($selectedTilePixel.B -le $selectedTilePixel.R -or
        $selectedTilePixel.B -le $selectedTilePixel.G) {
      throw "selected.png tile is not blue"
    }
    $selectedBackplatePixel = $selected.GetPixel(10, 50)
    if ($selectedBackplatePixel.B -le $selectedBackplatePixel.R -or
        $selectedBackplatePixel.B -le $selectedBackplatePixel.G) {
      throw "selected.png does not contain the official blue selection backplate"
    }
    $selectedCorner = $selected.GetPixel(0, 0)
    if ($selectedCorner.R -ne 255 -or $selectedCorner.G -ne 255 -or
        $selectedCorner.B -ne 255) {
      throw "selected.png selection backplate is not inset from the outer corner"
    }

    # The white subject mask must be identical in both states. The tile
    # highlight is outside this subject rectangle.
    $subjectPixels = 0
    for ($y = 10; $y -le 38; $y++) {
      for ($x = 15; $x -le 80; $x++) {
        $a = $selected.GetPixel($x, $y)
        $b = $unselected.GetPixel($x, $y)
        $aWhite = $a.R -eq 255 -and $a.G -eq 255 -and $a.B -eq 255
        $bWhite = $b.R -eq 255 -and $b.G -eq 255 -and $b.B -eq 255
        if ($aWhite -ne $bWhite) {
          throw "selected/unselected white subject masks differ at ($x,$y)"
        }
        if ($aWhite) { $subjectPixels++ }
      }
    }
    if ($subjectPixels -lt 20) {
      throw "CG50 icon subject is too small or missing"
    }
    if ($selectedTilePixel.ToArgb() -eq $tilePixel.ToArgb()) {
      throw "selected.png does not provide a distinct selected tile"
    }
  } finally {
    $selected.Dispose()
    $unselected.Dispose()
  }
}

Test-CG50IconLayout $selectedPath $unselectedPath

Write-Output "# CG50 G3A validation"
Write-Output ""
Write-Output "- Artifact: $($g3a.Name)"
Write-Output "- Bytes: $($data.Length)"
Write-Output "- Header: 0x7000 bytes"
Write-Output "- Icon sources: assets/icons/*.png, 92x64, RGB-8, no alpha/palette"
Write-Output "- Icon slots: 0x1000 (unselected), 0x4000 (selected)"
Write-Output "- Icon payloads: non-empty and distinct"
Write-Output "- Checksum: valid"
Write-Output "- Parser: absolute reads and handle-reported size checks present"
exit 0
