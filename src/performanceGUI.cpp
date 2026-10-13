#include <fxcg/display.h>
#include <fxcg/file.h>
#include <fxcg/keyboard.h>
#include <fxcg/rtc.h>
#include <fxcg/system.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "graphicsProvider.hpp"
#include "chronoGUI.hpp"
#include "constantsProvider.hpp"
#include "fileProvider.hpp"
#include "keyboardProvider.hpp"
#include "platformProvider.hpp"
#include "storageProvider.hpp"
#include "stringsProvider.hpp"
#include "timeProvider.hpp"

#define PERFORMANCE_RESULT_COUNT 5
#define PERFORMANCE_COPY_BYTES 4096
#define PERFORMANCE_COPY_PASSES 32
#define PERFORMANCE_VRAM_PASSES 4
#define PERFORMANCE_REPORT_CAPACITY 768
#define SCREEN_FILL_REFRESH_PASSES 8
#define SCREEN_FRAMERATE_PASSES 24

/* Keep storage failures distinguishable on the calculator.  The old UI
 * collapsed every Bfile failure into SAVE FAILED, which made a harmless
 * filename collision indistinguishable from a broken create/open/rename
 * syscall. */
#define SAVE_RESULT_EXISTS          (-2)
#define SAVE_RESULT_NO_BFILE        (-3)
#define SAVE_RESULT_STORAGE_GUARD   (-4)
#define SAVE_RESULT_ALLOC            (-5)
#define SAVE_RESULT_TEMP_COLLISION  (-6)
#define SAVE_RESULT_CREATE          (-10)
#define SAVE_RESULT_OPEN            (-11)
#define SAVE_RESULT_SEEK            (-12)
#define SAVE_RESULT_WRITE           (-13)
#define SAVE_RESULT_VERIFY_OPEN     (-14)
#define SAVE_RESULT_VERIFY_SIZE     (-15)
#define SAVE_RESULT_RENAME          (-16)

typedef struct {
  int ticks;
  int milliseconds;
  int detail;
  int errorMilliseconds;
  int status;
} BenchmarkResult;

static BenchmarkResult benchmarkResults[PERFORMANCE_RESULT_COUNT];
static unsigned char copySource[PERFORMANCE_COPY_BYTES];
static unsigned char copyDestination[PERFORMANCE_COPY_BYTES];
static volatile unsigned long benchmarkTimerCallbacks = 0;
static int startupMilliseconds = -1;
static int pageRefreshMilliseconds = -1;

void performanceRecordStartupMilliseconds(int milliseconds) {
  if (milliseconds >= 0 && startupMilliseconds < 0)
    startupMilliseconds = milliseconds;
}

static void resetBenchmarkResults() {
  for (int i = 0; i < PERFORMANCE_RESULT_COUNT; i++) {
    benchmarkResults[i].ticks = 0;
    benchmarkResults[i].milliseconds = 0;
    benchmarkResults[i].detail = 0;
    benchmarkResults[i].errorMilliseconds = 0;
    benchmarkResults[i].status = -2;
  }
}

static void benchmarkTimerHandler() {
  benchmarkTimerCallbacks++;
}

static void setBenchmarkResult(BenchmarkResult* result, int start, int detail,
                               int status) {
  int end = RTC_GetTicks();
  result->ticks = end - start;
  result->milliseconds = tickDifferenceToMilliseconds(start, end);
  result->detail = detail;
  result->errorMilliseconds = 0;
  result->status = status;
}

static const char* benchmarkStatusText(const char* message) {
  /* This transient view must remain readable even on OS builds whose compact
   * renderer has incomplete GBK coverage. */
  if (!strcmp(message, "Report saved.")) return "REPORT SAVED";
  if (!strcmp(message, "Report exists; not overwritten.")) return "REPORT EXISTS";
  if (!strcmp(message, "Save create failed.")) return "FAIL CREATE";
  if (!strcmp(message, "Save open failed.")) return "FAIL OPEN";
  if (!strcmp(message, "Save seek failed.")) return "FAIL SEEK";
  if (!strcmp(message, "Save write failed.")) return "FAIL WRITE";
  if (!strcmp(message, "Save verify-open failed.")) return "FAIL VERIFY";
  if (!strcmp(message, "Save verify-size failed.")) return "FAIL SIZE";
  if (!strcmp(message, "Save rename failed.")) return "FAIL RENAME";
  if (!strcmp(message, "Save storage unavailable.")) return "NO STORAGE";
  if (!strcmp(message, "Save memory unavailable.")) return "NO MEMORY";
  if (!strcmp(message, "Save could not be started.")) return "SAVE FAILED";
  if (!strcmp(message, "Report could not be saved.")) return "SAVE FAILED";
  return "RUNNING";
}

static void drawBenchmarkStatus(const char* message) {
  Bdisp_AllClr_VRAM();
  drawScreenTitle("Performance lab");
  int textX = 12;
  int textY = 82;
  mPrintMiniMiniRawAt(&textX, &textY, benchmarkStatusText(message), 0,
                      TEXT_COLOR_BLACK, 0);
  textX = 12;
  textY = 132;
  mPrintMiniMiniRawAt(&textX, &textY, "PRESS ANY KEY", 0,
                      TEXT_COLOR_BLACK, 0);
  Bdisp_PutDisp_DD();
}

static void runArithmeticBenchmark(BenchmarkResult* result) {
  volatile unsigned long value = 0x13579BDF;
  int start = RTC_GetTicks();
  for (int i = 0; i < 120000; i++) {
    value = value * 1664525UL + 1013904223UL + (unsigned long)i;
  }
  setBenchmarkResult(result, start, (int)(value & 0x7FFFFFFF), 0);
}

static void runCopyBenchmark(BenchmarkResult* result) {
  for (int i = 0; i < PERFORMANCE_COPY_BYTES; i++)
    copySource[i] = (unsigned char)(i * 13 + 7);
  int start = RTC_GetTicks();
  for (int pass = 0; pass < PERFORMANCE_COPY_PASSES; pass++) {
    for (int i = 0; i < PERFORMANCE_COPY_BYTES; i++)
      copyDestination[i] = (unsigned char)(copySource[i] ^ pass);
  }
  volatile unsigned char checksum = 0;
  for (int i = 0; i < PERFORMANCE_COPY_BYTES; i++)
    checksum ^= copyDestination[i];
  setBenchmarkResult(result, start,
                     PERFORMANCE_COPY_BYTES * PERFORMANCE_COPY_PASSES,
                     0);
  if (checksum == 0xFF) copyDestination[0] ^= checksum;
}

static void runVramBenchmark(BenchmarkResult* result) {
  int start = RTC_GetTicks();
  for (int pass = 0; pass < PERFORMANCE_VRAM_PASSES; pass++) {
    drawRectangle(0, 24, LCD_WIDTH_PX, LCD_HEIGHT_PX - 48,
                  pass & 1 ? COLOR_BLUE : COLOR_CYAN);
  }
  setBenchmarkResult(result, start,
                     LCD_WIDTH_PX * (LCD_HEIGHT_PX - 48) *
                       PERFORMANCE_VRAM_PASSES,
                     0);
  Bdisp_PutDisp_DD();
}

static void runTimerBenchmark(BenchmarkResult* result) {
  benchmarkTimerCallbacks = 0;
  if (!platformHasCapability(PLATFORM_CAP_TIMER)) {
    result->ticks = 0;
    result->milliseconds = 0;
    result->detail = -1;
    result->errorMilliseconds = 0;
    result->status = -1;
    return;
  }
  int timer = Timer_Install(0, benchmarkTimerHandler, 50);
  if (timer <= 0) {
    result->ticks = 0;
    result->milliseconds = 0;
    result->detail = timer;
    result->errorMilliseconds = 0;
    result->status = -1;
    return;
  }
  Timer_Start(timer);
  int start = RTC_GetTicks();
  blockForMilliseconds(500);
  int end = RTC_GetTicks();
  Timer_Stop(timer);
  Timer_Deinstall(timer);
  result->ticks = end - start;
  result->milliseconds = tickDifferenceToMilliseconds(start, end);
  result->detail = (int)benchmarkTimerCallbacks;
  result->errorMilliseconds = result->milliseconds -
                              (int)benchmarkTimerCallbacks * 50;
  result->status = 0;
}

static int runFileBenchmark(BenchmarkResult* result) {
  StorageGuard guard = {0};
  if (!platformHasCapability(PLATFORM_CAP_BFILE)) {
    result->ticks = 0;
    result->milliseconds = 0;
    result->detail = -1;
    result->errorMilliseconds = 0;
    result->status = -1;
    return -1;
  }
  if (storageOperationBegin(&guard) < 0) {
    result->ticks = 0;
    result->milliseconds = 0;
    result->detail = -1;
    result->errorMilliseconds = 0;
    result->status = -1;
    return -1;
  }

  const char* filename = SMEM_PREFIX "UTPERF.DAT";
  unsigned short path[MAX_FILENAME_SIZE + 1];
  Bfile_StrToName_ncpy(path, filename, MAX_FILENAME_SIZE + 1);
  int existing = Bfile_OpenFile_OS(path, READ, 0);
  if (existing >= 0) {
    Bfile_CloseFile_OS(existing);
    storageOperationEnd(&guard);
    result->ticks = 0;
    result->milliseconds = 0;
    result->detail = -2;
    result->errorMilliseconds = 0;
    result->status = -1;
    return -1;
  }

  /*
   * Bfile_WriteFile_OS requires a writable main-RAM source buffer.  A static
   * buffer works on some OS revisions but can make the file benchmark reboot
   * on a CG50, so keep this buffer heap-backed and release it on every path.
   */
  unsigned char* fileBenchmarkBuffer =
      (unsigned char*)malloc(PERFORMANCE_COPY_BYTES);
  if (fileBenchmarkBuffer == NULL) {
    storageOperationEnd(&guard);
    result->ticks = 0;
    result->milliseconds = 0;
    result->detail = -3;
    result->errorMilliseconds = 0;
    result->status = -1;
    return -1;
  }

  for (int i = 0; i < PERFORMANCE_COPY_BYTES; i++)
    fileBenchmarkBuffer[i] = (unsigned char)(i ^ 0x5A);

  size_t fileSize = PERFORMANCE_COPY_BYTES;
  int created = Bfile_CreateEntry_OS(path, CREATEMODE_FILE, &fileSize) >= 0;
  int handle = created ? Bfile_OpenFile_OS(path, READWRITE, 0) : -1;
  int start = 0;
  int end = 0;
  int success = handle >= 0;
  if (success) {
    start = RTC_GetTicks();
    success = Bfile_SeekFile_OS(handle, 0) >= 0 &&
              Bfile_WriteFile_OS(handle, fileBenchmarkBuffer,
                                 PERFORMANCE_COPY_BYTES) ==
              PERFORMANCE_COPY_BYTES;
    success = success &&
              Bfile_SeekFile_OS(handle, 0) >= 0 &&
              Bfile_ReadFile_OS(handle, copyDestination,
                                PERFORMANCE_COPY_BYTES, -1) ==
                PERFORMANCE_COPY_BYTES;
    end = RTC_GetTicks();
    Bfile_CloseFile_OS(handle);
  }
  free(fileBenchmarkBuffer);
  if (created) Bfile_DeleteEntry(path);
  storageOperationEnd(&guard);

  result->ticks = success ? end - start : 0;
  result->milliseconds = success ? tickDifferenceToMilliseconds(start, end) : 0;
  result->detail = success ? PERFORMANCE_COPY_BYTES : -3;
  result->errorMilliseconds = 0;
  result->status = success ? 0 : -1;
  return success ? 0 : -1;
}

static void runAllBenchmarks() {
  int chronometerWasRunning = isStubTimerRunning();
  if (chronometerWasRunning) stopAndUninstallStubTimer();
  drawBenchmarkStatus("Running benchmarks...");
  runArithmeticBenchmark(&benchmarkResults[0]);
  runCopyBenchmark(&benchmarkResults[1]);
  runVramBenchmark(&benchmarkResults[2]);
  runTimerBenchmark(&benchmarkResults[3]);
  runFileBenchmark(&benchmarkResults[4]);
  if (chronometerWasRunning) restartStubTimerFunction();
}

static int appendPerformanceText(char* report, int capacity, int* offset,
                                 const char* text) {
  if (report == NULL || offset == NULL || text == NULL || *offset < 0 ||
      *offset >= capacity) return -1;
  int added = (int)strlen(text);
  if (added >= capacity - *offset) return -1;
  memcpy(report + *offset, text, added);
  *offset += added;
  report[*offset] = 0;
  return 0;
}

static int saveBenchmarkReport() {
  /*
   * Bfile_WriteFile_OS must receive a writable main-RAM buffer.  Allocate the
   * report on the heap so this page does not consume a 1 KB stack frame on the
   * CG50, whose add-in stack is much smaller than a desktop stack.  The path
   * is deliberately sized to the actual root filename instead of the 270-byte
   * maximum file-browser path, keeping the remaining stack footprint small.
   */
  char* report = NULL;
  unsigned short path[32] = {0};
  unsigned short tempPath[32] = {0};
  const char* filename = SMEM_PREFIX "UTPERF.TXT";
  char tempFilename[40];
  PlatformInfo platform;
  int offset = 0;
  int handle = -1;
  int existing;
  size_t fileSize;
  int result = -1;
  int created = 0;
  int tempExisting = 0;
  int verifySize = -1;
  char verifyChunk[64];
  char hexadecimal[9];
  char line[160];

  if (!platformHasCapability(PLATFORM_CAP_BFILE)) return SAVE_RESULT_NO_BFILE;
  report = (char*)malloc(PERFORMANCE_REPORT_CAPACITY);
  if (report == NULL) return SAVE_RESULT_ALLOC;
  report[0] = 0;
  getPlatformInfo(&platform);
  if (appendPerformanceText(report, PERFORMANCE_REPORT_CAPACITY, &offset,
                            "Utilities performance report\r\n") < 0 ||
      appendPerformanceText(report, PERFORMANCE_REPORT_CAPACITY, &offset,
                            "RTC: 1/128 second\r\n\r\n") < 0) {
    free(report);
    return -1;
  }
  sprintf(line, "Model: %s\r\n", platformModelName(platform.model));
  if (appendPerformanceText(report, PERFORMANCE_REPORT_CAPACITY, &offset,
                            line) < 0) {
    free(report);
    return -1;
  }
  sprintf(line, "OS: %d.%d build %d revision %d\r\n",
          (int)platform.osMajor, (int)platform.osMinor,
          (int)platform.osBuild, (int)platform.osRevision);
  if (appendPerformanceText(report, PERFORMANCE_REPORT_CAPACITY, &offset,
                            line) < 0) {
    free(report);
    return -1;
  }
  sprintf(line, "Real hardware: %s\r\n",
          platform.isEmulated < 0 ? "Unknown" : platform.isEmulated ? "No" : "Yes");
  if (appendPerformanceText(report, PERFORMANCE_REPORT_CAPACITY, &offset,
                            line) < 0) {
    free(report);
    return -1;
  }
  formatHex32(hexadecimal, platform.capabilities);
  sprintf(line, "Capabilities: %s\r\n\r\n", hexadecimal);
  if (appendPerformanceText(report, PERFORMANCE_REPORT_CAPACITY, &offset,
                            line) < 0) {
    free(report);
    return -1;
  }
  if (startupMilliseconds >= 0)
    sprintf(line, "Startup: %d ms\r\n", startupMilliseconds);
  else
    strcpy(line, "Startup: unavailable\r\n");
  if (appendPerformanceText(report, PERFORMANCE_REPORT_CAPACITY, &offset,
                            line) < 0) {
    free(report);
    return -1;
  }
  if (pageRefreshMilliseconds >= 0)
    sprintf(line, "Page refresh (4x): %d ms\r\n\r\n",
            pageRefreshMilliseconds);
  else
    strcpy(line, "Page refresh (4x): unavailable\r\n\r\n");
  if (appendPerformanceText(report, PERFORMANCE_REPORT_CAPACITY, &offset,
                            line) < 0) {
    free(report);
    return -1;
  }
  for (int i = 0; i < PERFORMANCE_RESULT_COUNT; i++) {
    const char* label;
    switch (i) {
      case 0: label = "Arithmetic"; break;
      case 1: label = "Memory copy"; break;
      case 2: label = "VRAM draw"; break;
      case 3: label = "Timer"; break;
      default: label = "File I/O"; break;
    }
    if (benchmarkResults[i].status < 0) {
      sprintf(line, "%s: unavailable\r\n", label);
      if (appendPerformanceText(report, PERFORMANCE_REPORT_CAPACITY, &offset,
                                line) < 0) {
        free(report);
        return -1;
      }
    } else {
      sprintf(line, "%s: %d ticks, %d ms\r\n", label,
              benchmarkResults[i].ticks, benchmarkResults[i].milliseconds);
      if (appendPerformanceText(report, PERFORMANCE_REPORT_CAPACITY, &offset,
                                line) < 0) {
        free(report);
        return -1;
      }
    }
  }
  sprintf(line, "Timer callbacks: %d\r\n", benchmarkResults[3].detail);
  if (appendPerformanceText(report, PERFORMANCE_REPORT_CAPACITY, &offset,
                            line) < 0) {
    free(report);
    return -1;
  }
  sprintf(line, "Timer period error: %d ms\r\n",
          benchmarkResults[3].errorMilliseconds);
  if (appendPerformanceText(report, PERFORMANCE_REPORT_CAPACITY, &offset,
                            line) < 0 || offset <= 0) {
    free(report);
    return -1;
  }

  StorageGuard guard = {0};
  if (storageOperationBegin(&guard) < 0) {
    free(report);
    return SAVE_RESULT_STORAGE_GUARD;
  }

  // Never write the user-visible filename in place.  A uniquely named
  // temporary file means a power loss or a syscall failure cannot leave a
  // misleading partial UTPERF.TXT behind, and avoids asking the OS to replace
  // an existing report.
  Bfile_StrToName_ncpy(path, filename, sizeof(path) / sizeof(path[0]));
  existing = Bfile_OpenFile_OS(path, READ, 0);
  if (existing >= 0) {
    Bfile_CloseFile_OS(existing);
    result = SAVE_RESULT_EXISTS;
    goto save_done;
  }

  tempExisting = 1;
  for (int attempt = 0; attempt < 8; attempt++) {
    formatHex32(hexadecimal, (unsigned int)(RTC_GetTicks() + attempt));
    sprintf(tempFilename, SMEM_PREFIX "UTP%s.TMP", hexadecimal + 4);
    Bfile_StrToName_ncpy(tempPath, tempFilename,
                         sizeof(tempPath) / sizeof(tempPath[0]));
    existing = Bfile_OpenFile_OS(tempPath, READ, 0);
    if (existing < 0) {
      tempExisting = 0;
      break;
    }
    Bfile_CloseFile_OS(existing);
  }
  if (tempExisting) {
    // An extremely unlikely name collision is safer to report than to
    // overwrite an unrelated file.
    result = SAVE_RESULT_TEMP_COLLISION;
    goto save_done;
  }

  /* Create an empty file and let the sequential writer extend it.  Some CG50
   * OS revisions are unreliable when a newly preallocated file is reopened
   * for an in-place write, which was the remaining reset path on Save TXT. */
  fileSize = 0;
  if (Bfile_CreateEntry_OS(tempPath, CREATEMODE_FILE, &fileSize) < 0) {
    result = SAVE_RESULT_CREATE;
    goto save_done;
  }
  created = 1;
  handle = Bfile_OpenFile_OS(tempPath, READWRITE, 0);
  if (handle < 0) {
    result = SAVE_RESULT_OPEN;
    goto save_done;
  }
  // Start at zero so byte-count and file-position return conventions agree.
  if (Bfile_SeekFile_OS(handle, 0) < 0) {
    result = SAVE_RESULT_SEEK;
    goto save_done;
  }
  if (!storageWriteFile(handle, report, offset)) {
    result = SAVE_RESULT_WRITE;
    goto save_done;
  }
  Bfile_CloseFile_OS(handle);
  handle = -1;

  // Reopen read-only and verify the complete temporary file before exposing
  // it under the final name.
  handle = Bfile_OpenFile_OS(tempPath, READ, 0);
  if (handle < 0) {
    result = SAVE_RESULT_VERIFY_OPEN;
    goto save_done;
  }
  verifySize = Bfile_GetFileSize_OS(handle);
  if (verifySize != offset) {
    result = SAVE_RESULT_VERIFY_SIZE;
    goto save_done;
  }
  for (int position = 0; position < offset;) {
    int chunk = offset - position;
    if (chunk > (int)sizeof(verifyChunk)) chunk = sizeof(verifyChunk);
    if (Bfile_ReadFile_OS(handle, verifyChunk, chunk, position) != chunk ||
        memcmp(verifyChunk, report + position, chunk) != 0) {
      result = SAVE_RESULT_VERIFY_SIZE;
      goto save_done;
    }
    position += chunk;
  }
  Bfile_CloseFile_OS(handle);
  handle = -1;

  // Check again immediately before publishing in case the user created the
  // final name while the report was being written.
  existing = Bfile_OpenFile_OS(path, READ, 0);
  if (existing >= 0) {
    Bfile_CloseFile_OS(existing);
    result = SAVE_RESULT_EXISTS;
    goto save_done;
  }
  if (Bfile_RenameEntry(tempPath, path) < 0) {
    result = SAVE_RESULT_RENAME;
    goto save_done;
  }
  created = 0;
  result = 0;
save_done:
  if (handle >= 0) Bfile_CloseFile_OS(handle);
  // Do not leave a zero-byte or partial temporary report behind after a
  // failed save. Existing user files are never touched.
  if (result != 0 && created) Bfile_DeleteEntry(tempPath);
  storageOperationEnd(&guard);
  free(report);
  return result;
}

static void formatBenchmark(char* buffer, const BenchmarkResult* result) {
  if (result->status == -2) {
    // Keep this ASCII.  The Chinese translation of "not run" is GBK and the
    // mini-mini renderer is not a general Chinese text renderer.
    strcpy(buffer, "NOT RUN");
    return;
  }
  if (result->status < 0) {
    strcpy(buffer, "N/A");
    return;
  }
  // The value is rendered in the 18 px font starting at column 12. Remove
  // optional spaces so the largest normal result remains on-screen.
  sprintf(buffer, "%dt/%dms", result->ticks, result->milliseconds);
}

static void drawBenchmarkField(int row, const char* label, const char* value,
                               int color) {
  int textX = 145;
  int textY = 52 + (row - 2) * 24;
  /* Labels use PrintXY so the Chinese translation is rendered by the
   * GB18030-aware 24-pixel path.  Results are ASCII and use the compact
   * variable-width font, which leaves room for long tick counts. */
  mPrintXY(1, row, label, TEXT_MODE_TRANSPARENT_BACKGROUND,
           TEXT_COLOR_BLUE);
  mPrintMiniMiniRawClippedAt(&textX, &textY, value, 0, color,
                             LCD_WIDTH_PX - 149, 0);
}

static void drawBenchmarkScreen() {
  char value[40];
  Bdisp_AllClr_VRAM();
  drawScreenTitle("Performance lab");
  const char* labels[PERFORMANCE_RESULT_COUNT] = {
    "Arithmetic", "Memory copy", "VRAM draw", "Timer", "File I/O"
  };
  for (int i = 0; i < PERFORMANCE_RESULT_COUNT; i++) {
    formatBenchmark(value, &benchmarkResults[i]);
    if (i == 3 && benchmarkResults[i].status >= 0) {
      sprintf(value, "%dt/%dms C%d", benchmarkResults[i].ticks,
              benchmarkResults[i].milliseconds, benchmarkResults[i].detail);
    }
    drawBenchmarkField(i + 2, labels[i], value,
                       benchmarkResults[i].status < 0 ?
                         COLOR_RED : COLOR_BLACK);
  }
  /* F-key icons vary by OS language and are not always available.  Keep a
   * compact ASCII legend in the free strip so the three actions remain
   * discoverable even when an icon descriptor is missing. */
  // Put the legend beside the title, rather than in row 6 where it used to
  // overwrite the final "File I/O" label.
  int legendX = 180;
  int legendY = 34;
  mPrintMiniMiniRawClippedAt(&legendX, &legendY, "F1 RUN F2 CLEAR F3 SAVE", 0,
                             TEXT_COLOR_BLACK, LCD_WIDTH_PX - legendX, 0);
  drawFkeyLabels(0x040A, 0x0149, 0x0302, 0, 0, 0); // RUN, CLEAR, SAVE
  Bdisp_PutDisp_DD();
}

static void drawScreenToolsHome() {
  Bdisp_AllClr_VRAM();
  drawScreenTitle("Screen tools");
  int textX = 4;
  int textY = 58;
  mPrintMiniRaw(&textX, &textY, "F1  Save screenshot", 0,
                0xFFFFFFFF, 0, 0, COLOR_BLACK, COLOR_WHITE, 1, 0);
  textX = 4; textY = 76;
  mPrintMiniRaw(&textX, &textY, "F2  RGB565 color test", 0,
                0xFFFFFFFF, 0, 0, COLOR_BLACK, COLOR_WHITE, 1, 0);
  textX = 4; textY = 94;
  mPrintMiniRaw(&textX, &textY, "F3  Pixel coordinates", 0,
                0xFFFFFFFF, 0, 0, COLOR_BLACK, COLOR_WHITE, 1, 0);
  textX = 4; textY = 112;
  mPrintMiniRaw(&textX, &textY, "F4  Full fill/refresh", 0,
                0xFFFFFFFF, 0, 0, COLOR_BLACK, COLOR_WHITE, 1, 0);
  textX = 4; textY = 130;
  mPrintMiniRaw(&textX, &textY, "F5  Frame rate", 0,
                0xFFFFFFFF, 0, 0, COLOR_BLACK, COLOR_WHITE, 1, 0);
  textX = 4; textY = 145;
  mPrintMiniRaw(&textX, &textY, "Tests draw to VRAM only until refresh.",
                0, 0xFFFFFFFF, 0, 0, COLOR_BLACK, COLOR_WHITE, 1, 0);
  drawRectangle(0, LCD_HEIGHT_PX - 24, LCD_WIDTH_PX, 24, COLOR_WHITE);
  textX = 4; textY = 196;
  mPrintMiniMiniRawAt(&textX, &textY,
                      "F1 SAVE  F2 RGB  F3 XY  F4 FILL  F5 FPS", 0,
                      TEXT_COLOR_BLACK, 0);
  Bdisp_PutDisp_DD();
}

static int measurePageRefreshMilliseconds() {
  const int passes = 4;
  const int start = RTC_GetTicks();
  for (int pass = 0; pass < passes; pass++) {
    Bdisp_AllClr_VRAM();
    drawScreenTitle("Performance lab");
    Bdisp_PutDisp_DD();
  }
  return tickDifferenceToMilliseconds(start, RTC_GetTicks());
}

static void drawRGB565Test() {
  const color_t colors[6] = {
    (color_t)drawRGB24toRGB565(255, 0, 0),
    (color_t)drawRGB24toRGB565(0, 255, 0),
    (color_t)drawRGB24toRGB565(0, 0, 255),
    (color_t)drawRGB24toRGB565(255, 255, 255),
    (color_t)drawRGB24toRGB565(128, 128, 128),
    (color_t)drawRGB24toRGB565(0, 0, 0)
  };
  Bdisp_AllClr_VRAM();
  drawScreenTitle("RGB565 color test");
  for (int i = 0; i < 6; i++) {
    int x = 12 + (i % 3) * 124;
    int y = 60 + (i / 3) * 55;
    drawRectangle(x, y, 105, 38, colors[i]);
  }
  mPrintXY(1, 7, "RGB565: R5 G6 B5", TEXT_MODE_TRANSPARENT_BACKGROUND,
            TEXT_COLOR_BLACK);
  Bdisp_PutDisp_DD();
}

static void drawPixelTest() {
  Bdisp_AllClr_VRAM();
  drawScreenTitle("Pixel coordinates");
  drawRectangle(0, 24, LCD_WIDTH_PX, LCD_HEIGHT_PX - 48, COLOR_WHITE);
  Bdisp_SetPoint_VRAM(0, 24, COLOR_RED);
  Bdisp_SetPoint_VRAM(LCD_WIDTH_PX - 1, 24, COLOR_GREEN);
  Bdisp_SetPoint_VRAM(0, LCD_HEIGHT_PX - 25, COLOR_BLUE);
  Bdisp_SetPoint_VRAM(LCD_WIDTH_PX - 1, LCD_HEIGHT_PX - 25, COLOR_PURPLE);
  Bdisp_SetPoint_VRAM(LCD_WIDTH_PX / 2, LCD_HEIGHT_PX / 2, COLOR_BLACK);
  mPrintXY(1, 3, "(0,24)       (383,24)", TEXT_MODE_TRANSPARENT_BACKGROUND,
           TEXT_COLOR_BLACK);
  mPrintXY(1, 7, "(0,191)     (383,191)", TEXT_MODE_TRANSPARENT_BACKGROUND,
            TEXT_COLOR_BLACK);
  Bdisp_PutDisp_DD();
}

static void drawScreenMetric(int row, const char* label, int value) {
  char buffer[24];
  int textX = 170;
  int textY = 52 + (row - 3) * 24;
  mPrintXY(1, row, label, TEXT_MODE_TRANSPARENT_BACKGROUND,
           TEXT_COLOR_BLUE);
  sprintf(buffer, "%d", value);
  mPrintMiniRaw(&textX, &textY, buffer, 0, 0xFFFFFFFF, 0, 0,
                COLOR_BLACK, COLOR_WHITE, 1, 0);
}

static void drawFullFillRefreshTest() {
  int start = RTC_GetTicks();
  for (int pass = 0; pass < SCREEN_FILL_REFRESH_PASSES; pass++) {
    drawRectangle(0, 24, LCD_WIDTH_PX, LCD_HEIGHT_PX - 48,
                  pass & 1 ? COLOR_BLUE : COLOR_WHITE);
    Bdisp_PutDisp_DD();
  }
  int end = RTC_GetTicks();
  int milliseconds = tickDifferenceToMilliseconds(start, end);
  int refreshesPerSecond = milliseconds > 0 ?
                           (SCREEN_FILL_REFRESH_PASSES * 1000) / milliseconds :
                           0;

  Bdisp_AllClr_VRAM();
  drawScreenTitle("Full fill/refresh");
  drawScreenMetric(3, "Frames:", SCREEN_FILL_REFRESH_PASSES);
  drawScreenMetric(4, "Elapsed:", milliseconds);
  drawScreenMetric(5, "Refresh:", refreshesPerSecond);
  mPrintXY(1, 7, "Press any key to return",
           TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
  Bdisp_PutDisp_DD();
}

static void drawFrameRateTest() {
  int start = RTC_GetTicks();
  for (int frame = 0; frame < SCREEN_FRAMERATE_PASSES; frame++) {
    int x = (frame * (LCD_WIDTH_PX - 40)) /
            (SCREEN_FRAMERATE_PASSES - 1);
    drawRectangle(0, 24, LCD_WIDTH_PX, LCD_HEIGHT_PX - 48, COLOR_WHITE);
    drawRectangle(x, LCD_HEIGHT_PX / 2 - 20, 40, 40,
                  frame & 1 ? COLOR_CYAN : COLOR_BLUE);
    Bdisp_PutDisp_DD();
  }
  int end = RTC_GetTicks();
  int milliseconds = tickDifferenceToMilliseconds(start, end);
  int framesPerSecond = milliseconds > 0 ?
                        (SCREEN_FRAMERATE_PASSES * 1000) / milliseconds : 0;

  Bdisp_AllClr_VRAM();
  drawScreenTitle("Frame rate");
  drawScreenMetric(3, "Frames:", SCREEN_FRAMERATE_PASSES);
  drawScreenMetric(4, "Elapsed:", milliseconds);
  drawScreenMetric(5, "FPS:", framesPerSecond);
  mPrintXY(1, 7, "Press any key to return",
           TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
  Bdisp_PutDisp_DD();
}

void screenTools() {
  drawScreenToolsHome();
  while (1) {
    int key;
    mGetKey(&key);
    if (key == KEY_CTRL_EXIT) return;
    if (key == KEY_CTRL_F1) {
      if (platformHasCapability(PLATFORM_CAP_SCREENSHOT)) {
        TakeScreenshot();
        Bkey_ClrAllFlags();
      }
      drawScreenToolsHome();
    } else if (key == KEY_CTRL_F2) {
      drawRGB565Test();
      mGetKey(&key);
      drawScreenToolsHome();
    } else if (key == KEY_CTRL_F3) {
      drawPixelTest();
      mGetKey(&key);
      drawScreenToolsHome();
    } else if (key == KEY_CTRL_F4) {
      drawFullFillRefreshTest();
      mGetKey(&key);
      drawScreenToolsHome();
    } else if (key == KEY_CTRL_F5) {
      drawFrameRateTest();
      mGetKey(&key);
      drawScreenToolsHome();
    }
  }
}

void performanceLab() {
  pageRefreshMilliseconds = measurePageRefreshMilliseconds();
  resetBenchmarkResults();
  drawBenchmarkScreen();
  while (1) {
    int key;
    mGetKey(&key);
    if (key == KEY_CTRL_EXIT) return;
    if (key == KEY_CTRL_F1) {
      runAllBenchmarks();
      drawBenchmarkScreen();
    } else if (key == KEY_CTRL_F2) {
      resetBenchmarkResults();
      drawBenchmarkScreen();
    } else if (key == KEY_CTRL_F3) {
      mMsgBoxPush(4);
      multiPrintXY(3, 2, "Save benchmark\n"
                         "report to UTPERF.TXT?",
                   TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
      if (closeMsgBox(1, 4)) {
        int result = saveBenchmarkReport();
        const char* status = result == 0 ? "Report saved." :
          result == SAVE_RESULT_EXISTS ? "Report exists; not overwritten." :
          result == SAVE_RESULT_CREATE ? "Save create failed." :
          result == SAVE_RESULT_OPEN ? "Save open failed." :
          result == SAVE_RESULT_SEEK ? "Save seek failed." :
          result == SAVE_RESULT_WRITE ? "Save write failed." :
          result == SAVE_RESULT_VERIFY_OPEN ? "Save verify-open failed." :
          result == SAVE_RESULT_VERIFY_SIZE ? "Save verify-size failed." :
          result == SAVE_RESULT_RENAME ? "Save rename failed." :
          result == SAVE_RESULT_NO_BFILE ? "Save storage unavailable." :
          result == SAVE_RESULT_ALLOC ? "Save memory unavailable." :
          "Save could not be started.";
        drawBenchmarkStatus(status);
        mGetKey(&key);
        drawBenchmarkScreen();
      } else {
        drawBenchmarkScreen();
      }
    }
  }
}
