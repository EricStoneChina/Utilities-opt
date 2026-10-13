#include <fxcg/display.h>
#include <fxcg/app.h>
#include <fxcg/keyboard.h>
#include <stdio.h>
#include <string.h>

#include "appGUI.hpp"
#include "appProvider.hpp"
#include "graphicsProvider.hpp"
#include "keyboardProvider.hpp"
#include "chronoGUI.hpp"
#include "storageProvider.hpp"
#include "settingsProvider.hpp"
#include "platformProvider.hpp"
#include "stringsProvider.hpp"

static G3ARecord applicationRecords[APP_MAX_G3A_RECORDS];
static G3ABackupRecord applicationBackups[APP_MAX_G3A_BACKUPS];

static const char* yesNo(int value) {
  return value ? "Yes" : "No";
}

static int compareG3ARecords(const G3ARecord* left, const G3ARecord* right,
                             int sortMode) {
  int result;
  switch (sortMode) {
    case 1:
      result = strcmp(left->displayName, right->displayName);
      break;
    case 2:
      if (left->fileSize < right->fileSize) result = -1;
      else if (left->fileSize > right->fileSize) result = 1;
      else result = 0;
      break;
    case 3:
      result = strcmp(left->version, right->version);
      break;
    default:
      result = strcmp(left->filename, right->filename);
      break;
  }
  if (result == 0) result = strcmp(left->filename, right->filename);
  return result;
}

static void sortG3ARecords(G3ARecord records[], int count, int sortMode) {
  for (int i = 1; i < count; i++) {
    G3ARecord current = records[i];
    int j = i - 1;
    while (j >= 0 && compareG3ARecords(&records[j], &current, sortMode) > 0) {
      records[j + 1] = records[j];
      j--;
    }
    records[j + 1] = current;
  }
}

static void showApplicationIcon(const G3ARecord* record) {
  unsigned short* pixels = (unsigned short*)malloc(
      G3A_ICON_WIDTH * G3A_ICON_HEIGHT * sizeof(unsigned short));
  Bdisp_AllClr_VRAM();
  drawScreenTitle("Application icon");
  if (pixels != NULL && readG3AIcon(record, pixels,
                                    G3A_ICON_WIDTH * G3A_ICON_HEIGHT)) {
    const int iconX = (LCD_WIDTH_PX - G3A_ICON_WIDTH) / 2;
    const int iconY = 58;
    for (int iconYOffset = 0; iconYOffset < G3A_ICON_HEIGHT;
         iconYOffset++) {
      for (int iconXOffset = 0; iconXOffset < G3A_ICON_WIDTH;
           iconXOffset++) {
        Bdisp_SetPoint_VRAM(iconX + iconXOffset, iconY + iconYOffset,
                            pixels[iconYOffset * G3A_ICON_WIDTH +
                                   iconXOffset]);
      }
    }
    int nameX = 4;
    int nameY = 148;
    mPrintMiniMiniRawClippedAt(&nameX, &nameY, record->displayName, 0,
                               TEXT_COLOR_BLACK, LCD_WIDTH_PX - 8, 0);
  } else {
    mPrintXY(2, 4, "Icon unavailable.",
             TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_RED);
  }
  if (pixels != NULL) free(pixels);
  mPrintXY(2, 7, "Press EXIT to close",
           TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
  Bdisp_PutDisp_DD();
  int key;
  do {
    mGetKey(&key);
  } while (key != KEY_CTRL_EXIT);
}

static int drawApplicationDetails(const G3ARecord* record) {
  char buffer[70];
  if (record == NULL) return 0;

  while (1) {
    int textX;
    int textY;
    Bdisp_AllClr_VRAM();
    drawScreenTitle("Application details");

    mPrintXY(1, 2, "File", TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLUE);
    textX = 145; textY = 52;
    mPrintMiniMiniRawClippedAt(&textX, &textY, record->filename, 0,
                               TEXT_COLOR_BLACK, LCD_WIDTH_PX - 149, 0);
    mPrintXY(1, 3, "Name", TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLUE);
    textX = 145; textY = 76;
    mPrintMiniMiniRawClippedAt(&textX, &textY, record->displayName, 0,
                               TEXT_COLOR_BLACK, LCD_WIDTH_PX - 149, 0);
    mPrintXY(1, 4, "Version", TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLUE);
    textX = 145; textY = 100;
    mPrintMiniMiniRawClippedAt(&textX, &textY, record->version, 0,
                               TEXT_COLOR_BLACK, LCD_WIDTH_PX - 149, 0);
    mPrintXY(1, 5, "Size", TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLUE);
    sprintf(buffer, "%lu KB", record->fileSize / 1024);
    textX = 145; textY = 124;
    mPrintMiniMiniRawClippedAt(&textX, &textY, buffer, 0, TEXT_COLOR_BLACK,
                               LCD_WIDTH_PX - 149, 0);
    mPrintXY(1, 6, "Status", TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLUE);
    sprintf(buffer, "%lu %s", record->codeOffset, g3aStatusName(record->status));
    textX = 145; textY = 148;
    mPrintMiniMiniRawClippedAt(&textX, &textY, buffer, 0,
                               record->status == APP_STATUS_OK ? TEXT_COLOR_BLACK :
                                                                  TEXT_COLOR_RED,
                               LCD_WIDTH_PX - 149, 0);
    drawFkeyLabels(0, 0, 0, 0x049F, 0x0038, 0); // VIEW, DELETE/quarantine
    Bdisp_PutDisp_DD();

    int key;
    mGetKey(&key);
    if (key == KEY_CTRL_EXIT) return 0;
    if (key == KEY_CTRL_F4) {
      showApplicationIcon(record);
      continue;
    }
    if (key == KEY_CTRL_F5) {
      mMsgBoxPush(6);
      multiPrintXY(3, 2, "Back up this add-in\n"
                           "and remove it from root?",
                   TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
      if (!closeMsgBox(1, 5)) continue;

      mMsgBoxPush(6);
      multiPrintXY(3, 2, "Confirm quarantine of\n"
                           "the selected .g3a file?",
                   TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
      if (!closeMsgBox(1, 5)) continue;

      int result = quarantineG3ARecord(record);
      const char* message = result == APP_MANAGE_OK ?
                            "Backup saved; add-in removed." :
                            result == APP_MANAGE_BACKUP_EXISTS ?
                            "Backup exists; nothing removed." :
                            result == APP_MANAGE_DELETE_FAILED ?
                            "Backup saved; remove failed." :
                            "Backup failed; nothing removed.";
      Bdisp_AllClr_VRAM();
      drawScreenTitle("Application audit");
      mPrintXY(2, 5, message, TEXT_MODE_TRANSPARENT_BACKGROUND,
               result == APP_MANAGE_OK ? TEXT_COLOR_BLACK : TEXT_COLOR_RED);
      mPrintXY(2, 7, "Press EXIT to close",
               TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
      do {
        mGetKey(&key);
      } while (key != KEY_CTRL_EXIT);
      return result == APP_MANAGE_OK;
    }
  }
}

static void showNoApplications() {
  Bdisp_AllClr_VRAM();
  drawScreenTitle("Application audit");
  mPrintXY(2, 4, "No .g3a applications found.",
           TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
  mPrintXY(2, 7, "Press EXIT to close", TEXT_MODE_TRANSPARENT_BACKGROUND,
           TEXT_COLOR_BLACK);
  int key;
  do {
    mGetKey(&key);
  } while (key != KEY_CTRL_EXIT);
}

static void showApplicationUnavailable() {
  Bdisp_AllClr_VRAM();
  drawScreenTitle("Application audit");
  mPrintXY(2, 4, "Storage access unavailable.",
           TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_RED);
  mPrintXY(2, 7, "Press EXIT to close",
           TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
  int key;
  do {
    mGetKey(&key);
  } while (key != KEY_CTRL_EXIT);
}

static void restoreApplicationBackups() {
  int truncated = 0;
  StorageGuard storageGuard = {0};
  storageOperationBegin(&storageGuard);
  int count = scanG3ABackups(applicationBackups, APP_MAX_G3A_BACKUPS,
                              &truncated);
  storageOperationEnd(&storageGuard);
  if (count <= 0) {
    Bdisp_AllClr_VRAM();
    drawScreenTitle("Restore quarantined Add-Ins");
    mPrintXY(2, 4, "No quarantined backups found.",
             TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
    mPrintXY(2, 7, "Press EXIT to close",
             TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
    int key;
    do {
      mGetKey(&key);
    } while (key != KEY_CTRL_EXIT);
    return;
  }

  MenuItem menuItems[APP_MAX_G3A_BACKUPS];
  Menu menu;
  menu.items = menuItems;
  menu.numitems = count;
  menu.height = 7;
  menu.width = 21;
  menu.scrollout = 1;
  menu.type = MENUTYPE_FKEYS;
  menu.title = (char*)"Restore quarantined Add-Ins";
  menu.subtitle = truncated ? (char*)"Showing first 32 backups" : NULL;
  menu.selection = 1;
  menu.scroll = 0;
  for (int i = 0; i < count; i++) {
    menuItems[i].text = applicationBackups[i].backupFilename + 10;
    menuItems[i].color = TEXT_COLOR_BLACK;
  }

  while (1) {
    int result = doMenu(&menu);
    if (result == MENU_RETURN_EXIT) return;
    if (result != MENU_RETURN_SELECTION) continue;

    mMsgBoxPush(6);
    multiPrintXY(3, 2, "Restore this quarantined\n"
                         "Add-In to the root?",
                 TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
    if (!closeMsgBox(1, 5)) continue;
    mMsgBoxPush(6);
    multiPrintXY(3, 2, "Confirm restore; existing\n"
                         "files will never be overwritten.",
                 TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
    if (!closeMsgBox(1, 5)) continue;

    int restoreResult =
      restoreG3ABackup(&applicationBackups[menu.selection - 1]);
    const char* message = restoreResult == APP_MANAGE_OK ?
                          "Add-In restored; backup kept." :
                          restoreResult == APP_MANAGE_TARGET_EXISTS ?
                          "Original exists; nothing restored." :
                          "Restore failed; backup kept.";
    Bdisp_AllClr_VRAM();
    drawScreenTitle("Restore quarantined Add-Ins");
    mPrintXY(2, 5, message, TEXT_MODE_TRANSPARENT_BACKGROUND,
             restoreResult == APP_MANAGE_OK ? TEXT_COLOR_BLACK : TEXT_COLOR_RED);
    mPrintXY(2, 7, "Press EXIT to close",
             TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
    int key;
    do {
      mGetKey(&key);
    } while (key != KEY_CTRL_EXIT);
    return;
  }
}

void applicationAudit() {
  if (!platformHasCapability(PLATFORM_CAP_BFILE)) {
    showApplicationUnavailable();
    return;
  }
  int sortMode = 1;
  int truncated = 0;
  int count;
  StorageGuard storageGuard = {0};
  storageOperationBegin(&storageGuard);
  count = scanG3ARecords(applicationRecords, APP_MAX_G3A_RECORDS, &truncated);
  storageOperationEnd(&storageGuard);
  if (count <= 0) {
    showNoApplications();
    return;
  }

  Bdisp_AllClr_VRAM();
  drawFkeyLabels(0, 0, 0, 0, 0, 0);
  MenuItem menuItems[APP_MAX_G3A_RECORDS];
  Menu menu;
  menu.items = menuItems;
  menu.numitems = count;
  menu.height = 7;
  menu.width = 21;
  menu.scrollout = 1;
  menu.type = MENUTYPE_FKEYS;
  menu.title = (char*)"Application audit";
  menu.selection = 1;
  menu.scroll = 0;

  while (1) {
    sortG3ARecords(applicationRecords, count, sortMode);
    for (int i = 0; i < count; i++) {
      menuItems[i].text = applicationRecords[i].displayName[0] ?
                          applicationRecords[i].displayName :
                          applicationRecords[i].filename;
      menuItems[i].color = applicationRecords[i].status == APP_STATUS_OK ?
                           TEXT_COLOR_BLACK : TEXT_COLOR_RED;
    }
    menu.subtitle = (char*)"F5 restore backups";

    int result = doMenu(&menu);
    if (result == MENU_RETURN_EXIT) return;
    if (result == MENU_RETURN_SELECTION) {
      if (drawApplicationDetails(&applicationRecords[menu.selection - 1])) {
        StorageGuard refreshGuard = {0};
        storageOperationBegin(&refreshGuard);
        count = scanG3ARecords(applicationRecords, APP_MAX_G3A_RECORDS,
                                &truncated);
        storageOperationEnd(&refreshGuard);
        if (count <= 0) return;
        if (menu.selection > count) menu.selection = count;
      }
    } else if (result == KEY_CTRL_F5) {
      restoreApplicationBackups();
    } else if (result == KEY_CTRL_F1) {
      sortMode = 1;
    } else if (result == KEY_CTRL_F2) {
      sortMode = 2;
    } else if (result == KEY_CTRL_F3) {
      sortMode = 3;
    } else if (result == KEY_CTRL_F4) {
      StorageGuard refreshGuard = {0};
      storageOperationBegin(&refreshGuard);
      count = scanG3ARecords(applicationRecords, APP_MAX_G3A_RECORDS,
                              &truncated);
      storageOperationEnd(&refreshGuard);
      if (count <= 0) return;
      if (menu.selection > count) menu.selection = count;
    }
  }
}

static void showConfigurationResult(const char* title, const char* message) {
  Bdisp_AllClr_VRAM();
  drawScreenTitle(title);
  mPrintXY(2, 4, message, TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
  mPrintXY(2, 7, "Press EXIT to close", TEXT_MODE_TRANSPARENT_BACKGROUND,
           TEXT_COLOR_BLACK);
  int key;
  do {
    mGetKey(&key);
  } while (key != KEY_CTRL_EXIT);
}

void configurationManager() {
  if (!platformHasCapability(PLATFORM_CAP_MCS)) {
    Bdisp_AllClr_VRAM();
    drawScreenTitle("Configuration backup");
    mPrintXY(2, 4, "Main memory access unavailable.",
             TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_RED);
    mPrintXY(2, 7, "Press EXIT to close",
             TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
    int unavailableKey;
    do {
      mGetKey(&unavailableKey);
    } while (unavailableKey != KEY_CTRL_EXIT);
    return;
  }
  while (1) {
    Bdisp_AllClr_VRAM();
    drawScreenTitle("Configuration backup");
    mPrintXY(1, 3, "User backup", TEXT_MODE_TRANSPARENT_BACKGROUND,
             TEXT_COLOR_BLUE);
    mPrintXY(14, 3, yesNo(hasSettingsBackup()),
             TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
    mPrintXY(1, 5, "F1  Save current config", TEXT_MODE_TRANSPARENT_BACKGROUND,
             TEXT_COLOR_BLACK);
    mPrintXY(1, 6, "F2  Restore config", TEXT_MODE_TRANSPARENT_BACKGROUND,
             TEXT_COLOR_BLACK);
    mPrintXY(1, 7, "F3  Open native Memory", TEXT_MODE_TRANSPARENT_BACKGROUND,
             TEXT_COLOR_BLACK);
    mPrintXY(1, 4, "Only Utilities settings are stored.",
             TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);

    int key;
    mGetKey(&key);
    if (key == KEY_CTRL_EXIT) return;
    if (key == KEY_CTRL_F1) {
      StorageGuard guard = {0};
      storageOperationBegin(&guard);
      int result = backupSettings();
      storageOperationEnd(&guard);
      showConfigurationResult("Configuration backup",
                              result == 0 ? "Backup saved." :
                                            "Backup could not be saved.");
    } else if (key == KEY_CTRL_F2) {
      if (!hasSettingsBackup()) {
        showConfigurationResult("Configuration backup", "No backup found.");
        continue;
      }
      mMsgBoxPush(4);
      multiPrintXY(3, 2, "Restore the saved\n"
                         "Utilities config?",
                   TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
      int confirmed = closeMsgBox(1, 5);
      if (!confirmed) continue;
      StorageGuard guard = {0};
      storageOperationBegin(&guard);
      int result = restoreSettingsBackup();
      storageOperationEnd(&guard);
      showConfigurationResult("Configuration backup",
                              result == 0 ? "Config restored." :
                                            "Restore failed; current config kept.");
    } else if (key == KEY_CTRL_F3) {
      mMsgBoxPush(4);
      multiPrintXY(3, 2, "Open native Memory?\n"
                         "Choose BACKUP then SAVE.",
                   TEXT_MODE_TRANSPARENT_BACKGROUND, TEXT_COLOR_BLACK);
      if (!closeMsgBox(1, 5)) continue;

      /*
       * The native Memory application is the only supported safe owner of
       * the complete MCS/Main Memory store. Do not try to enumerate or dump
       * raw MCS addresses here. Stop our periodic callback before handing
       * control to the OS app; if an older OS returns normally, restart it
       * and rebuild our framebuffer on the next loop iteration.
       */
      int chronometerWasRunning = isStubTimerRunning();
      if (chronometerWasRunning) stopAndUninstallStubTimer();
      Bkey_ClrAllFlags();
      APP_MEMORY();
      if (chronometerWasRunning) restartStubTimerFunction();
      Bkey_ClrAllFlags();
      Bdisp_EnableColor(1);
      Bdisp_AllClr_VRAM();
    }
  }
}
