# Utilities-opt CG50 interface lab

Utilities-opt is being evolved into a safety-first control center for the
fx-CG 50. The native interface layer is intentionally small so that hardware
register access does not leak into feature code.

## Implemented in the current branch

- platformProvider: model/emulator/OS detection, capability flags, and the
  CG50 keyboard matrix probe. Device-ID reads are also gated by a recognized
  model, so unknown hardware falls back to n/a instead of touching legacy
  fixed addresses. Function-key color access is also isolated behind this
  layer and gated by the same model check.
- diagnosticsProvider and diagnosticsGUI: an eight-entry event history,
  accessible from the diagnostics page with F2, plus last error, matrix-code
  sampling, SHIFT/EXIT combination status, and a timer heartbeat.
- Run-Mat return cleanup: timer stop/deinstall, key/F-key/cursor cleanup, and
  recovery through the existing setjmp return point.
- storageProvider: nested storage-operation guards that stop the Utilities
  chronometer before Bfile operations and restore it afterward.
- Device dashboard: model, OS, storage/main-memory headroom, centivolt battery
  reading, date/time, power timeouts, native system-language code, Utilities UI
  language/version, and capabilities.
- Read-only G3A application audit: metadata, size, code offset, native icon
  preview, duplicate names, and header/checksum status. F4 opens a decoded
  92x64 RGB565 icon without modifying the add-in.
- Optional application quarantine: F5 requires two confirmations, creates a
  never-overwritten root backup before removal, and leaves the original
  untouched if copying or backup verification fails.
- Reversible quarantine recovery: Ctrl+F5 lists UTILS-APP-*.bak files,
  restores only when the original root name is absent, and retains the backup
  after success for repeatable testing or manual recovery.
- The legacy advanced Add-In Manager no longer performs a direct delete; its
  remove action routes through the same verified backup and quarantine path.
- Utilities configuration backup and rollback with version validation and a
  second confirmation before restore.
- Main Memory backup handoff: the configuration page can open the native
  Memory application after confirmation and explains the official
  `BACKUP -> SAVE` flow. Utilities stops its own periodic callback before the
  handoff and never enumerates or dumps raw MCS addresses.
- Screen tools and performance lab: screenshot handoff, RGB565 swatches,
  pixel coordinates, full fill/refresh, frame-rate timing, arithmetic, memory,
  VRAM, timer, and file-I/O timings.
  Saved reports include the model, OS build/revision, emulator status, and
  capability mask, startup time, and page-refresh time so results can be
  compared across CG50 installations. The benchmark temporarily pauses the
  Utilities chronometer so its Timer test does not compete with the product's
  own periodic callback, then restores it.
- Native system-settings handoff through APP_SYSTEM_*; Utilities does not
  write OS settings or flash directly.
- Link transfer handoff through the calculator's native Link dialog; custom
  serial/file synchronization remains outside the first implementation.
- Legacy fixed-address system-information and registry reads are not used on
  CG50/unknown models; those screens fall back to the public platform probe.
- storage-audit.ps1 checks independently callable C++ and PicoC file/MCS
  entry points for the timer guard, with conversion-only helpers explicitly
  excluded.
- localization-audit.ps1 checks that the fixed labels of the new control
  center pages have Chinese translations before CI publishes an artifact.

## Deliberate limits

The application scan is read-only by default. The explicitly marked quarantine
action can remove a selected add-in only after creating and verifying a
non-overwriting backup; the separate recovery action refuses to overwrite an
existing add-in and keeps its backup. Configuration restore only touches
Utilities' own settings and creates a rollback copy first. Utilities does not
implement a parallel Main Memory serializer: the F3 handoff opens the
calculator's native Memory workflow, which is responsible for creating
`BACKUP.g3m`. Custom Link/USB synchronization remains deferred; the Link page
opens the calculator's native Link workflow for that operation. The public MCS interface
selects a known directory/item pair rather than exposing a safe full
enumeration; see
[MCSGetDlen2](https://prizm.cemetech.net/Syscalls/MCS/MCSGetDlen2/) and the
[Prizm file-system documentation](https://prizm.cemetech.net/OS_Information/File_System/).
The official fx-CG50 manual documents the native Memory application's
`BACKUP`/`SAVE` operation in its Main Memory section:
[fx-CG50 User's Guide](https://www.casio.com/content/dam/casio/global/support/manuals/calculators/pdf/004-en/f/fx-CG50_Soft_v300_EN.pdf).

Utilities does not expose OS flashing, bootloader/flash erase, forced language
changes, default overclocking, exam-mode bypasses, or password-history
reading. The legacy userInfo() entry is retained only as a compatibility
screen and now reports that sensitive information is hidden.

## Real CG50 acceptance checklist

The emulator is useful for compilation and layout checks, but the following
must be completed on a real CG50:

1. Enter and leave Run-Mat 100 consecutive times.
2. Test EXIT, SHIFT+EXIT, MENU, and long-press/repeated-key cases.
3. Confirm the diagnostics heartbeat and matrix queue after each return.
4. Scan a storage card containing valid, duplicate, truncated, and corrupted
   .g3a files while a chronometer is running.
5. Verify the device dashboard is read-only and the battery is displayed as
   volts from the centivolt API, not as an invented percentage.
6. Create, restore, and intentionally reject a Utilities configuration
   backup; confirm the rollback file remains available after a failed write.
7. Run the performance lab repeatedly and record the results for the same OS
   and battery condition.

The repository build is reproducible with PrizmSDK 0.6 through GitHub Actions.
Use docs/CG50_TEST_REPORT.md to record the required real-device runs before
calling the Run-Mat path production-ready.
tools/validate-g3a.ps1, tools/perf-audit.ps1, and tools/storage-audit.ps1 run
before the artifact is published. tools/runmat-audit.ps1 also checks that the
setjmp/longjmp landing point, timer cleanup, and keyboard/F-key cleanup remain
present; it complements, but does not replace, real-CG50 repetition testing.
