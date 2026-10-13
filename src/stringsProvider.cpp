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
#include <ctype.h>

#include "stringsProvider.hpp"
#include "graphicsProvider.hpp"
#include "settingsProvider.hpp"

void formatHex32(char buffer[9], unsigned int value) {
  if (buffer == NULL) return;
  const char digits[] = "0123456789ABCDEF";
  for (int i = 7; i >= 0; i--) {
    buffer[i] = digits[value & 15];
    value >>= 4;
  }
  buffer[8] = 0;
}

int isMBfirst(char s) { // there is probably a syscall for this, but it's not documented
  return (s == '\xE5' || s == '\xE6' || s == '\xE7' || s == '\x7F' || s == '\xF7' || s == '\xF9');
}

int isChineseLanguage() {
  return getSetting(SETTING_LANGUAGE) == LANGUAGE_CHINESE;
}

/*
 * The Prizm Chinese font is selected by the two-byte 0x03 0xA8 prefix passed
 * to PrintXY. Keep that prefix out of this table so translated strings can
 * still be measured and padded by the menu renderer.
 */
static const char* translateText(const char* text) {
  if (!text) return text;
  if (!strcmp(text, "Power options")) return "\xb5\xe7\xd4\xb4\xd1\xa1\xcf\xee";
  if (!strcmp(text, "Auto power off")) return "\xd7\xd4\xb6\xaf\xb9\xd8\xbb\xfa";
  if (!strcmp(text, "Backlight duration")) return "\xb1\xb3\xb9\xe2\xca\xb1\xb3\xa4";
  if (!strcmp(text, "Backlight level")) return "\xb1\xb3\xb9\xe2\xc1\xc1\xb6\xc8";
  if (!strcmp(text, "Power information")) return "\xb5\xe7\xd4\xb4\xd0\xc5\xcf\xa2";
  if (!strcmp(text, "Light tools")) return "\xb5\xc6\xb9\xe2\xb9\xa4\xbe\xdf";
  if (!strcmp(text, "Lantern")) return "\xd5\xd5\xc3\xf7\xb5\xc6";
  if (!strcmp(text, "Flashlight")) return "\xca\xd6\xb5\xe7\xcd\xb2";
  if (!strcmp(text, "Morse light")) return "\xc4\xa6\xb6\xfb\xcb\xb9\xb5\xc6";
  if (!strcmp(text, "Color light")) return "\xb2\xca\xc9\xab\xb5\xc6";
  if (!strcmp(text, "Time tools")) return "\xca\xb1\xbc\xe4\xb9\xa4\xbe\xdf";
  if (!strcmp(text, "Calendar")) return "\xc8\xd5\xc0\xfa";
  if (!strcmp(text, "Tasks")) return "\xc8\xce\xce\xf1";
  if (!strcmp(text, "Chronometer")) return "\xbc\xc6\xca\xb1\xc6\xf7";
  if (!strcmp(text, "Tools")) return "\xb9\xa4\xbe\xdf";
  if (!strcmp(text, "Balance manager")) return "\xd3\xe0\xb6\xee\xb9\xdc\xc0\xed";
  if (!strcmp(text, "Password generator")) return "\xc3\xdc\xc2\xeb\xc9\xfa\xb3\xc9\xc6\xf7";
  if (!strcmp(text, "TOTP authenticator")) return "\xd2\xbb\xb4\xce\xd0\xd4\xc3\xdc\xc2\xeb";
  if (!strcmp(text, "Memory & System")) return "\xc4\xda\xb4\xe6\xd3\xeb\xcf\xb5\xcd\xb3";
  if (!strcmp(text, "Device dashboard")) return "\xc9\xe8\xb1\xb8\xb8\xc5\xc0\xc0";
  if (!strcmp(text, "File manager")) return "\xce\xc4\xbc\xfe\xb9\xdc\xc0\xed\xc6\xf7";
  if (!strcmp(text, "Memory usage")) return "\xc4\xda\xb4\xe6\xca\xb9\xd3\xc3";
  if (!strcmp(text, "Memory Usage")) return "\xc4\xda\xb4\xe6\xca\xb9\xd3\xc3";
  if (!strcmp(text, "Function key color")) return "\xb9\xa6\xc4\xdc\xbc\xfc\xd1\xd5\xc9\xab";
  if (!strcmp(text, "Function key color unavailable.")) return "\xb9\xa6\xc4\xdc\xbc\xfc\xd1\xd5\xc9\xab\xb2\xbb\xbf\xc9\xd3\xc3\xa1\xa3";
  if (!strcmp(text, "Function Key Color")) return "\xb9\xa6\xc4\xdc\xbc\xfc\xd1\xd5\xc9\xab";
  if (!strcmp(text, "System information")) return "\xcf\xb5\xcd\xb3\xd0\xc5\xcf\xa2";
  if (!strcmp(text, "Add-In manager")) return "\xbc\xd3\xd4\xd8\xcf\xee\xb9\xdc\xc0\xed\xc6\xf7";
  if (!strcmp(text, "Settings")) return "\xc9\xe8\xd6\xc3";
  if (!strcmp(text, "Set time")) return "\xc9\xe8\xd6\xc3\xca\xb1\xbc\xe4";
  if (!strcmp(text, "Set date")) return "\xc9\xe8\xd6\xc3\xc8\xd5\xc6\xda";
  if (!strcmp(text, "Time format")) return "\xca\xb1\xbc\xe4\xb8\xf1\xca\xbd";
  if (!strcmp(text, "Date format")) return "\xc8\xd5\xc6\xda\xb8\xf1\xca\xbd";
  if (!strcmp(text, "Home settings")) return "\xd6\xf7\xd2\xb3\xc9\xe8\xd6\xc3";
  if (!strcmp(text, "Calendar settings")) return "\xc8\xd5\xc0\xfa\xc9\xe8\xd6\xc3";
  if (!strcmp(text, "Calc. lock settings")) return "\xcb\xf8\xb6\xa8\xc9\xe8\xd6\xc3";
  if (!strcmp(text, "Chrono. notification")) return "\xbc\xc6\xca\xb1\xcd\xa8\xd6\xaa";
  if (!strcmp(text, "Show advanced tools")) return "\xb8\xdf\xbc\xb6\xb9\xa6\xc4\xdc";
  if (!strcmp(text, "Startup brightness")) return "\xc6\xf4\xb6\xaf\xc1\xc1\xb6\xc8";
  if (!strcmp(text, "Language")) return "\xd3\xef\xd1\xd4";
  if (!strcmp(text, "About this add-in")) return "\xb9\xd8\xd3\xda\xb1\xbe\xb3\xcc\xd0\xf2";
  if (!strcmp(text, "English")) return "\xd3\xa2\xce\xc4";
  if (!strcmp(text, "Chinese")) return "\xd6\xd0\xce\xc4";
  if (!strcmp(text, "Set time format")) return "\xc9\xe8\xd6\xc3\xca\xb1\xbc\xe4\xb8\xf1\xca\xbd";
  if (!strcmp(text, "Set date format")) return "\xc9\xe8\xd6\xc3\xc8\xd5\xc6\xda\xb8\xf1\xca\xbd";
  if (!strcmp(text, "No notification")) return "\xb2\xbb\xcd\xa8\xd6\xaa";
  if (!strcmp(text, "Flashing pop-up")) return "\xc9\xc1\xcb\xb8\xb5\xaf\xb4\xb0";
  if (!strcmp(text, "Simple pop-up")) return "\xc6\xd5\xcd\xa8\xb5\xaf\xb4\xb0";
  if (!strcmp(text, "Note on home")) return "\xd6\xf7\xd2\xb3\xcc\xe1\xca\xbe";
  if (!strcmp(text, "Chrono. notif.")) return "\xbc\xc6\xca\xb1\xcd\xa8\xd6\xaa";
  if (!strcmp(text, "Set start brightness")) return "\xc9\xe8\xd6\xc3\xc6\xf4\xb6\xaf\xc1\xc1\xb6\xc8";
  if (!strcmp(text, "Set password")) return "\xc9\xe8\xd6\xc3\xc3\xdc\xc2\xeb";
  if (!strcmp(text, "Show last character")) return "\xcf\xd4\xca\xbe\xc4\xa9\xce\xbb\xd7\xd6\xb7\xfb";
  if (!strcmp(text, "Off after locking")) return "\xcb\xf8\xb6\xa8\xba\xf3\xb9\xd8\xbb\xfa";
  if (!strcmp(text, "Lock on [EXE]")) return "\xb0\xb4 EXE \xcb\xf8\xb6\xa8";
  if (!strcmp(text, "Show owner info")) return "\xcf\xd4\xca\xbe\xcb\xf9\xd3\xd0\xd5\xdf";
  if (!strcmp(text, "Run-Mat on unlock")) return "\xbd\xe2\xcb\xf8\xba\xf3\xd4\xcb\xd0\xd0\xd6\xf7\xb3\xcc\xd0\xf2";
  if (!strcmp(text, "Select Panel Type")) return "\xd1\xa1\xd4\xf1\xc3\xe6\xb0\xe5\xc0\xe0\xd0\xcd";
  if (!strcmp(text, "Disabled")) return "\xbd\xfb\xd3\xc3";
  if (!strcmp(text, "Today events")) return "\xbd\xf1\xc8\xd5\xca\xc2\xbc\xfe";
  if (!strcmp(text, "Run-Mat shortcut")) return "\xd6\xf7\xb3\xcc\xd0\xf2\xbf\xec\xbd\xdd\xb7\xbd\xca\xbd";
  if (!strcmp(text, "File mgr.shortcut")) return "\xce\xc4\xbc\xfe\xb9\xdc\xc0\xed\xbf\xec\xbd\xdd\xb7\xbd\xca\xbd";
  if (!strcmp(text, "Calendar shortcut")) return "\xc8\xd5\xc0\xfa\xbf\xec\xbd\xdd\xb7\xbd\xca\xbd";
  if (!strcmp(text, "Tasks shortcut")) return "\xc8\xce\xce\xf1\xbf\xec\xbd\xdd\xb7\xbd\xca\xbd";
  if (!strcmp(text, "Chrono. shortcut")) return "\xbc\xc6\xca\xb1\xc6\xf7\xbf\xec\xbd\xdd\xb7\xbd\xca\xbd";
  if (!strcmp(text, "Bal. mgr.shortcut")) return "\xd3\xe0\xb6\xee\xb9\xdc\xc0\xed\xbf\xec\xbd\xdd\xb7\xbd\xca\xbd";
  if (!strcmp(text, "Pw. gen. shortcut")) return "\xc3\xdc\xc2\xeb\xc9\xfa\xb3\xc9\xbf\xec\xbd\xdd\xb7\xbd\xca\xbd";
  if (!strcmp(text, "TOTP shortcut")) return "\xd2\xbb\xb4\xce\xd0\xd4\xc3\xdc\xc2\xeb\xbf\xec\xbd\xdd\xb7\xbd\xca\xbd";
  if (!strcmp(text, "Add-in mgr. s.cut")) return "\xbc\xd3\xd4\xd8\xcf\xee\xb9\xdc\xc0\xed\xbf\xec\xbd\xdd\xb7\xbd\xca\xbd";
  if (!strcmp(text, "Clock type")) return "\xca\xb1\xd6\xd3\xc0\xe0\xd0\xcd";
  if (!strcmp(text, "Date appearance")) return "\xc8\xd5\xc6\xda\xcf\xd4\xca\xbe";
  if (!strcmp(text, "Show seconds")) return "\xcf\xd4\xca\xbe\xc3\xeb\xca\xfd";
  if (!strcmp(text, "Show F. keys labels")) return "\xcf\xd4\xca\xbe\xb9\xa6\xc4\xdc\xbc\xfc\xb1\xea\xc7\xa9";
  if (!strcmp(text, "Display statusbar")) return "\xcf\xd4\xca\xbe\xd7\xb4\xcc\xac\xc0\xb8";
  if (!strcmp(text, "Dark theme")) return "\xc9\xee\xc9\xab\xd6\xf7\xcc\xe2";
  if (!strcmp(text, "Top home panel")) return "\xd6\xf7\xd2\xb3\xc9\xcf\xb7\xbd\xc3\xe6\xb0\xe5";
  if (!strcmp(text, "Right home panel")) return "\xd6\xf7\xd2\xb3\xd3\xd2\xb2\xe0\xc3\xe6\xb0\xe5";
  if (!strcmp(text, "Bottom home panel")) return "\xd6\xf7\xd2\xb3\xcf\xc2\xb7\xbd\xc3\xe6\xb0\xe5";
  if (!strcmp(text, "Left home panel")) return "\xd6\xf7\xd2\xb3\xd7\xf3\xb2\xe0\xc3\xe6\xb0\xe5";
  if (!strcmp(text, "Set long date format")) return "\xc9\xe8\xd6\xc3\xb3\xa4\xc8\xd5\xc6\xda\xb8\xf1\xca\xbd";
  if (!strcmp(text, "Show events count")) return "\xcf\xd4\xca\xbe\xca\xc2\xbc\xfe\xca\xfd\xc1\xbf";
  if (!strcmp(text, "Default calendar view")) return "\xc4\xac\xc8\xcf\xc8\xd5\xc0\xfa\xca\xd3\xcd\xbc";
  if (!strcmp(text, "First day of week")) return "\xc3\xbf\xd6\xdc\xc6\xf0\xca\xbc\xc8\xd5";
  if (!strcmp(text, "Show busy timetables")) return "\xcf\xd4\xca\xbe\xc3\xa6\xcf\xd0\xca\xb1\xbc\xe4\xb1\xed";
  if (!strcmp(text, "Default Calendar")) return "\xc4\xac\xc8\xcf\xc8\xd5\xc0\xfa";
  if (!strcmp(text, "Week")) return "\xd6\xdc";
  if (!strcmp(text, "Month")) return "\xd4\xc2";
  if (!strcmp(text, "Week starts on")) return "\xc3\xbf\xd6\xdc\xbf\xaa\xca\xbc\xd3\xda";
  if (!strcmp(text, "Calculator model")) return "\xbc\xc6\xcb\xe3\xc6\xf7\xd0\xcd\xba\xc5";
  if (!strcmp(text, "Calculator model:")) return "\xbc\xc6\xcb\xe3\xc6\xf7\xd0\xcd\xba\xc5:";
  if (!strcmp(text, "Model")) return "\xc4\xa3\xd0\xcd";
  if (!strcmp(text, "OS")) return "\xcf\xb5\xcd\xb3";
  if (!strcmp(text, "OS version:")) return "OS\xb0\xe6\xb1\xbe:";
  if (!strcmp(text, "Main")) return "\xd6\xf7\xc4\xda\xb4\xe6";
  if (!strcmp(text, "Password")) return "\xc3\xdc\xc2\xeb";
  if (!strcmp(text, "UI")) return "\xbd\xe7\xc3\xe6";
  if (!strcmp(text, "OS build:")) return "OS\xb9\xb9\xbd\xa8:";
  if (!strcmp(text, "Capabilities:")) return "\xc4\xdc\xc1\xa6:";
  if (!strcmp(text, "Ver")) return "\xb0\xe6\xb1\xbe";
  if (!strcmp(text, "HW")) return "\xd3\xb2\xbc\xfe";
  if (!strcmp(text, "Caps")) return "\xbd\xd3\xbf\xda";
  if (!strcmp(text, "OS version")) return "OS\xb0\xe6\xb1\xbe";
  if (!strcmp(text, "Storage")) return "\xb4\xe6\xb4\xa2";
  if (!strcmp(text, "Main memory")) return "\xd6\xf7\xc4\xda\xb4\xe6";
  if (!strcmp(text, "Memory")) return "\xc4\xda\xb4\xe6";
  if (!strcmp(text, "Battery")) return "\xb5\xe7\xb3\xd8";
  if (!strcmp(text, "Power")) return "\xb5\xe7\xd4\xb4";
  if (!strcmp(text, "Date")) return "\xc8\xd5\xc6\xda";
  if (!strcmp(text, "Time")) return "\xca\xb1\xbc\xe4";
  if (!strcmp(text, "File")) return "\xce\xc4\xbc\xfe";
  if (!strcmp(text, "Name")) return "\xc3\xfb\xb3\xc6";
  if (!strcmp(text, "Size")) return "\xb4\xf3\xd0\xa1";
  if (!strcmp(text, "Code")) return "\xb4\xfa\xc2\xeb";
  if (!strcmp(text, "Header")) return "\xcd\xb7\xb2\xbf";
  if (!strcmp(text, "Checksum")) return "\xd0\xa3\xd1\xe9\xba\xcd";
  if (!strcmp(text, "Icon")) return "\xcd\xbc\xb1\xea";
  if (!strcmp(text, "Duplicate")) return "\xd6\xd8\xb8\xb4";
  if (!strcmp(text, "Version")) return "\xb0\xe6\xb1\xbe";
  if (!strcmp(text, "Hardware")) return "\xd3\xb2\xbc\xfe";
  if (!strcmp(text, "Capabilities")) return "\xbd\xd3\xbf\xda\xc4\xdc\xc1\xa6";
  if (!strcmp(text, "EXIT to close")) return "EXIT\xb9\xd8\xb1\xd5";
  if (!strcmp(text, "Storage free")) return "\xb4\xe6\xb4\xa2\xca\xa3\xd3\xe0";
  if (!strcmp(text, "Main memory free")) return "\xd6\xf7\xc4\xda\xb4\xe6\xca\xa3\xd3\xe0";
  if (!strcmp(text, "Battery voltage")) return "\xb5\xe7\xb3\xd8\xb5\xe7\xd1\xb9";
  if (!strcmp(text, "Real hardware")) return "\xd5\xe6\xca\xb5\xd3\xb2\xbc\xfe";
  if (!strcmp(text, "Real hardware:")) return "\xd5\xe6\xca\xb5\xd3\xb2\xbc\xfe:";
  if (!strcmp(text, "Device ID:")) return "\xc9\xe8\xb1\xb8 ID:";
  if (!strcmp(text, "System Information")) return "\xcf\xb5\xcd\xb3\xd0\xc5\xcf\xa2";
  if (!strcmp(text, "User Information")) return "\xd3\xc3\xbb\xa7\xd0\xc5\xcf\xa2";
  if (!strcmp(text, "Yes")) return "\xca\xc7";
  if (!strcmp(text, "No")) return "\xb7\xf1";
  if (!strcmp(text, "Press EXIT to close")) return "\xb0\xb4 EXIT \xb9\xd8\xb1\xd5";
  if (!strcmp(text, "Interface diagnostics")) return "\xbd\xd3\xbf\xda\xd5\xef\xb6\xcf";
  if (!strcmp(text, "Application audit")) return "\xd3\xa6\xd3\xc3\xb3\xcc\xd0\xf2\xc9\xf3\xba\xcb";
  if (!strcmp(text, "Application details")) return "\xd3\xa6\xd3\xc3\xb3\xcc\xd0\xf2\xcf\xea\xc7\xe9";
  if (!strcmp(text, "Application icon")) return "\xd3\xa6\xd3\xc3\xb3\xcc\xd0\xf2\xcd\xbc\xb1\xea";
  if (!strcmp(text, "Icon unavailable.")) return "\xcd\xbc\xb1\xea\xb2\xbb\xbf\xc9\xd3\xc3\xa1\xa3";
  if (!strcmp(text, "Screen tools")) return "\xc6\xc1\xc4\xbb\xb9\xa4\xbe\xdf";
  if (!strcmp(text, "Performance lab")) return "\xd0\xd4\xc4\xdc\xca\xb5\xd1\xe9\xca\xd2";
  if (!strcmp(text, "Native system settings")) return "\xd4\xad\xc9\xfa\xcf\xb5\xcd\xb3\xc9\xe8\xd6\xc3";
  if (!strcmp(text, "Configuration backup")) return "\xc5\xe4\xd6\xc3\xb1\xb8\xb7\xdd";
  if (!strcmp(text, "Showing first 64 entries")) return "\xbd\xf6\xcf\xd4\xca\xbe\xc7\xb0 64 \xcf\xee";
  if (!strcmp(text, "Native settings unavailable")) return "\xd4\xad\xc9\xfa\xc9\xe8\xd6\xc3\xb2\xbb\xbf\xc9\xd3\xc3";
  if (!strcmp(text, "This platform has no native settings.")) return "\xb4\xcb\xc6\xbd\xcc\xa8\xc3\xbb\xd3\xd0\xd4\xad\xc9\xfa\xc9\xe8\xd6\xc3";
  if (!strcmp(text, "Native power")) return "\xd4\xad\xc9\xfa\xb5\xe7\xd4\xb4";
  if (!strcmp(text, "Native display")) return "\xd4\xad\xc9\xfa\xcf\xd4\xca\xbe";
  if (!strcmp(text, "Native language")) return "\xd4\xad\xc9\xfa\xd3\xef\xd1\xd4";
  if (!strcmp(text, "Native version")) return "\xd4\xad\xc9\xfa\xb0\xe6\xb1\xbe";
  if (!strcmp(text, "Link transfer")) return "\xc1\xac\xbd\xd3\xb4\xab\xca\xe4";
  if (!strcmp(text, "Link unavailable")) return "\xc1\xac\xbd\xd3\xb2\xbb\xbf\xc9\xd3\xc3";
  if (!strcmp(text, "This platform has no native Link.")) return "\xb4\xcb\xc6\xbd\xcc\xa8\xc3\xbb\xd3\xd0\xd4\xad\xc9\xfa Link\xa1\xa3";
  if (!strcmp(text, "Open native Link transfer?")) return "\xb4\xf2\xbf\xaa\xd4\xad\xc9\xfa Link \xb4\xab\xca\xe4\xa3\xbf";
  if (!strcmp(text, "User backup")) return "\xd3\xc3\xbb\xa7\xb1\xb8\xb7\xdd";
  if (!strcmp(text, "F1  Save current config")) return "F1  \xb1\xa3\xb4\xe6\xb5\xb1\xc7\xb0\xc5\xe4\xd6\xc3";
  if (!strcmp(text, "F2  Restore config")) return "F2  \xbb\xd6\xb8\xb4\xc5\xe4\xd6\xc3";
  if (!strcmp(text, "F3  Open native Memory")) return "F3  \xb4\xf2\xbf\xaa\xd4\xad\xc9\xfa\xc4\xda\xb4\xe6";
  if (!strcmp(text, "Only Utilities settings are stored.")) return "\xbd\xf6\xb1\xa3\xb4\xe6 Utilities \xc9\xe8\xd6\xc3\xa1\xa3";
  if (!strcmp(text, "Restore creates a rollback copy first.")) return "\xbb\xd6\xb8\xb4\xc7\xb0\xcf\xc8\xb4\xb4\xbd\xa8\xbb\xd8\xb9\xf6\xb1\xb8\xb7\xdd\xa1\xa3";
  if (!strcmp(text, "Open native Memory?")) return "\xb4\xf2\xbf\xaa\xd4\xad\xc9\xfa\xc4\xda\xb4\xe6\xa3\xbf";
  if (!strcmp(text, "Choose BACKUP then SAVE.")) return "\xc7\xeb\xd1\xa1\xd4\xf1 BACKUP \xd4\xd9\xd1\xa1 SAVE\xa1\xa3";
  if (!strcmp(text, "Backup saved.")) return "\xb1\xb8\xb7\xdd\xd2\xd1\xb1\xa3\xb4\xe6\xa1\xa3";
  if (!strcmp(text, "Backup could not be saved.")) return "\xb1\xb8\xb7\xdd\xb1\xa3\xb4\xe6\xca\xa7\xb0\xdc\xa1\xa3";
  if (!strcmp(text, "Restore the saved")) return "\xbb\xd6\xb8\xb4\xd2\xd1\xb1\xa3\xb4\xe6\xb5\xc4";
  if (!strcmp(text, "Utilities config?")) return "Utilities \xc5\xe4\xd6\xc3\xa3\xbf";
  if (!strcmp(text, "No backup found.")) return "\xce\xb4\xd5\xd2\xb5\xbd\xb1\xb8\xb7\xdd\xa1\xa3";
  if (!strcmp(text, "Config restored.")) return "\xc5\xe4\xd6\xc3\xd2\xd1\xbb\xd6\xb8\xb4\xa1\xa3";
  if (!strcmp(text, "Restore failed; current config kept.")) return "\xbb\xd6\xb8\xb4\xca\xa7\xb0\xdc\xa3\xbb\xb5\xb1\xc7\xb0\xc5\xe4\xd6\xc3\xb1\xa3\xc1\xf4\xa1\xa3";
  if (!strcmp(text, "Running benchmarks...")) return "\xd5\xfd\xd4\xda\xd4\xcb\xd0\xd0\xd0\xd4\xc4\xdc\xb2\xe2\xca\xd4...";
  if (!strcmp(text, "Arithmetic")) return "\xcb\xe3\xca\xf5";
  if (!strcmp(text, "Memory copy")) return "\xc4\xda\xb4\xe6\xb8\xb4\xd6\xc6";
  if (!strcmp(text, "VRAM draw")) return "VRAM \xbb\xe6\xcd\xbc";
  if (!strcmp(text, "Timer")) return "\xb6\xa8\xca\xb1\xc6\xf7";
  if (!strcmp(text, "File I/O")) return "\xce\xc4\xbc\xfe I/O";
  if (!strcmp(text, "unavailable")) return "\xb2\xbb\xbf\xc9\xd3\xc3";
  if (!strcmp(text, "Timer callbacks:")) return "\xb6\xa8\xca\xb1\xc6\xf7\xbb\xd8\xb5\xf7\xa3\xba";
  if (!strcmp(text, "Timer error:")) return "\xb6\xa8\xca\xb1\xc6\xf7\xce\xf3\xb2\xee\xa3\xba";
  if (!strcmp(text, "F1 run  F2 reset  F3 save")) return "F1  \xd4\xcb\xd0\xd0  F2  \xd6\xd8\xd6\xc3  F3  \xb1\xa3\xb4\xe6";
  if (!strcmp(text, "Save benchmark")) return "\xb1\xa3\xb4\xe6\xb2\xe2\xca\xd4";
  if (!strcmp(text, "report to UTPERF.TXT?")) return "\xb1\xa8\xb8\xe6\xb5\xbd UTPERF.TXT?";
  if (!strcmp(text, "Report saved.")) return "\xb1\xa8\xb8\xe6\xd2\xd1\xb1\xa3\xb4\xe6\xa1\xa3";
  if (!strcmp(text, "Report exists; not overwritten.")) return "\xb1\xa8\xb8\xe6\xd2\xd1\xb4\xe6\xd4\xda\xa3\xac\xce\xb4\xb8\xc4\xb6\xaf\xa1\xa3";
  if (!strcmp(text, "Report could not be saved.")) return "\xce\xde\xb7\xa8\xb1\xa3\xb4\xe6\xb1\xa8\xb8\xe6\xa1\xa3";
  if (!strcmp(text, "Results use RTC ticks (1/128 s).")) return "\xbd\xe1\xb9\xfb\xca\xb9\xd3\xc3 RTC \xca\xb1\xbc\xe4\xa3\xa8""1/128 \xc3\xeb\xa3\xa9\xa1\xa3";
  if (!strcmp(text, "Save screenshot")) return "\xb1\xa3\xb4\xe6\xbd\xd8\xcd\xbc";
  if (!strcmp(text, "F1  Save screenshot")) return "F1  \xb1\xa3\xb4\xe6\xbd\xd8\xcd\xbc";
  if (!strcmp(text, "F2  RGB565 color test")) return "F2  RGB565 \xd1\xd5\xc9\xab\xb2\xe2\xca\xd4";
  if (!strcmp(text, "F3  Pixel coordinates")) return "F3  \xcf\xf1\xcb\xd8\xd7\xf8\xb1\xea";
  if (!strcmp(text, "F4  Full fill/refresh")) return "F4  \xc8\xab\xc6\xc1\xcc\xee\xb3\xe4/\xcb\xa2\xd0\xc2";
  if (!strcmp(text, "F5  Frame rate")) return "F5  \xd6\xa1\xc2\xca\xb2\xe2\xca\xd4";
  if (!strcmp(text, "RGB565 color test")) return "RGB565 \xd1\xd5\xc9\xab\xb2\xe2\xca\xd4";
  if (!strcmp(text, "Pixel coordinates")) return "\xcf\xf1\xcb\xd8\xd7\xf8\xb1\xea";
  if (!strcmp(text, "Full fill/refresh")) return "\xc8\xab\xc6\xc1\xcc\xee\xb3\xe4/\xcb\xa2\xd0\xc2";
  if (!strcmp(text, "Frame rate")) return "\xd6\xa1\xc2\xca\xb2\xe2\xca\xd4";
  if (!strcmp(text, "Frames:")) return "\xd6\xa1\xca\xfd:";
  if (!strcmp(text, "Elapsed:")) return "\xd3\xc3\xca\xb1:";
  if (!strcmp(text, "Refresh:")) return "\xcb\xa2\xd0\xc2\xc2\xca:";
  if (!strcmp(text, "Tests draw to VRAM only until refresh.")) return "\xb2\xe2\xca\xd4\xd6\xbb\xbb\xe6\xd6\xc1 VRAM\xa3\xac\xcb\xa2\xd0\xc2\xba\xf3\xb2\xc5\xcf\xd4\xca\xbe\xa1\xa3";
  if (!strcmp(text, "RGB565: R5 G6 B5")) return "RGB565\xa3\xba R5 G6 B5";
  if (!strcmp(text, "Press any key to return")) return "\xb0\xb4\xc8\xce\xd2\xe2\xbc\xfc\xb7\xb5\xbb\xd8";
  if (!strcmp(text, "not run")) return "\xce\xb4\xd4\xcb\xd0\xd0";
  if (!strcmp(text, "No .g3a applications found.")) return "\xce\xb4\xd5\xd2\xb5\xbd .g3a \xd3\xa6\xd3\xc3\xb3\xcc\xd0\xf2";
  if (!strcmp(text, "Read-only  EXIT close")) return "\xd6\xbb\xb6\xc1 EXIT \xb9\xd8\xb1\xd5";
  if (!strcmp(text, "F5 quarantine  EXIT close")) return "F5 \xb8\xf4\xc0\xeb\xb1\xb8\xb7\xdd  EXIT \xb9\xd8\xb1\xd5";
  if (!strcmp(text, "Back up this add-in")) return "\xb1\xb8\xb7\xdd\xb4\xcb\xbc\xd3\xd4\xd8\xcf\xee";
  if (!strcmp(text, "and remove it from root?")) return "\xb2\xa2\xb4\xd3\xb8\xf9\xc4\xbf\xc2\xbc\xd2\xc6\xb3\xfd\xa3\xbf";
  if (!strcmp(text, "Confirm quarantine of")) return "\xc8\xb7\xc8\xcf\xb8\xf4\xc0\xeb";
  if (!strcmp(text, "the selected .g3a file?")) return "\xd2\xd1\xd1\xa1\xd4\xf1\xb5\xc4 .g3a \xce\xc4\xbc\xfe\xa3\xbf";
  if (!strcmp(text, "Backup saved; add-in removed.")) return "\xb1\xb8\xb7\xdd\xd2\xd1\xb1\xa3\xb4\xe6\xa3\xbb\xbc\xd3\xd4\xd8\xcf\xee\xd2\xd1\xd2\xc6\xb3\xfd\xa1\xa3";
  if (!strcmp(text, "Backup exists; nothing removed.")) return "\xb1\xb8\xb7\xdd\xd2\xd1\xb4\xe6\xd4\xda\xa3\xbb\xce\xb4\xd2\xc6\xb3\xfd\xa1\xa3";
  if (!strcmp(text, "Backup saved; remove failed.")) return "\xb1\xb8\xb7\xdd\xd2\xd1\xb1\xa3\xb4\xe6\xa3\xbb\xd2\xc6\xb3\xfd\xca\xa7\xb0\xdc\xa1\xa3";
  if (!strcmp(text, "Backup failed; nothing removed.")) return "\xb1\xb8\xb7\xdd\xca\xa7\xb0\xdc\xa3\xbb\xce\xb4\xd2\xc6\xb3\xfd\xa1\xa3";
  if (!strcmp(text, "Restore quarantined Add-Ins")) return "\xbb\xd6\xb8\xb4\xb8\xf4\xc0\xeb\xb5\xc4\xbc\xd3\xd4\xd8\xcf\xee";
  if (!strcmp(text, "No quarantined backups found.")) return "\xce\xb4\xd5\xd2\xb5\xbd\xb8\xf4\xc0\xeb\xb1\xb8\xb7\xdd\xa1\xa3";
  if (!strcmp(text, "Showing first 32 backups")) return "\xbd\xf6\xcf\xd4\xca\xbe\xc7\xb0 32 \xb7\xdd";
  if (!strcmp(text, "F5 restore backups")) return "F5 \xbb\xd6\xb8\xb4\xb1\xb8\xb7\xdd";
  if (!strcmp(text, "Restore this quarantined")) return "\xbb\xd6\xb8\xb4\xb4\xcb\xb8\xf4\xc0\xeb\xb5\xc4";
  if (!strcmp(text, "Add-In to the root?")) return "\xbc\xd3\xd4\xd8\xcf\xee\xb5\xbd\xb8\xf9\xc4\xbf\xc2\xbc\xa3\xbf";
  if (!strcmp(text, "Confirm restore; existing")) return "\xc8\xb7\xc8\xcf\xbb\xd6\xb8\xb4\xa3\xbb\xd2\xd1\xd3\xd0\xce\xc4\xbc\xfe";
  if (!strcmp(text, "files will never be overwritten.")) return "\xce\xc4\xbc\xfe\xb2\xbb\xbb\xe1\xb1\xbb\xb8\xb2\xb8\xc7\xa1\xa3";
  if (!strcmp(text, "Add-In restored; backup kept.")) return "\xbc\xd3\xd4\xd8\xcf\xee\xd2\xd1\xbb\xd6\xb8\xb4\xa3\xbb\xb1\xa3\xc1\xf4\xb1\xb8\xb7\xdd\xa1\xa3";
  if (!strcmp(text, "Original exists; nothing restored.")) return "\xd4\xad\xce\xc4\xbc\xfe\xd2\xd1\xb4\xe6\xd4\xda\xa3\xbb\xce\xb4\xbb\xd6\xb8\xb4\xa1\xa3";
  if (!strcmp(text, "Restore failed; backup kept.")) return "\xbb\xd6\xb8\xb4\xca\xa7\xb0\xdc\xa3\xbb\xb1\xa3\xc1\xf4\xb1\xb8\xb7\xdd\xa1\xa3";
  if (!strcmp(text, "Bad")) return "\xb4\xed\xce\xf3";
  if (!strcmp(text, "Bad header")) return "\xcd\xb7\xb2\xbf\xb4\xed\xce\xf3";
  if (!strcmp(text, "Checksum mismatch")) return "\xd0\xa3\xd1\xe9\xba\xcd\xb2\xbb\xd2\xbb\xd6\xc2";
  if (!strcmp(text, "Unreadable")) return "\xce\xde\xb7\xa8\xb6\xc1\xc8\xa1";
  if (!strcmp(text, "Suspected")) return "\xd2\xc9\xcb\xc6";
  if (!strcmp(text, "Created")) return "\xb4\xb4\xbd\xa8";
  if (!strcmp(text, "Run-Mat unavailable")) return "Run-Mat\xb2\xbb\xbf\xc9\xd3\xc3";
  if (!strcmp(text, "This calculator model is not")) return "\xb4\xcb\xbc\xc6\xcb\xe3\xc6\xf7\xd0\xcd\xba\xc5\xb2\xbb";
  if (!strcmp(text, "supported by the safe return path.")) return "\xb0\xb2\xc8\xab\xb7\xb5\xbb\xd8\xc2\xb7\xbe\xb6\xb2\xbb\xbf\xc9\xd3\xc3";
  if (!strcmp(text, "The return timer could not")) return "\xce\xde\xb7\xa8\xb0\xb2\xd7\xb0\xb7\xb5\xbb\xd8\xb6\xa8\xca\xb1\xc6\xf7";
  if (!strcmp(text, "be installed. Run-Mat was not opened.")) return "\xce\xb4\xb4\xf2\xbf\xaa Run-Mat";
  if (!strcmp(text, "Diagnostics unavailable")) return "\xd5\xef\xb6\xcf\xb2\xbb\xbf\xc9\xd3\xc3";
  if (!strcmp(text, "Run-Mat active")) return "Run-Mat\xd7\xb4\xcc\xac";
  if (!strcmp(text, "Run-Mat timer")) return "Run-Mat\xb6\xa8\xca\xb1\xc6\xf7";
  if (!strcmp(text, "Run-Mat loops")) return "Run-Mat\xb4\xce\xca\xfd";
  if (!strcmp(text, "Active")) return "\xd2\xd1\xc6\xf4\xd3\xc3";
  if (!strcmp(text, "Key")) return "\xb0\xb4\xbc\xfc";
  if (!strcmp(text, "Status")) return "\xd7\xb4\xcc\xac";
  if (!strcmp(text, "UI/Ver")) return "UI/\xb0\xe6\xb1\xbe";
  if (!strcmp(text, "Sys/Ver")) return "\xcf\xb5\xcd\xb3/\xb0\xe6\xb1\xbe";
  if (!strcmp(text, "Run timer")) return "\xd4\xcb\xd0\xd0\xb6\xa8\xca\xb1\xc6\xf7";
  if (!strcmp(text, "Run loops")) return "\xd4\xcb\xd0\xd0\xb4\xce\xca\xfd";
  if (!strcmp(text, "Combo")) return "\xd7\xe9\xba\xcf\xbc\xfc";
  if (!strcmp(text, "Queue")) return "\xb6\xd3\xc1\xd0";
  if (!strcmp(text, "Matrix")) return "\xbe\xd8\xd5\xf3";
  if (!strcmp(text, "Heartbeat")) return "\xd0\xc4\xcc\xf8";
  if (!strcmp(text, "Event")) return "\xca\xc2\xbc\xfe";
  if (!strcmp(text, "Error")) return "\xb4\xed\xce\xf3";
  if (!strcmp(text, "Matrix queue")) return "\xbe\xd8\xd5\xf3\xb6\xd3\xc1\xd0";
  if (!strcmp(text, "Matrix code")) return "\xbe\xd8\xd5\xf3\xc2\xeb";
  if (!strcmp(text, "Last GetKey")) return "\xd7\xee\xba\xf3\xb0\xb4\xbc\xfc";
  if (!strcmp(text, "Timer heartbeat")) return "\xb6\xa8\xca\xb1\xc6\xf7\xd0\xc4\xcc\xf8";
  if (!strcmp(text, "Last event")) return "\xd7\xee\xba\xf3\xca\xc2\xbc\xfe";
  if (!strcmp(text, "Last error")) return "\xd7\xee\xba\xf3\xb4\xed\xce\xf3";
  if (!strcmp(text, "F1 clear log   EXIT close")) return "F1\xc7\xe5\xb3\xfd\xc8\xd5\xd6\xbe EXIT\xb9\xd8\xb1\xd5";
  if (!strcmp(text, "EXIT close")) return "EXIT\xb9\xd8\xb1\xd5";
  if (!strcmp(text, "None")) return "\xce\xde";
  if (!strcmp(text, "The diagnostic timer could not be installed.")) return "\xce\xde\xb7\xa8\xb0\xb2\xd7\xb0\xd5\xef\xb6\xcf\xb6\xa8\xca\xb1\xc6\xf7";
  if (!strcmp(text, "Run-Mat entered")) return "Run-Mat\xd2\xd1\xbd\xf8\xc8\xeb";
  if (!strcmp(text, "Run-Mat returned")) return "Run-Mat\xd2\xd1\xb7\xb5\xbb\xd8";
  if (!strcmp(text, "Run-Mat error")) return "Run-Mat\xb4\xed\xce\xf3";
  if (!strcmp(text, "Timer started")) return "\xb6\xa8\xca\xb1\xc6\xf7\xd2\xd1\xc6\xf4\xb6\xaf";
  if (!strcmp(text, "Timer stopped")) return "\xb6\xa8\xca\xb1\xc6\xf7\xd2\xd1\xcd\xa3\xd6\xb9";
  if (!strcmp(text, "Key matrix sampled")) return "\xb0\xb4\xbc\xfc\xbe\xd8\xd5\xf3\xd2\xd1\xb2\xc9\xd1\xf9";
  if (!strcmp(text, "Storage guard")) return "\xb4\xe6\xb4\xa2\xb1\xa3\xbb\xa4";
  if (!strcmp(text, "Unknown event")) return "\xce\xb4\xd6\xaa\xca\xc2\xbc\xfe";
  if (!strcmp(text, "Unknown")) return "\xce\xb4\xd6\xaa";
  if (!strcmp(text, "n/a")) return "\xce\xde";
  if (!strcmp(text, "Press EXIT to close.")) return "\xb0\xb4 EXIT \xb9\xd8\xb1\xd5\xa1\xa3";
  if (!strcmp(text, "Delete the")) return "\xc9\xbe\xb3\xfd";
  if (!strcmp(text, "Selected Add-In?")) return "\xd1\xa1\xd6\xd0\xb5\xc4\xbc\xd3\xd4\xd8\xcf\xee\xa3\xbf";
  if (!strcmp(text, "Back up and quarantine the")) return "\xb1\xb8\xb7\xdd\xb2\xa2\xb8\xf4\xc0\xeb";
  if (!strcmp(text, "Change the")) return "\xb8\xc4\xb1\xe4";
  if (!strcmp(text, "Selected Add-In state?")) return "\xd1\xa1\xd6\xd0\xbc\xd3\xd4\xd8\xcf\xee\xd7\xb4\xcc\xac\xa3\xbf";
  if (!strcmp(text, "Recent diagnostic events")) return "\xd7\xee\xbd\xfc\xd5\xef\xb6\xcf\xca\xc2\xbc\xfe";
  if (!strcmp(text, "Newest event first")) return "\xd7\xee\xd0\xc2\xca\xc2\xbc\xfe\xd4\xda\xc7\xb0";
  if (!strcmp(text, "No events recorded.")) return "\xce\xde\xbc\xc7\xc2\xbc\xb5\xc4\xca\xc2\xbc\xfe\xa3\xa1";
  if (!strcmp(text, "EXIT return")) return "EXIT\xb7\xb5\xbb\xd8";
  if (!strcmp(text, "Storage access unavailable.")) return "\xb4\xe6\xb4\xa2\xb7\xc3\xce\xca\xb2\xbb\xbf\xc9\xd3\xc3\xa1\xa3";
  if (!strcmp(text, "Main memory access unavailable.")) return "\xd6\xf7\xc4\xda\xb4\xe6\xb7\xc3\xce\xca\xb2\xbb\xbf\xc9\xd3\xc3\xa1\xa3";
  if (!strcmp(text, "Password can't be")) return "\xc3\xdc\xc2\xeb\xb2\xbb\xc4\xdc\xce\xaa";
  if (!strcmp(text, "empty.")) return "\xbf\xd5\xa1\xa3";
  if (!strcmp(text, "Open Run-Mat?")) return "\xb4\xf2\xbf\xaa Run-Mat\xa3\xbf";
  if (!strcmp(text, "Events starting today")) return "\xbd\xf1\xc8\xd5\xbf\xaa\xca\xbc\xb5\xc4\xca\xc2\xbc\xfe";
  if (!strcmp(text, "Sunday")) return "\xd0\xc7\xc6\xda\xc8\xd5";
  if (!strcmp(text, "Monday")) return "\xd0\xc7\xc6\xda\xd2\xbb";
  if (!strcmp(text, "Tuesday")) return "\xd0\xc7\xc6\xda\xb6\xfe";
  if (!strcmp(text, "Wednesday")) return "\xd0\xc7\xc6\xda\xc8\xfd";
  if (!strcmp(text, "Thursday")) return "\xd0\xc7\xc6\xda\xcb\xc4";
  if (!strcmp(text, "Friday")) return "\xd0\xc7\xc6\xda\xce\xe5";
  if (!strcmp(text, "Saturday")) return "\xd0\xc7\xc6\xda\xc1\xf9";
  if (!strcmp(text, "January")) return "\xd2\xbb\xd4\xc2";
  if (!strcmp(text, "February")) return "\xb6\xfe\xd4\xc2";
  if (!strcmp(text, "March")) return "\xc8\xfd\xd4\xc2";
  if (!strcmp(text, "April")) return "\xcb\xc4\xd4\xc2";
  if (!strcmp(text, "May")) return "\xce\xe5\xd4\xc2";
  if (!strcmp(text, "June")) return "\xc1\xf9\xd4\xc2";
  if (!strcmp(text, "July")) return "\xc6\xdf\xd4\xc2";
  if (!strcmp(text, "August")) return "\xb0\xcb\xd4\xc2";
  if (!strcmp(text, "September")) return "\xbe\xc5\xd4\xc2";
  if (!strcmp(text, "October")) return "\xca\xae\xd4\xc2";
  if (!strcmp(text, "November")) return "\xca\xae\xd2\xbb\xd4\xc2";
  if (!strcmp(text, "December")) return "\xca\xae\xb6\xfe\xd4\xc2";
  return text;
}

const char* localizeText(const char* text) {
  if (!isChineseLanguage()) return text;
  return translateText(text);
}

int isChineseText(const char* text) {
  if (!text) return 0;
  while (*text && (unsigned char)*text < 0x80) text++;
  unsigned char first = (unsigned char)*text;
  // E5/E6/E7/F7/F9 are the add-in's legacy glyph encodings, not GB18030
  // text. Excluding them avoids changing arrows, checkboxes and icons.
  if (first == 0xE5 || first == 0xE6 || first == 0xE7 ||
      first == 0xF7 || first == 0xF9) return 0;
  return first >= 0xB0 && first <= 0xF7;
}
/* copy over the next token from an input string, WITHOUT
skipping leading blanks. The token is terminated by the
first appearance of tokchar, or by the end of the source
string.

The caller must supply sufficient space in token to
receive any token, Otherwise tokens will be truncated.

Returns: a pointer past the terminating tokchar.

This will happily return an infinity of empty tokens if
called with src pointing to the end of a string. Tokens
will never include a copy of tokchar.

A better name would be "strtkn", except that is reserved
for the system namespace. Change to that at your risk.

released to Public Domain, by C.B. Falconer.
Published 2006-02-20. Attribution appreciated.
Modified by gbl08ma not to skip blanks at the beginning,
and to not break on a token if it is the body of a multi-byte char.
*/

const char *toksplit(const char *src, /* Source of tokens */
const char tokchar, /* token delimiting char */
char *token, /* receiver of parsed token */
int lgh) /* length token can receive */
/* not including final '\0' */
{
  int prevWasMBLead = 0;
  if (src) {
    while (*src && (tokchar != *src || prevWasMBLead)) {
      if (lgh) {
        *token++ = *src;
        --lgh;
      }
      if(isMBfirst(*src)) prevWasMBLead = 1;
      else prevWasMBLead = 0;
      src++;
    }
    if (*src && (tokchar == *src)) src++;
  }
  *token = '\0';
  return src;
} /* toksplit */


int strEndsWith(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr)
        return 0;
    //return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
    return strncasecmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

// not really for strings, but anyway:
// based on http://dsss.be/w/c:memmem
// added case-insensitive functionality
void* memmem(char* haystack, int hlen, const char* needle, int nlen, int matchCase) {
  if (nlen > hlen) return 0;
  int i,j=0;
  switch(nlen) { // we have a few specialized compares for certain needle sizes
  case 0: // no needle? just give the haystack
    return haystack;
  case 1: // just use memchr for 1-byte needle
    if(matchCase) return memchr(haystack, needle[0], hlen);
    else {
      void* lc = memchr(haystack, tolower(needle[0]), hlen);
      if(lc!=NULL) return lc;
      else return memchr(haystack, toupper(needle[0]), hlen);
    }
  default: // generic compare for any other needle size
    // walk i through the haystack, matching j as long as needle[j] matches haystack[i]
    for (i=0; i<hlen-nlen+1; i++) {
      if (matchCase ? haystack[i]==needle[j] : tolower(haystack[i])==tolower(needle[j])) {
        if (j==nlen-1) { // end of needle and it all matched?  win.
          return haystack+i-j;
        } else { // keep advancing j (and i, implicitly)
          j++;
        }
      } else { // no match, rewind i the length of the failed match (j), and reset j
        i-=j;
        j=0;
      }
    }
  }
  return NULL;
}

// strncpy_retlen works like strncpy, but returns the length copied to the string.
int strncpy_retlen(char* dest, const char* src, int n) {
  if (dest == NULL || src == NULL || n <= 0) return 0;
  int copied = 0;
  while (copied < n && src[copied]) {
    unsigned char current = (unsigned char)src[copied];
    /* Legacy menu glyphs and GBK Chinese both occupy two bytes.  Never copy
     * the lead byte alone, otherwise PrintXY reads beyond the menu buffer. */
    int multibyte = isMBfirst(src[copied]) ||
                    (isChineseLanguage() && current >= 0x81 && current <= 0xFE);
    int bytes = multibyte ? 2 : 1;
    if (copied + bytes > n || (bytes == 2 && src[copied + 1] == 0)) break;
    dest[copied] = src[copied];
    if (bytes == 2) dest[copied + 1] = src[copied + 1];
    copied += bytes;
  }
  for (int i = copied; i < n; i++) dest[i] = 0;
  return copied;
}

// strncpy_replace works like strncpy, but replaces all occurences of char a for char b, unless a
// is the second half of a MB char.
char* strncpy_replace(char* dest, const char* src, int n, char a, char b) {
  int i;
  for (i = 0; i < n; i++) {
    if(src[i]) {
      if(src[i] != a || (i > 0 && isMBfirst(src[i-1])))
        dest[i] = src[i];
      else dest[i] = b;
    }
    else break;
  }
  for (; i < n; i++) dest[i] = 0;
  return dest;
}


// convert a normal text string into a multibyte one where letters become their mini variants
// (F5 screen of the OS's character select dialog)
// dest must be at least double the size of orig.
void stringToMini(char* dest, const char* orig) {
  int len = strlen(orig);
  int dlen = 0;
  for (int i = 0; i < len; i++) {
    if((orig[i] >= 65 && orig[i] <= 90) || (orig[i] >= 97 && orig[i] <= 122)) { // A-Z a-z
      dest[dlen++] = '\xe7';
      dest[dlen++] = orig[i];
    } else if((orig[i] >= 48 && orig[i] <= 57)) { // 0-9
      dest[dlen++] = '\xe5';
      dest[dlen++] = orig[i]-48+208;
    } else if(orig[i] == '+') {
      dest[dlen++] = '\xe5';
      dest[dlen++] = '\xdb';
    } else dest[dlen++] = orig[i];
  }
  dest[dlen] = '\0';
}

int base32_decode(const unsigned char *encoded, unsigned char *result, int size) {
  // assumes encoded value is perfect (no whitespace, invalid chars, etc.)
  // further assumes all letters are upper-case
  int bytes_buffer = 0, bits_count = 0, i = 0;
  for(; i < size && *encoded; encoded++) {
    unsigned char cur_bits;
    if(isupper(*encoded)) {
      cur_bits = *encoded - 'A';
    } else if(*encoded > '1' && *encoded < '8') {
      cur_bits = *encoded - '2' + 26;
    } else return -1;
    // add new bits to end of queue:
    bytes_buffer <<= 5;
    bytes_buffer |= cur_bits;
    bits_count += 5;
    if(bits_count >= 8) {
      // we have enough to fill another byte of result
      bits_count -= 8;
      result[i++] = bytes_buffer >> bits_count;
    }
  }
  if (i < size) {
    result[i] = 0;
  }
  return i;
}

int base32_validate(const char* string) {
  // returns 1 if the provided string is valid base32
  // returns 0 if not
  for(; *string; string++) {
    if(!(isupper(*string) || (*string > '1' && *string < '8'))) return 0;
  }
  return 1;
}

void itoa_zeropad(int n, char* buffer, int digits) {
  // pad with zeros:
  char* ptr = buffer;
  int tens = ipow(10,--digits);
  for(; digits; digits--) {
    if(n < tens) {
      *ptr = '0';
      ptr++;
    }
    tens /= 10;
  }
  itoa(n, (unsigned char*)ptr);
}
