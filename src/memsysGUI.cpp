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

#include "memsysGUI.hpp"
#include "appProvider.hpp"
#include "chronoGUI.hpp"
#include "menuGUI.hpp"
#include "textGUI.hpp"
#include "settingsProvider.hpp"
#include "keyboardProvider.hpp"
#include "hardwareProvider.hpp"
#include "platformProvider.hpp"
#include "graphicsProvider.hpp"
#include "stringsProvider.hpp"
#include "versionProvider.hpp"
#include "timeProvider.hpp"
#include "storageProvider.hpp"
#include "selectorGUI.hpp" 
#include "fileProvider.hpp"

// the following two functions are auxiliars to memoryCapacityScreen
static void drawCompactCapacityBar(int barY, long long int cur,
                                   long long int full);

void drawCapacityBar(int textY, long long int cur, long long int full) {
  // The home-pane view uses the legacy text flow, but it should use the same
  // bar renderer as the full memory page.  This avoids the old indexed-color
  // percentage path, which became a blank/white block on some CG50 OS builds.
  drawCompactCapacityBar(textY + 24, cur, full);
}

void drawCapacityText(int* textY, const char* desc, int cur, int full) {
  const char* localizedDesc = localizeText(desc);
  const char* unavailableText = localizeText("n/a");
  if (full <= 0) {
    char unavailable[64];
    int textX = 0;
    *textY += 22;
    sprintf(unavailable, "%s: %s", localizedDesc, unavailableText);
    mPrintMiniRaw(&textX, textY, unavailable, 0, 0xFFFFFFFF, 0, 0,
                  COLOR_BLACK, COLOR_WHITE, 1, 0);
    *textY += 29;
    return;
  }
  if (full < 0) full = 0;
  if (cur < 0) cur = 0;
  if (full > 0 && cur > full) cur = full;
  char buffer[80];
  *textY=*textY+22; int textX = 0;
  // Keep the 18 px line short enough for the 384 px CG50 display. The old
  // byte-count wording could run past the right edge, especially in Chinese.
  sprintf(buffer, "%s: %d KB free", localizedDesc, (full-cur)/1024);
  mPrintMiniRaw(&textX, textY, buffer, 0, 0xFFFFFFFF, 0, 0,
                COLOR_BLACK, COLOR_WHITE, 1, 0);

  *textY = *textY + 17; textX = 0;
  sprintf(buffer, "%d/%d KB used", cur/1024, full/1024);
  mPrintMiniRaw(&textX, textY, buffer, 0, 0xFFFFFFFF, 0, 0,
                COLOR_BLACK, COLOR_WHITE, 1, 0);
  *textY = *textY + 19;
}

static void drawCompactCapacityBar(int barY, long long int cur,
                                   long long int full) {
  if (full <= 0) {
    drawRectangle(0, barY, LCD_WIDTH_PX, 18, COLOR_LIGHTGRAY);
    // CG50 has no legacy password-memory capacity.  Show an explicit ASCII
    // N/A badge instead of leaving a misleading zero or an empty bar.
    const int badgeWidth = 46;
    const int badgeX = (LCD_WIDTH_PX - badgeWidth) / 2;
    drawRectangle(badgeX, barY, badgeWidth, 18, COLOR_BLACK);
    int textX = badgeX + 11;
    int textY = barY + 3;
    // PrintMiniMini's colour argument is a palette/background colour unless
    // the invert bit is set.  Passing TEXT_COLOR_WHITE with mode 0 therefore
    // paints a white box and no visible glyph on real CG50 firmware.  Mode
    // 16 is the same inverted badge mode used by the native clock UI: black
    // palette input produces white text on the black badge.
    mPrintMiniMiniRawAt(&textX, &textY, "N/A", 16, TEXT_COLOR_BLACK, 0);
    return;
  }
  if (cur < 0) cur = 0;
  if (cur > full) cur = full;
  long long int barWidth = (long long int)LCD_WIDTH_PX * cur / full;
  drawRectangle(0, barY, LCD_WIDTH_PX, 18, COLOR_GRAY);
  drawRectangle(0, barY, (int)barWidth, 18, COLOR_BLUE);

  char percent[16];
  int percentValue = (int)(100 * cur / full);
  sprintf(percent, "%d%%", percentValue);

  // Keep the badge compact and render the ASCII percentage directly.  The
  // localized mini-mini path can turn the translated state into GBK bytes;
  // on some CG50 OS revisions that produces a blank or low-contrast badge.
  const int badgeWidth = 46;
  int badgeX = ((int)LCD_WIDTH_PX - badgeWidth) / 2;
  if (badgeX < 0) badgeX = 0;
  /* A black/white badge has reliable contrast on both the blue used portion
   * and the gray free portion.  The old cyan-on-white rendering was nearly
   * invisible on a real CG50. */
  drawRectangle(badgeX, barY, badgeWidth, 18, COLOR_BLACK);
  int textX = badgeX + 7;
  int textY = barY + 3;
  mPrintMiniMiniRawAt(&textX, &textY, percent, 16, TEXT_COLOR_BLACK, 0);
}

static void drawCompactCapacityBlock(int index, const char* desc, int cur,
                                     int full) {
  /* PrintMiniMini is intentionally limited to ASCII metadata on the CG50.
   * The capacity labels are localized GBK strings, so use PrintXY for them.
   * Leave a full 24-pixel row below each label before beginning its bar. */
  const int labelRow = 2 + index * 2;
  const int valueY = 52 + index * 48;
  const int barY = 72 + index * 48;
  char value[40];

  mPrintXY(1, labelRow, desc, TEXT_MODE_TRANSPARENT_BACKGROUND,
           TEXT_COLOR_BLUE);
  int textX = 145;
  int textY = valueY;
  if (full <= 0 || cur < 0) {
    // Keep this ASCII: the CG50 has no legacy password-memory capacity.
    strcpy(value, "N/A");
  } else {
    if (cur < 0) cur = 0;
    if (cur > full) cur = full;
    sprintf(value, "%d KB free", (full - cur) / 1024);
  }
  mPrintMiniMiniRawClippedAt(&textX, &textY, value, 0, TEXT_COLOR_BLACK,
                             LCD_WIDTH_PX - 149, 0);
  drawCompactCapacityBar(barY, cur, full);
}

void memoryCapacityScreen(int isPaneOffset) {
  if(!isPaneOffset) {
    Bdisp_AllClr_VRAM();
    drawScreenTitle("Memory Usage");
  }
  StorageGuard storageGuard = {0};
  const int canReadStorage = platformHasCapability(PLATFORM_CAP_BFILE);
  const int canReadMainMemory = platformHasCapability(PLATFORM_CAP_MCS);
  const int storageGuardActive =
      (canReadStorage || canReadMainMemory) &&
      storageOperationBegin(&storageGuard) == 0;
  int smemfree = -1;
  unsigned short smemMedia[10]={'\\','\\','f','l','s','0',0};
  int storageStatus = storageGuardActive && canReadStorage ?
    Bfile_GetMediaFree_OS(smemMedia, &smemfree) : -1;

  int MCSmaxspace = 0; int MCScurrentload = 0; int MCSfreespace = 0;
  int mcsStatus = storageGuardActive && canReadMainMemory ?
    MCS_GetState(&MCSmaxspace, &MCScurrentload, &MCSfreespace) : -1;
  if (storageGuardActive) storageOperationEnd(&storageGuard);

  int passwordUsed = 0;
  int passwordCapacity = 0;
  const int passwordStatus = platformGetLegacyPasswordUsage(
      &passwordUsed, &passwordCapacity);

  const int storageUsed = (storageStatus < 0 || smemfree < 0) ? -1 :
                          TOTAL_SMEM - smemfree;
  const int mainUsed = (mcsStatus < 0 || MCSmaxspace <= 0) ? -1 :
                       MCScurrentload;
  const int mainCapacity = (mcsStatus < 0 || MCSmaxspace <= 0) ? 0 :
                           MCSmaxspace;
  const int usablePassword = passwordStatus == PLATFORM_OK ? passwordUsed : -1;
  const int usablePasswordCapacity = passwordStatus == PLATFORM_OK ?
                                     passwordCapacity : 0;

  if (!isPaneOffset) {
    // The full page has enough room for a fixed three-block layout. Keep
    // labels in the normal Chinese font, but render values and bars compactly.
    drawCompactCapacityBlock(0, "Storage", storageUsed,
                             storageUsed < 0 ? 0 : TOTAL_SMEM);
    drawCompactCapacityBlock(1, "Main", mainUsed, mainCapacity);
    drawCompactCapacityBlock(2, "Password", usablePassword,
                             usablePasswordCapacity);
    Bdisp_PutDisp_DD();
  } else {
    int textY = 8 + isPaneOffset;
    if (storageUsed < 0) {
      drawCapacityText(&textY, "Storage", 0, 0);
      drawCapacityBar(textY, 0, 0);
    } else {
      drawCapacityText(&textY, "Storage", storageUsed, TOTAL_SMEM);
      drawCapacityBar(textY, storageUsed, TOTAL_SMEM);
    }
    if (mainUsed < 0) {
      drawCapacityText(&textY, "Main", 0, 0);
      drawCapacityBar(textY, 0, 0);
    } else {
      drawCapacityText(&textY, "Main", mainUsed, mainCapacity);
      drawCapacityBar(textY, mainUsed, mainCapacity);
    }
    if (usablePassword < 0) {
      drawCapacityText(&textY, "Password", 0, 0);
      drawCapacityBar(textY, 0, 0);
    } else {
      drawCapacityText(&textY, "Password", usablePassword,
                       usablePasswordCapacity);
      drawCapacityBar(textY, usablePassword, usablePasswordCapacity);
    }
    return;
  }

  while(!isPaneOffset) {
    int key;
    mGetKey(&key);
    if(key == KEY_CTRL_EXIT) {
      return;
    }
  }
}

void deviceDashboard() {
  StorageGuard storageGuard = {0};
  const int storageGuardActive = storageOperationBegin(&storageGuard) == 0;
  PlatformInfo platform;
  getPlatformInfo(&platform);

  int storageFree = -1;
  unsigned short smemMedia[10] = {'\\','\\','f','l','s','0',0};
  int storageStatus = -1;
  if (storageGuardActive && platformHasCapability(PLATFORM_CAP_BFILE)) {
    storageStatus = Bfile_GetMediaFree_OS(smemMedia, &storageFree);
  }

  int mainMax = 0;
  int mainUsed = 0;
  int mainFree = 0;
  int mcsStatus = -1;
  if (storageGuardActive && platformHasCapability(PLATFORM_CAP_MCS)) {
    mcsStatus = MCS_GetState(&mainMax, &mainUsed, &mainFree);
  }
  if (storageGuardActive) storageOperationEnd(&storageGuard);

  char osVersion[24];
  char date[32] = "";
  char time[32] = "";
  char value[60];
  char battery[20];
  char power[30];
  char capabilities[20];
  char systemLanguage[8];
  if (platformHasCapability(PLATFORM_CAP_RTC)) {
    currentDateToString(date);
    currentTimeToString(time);
  } else {
    strcpy(date, "n/a");
    strcpy(time, "n/a");
  }
  sprintf(osVersion, "%d.%d", (int)platform.osMajor, (int)platform.osMinor);
  if (platformHasCapability(PLATFORM_CAP_BATTERY)) {
    formatBatteryVoltage(battery);
  } else {
    strcpy(battery, "n/a");
  }
  if (platformHasCapability(PLATFORM_CAP_SYSTEM_APPS)) {
    sprintf(power, "BL %d / OFF %d", (int)GetBacklightDuration(),
            GetAutoPowerOffTime());
  } else {
    strcpy(power, "n/a");
  }
  formatHex32(capabilities, platform.capabilities);
  if (platformGetSystemLanguageCode(systemLanguage) != PLATFORM_OK) {
    strcpy(systemLanguage, "N/A");
  }

  Bdisp_AllClr_VRAM();
  drawScreenTitle("Device dashboard");

  int textX = 145;
  int textY = 52;
  mPrintXY(1, 2, "Model", TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLUE);
  mPrintMiniMiniRawClippedAt(&textX, &textY, platformModelName(platform.model), 0,
                             TEXT_COLOR_BLACK, LCD_WIDTH_PX - 149, 0);
  mPrintXY(1, 3, "OS", TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLUE);
  sprintf(value, "%s C%s", osVersion, capabilities);
  textX = 145; textY = 76;
  mPrintMiniMiniRawClippedAt(&textX, &textY, value, 0, TEXT_COLOR_BLACK,
                             LCD_WIDTH_PX - 149, 0);

  mPrintXY(1, 4, "Memory", TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLUE);
  char storageText[60];
  char mainText[60];
  if (storageStatus < 0 || storageFree < 0) strcpy(storageText, "n/a");
  else sprintf(storageText, "%d", storageFree / 1024);
  if (mcsStatus < 0 || mainFree < 0) strcpy(mainText, "n/a");
  else sprintf(mainText, "%d", mainFree / 1024);
  sprintf(value, "S%s M%s KB", storageText, mainText);
  textX = 145; textY = 100;
  mPrintMiniMiniRawClippedAt(&textX, &textY, value, 0, TEXT_COLOR_BLACK,
                             LCD_WIDTH_PX - 149, 0);

  mPrintXY(1, 5, "Battery", TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLUE);
  sprintf(value, "%s %s", battery, power);
  textX = 145; textY = 124;
  mPrintMiniMiniRawClippedAt(&textX, &textY, value, 0, TEXT_COLOR_BLACK,
                             LCD_WIDTH_PX - 149, 0);

  mPrintXY(1, 6, "Date", TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLUE);
  sprintf(value, "%s %s", date, time);
  textX = 145; textY = 148;
  mPrintMiniMiniRawClippedAt(&textX, &textY, value, 0, TEXT_COLOR_BLACK,
                             LCD_WIDTH_PX - 149, 0);

  mPrintXY(1, 7, "Sys/Ver", TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLUE);
  sprintf(value, "%s U%s H%s", systemLanguage, getVersion(),
          platform.isEmulated < 0 ? "?" : platform.isEmulated ? "No" : "Yes");
  textX = 145; textY = 172;
  mPrintMiniMiniRawClippedAt(&textX, &textY, value, 0, TEXT_COLOR_BLACK,
                             LCD_WIDTH_PX - 149, 0);
  while(1) {
    int key;
    mGetKey(&key);
    if(key == KEY_CTRL_EXIT) return;
  }
}

static int chooseNativeSystemSetting() {
  static const char* const itemText[4] = {
    "Native power", "Native display", "Native language", "Native version"
  };
  int selection = 0;

  while (1) {
    Bdisp_AllClr_VRAM();
    Bdisp_EnableColor(1);
    resetFunctionKeyMappings();
    drawRectangle(0, LCD_HEIGHT_PX - 24, LCD_WIDTH_PX, 24, COLOR_WHITE);
    drawScreenTitle("Native system settings");

    // Do not use the generic scrolling menu here.  Its scrollbar and bottom
    // F-key surface are useful for long lists, but native system apps need a
    // completely clean hand-off screen and this list is fixed at four rows.
    for (int i = 0; i < 4; i++) {
      const int row = 2 + i;
      const int rowY = row * 24;
      const int selected = i == selection;
      drawRectangle(0, rowY, LCD_WIDTH_PX, 24,
                    selected ? COLOR_BLUE : COLOR_WHITE);
      mPrintXY(1, row, itemText[i], TEXT_MODE_TRANSPARENT_BACKGROUND,
               selected ? TEXT_COLOR_WHITE : TEXT_COLOR_BLACK);
      drawLine(0, rowY + 23, LCD_WIDTH_PX - 1, rowY + 23, COLOR_LIGHTGRAY);
    }
    int helpX = 4;
    int helpY = 148;
    mPrintMiniMiniRawAt(&helpX, &helpY,
                        "UP/DOWN SELECT  EXE OPEN  EXIT CLOSE", 0,
                        TEXT_COLOR_BLACK, 0);
    Bdisp_PutDisp_DD();

    int key;
    mGetKey(&key);
    if (key == KEY_CTRL_UP) {
      selection = selection == 0 ? 3 : selection - 1;
    } else if (key == KEY_CTRL_DOWN) {
      selection = selection == 3 ? 0 : selection + 1;
    } else if (key == KEY_CTRL_EXE) {
      return selection + 1;
    } else if (key == KEY_CTRL_EXIT) {
      return 0;
    }
  }
}

void nativeSystemSettings() {
  if (!platformHasCapability(PLATFORM_CAP_SYSTEM_APPS)) {
    Bdisp_AllClr_VRAM();
    drawScreenTitle("Native settings unavailable");
    mPrintXY(2, 4, "This platform has no native settings.",
             TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
    mPrintXY(2, 7, "Press EXIT to close", TEXT_MODE_TRANSPARENT_BACKGROUND,
             TEXT_COLOR_BLACK);
    int key;
    do {
      mGetKey(&key);
    } while (key != KEY_CTRL_EXIT);
    return;
  }

  const int selection = chooseNativeSystemSetting();
  if (selection <= 0) return;

  // Native applications take over the calculator's event loop.  Do not leave
  // Utilities' chronometer callback or function-key mappings active while
  // that happens; both can redraw or consume state owned by the system app.
  int chronometerWasRunning = isStubTimerRunning();
  if (chronometerWasRunning) stopAndUninstallStubTimer();
  Bkey_ClrAllFlags();
  const int previousMenuReturnFlag = GetGetkeyToMainFunctionReturnFlag();
  SetGetkeyToMainFunctionReturnFlag(0);

  // Built-in applications own their GetKey loop and may not return on every
  // OS revision.  If one does return, leave this helper immediately instead
  // of drawing the old menu over the native screen's restored framebuffer.
  switch (selection) {
    case 1: APP_SYSTEM_POWER(0); break;
    case 2: APP_SYSTEM_DISPLAY(0); break;
    case 3: APP_SYSTEM_LANGUAGE(0); break;
    case 4: APP_SYSTEM_VERSION(); break;
  }
  SetGetkeyToMainFunctionReturnFlag(previousMenuReturnFlag);
  Bkey_ClrAllFlags();
  resetFunctionKeyMappings();
  Bdisp_EnableColor(1);
  Bdisp_AllClr_VRAM();
  drawRectangle(0, LCD_HEIGHT_PX - 24, LCD_WIDTH_PX, 24, COLOR_WHITE);
  Bdisp_PutDisp_DD();
  if (chronometerWasRunning) restartStubTimerFunction();
}

int getAddins(AddIn addins[]) {
  StorageScope storageScope;
  /*searches storage memory for active and inactive add-ins, returns their count*/
  if (addins == NULL) return 0;
  unsigned short path[MAX_FILENAME_SIZE+1], found[MAX_FILENAME_SIZE+1];
  char buffer[MAX_FILENAME_SIZE+1];

  // make the buffer
  strcpy(buffer, SMEM_PREFIX"*.g3a");
  
  int curitem = 0;
  file_type_t fileinfo;
  int findhandle = -1;
  Bfile_StrToName_ncpy(path, buffer, MAX_FILENAME_SIZE+1);
  int ret = Bfile_FindFirst_NON_SMEM((const char*)path, &findhandle, (char*)found, &fileinfo);
  while(!ret) {
    Bfile_NameToStr_ncpy(buffer, found, MAX_FILENAME_SIZE+1);
    buffer[MAX_FILENAME_SIZE] = 0;
    if(strcmp(buffer, (char*)SELFFILE) && fileinfo.fsize) {
      // Keep scanning after the fixed-size UI list is full, but never write
      // beyond the array supplied by addinManagerChild.
      if (curitem < MAX_ITEMS_IN_DIR) {
        strncpy(addins[curitem].filename, (char*)buffer,
                sizeof(addins[curitem].filename) - 1);
        addins[curitem].filename[sizeof(addins[curitem].filename) - 1] = 0;

        // get friendly add-in name from the file:
        char filename[MAX_FILENAME_SIZE];
        strcpy(filename, SMEM_PREFIX);
        strncat(filename, buffer,
                sizeof(filename) - strlen(filename) - 1);
        int hFile = fileOpen(filename);
        if(hFile >= 0) {
          int flagsRead = 0;
          memset(addins[curitem].name, 0,
                 sizeof(addins[curitem].name));
          flagsRead = Bfile_ReadFile_OS(hFile, buffer, 2, 0x0024) == 2;
          Bfile_ReadFile_OS(hFile, addins[curitem].name, 24, 0x006B);
          addins[curitem].name[sizeof(addins[curitem].name) - 1] = 0;
          Bfile_CloseFile_OS(hFile);
          addins[curitem].active = flagsRead &&
                                   ((unsigned char)buffer[0] == 1);
        } else {
          strncpy(addins[curitem].name, (char*)buffer,
                  sizeof(addins[curitem].name) - 1);
          addins[curitem].name[sizeof(addins[curitem].name) - 1] = 0;
          addins[curitem].active = 0;
        }
        curitem++;
      }
    }
    ret = Bfile_FindNext_NON_SMEM(findhandle, (char*)found, (char*)&fileinfo);
  }

  if (findhandle >= 0) Bfile_FindClose(findhandle);
  return curitem;
}

void addinManager() {
  if (!platformHasCapability(PLATFORM_CAP_BFILE)) {
    Bdisp_AllClr_VRAM();
    drawScreenTitle("Add-In Manager");
    mPrintXY(2, 4, "Storage access unavailable.",
             TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_RED);
    mPrintXY(2, 7, "Press EXIT to close",
             TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
    int unavailableKey;
    do {
      mGetKey(&unavailableKey);
    } while (unavailableKey != KEY_CTRL_EXIT);
    return;
  }
  StorageGuard storageGuard = {0};
  storageOperationBegin(&storageGuard);
  Menu menu;
  
  menu.scrollout=1;
  menu.height=7;
  menu.type=MENUTYPE_FKEYS;
  menu.nodatamsg = (char*)"No Add-Ins";
  menu.title = (char*)"Add-In Manager";
  while(addinManagerChild(&menu));
  storageOperationEnd(&storageGuard);
}

int addinManagerChild(Menu* menu) {
  StorageScope storageScope;
  //returns 1 when it wants to be restarted (refresh addins)
  //returns 0 if the idea really is to exit the screen
  AddIn addins[200]; 
  
  menu->numitems = getAddins(addins);
  MenuItem* menuitems = menu->numitems > 0 ?
    (MenuItem*)malloc(menu->numitems*sizeof(MenuItem)) : NULL;
  if(menu->numitems > 0 && menuitems == NULL) {
    menu->numitems = 0;
    menu->items = NULL;
    return 0;
  }
  menu->items = menuitems;
 
  Bdisp_AllClr_VRAM();
  for(int curaddin = 0; curaddin < menu->numitems; curaddin++) {
    menuitems[curaddin].text = (char*)addins[curaddin].name;
    menuitems[curaddin].color = (addins[curaddin].active ? TEXT_COLOR_BLACK : TEXT_COLOR_CYAN);
  }
  
  if(menu->numitems>0) drawFkeyLabels(0x0103, 0x0038); // CHANGE (white), DELETE
  
  char buffer[MAX_FILENAME_SIZE+1];
  int result = 1;
  switch(doMenu(menu)) {
    case KEY_CTRL_F1:
      if(menu->numitems > 0) {
        mMsgBoxPush(4);
        multiPrintXY(3, 2, "Change the\n"
                           "Selected Add-In state?",
                     TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
        if (!closeMsgBox(1, 4)) break;
        strcpy(buffer, SMEM_PREFIX);
        strcat(buffer, addins[menu->selection-1].filename);
        int hFile = fileOpen(buffer);
        if(hFile >= 0) {
          Bfile_SeekFile_OS(hFile, 0x0024);
          // toggle two bits that the OS checks to determine if a g3a is valid
          buffer[0] = buffer[1] = 1 + addins[menu->selection-1].active;
          buffer[2] = 0;
          Bfile_WriteFile_OS(hFile, buffer, 2);
          Bfile_CloseFile_OS(hFile);
        }
        result = 1; //reload list
        break;
      }
      break;
    case KEY_CTRL_F2:
    case KEY_CTRL_DEL:
      if(menu->numitems > 0) {
        mMsgBoxPush(4);
        multiPrintXY(3, 2, "Back up and quarantine the\n"
                           "Selected Add-In?",
                     TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
        if(closeMsgBox(1, 4)) {
          mMsgBoxPush(4);
          multiPrintXY(3, 2, "Confirm quarantine of\n"
                             "the selected .g3a file?",
                       TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
          if (closeMsgBox(1, 4)) {
            int quarantineResult = quarantineG3AByFilename(
                addins[menu->selection - 1].filename);
            const char* message = quarantineResult == APP_MANAGE_OK ?
                                  "Backup saved; add-in removed." :
                                  "Backup failed; nothing removed.";
            mMsgBoxPush(4);
            multiPrintXY(3, 2, message,
                         TEXT_MODE_TRANSPARENT_BACKGROUND,
                         quarantineResult == APP_MANAGE_OK ?
                           TEXT_COLOR_BLACK : TEXT_COLOR_RED);
            closeMsgBox();
          }
        }
        result = 1;
        break;
      }
      break;
    case MENU_RETURN_EXIT: result = 0; break;
  }
  free(menuitems);
  return result;
}

void changeFKeyColor() {
  Bdisp_AllClr_VRAM();
  DisplayStatusArea();
  drawScreenTitle("Function Key Color");
  textArea text;
  text.type = TEXTAREATYPE_INSTANT_RETURN;
  text.y = 132;
  text.lineHeight=19;
  textElement elem[1];
  text.elements = elem;
  text.scrollbar=0;
  
  elem[0].text = (char*)"Please note that only the " ADDIN_FRIENDLYNAME " add-in and a hidden "
                        "debug screen on your calculator are able to change this setting, which "
                        "persists across reboots. To reset it back to the black color you need to "
                        "use this add-in, the hidden debug screen or to reset the Main Memory.";
  elem[0].minimini = 1;
  text.numelements = 1;
  doTextArea(&text);
  unsigned char currentColor = 0;
  if (platformGetFunctionKeyColor(&currentColor) != PLATFORM_OK) {
    mPrintXY(2, 6, "Function key color unavailable.",
             TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
    mPrintXY(2, 7, "Press EXIT to close",
             TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
    int key;
    do {
      mGetKey(&key);
    } while (key != KEY_CTRL_EXIT);
    return;
  }
  unsigned char selcolor = ColorIndexDialog1(currentColor, 0);
  if(selcolor != (unsigned char)0xFF) {
    //user didn't press EXIT, QUIT or AC/ON. input is validated.
    platformSetFunctionKeyColor(selcolor);
  }
}

void systemInfo() {
  /*
   * The former page read OS/ABS/CPU data from fixed legacy addresses.  Those
   * fields are not a stable CG50 interface, so keep this page on the public
   * platform probe and the documented device identifier only.
   */
  PlatformInfo platform;
  getPlatformInfo(&platform);
  char osVersion[32];
  char osBuild[32];
  char capabilities[12];
  char devID[10];
  sprintf(osVersion, "%d.%d", (int)platform.osMajor, (int)platform.osMinor);
  sprintf(osBuild, "%d / %d", (int)platform.osBuild, (int)platform.osRevision);
  formatHex32(capabilities, platform.capabilities);
  if (platformGetDeviceID(devID) != PLATFORM_OK) {
    strcpy(devID, "n/a");
  }

  textArea text;
  text.title = (char*)"System Information";

  textElement elem[15];
  text.elements = elem;

  elem[0].text = (char*)"OS version:";
  elem[0].spaceAtEnd=1;
  elem[1].text = osVersion;

  elem[2].newLine = 1;
  elem[2].text = (char*)"OS build:";
  elem[2].spaceAtEnd=1;
  elem[3].text = osBuild;

  elem[4].newLine = 1;
  elem[4].text = (char*)"Calculator model:";
  elem[4].spaceAtEnd=1;
  elem[5].text = (char*)platformModelName(platform.model);

  elem[6].newLine = 1;
  elem[6].text = (char*)"Real hardware:";
  elem[6].spaceAtEnd=1;
  elem[7].text = platform.isEmulated < 0 ? (char*)"Unknown" :
                 platform.isEmulated ? (char*)"No" : (char*)"Yes";

  elem[8].newLine = 1;
  elem[8].text = (char*)"Capabilities:";
  elem[8].spaceAtEnd=1;
  elem[9].text = capabilities;

  elem[10].newLine = 1;
  elem[10].text = (char*)"Device ID:";
  elem[10].spaceAtEnd=1;
  elem[11].text = devID;

  // Do not expose the calculator's registered-user/password history from
  // Utilities.  The old page read undocumented memory and violated the
  // security boundary of the device-management center.
  text.numelements = 12;
  text.allowF1=0;
  doTextArea(&text);
}

void userInfo() {
  textArea text;
  text.title = (char*)"User Information";
  textElement elem[3];
  text.elements = elem;
  elem[0].text = (char*)"User information is hidden for safety.";
  elem[1].newLine = 1;
  elem[1].text = (char*)"Utilities does not read password history.";
  elem[2].newLine = 1;
  elem[2].text = (char*)"Press EXIT to close.";
  text.numelements = 3;
  doTextArea(&text);
}
