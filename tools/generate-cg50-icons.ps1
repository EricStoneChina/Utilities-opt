param(
  [string]$OutputDirectory = (Split-Path -Parent $PSScriptRoot)
)

Add-Type -AssemblyName System.Drawing

function New-UtilitiesIcon {
  param(
    [string]$Path,
    [System.Drawing.Color]$Background
  )

  $bitmap = New-Object System.Drawing.Bitmap(92, 64, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
  $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
  $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::None
  $graphics.Clear($Background)

  $frame = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(0, 17, 50))
  $blue = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(0, 90, 190))
  $cyan = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(0, 205, 220))
  $white = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)

  # Compact calculator-screen badge, using the CG50 blue/cyan palette.
  $graphics.FillRectangle($frame, 10, 7, 72, 50)
  $graphics.FillRectangle($blue, 10, 7, 72, 2)
  $graphics.FillRectangle($blue, 10, 55, 72, 2)
  $graphics.FillRectangle($blue, 10, 7, 2, 50)
  $graphics.FillRectangle($blue, 80, 7, 2, 50)

  # Block U mark.
  $graphics.FillRectangle($cyan, 24, 16, 7, 27)
  $graphics.FillRectangle($blue, 47, 16, 7, 27)
  $graphics.FillRectangle($cyan, 31, 36, 16, 7)
  $graphics.FillRectangle($blue, 27, 13, 24, 3)

  # Small status bars make the icon read as a calculator utility tool.
  $graphics.FillRectangle($white, 59, 19, 13, 3)
  $graphics.FillRectangle($cyan, 59, 27, 9, 3)
  $graphics.FillRectangle($blue, 59, 35, 13, 3)
  $graphics.FillRectangle($cyan, 59, 43, 6, 3)

  $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Bmp)
  $white.Dispose()
  $cyan.Dispose()
  $blue.Dispose()
  $frame.Dispose()
  $graphics.Dispose()
  $bitmap.Dispose()
}

New-UtilitiesIcon (Join-Path $OutputDirectory 'selected.bmp') ([System.Drawing.Color]::FromArgb(225, 225, 225))
New-UtilitiesIcon (Join-Path $OutputDirectory 'unselected.bmp') ([System.Drawing.Color]::Black)
