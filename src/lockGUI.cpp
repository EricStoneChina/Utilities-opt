#include <fxcg/display.h>
#include <fxcg/file.h>
#include <fxcg/keyboard.h>
#include <fxcg/system.h>
#include <fxcg/misc.h>
#include <fxcg/app.h>
#include <fxcg/serial.h>
#include <fxcg/rtc.h>
#include <fxcg/heap.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <setjmp.h>

#include "homeGUI.hpp"
#include "menuGUI.hpp"
#include "textGUI.hpp"
#include "lockGUI.hpp"
#include "keyboardProvider.hpp"
#include "graphicsProvider.hpp"
#include "lockProvider.hpp"
#include "constantsProvider.hpp"
#include "inputGUI.hpp"
#include "timeProvider.hpp"
#include "settingsProvider.hpp"
#include "hardwareProvider.hpp"
#include "platformProvider.hpp"
#include "diagnosticsProvider.hpp"

int passwordInput(int x, int y, unsigned char* buffer) {
  //returns: 0 on user abort (EXIT), 1 on EXE
  int start = 0, cursor = 0, key=0;
  int charlimit = 256;
  unsigned char dispbuffer[256]; //will hold asterisks instead of real characters...
  //clean buffer and display buffer
  memset(buffer, 0, 256);
  while(1)
  {   
    strcpy((char*)dispbuffer, "");
    int numchars = strlen((char*)buffer) - 1;

    if(numchars >= 0) {
      memset(dispbuffer, '*', numchars);
      dispbuffer[numchars] = 0;
      if(getSetting(SETTING_PASSWORD_PRIVACY) && key<30000 && cursor==numchars + 1) {
        //show last character for easier typing
        //only show last char if last key was a CHAR key and cursor is at the end
        strcat((char*)dispbuffer, " ");
        dispbuffer[numchars] = buffer[numchars];
      } else {
        strcat((char*)dispbuffer, "*");
      }
    }

    drawFkeyLabels(-1, -1, -1, -1, 0x0307, 0x02A1); // A<>a, CHAR
    Cursor_SetFlashOn(5);
    DisplayMBString((unsigned char*)dispbuffer, start, cursor, x,y);

    int keyflag = GetSetupSetting( (unsigned int)0x14);
    DisplayStatusArea();
    GetKey(&key);
    if (GetSetupSetting((unsigned int)0x14) == 0x01 ||
        GetSetupSetting((unsigned int)0x14) == 0x04 ||
        GetSetupSetting((unsigned int)0x14) == 0x84) {
      // make sure the flag we're using is the updated one.
      keyflag = GetSetupSetting( (unsigned int)0x14);
      // we can't update always because that way alpha-not-lock will cancel when F5 is pressed.
    }
    if(key == KEY_CTRL_F5) {
      switch(keyflag) {
        case 0x08:
        case 0x88:
          SetSetupSetting((unsigned int)0x14, keyflag-0x04);
          continue; //do not process the key, because otherwise we will leave alpha status
        case 0x04:
        case 0x84:
          SetSetupSetting((unsigned int)0x14, keyflag+0x04);
          continue; //do not process the key, because otherwise we will leave alpha status
      }
    } else if(key == KEY_CTRL_F6) {
      SaveVRAM_1();
      Bkey_ClrAllFlags();
      short character;
      character = CharacterSelectDialog();
      if (character)
        cursor = EditMBStringChar((unsigned char*)buffer, charlimit, cursor, character);
      LoadVRAM_1();
    } else if (key == KEY_CTRL_EXIT) {
      Cursor_SetFlashOff();
      return 0;
    } else if (key == KEY_CTRL_EXE) {
      if (buffer[0]) {
        Cursor_SetFlashOff(); return 1;
      } else {
        mMsgBoxPush(3);
        multiPrintXY(3, 3, "Password can't be\nempty.", TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
        closeMsgBox();
      }
    }
    if(key && key < 30000) {
      if ((keyflag == 0x08 || keyflag == 0x88) && key >= KEY_CHAR_A && key <= KEY_CHAR_Z) {
        //if lowercase and key is char...
        key = key + 32; //so we switch to lowercase characters
      }
      cursor = EditMBStringChar((unsigned char*)buffer, charlimit, cursor, key);
    } else {
      EditMBStringCtrl((unsigned char*)buffer, charlimit, &start, &cursor, &key, x, y);
    }
  }
  Cursor_SetFlashOff();
  return 0;
}

int setPassword() {
  unsigned char password[256];
  unsigned char confirmation[256];
  while(1) {
    Bdisp_AllClr_VRAM();
    drawScreenTitle("Calculator lock", "Set new password:");
    if (!passwordInput(1, 3, password))
      return 0;
    Bdisp_AllClr_VRAM();
    drawScreenTitle("Calculator lock", "Confirm password:");
    if (passwordInput(1, 3, confirmation)) {
      if(!strcmp((char*)password, (char*)confirmation)) {
        savePassword(password);
        return 1;
      } else
        AUX_DisplayErrorMessage(0x13); // Mismatch
    } else return 0;
  }
}

int unlockCalc() {
  //returns 1 on unlocked and 0 on still locked
  unsigned char password[256];
  
  Bdisp_AllClr_VRAM();
  drawScreenTitle("Calculator lock", "Enter password:");
  if (!passwordInput(1, 3, password)) return 0;
  else {
    if(!comparePasswordHash(password)) return 1;
    else AUX_DisplayErrorMessage(0x13); // Mismatch
  }
  return 0;
}

extern jmp_buf utilities_return;
int rettimer = -1;
static volatile int runMatActive = 0;
static volatile int runMatReturnPending = 0;
static volatile unsigned int runMatShiftSeen = 0;
static volatile unsigned int runMatExitSeen = 0;
static volatile unsigned int runMatF6Seen = 0;
static volatile unsigned int runMatMenuSeen = 0;
static unsigned int runMatLaunchCount = 0;
static unsigned int runMatReturnCount = 0;
static int runMatLastResult = RUNMAT_RESULT_NONE;
static int runMatLastError = 0;
static int runMatPreviousMenuReturnFlag = 1;

static void stopRunMatTimer() {
  int timer = rettimer;
  rettimer = -1;
  if (timer > 0) {
    Timer_Stop(timer);
    Timer_Deinstall(timer);
    diagnosticsRecordEvent(DIAGNOSTIC_EVENT_TIMER_STOP, timer);
  }
}

static void showRunMatError(int errorCode) {
  Bdisp_AllClr_VRAM();
  drawScreenTitle("Run-Mat unavailable");
  if (errorCode == RUNMAT_ERROR_NO_CAPABILITY) {
    multiPrintXY(2, 3, "This calculator model is not\n"
                         "supported by the safe return path.",
                  TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
  } else {
    multiPrintXY(2, 3, "The return timer could not\n"
                         "be installed. Run-Mat was not opened.",
                  TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
  }
  mPrintXY(2, 7, "Press EXIT to close", TEXT_MODE_TRANSPARENT_BACKGROUND,
           TEXT_COLOR_BLACK);
  int key;
  do {
    mGetKey(&key);
  } while (key != KEY_CTRL_EXIT);
}

void returnToUtilitiesHandler() {
  if(!runMatActive || runMatReturnPending) return;
  const int shiftPressed = platformIsKeyPressed(KEY_PRGM_SHIFT);
  const int exitPressed = platformIsKeyPressed(KEY_PRGM_EXIT);
  const int f6Pressed = platformIsKeyPressed(KEY_PRGM_F6);
  const int menuPressed = platformIsKeyPressed(KEY_PRGM_MENU);
  if (shiftPressed) runMatShiftSeen = 1;
  if (exitPressed) runMatExitSeen = 1;
  if (f6Pressed) runMatF6Seen = 1;
  if (menuPressed) runMatMenuSeen = 1;
  /*
   * The CG50 matrix must be sampled frequently; at 100 ms a short EXIT
   * press is commonly missed.  Accept EXIT alone as the safe escape route,
   * while retaining SHIFT+EXIT for compatibility with the old shortcut.
   */
  if (exitPressed || (runMatShiftSeen &&
      (runMatExitSeen || runMatF6Seen || runMatMenuSeen))) {
    runMatReturnPending = 1;
    runMatActive = 0;
    runMatLastResult = (runMatShiftSeen && runMatExitSeen) ?
                       RUNMAT_RESULT_SHIFT_EXIT : RUNMAT_RESULT_EXIT;
    longjmp(utilities_return, 1);
  }
}

void openRunMat() {
  if (!platformHasCapability(PLATFORM_CAP_RUNMAT)) {
    runMatLastError = RUNMAT_ERROR_NO_CAPABILITY;
    diagnosticsRecordError(runMatLastError, getHardwareModel());
    showRunMatError(runMatLastError);
    return;
  }

  if (runMatActive) return;
  int timer = Timer_Install(0, returnToUtilitiesHandler, 10);
  if (timer <= 0) {
    runMatLastError = RUNMAT_ERROR_TIMER_INSTALL;
    diagnosticsRecordError(runMatLastError, timer);
    showRunMatError(runMatLastError);
    return;
  }

  rettimer = timer;
  runMatActive = 1;
  runMatReturnPending = 0;
  runMatShiftSeen = 0;
  runMatExitSeen = 0;
  runMatF6Seen = 0;
  runMatMenuSeen = 0;
  runMatLastError = 0;
  runMatLastResult = RUNMAT_RESULT_NONE;
  runMatPreviousMenuReturnFlag = GetGetkeyToMainFunctionReturnFlag();
  // Keep the native Run-Mat loop from consuming the shortcut as a global
  // return-to-main request. Our timer owns the return decision instead.
  SetGetkeyToMainFunctionReturnFlag(0);
  SetSetupSetting((unsigned int)0x14, 0);
  runMatLaunchCount++;
  diagnosticsRecordEvent(DIAGNOSTIC_EVENT_RUNMAT_ENTER, timer);
  diagnosticsRecordEvent(DIAGNOSTIC_EVENT_TIMER_START, timer);
  Timer_Start(rettimer);
  APP_RUNMAT(0,0);

  /* Some OS revisions return from APP_RUNMAT normally.  Treat that path as
   * a valid return and perform the same timer cleanup as the longjmp path. */
  if (runMatActive) {
    runMatLastResult = RUNMAT_RESULT_APP_RETURN;
    runMatReturnCount++;
    finalizeRunMatReturn();
    diagnosticsRecordEvent(DIAGNOSTIC_EVENT_RUNMAT_RETURN,
                           runMatLastResult);
  }
}

void finalizeRunMatReturn() {
  stopRunMatTimer();
  runMatActive = 0;
  if (runMatReturnPending) {
    runMatReturnCount++;
    diagnosticsRecordEvent(DIAGNOSTIC_EVENT_RUNMAT_RETURN,
                           runMatLastResult);
  }
  runMatReturnPending = 0;
  runMatShiftSeen = 0;
  runMatExitSeen = 0;
  runMatF6Seen = 0;
  runMatMenuSeen = 0;
  SetSetupSetting((unsigned int)0x14, 0);
  SetGetkeyToMainFunctionReturnFlag(runMatPreviousMenuReturnFlag);
  Cursor_SetFlashOff();
  Bkey_ClrAllFlags();
  resetFunctionKeyMappings();
  Bdisp_EnableColor(1);
}

void getRunMatStatus(RunMatStatus* status) {
  if (status == NULL) return;
  status->active = runMatActive;
  status->timerId = rettimer;
  status->launchCount = runMatLaunchCount;
  status->returnCount = runMatReturnCount;
  status->lastResult = runMatLastResult;
  status->lastError = runMatLastError;
}
void lockApp() {
  if (!getSetting(SETTING_ENABLE_LOCK))
    return;

  if(!isPasswordSet()) {
    textArea text;
    text.title = (char*)"Calculator lock";
    
    textElement elem[4];
    text.elements = elem;
    
    elem[0].text = (char*)"This is probably the first time you are using the calculator lock "
                          "function, which lets you lock your calculator with a password. When "
                          "proceeding, you'll be prompted for a new password.";
    
    elem[1].newLine = 1;
    elem[1].lineSpacing = 8;
    elem[1].text = (char*)"Press EXE to proceed, or EXIT to cancel.";
    
    text.numelements = 2;
    text.allowEXE = 1;
    if(!doTextArea(&text) || !setPassword()) return;
    
    elem[0].text = (char*)"You successfully set a password for locking your calculator.";
    elem[1].text = (char*)"The next time you press F6 on the home screen, your calculator will "
                          "lock. To unlock, press the ALPHA key. You'll be prompted for the "
                          "password you just set.";
    
    elem[2].newLine = 1;
    elem[2].lineSpacing = 8;
    elem[2].text = (char*)"You can change the password and settings of the calculator lock "
                          "function on the settings menu (press Shift then Menu).";
    
    elem[3].newLine = 1;
    elem[3].lineSpacing = 8;
    elem[3].text = (char*)"Press EXIT to close this message.";
    
    text.numelements = 4;
    doTextArea(&text);
    return;
  }
  setmGetKeyMode(MGETKEY_MODE_RESTRICT_SETTINGS_RESTART);
  SetGetkeyToMainFunctionReturnFlag(0); // Disable menu return
  if(getSetting(SETTING_LOCK_AUTOOFF))
    PowerOff(1);

  while(1) {
    homeScreen(1);
    // avoid alpha still being triggered at start of text input:
    SetSetupSetting( (unsigned int)0x14, 0);
    if(1==unlockCalc()) {
      break;
    }
  }

  SetGetkeyToMainFunctionReturnFlag(1); // Enable menu return
  setmGetKeyMode(MGETKEY_MODE_NORMAL);
  if (getSetting(SETTING_UNLOCK_RUNMAT) == 1) {
    openRunMat();
  } else if (getSetting(SETTING_UNLOCK_RUNMAT) == 2) {
    mMsgBoxPush(4);
    multiPrintXY(3, 2, "Open Run-Mat?\n\n   Yes:[F1]/[1]\n   No :Other key",
                 TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
    int key,inscreen=1;
    while(inscreen) {
      mGetKey(&key);
      switch(key) {
        case KEY_CTRL_F1:
        case KEY_CHAR_1:
          mMsgBoxPop();
          openRunMat();
          break;
        default:
          inscreen=0;
          break;
      }
    }
    mMsgBoxPop();
  }
}
