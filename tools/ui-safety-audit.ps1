param(
  [string]$RootPath = (Split-Path -Parent $PSScriptRoot)
)

$menu = Get-Content (Join-Path $RootPath 'src/menuGUI.cpp') -Raw
$performance = Get-Content (Join-Path $RootPath 'src/performanceGUI.cpp') -Raw
$appProvider = Get-Content (Join-Path $RootPath 'src/appProvider.cpp') -Raw
$graphics = Get-Content (Join-Path $RootPath 'src/graphicsProvider.cpp') -Raw
$memory = Get-Content (Join-Path $RootPath 'src/memsysGUI.cpp') -Raw
$fileProvider = Get-Content (Join-Path $RootPath 'src/fileProvider.cpp') -Raw
$failures = New-Object System.Collections.Generic.List[string]

if ($menu -notmatch 'availableHeight\s*=\s*8\s*-\s*itemsStartY') {
  $failures.Add('menu drawing is not clamped to the drawable rows')
}
if ($graphics -notmatch '(?s)void\s+drawScreenTitle\s*\(.*?if\s*\(title\s*!=\s*NULL\)\s*drawRectangle\(0,\s*24,\s*LCD_WIDTH_PX,\s*24') {
  $failures.Add('screen title redraw does not preserve the main title when only the subtitle changes')
}
if ($menu -notmatch '(?s)menu->statusText\s*!=\s*NULL\).*?else\s+if\s*\(showTitle\).*?drawRectangle\(0,\s*0,\s*LCD_WIDTH_PX,\s*24') {
  $failures.Add('menus without a status message are not clearing the complete status row')
}
if ($performance -match 'static\s+char\s+report\s*\[1024\]') {
  $failures.Add('benchmark report buffer is static and unsafe for Bfile_WriteFile_OS')
}
if ($performance -notmatch 'PERFORMANCE_REPORT_CAPACITY' -or
    $performance -notmatch 'report\s*=\s*\(char\s*\*\)malloc') {
  $failures.Add('benchmark report does not use a checked writable RAM buffer')
}
if ($performance -notmatch 'fileSize\s*=\s*0\s*;\s*if\s*\(Bfile_CreateEntry_OS\(tempPath') {
  $failures.Add('benchmark report does not create an empty temporary file before sequential writes')
}
if ($performance -match 'static\s+unsigned\s+char\s+fileBenchmarkBuffer') {
  $failures.Add('file benchmark source buffer is static and unsafe for Bfile_WriteFile_OS')
}
if ($appProvider -notmatch '(?s)int\s+quarantineG3ARecord\s*\(.*?buffer\s*=\s*\(unsigned\s+char\*\)malloc\(STORAGE_COPY_BUFFER_SIZE\)') {
  $failures.Add('application backup does not use a checked writable RAM buffer')
}
if ($appProvider -notmatch '(?s)static\s+int\s+copyStorageFileNoOverwrite\s*\(.*?buffer\s*=\s*\(unsigned\s+char\*\)malloc\(STORAGE_COPY_BUFFER_SIZE\)') {
  $failures.Add('application restore does not use a checked writable RAM buffer')
}
if ($graphics -match '(?s)static\s+const\s+char\*\s+prepareMiniText.*?0x03\s*:\s*0x20') {
  $failures.Add('PrintMini wrapper still injects the PrintXY font prefix')
}
if ($memory -match '(?s)static\s+void\s+drawCompactCapacityBar.*?mPrintMini\([^;]*,\s*0x42\b') {
  $failures.Add('memory percentage still passes PrintMiniMini coordinate flags to PrintMini')
}
if ($memory -match 'mPrintMiniMiniAt\(&labelX,\s*&labelTextY,\s*desc') {
  $failures.Add('memory labels use PrintMiniMini, which cannot reliably render Chinese GBK text')
}
if ($performance -match 'int\s+legendY\s*=\s*154') {
  $failures.Add('performance legend still overlaps the final benchmark row')
}
if ($fileProvider -match 'unsigned\s+char\s+in_buf\s*\[\s*FILE_COMPORIGBUF') {
  $failures.Add('file decompression still places the 24 KB input buffer on the add-in stack')
}
if ($fileProvider -match '(?s)void\s+compressFile\s*\(.*?unsigned\s+(?:int|long)\s+read_sz\s*=\s*Bfile_ReadFile_OS') {
  $failures.Add('compression/decompression still stores Bfile read errors in an unsigned length')
}
if ($fileProvider -notmatch '(?s)void\s+compressFile\s*\(.*?transformFailed\s*=\s*1.*?Bfile_DeleteEntry\(tempfilenameshort\)') {
  $failures.Add('compression/decompression does not discard failed temporary output')
}
if ($memory -match 'drawFkeyLabels\(0\s*,\s*0\s*,\s*0\s*,\s*0\s*,\s*0\s*,\s*0\s*\)') {
  $failures.Add('native settings uses a zero F-key descriptor instead of clearing the display bar')
}

if ($failures.Count -gt 0) {
  $failures | ForEach-Object { Write-Error $_ }
  exit 1
}

'UI_SAFETY_AUDIT=pass'
exit 0
