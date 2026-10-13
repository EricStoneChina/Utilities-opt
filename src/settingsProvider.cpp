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

#include "settingsProvider.hpp"
#include "constantsProvider.hpp"
#include "storageProvider.hpp"

static unsigned char settings[NUMBER_OF_SETTINGS+1];

static int readSettingsItem(unsigned char* item, unsigned char* buffer,
                            int bufferSize, int* actualSize) {
  int size = 0;
  if (item == NULL || buffer == NULL || bufferSize <= 0) return -1;
  if (MCSGetDlen2(DIRNAME, item, &size) < 0 ||
      size <= 0 || size > bufferSize) return -1;
  if (MCSGetData1(0, size, buffer) < 0) return -1;
  if (actualSize != NULL) *actualSize = size;
  return 0;
}

static int writeSettingsItem(unsigned char* item, const unsigned char* buffer,
                             int size) {
  if (item == NULL || buffer == NULL || size <= 0) return -1;
  MCS_CreateDirectory(DIRNAME);
  MCSDelVar2(DIRNAME, item);
  return MCSPutVar2(DIRNAME, item, size, (void*)buffer) < 0 ? -1 : 0;
}

// Routines for accessing and setting settings
// NOTE: directly accessing the settings array is strictly forbidden!

unsigned char getSetting(const int setting) {
  return settings[setting];
}

void setSetting(const int setting, const unsigned char value, const int autosave) {
  settings[setting] = value;
  if (setting == SETTING_LANGUAGE) {
    if (value == LANGUAGE_CHINESE) EnableGB18030();
    else DisableGB18030();
  }
  if (autosave) saveSettings();
}

// Routines for saving and loading settings
int loadSettings() { // returns 0 on success, 1 if settings were reset (first run / incompatibility)
  StorageScope storageScope;
  int size;
  int bytesToRead;
  MCSGetDlen2(DIRNAME, SETTINGSFILE, &size);
  if (size <= 16) goto resetSettings; // no settings file or incompatible, reset settings.

  // Read only the bytes that are actually present. This also lets us migrate
  // the previous settings format without reading past the old file.
  memset(settings, 0, sizeof(settings));
  bytesToRead = size < (int)sizeof(settings) ? size : (int)sizeof(settings);
  MCSGetData1(0, bytesToRead, settings);
  // ^ +1 because NUMBER_OF_SETTINGS does not include the file version

  // detect if the current settings file version is compatible with the one we're expecting.
  // file version is always the first byte, except for the settings files of versions 1.0 and
  // earlier.
  // for these, the filesize happened to be equal or smaller than 16 (checked above).
  // if we got here, the file is a "modern" one, so check the version byte.
  if (settings[0] == SETTINGSFILE_VERSION && size >= (int)sizeof(settings)) return 0;

  // Version 15 is the last English-only format. Keep all old values and add
  // the new language setting with the Chinese default requested by the fork.
  if (settings[0] == SETTINGSFILE_VERSION - 1 && size >= NUMBER_OF_SETTINGS) {
    settings[SETTING_LANGUAGE] = LANGUAGE_CHINESE;
    settings[SETTING_SELF_FILEVERSION] = SETTINGSFILE_VERSION;
    saveSettings();
    return 0;
  }

  goto resetSettings; // incompatible format

  resetSettings:
  memset(settings+1, 0, (sizeof(settings)-1)*sizeof(unsigned char));
  // the defaults for some settings are not zero:
  settings[SETTING_DISPLAY_STATUSBAR] = 1;
  settings[SETTING_STARTUP_BRIGHTNESS] = 250;
  settings[SETTING_DISPLAY_FKEYS] = 1;
  settings[SETTING_PASSWORD_PRIVACY] = 1;
  settings[SETTING_SHOW_CALENDAR_EVENTS_COUNT] = 1;
  settings[SETTING_ENABLE_LOCK] = 1;
  settings[SETTING_CLOCK_SECONDS] = 1;
  settings[SETTING_DEFAULT_CALENDAR_VIEW] = 1;
  settings[SETTING_SHOW_CALENDAR_BUSY_MAP] = 1;
  settings[SETTING_CHRONO_NOTIFICATION_TYPE] = 1;
  settings[SETTING_FILE_MANAGER_SORT] = 1;
  settings[SETTING_FILE_MANAGER_SEARCH] = 1;
  settings[SETTING_TIMEZONE] = 51;
  settings[SETTING_LANGUAGE] = LANGUAGE_CHINESE;
  return 1;
}

void saveSettings() {
  StorageScope storageScope;
  if(MCS_CreateDirectory(DIRNAME)) // Check if directory exists
  { // directory already exists, so delete the exiting file that may be there
    MCSDelVar2(DIRNAME, SETTINGSFILE);
  }
  settings[SETTING_SELF_FILEVERSION] = SETTINGSFILE_VERSION;
  MCSPutVar2(DIRNAME, SETTINGSFILE, NUMBER_OF_SETTINGS+1, settings);
  // ^ +1 on purpose again, for the same reason
}

int hasSettingsBackup() {
  StorageScope storageScope;
  int size = 0;
  if (MCSGetDlen2(DIRNAME, SETTINGS_BACKUP_FILE, &size) < 0) return 0;
  return size >= 17 && size <= (int)sizeof(settings);
}

int backupSettings() {
  StorageScope storageScope;
  return writeSettingsItem(SETTINGS_BACKUP_FILE, settings, sizeof(settings));
}

int restoreSettingsBackup() {
  StorageScope storageScope;
  unsigned char backup[NUMBER_OF_SETTINGS+1];
  unsigned char current[NUMBER_OF_SETTINGS+1];
  int size = 0;
  if (readSettingsItem(SETTINGS_BACKUP_FILE, backup, sizeof(backup), &size) < 0)
    return -1;
  if (!((backup[SETTING_SELF_FILEVERSION] == SETTINGSFILE_VERSION &&
         size == (int)sizeof(settings)) ||
        (backup[SETTING_SELF_FILEVERSION] == SETTINGSFILE_VERSION - 1 &&
         size >= NUMBER_OF_SETTINGS))) {
    return -2;
  }
  memcpy(current, settings, sizeof(current));
  if (writeSettingsItem(SETTINGS_ROLLBACK_FILE, current, sizeof(current)) < 0)
    return -3;
  if (writeSettingsItem(SETTINGSFILE, backup, size) < 0) {
    // writeSettingsItem replaces the old MCS item, so restore the in-memory
    // version if the replacement could not be completed.
    writeSettingsItem(SETTINGSFILE, current, sizeof(current));
    return -4;
  }
  if (loadSettings() != 0) {
    writeSettingsItem(SETTINGSFILE, current, sizeof(current));
    memcpy(settings, current, sizeof(settings));
    return -5;
  }
  return 0;
}
