#include <fxcg/display.h>
#include <fxcg/keyboard.h>
#include <fxcg/system.h>
#include <stdio.h>

#include "diagnosticsGUI.hpp"
#include "diagnosticsProvider.hpp"
#include "graphicsProvider.hpp"
#include "hardwareProvider.hpp"
#include "keyboardProvider.hpp"
#include "lockGUI.hpp"
#include "platformProvider.hpp"
#include "settingsProvider.hpp"
#include "stringsProvider.hpp"
#include "timeProvider.hpp"

static volatile unsigned long diagnosticHeartbeatCount = 0;

static void diagnosticsHeartbeat() {
  diagnosticHeartbeatCount++;
}

static void stopDiagnosticsTimer(int* timer) {
  if (timer != NULL && *timer > 0) {
    Timer_Stop(*timer);
    Timer_Deinstall(*timer);
    diagnosticsRecordEvent(DIAGNOSTIC_EVENT_TIMER_STOP, *timer);
    *timer = -1;
  }
}

static const char* yesNo(int value) {
  return value ? "Yes" : "No";
}

/*
 * PrintXY is the only reliable path for the calculator's GB18030 glyphs.
 * Keep labels in the fixed 24-pixel rows, and put the variable ASCII data in
 * the 10-pixel font.  The function-key bar occupies row 7, so five rows are
 * deliberately used here; the matrix row also carries the heartbeat and
 * last-error counters.
 */
static void drawDiagnosticField(int row, const char* label,
                                const char* value, int color) {
  int textX = 145;
  int textY = 52 + (row - 2) * 24;
  mPrintXY(1, row, label, TEXT_MODE_TRANSPARENT_BACKGROUND,
           TEXT_COLOR_BLUE);
  mPrintMiniMiniRawClippedAt(&textX, &textY, value, 0, color,
                             LCD_WIDTH_PX - 149, 0);
}

static void diagnosticsHistoryScreen() {
  Bdisp_AllClr_VRAM();
  drawScreenTitle("Recent diagnostic events");
  mPrintXY(1, 2, "Newest event first",
           TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLUE);

  int shown = 0;
  for (int offset = 0; offset < DIAGNOSTICS_EVENT_HISTORY_SIZE; offset++) {
    DiagnosticsEvent event = {0, 0, 0};
    char line[52];
    if (!diagnosticsGetRecentEvent(offset, &event)) break;
    sprintf(line, "%lu %s", event.sequence,
            diagnosticsEventName(event.type));
    mPrintXY(1, 3 + shown, line,
             TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
    shown++;
    if (shown >= 5) break;
  }
  if (shown == 0) {
    mPrintXY(2, 4, "No events recorded.",
             TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
  }
  Bdisp_PutDisp_DD();

  int key;
  do {
    mGetKey(&key);
  } while (key != KEY_CTRL_EXIT);
}

static void drawDiagnosticsScreen(int lastKey, unsigned long heartbeat,
                                  int matrixQueue, int matrixColumn,
                                  int matrixRow) {
  PlatformInfo platform;
  RunMatStatus runMat;
  DiagnosticsEvent event = {0, 0, 0};
  int errorCode = 0;
  int errorDetail = 0;
  char buffer[80];
  char osVersion[20];
  char keyInfo[40];
  char keyHex[9];
  char matrixInfo[40];

  getPlatformInfo(&platform);
  getRunMatStatus(&runMat);
  diagnosticsGetLastEvent(&event);
  diagnosticsGetLastError(&errorCode, &errorDetail);
  sprintf(osVersion, "%d.%d (%d)", (int)platform.osMajor,
          (int)platform.osMinor, (int)platform.osBuild);

  Bdisp_AllClr_VRAM();
  drawScreenTitle("Interface diagnostics");

  drawDiagnosticField(2, "Model", platformModelName(platform.model),
                      TEXT_COLOR_BLACK);
  drawDiagnosticField(3, "OS version", osVersion, TEXT_COLOR_BLACK);
  sprintf(buffer, "%s T%d L%u/%u", yesNo(runMat.active),
          runMat.timerId, runMat.launchCount, runMat.returnCount);
  drawDiagnosticField(4, "Run-Mat", buffer, TEXT_COLOR_BLACK);

  // This line is drawn from column 8. Compact flags keep it inside the
  // 384-pixel screen while retaining the important state information.
  formatHex32(keyHex, (unsigned int)lastKey & 0xFFFF);
  sprintf(keyInfo, "S%d X%d C%d K%s",
          platformIsKeyPressed(KEY_PRGM_SHIFT) ? 1 : 0,
          platformIsKeyPressed(KEY_PRGM_EXIT) ? 1 : 0,
          platformIsKeyPressed(KEY_PRGM_SHIFT) &&
          platformIsKeyPressed(KEY_PRGM_EXIT) ? 1 : 0,
          keyHex + 4);
  drawDiagnosticField(5, "Key", keyInfo, TEXT_COLOR_BLACK);

  /* Keep row 6 above the F-key surface.  C/R/Q are matrix column/row/queue,
   * H is the timer heartbeat, E is the last event, and the final pair is the
   * last error code/detail. */
  sprintf(matrixInfo, "C%d R%d Q%d H%lu E%d %d/%d", matrixColumn,
          matrixRow, matrixQueue, heartbeat, event.type, errorCode,
          errorDetail);
  drawDiagnosticField(6, "Matrix", matrixInfo,
                      errorCode ? TEXT_COLOR_RED : TEXT_COLOR_BLACK);
  drawFkeyLabels(0x0149, 0x049F, 0, 0, 0, 0); // CLEAR, VIEW
  Bdisp_PutDisp_DD();
}

void interfaceDiagnostics() {
  int timer = platformHasCapability(PLATFORM_CAP_TIMER) ?
              Timer_Install(0, diagnosticsHeartbeat, 100) : -1;
  if (timer <= 0) {
    diagnosticsRecordError(-3, timer);
    Bdisp_AllClr_VRAM();
    drawScreenTitle("Diagnostics unavailable");
    mPrintXY(2, 4, "The diagnostic timer could not be installed.",
             TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
    mPrintXY(2, 7, "Press EXIT to close", TEXT_MODE_TRANSPARENT_BACKGROUND,
             TEXT_COLOR_BLACK);
    int key;
    do {
      GetKey(&key);
    } while (key != KEY_CTRL_EXIT);
    return;
  }

  diagnosticHeartbeatCount = 0;
  diagnosticsRecordEvent(DIAGNOSTIC_EVENT_TIMER_START, timer);
  Timer_Start(timer);

  int lastKey = 0;
  int matrixColumn = 0;
  int matrixRow = 0;
  int matrixQueue = 0;
  int keyColumn = 0;
  int keyRow = 0;
  unsigned short osKey = 0;
  drawDiagnosticsScreen(0, 0, 0, 0, 0);
  while (1) {
    // Wait in the OS so short presses and timer dispatch do not depend on RTC polling.
    int event = GetKeyWait_OS(&keyColumn, &keyRow, KEYWAIT_HALTON_TIMERON,
                              1, 0, &osKey);
    int key = event == KEYREP_KEYEVENT ? keyColumn * 10 + keyRow - 1 : 0;
    if (key) lastKey = key;
    if (key == KEY_PRGM_EXIT || key == KEY_CTRL_EXIT) break;
    if (key == KEY_PRGM_F1 || key == KEY_CTRL_F1) diagnosticsClear();
    if (key == KEY_PRGM_F2 || key == KEY_CTRL_F2) {
      diagnosticsHistoryScreen();
    }

    char matrixCol = 0;
    char matrixR = 0;
    matrixQueue = Keyboard_SpyMatrixCode(&matrixCol, &matrixR);
    matrixColumn = (unsigned char)matrixCol;
    matrixRow = (unsigned char)matrixR;

    unsigned long heartbeat = diagnosticHeartbeatCount;
    drawDiagnosticsScreen(lastKey, heartbeat, matrixQueue, matrixColumn,
                          matrixRow);
  }

  stopDiagnosticsTimer(&timer);
  Bkey_ClrAllFlags();
}
