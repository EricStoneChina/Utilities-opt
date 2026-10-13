param(
  [string]$OutputDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) 'assets/icons')
)

Add-Type -AssemblyName System.Drawing
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null

function New-RoundedRectanglePath {
  param(
    [int]$X,
    [int]$Y,
    [int]$Width,
    [int]$Height,
    [int]$Radius
  )

  $path = New-Object System.Drawing.Drawing2D.GraphicsPath
  $diameter = $Radius * 2
  $path.AddArc($X, $Y, $diameter, $diameter, 180, 90)
  $path.AddArc($X + $Width - $diameter, $Y, $diameter, $diameter, 270, 90)
  $path.AddArc($X + $Width - $diameter, $Y + $Height - $diameter,
               $diameter, $diameter, 0, 90)
  $path.AddArc($X, $Y + $Height - $diameter, $diameter, $diameter, 90, 90)
  $path.CloseFigure()
  return $path
}

function New-GearPoints {
  param(
    [int]$CenterX,
    [int]$CenterY,
    [int]$OuterRadius,
    [int]$InnerRadius
  )

  $points = New-Object System.Collections.Generic.List[System.Drawing.Point]
  for ($i = 0; $i -lt 32; $i++) {
    $angle = (($i * 11.25) - 90) * [Math]::PI / 180
    $radius = if (($i % 4) -lt 2) { $OuterRadius } else { $InnerRadius }
    $x = [int][Math]::Round($CenterX + [Math]::Cos($angle) * $radius)
    $y = [int][Math]::Round($CenterY + [Math]::Sin($angle) * $radius)
    $points.Add((New-Object System.Drawing.Point($x, $y)))
  }
  return $points.ToArray()
}

function New-UtilitiesIcon {
  param(
    [string]$Path,
    [bool]$Selected
  )

  $bitmap = New-Object System.Drawing.Bitmap(
    92, 64, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
  $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
  $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
  $graphics.Clear([System.Drawing.Color]::White)

  # The official selected-state icons carry a blue selection backplate across
  # the full icon cell, including the title strip.  The four outer corners
  # remain white; the colored utility tile is drawn on top of this backplate.
  $selectionBrush = $null
  $selectionPath = $null
  if ($Selected) {
    $selectionBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
      (New-Object System.Drawing.Rectangle(0, 0, 92, 64)),
      [System.Drawing.Color]::FromArgb(140, 223, 255),
      [System.Drawing.Color]::FromArgb(41, 190, 255),
      [System.Drawing.Drawing2D.LinearGradientMode]::ForwardDiagonal)
    $selectionPath = New-RoundedRectanglePath 1 1 90 62 10
    $graphics.FillPath($selectionBrush, $selectionPath)
  }
  # Keep the utility tile and its lower boundary pixel-exact.  The official
  # title strip starts at y=43, so anti-aliasing must not bleed the tile into
  # that reserved row.
  $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::None

  $tileColor = if ($Selected) {
    # The CG50 menu selection is a blue tile, not a cyan tile.
    [System.Drawing.Color]::FromArgb(28, 96, 210)
  } else {
    [System.Drawing.Color]::FromArgb(57, 76, 177)
  }
  $tileHighlight = if ($Selected) {
    [System.Drawing.Color]::FromArgb(100, 166, 250)
  } else {
    [System.Drawing.Color]::FromArgb(115, 131, 220)
  }
  $tileShadow = if ($Selected) {
    [System.Drawing.Color]::FromArgb(14, 50, 137)
  } else {
    [System.Drawing.Color]::FromArgb(31, 49, 127)
  }
  $white = [System.Drawing.Brushes]::White
  $tileBrush = New-Object System.Drawing.SolidBrush($tileColor)
  $highlightBrush = New-Object System.Drawing.SolidBrush($tileHighlight)
  $shadowBrush = New-Object System.Drawing.SolidBrush($tileShadow)
  $gearCutoutBrush = New-Object System.Drawing.SolidBrush($tileColor)

  # Official-style compact tile: x=5..86, y=5..42, with a clear white
  # canvas and a reserved lower strip for the OS-supplied add-in name.
  $shadowPath = New-RoundedRectanglePath 6 6 82 37 7
  $graphics.FillPath($shadowBrush, $shadowPath)
  $shadowPath.Dispose()
  $tilePath = New-RoundedRectanglePath 5 5 82 37 7
  $graphics.FillPath($tileBrush, $tilePath)
  $tilePath.Dispose()

  # A restrained highlight follows the top and left edges without changing
  # the subject between selected and unselected states.
  $graphics.FillRectangle($highlightBrush, 13, 7, 58, 2)
  $graphics.FillRectangle($highlightBrush, 8, 12, 2, 22)

  # White gear silhouette: a single, high-contrast utility symbol.
  $gear = New-GearPoints 34 24 12 9
  $graphics.FillPolygon($white, $gear)
  $graphics.FillEllipse($gearCutoutBrush, 30, 20, 8, 8)
  $graphics.FillRectangle($tileBrush, 33, 11, 2, 4)
  $graphics.FillRectangle($tileBrush, 33, 33, 2, 4)
  $graphics.FillRectangle($tileBrush, 21, 23, 4, 2)
  $graphics.FillRectangle($tileBrush, 43, 23, 4, 2)

  # Three large control sliders make the control-center purpose readable at
  # the native 92x64 resolution without adding text or tiny decorations.
  for ($y = 15; $y -le 31; $y += 8) {
    $graphics.FillRectangle($white, 51, $y, 27, 2)
  }
  $graphics.FillRectangle($tileBrush, 60, 12, 3, 8)
  $graphics.FillRectangle($white, 58, 13, 7, 6)
  $graphics.FillRectangle($tileBrush, 70, 20, 3, 8)
  $graphics.FillRectangle($white, 68, 21, 7, 6)
  $graphics.FillRectangle($tileBrush, 55, 28, 3, 8)
  $graphics.FillRectangle($white, 53, 29, 7, 6)

  # mkg3a expects an RGB-8 PNG for the editable icon source. Keep the bitmap
  # explicitly 24bpp so the PNG encoder cannot introduce a palette or alpha.
  $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
  if ($selectionPath -ne $null) { $selectionPath.Dispose() }
  if ($selectionBrush -ne $null) { $selectionBrush.Dispose() }
  $gearCutoutBrush.Dispose()
  $shadowBrush.Dispose()
  $highlightBrush.Dispose()
  $tileBrush.Dispose()
  $graphics.Dispose()
  $bitmap.Dispose()
}

New-UtilitiesIcon (Join-Path $OutputDirectory 'selected.png') $true
New-UtilitiesIcon (Join-Path $OutputDirectory 'unselected.png') $false
