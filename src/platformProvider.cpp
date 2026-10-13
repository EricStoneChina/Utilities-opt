#include <fxcg/keyboard.h>
#include <fxcg/display.h>
#include <fxcg/system.h>
#include <string.h>

#include "hardwareProvider.hpp"
#include "platformProvider.hpp"

static PlatformInfo platformInfo;
static int platformInfoValid = 0;

static unsigned short decodeVersionBCD(unsigned short value) {
  unsigned short result = 0;
  unsigned short place = 1;
  for (int digit = 0; digit < 4; digit++) {
    if ((value & 15) > 9) return 0;
    result += (value & 15) * place;
    value >>= 4;
    place *= 10;
  }
  return result;
}

static void refreshPlatformInfo() {
  char major = 0;
  char minor = 0;
  short build = 0;
  short revision = 0;

  GlibGetOSVersionInfo(&major, &minor, &build, &revision);
  platformInfo.model = getHardwareModel();
  platformInfo.isEmulated = getIsEmulated();
  // The OS returns packed decimal digits, including the two build words.
  platformInfo.osMajor = decodeVersionBCD((unsigned char)major);
  platformInfo.osMinor = decodeVersionBCD((unsigned char)minor);
  platformInfo.osBuild = decodeVersionBCD((unsigned short)build);
  platformInfo.osRevision = decodeVersionBCD((unsigned short)revision);

  /* Do not assume that a syscall is safe on an unrecognised machine.
   * The public Prizm ABI is shared by several models, but fixed-address
   * probes and storage calls must remain behind a recognised model gate. */
  platformInfo.capabilities = 0;
  if (platformInfo.model != 0) {
    platformInfo.capabilities = PLATFORM_CAP_TIMER |
                                PLATFORM_CAP_BFILE |
                                PLATFORM_CAP_MCS |
                                PLATFORM_CAP_BATTERY |
                                PLATFORM_CAP_RTC |
                                PLATFORM_CAP_SCREENSHOT |
                                PLATFORM_CAP_SYSTEM_APPS |
                                PLATFORM_CAP_SYSTEM_LANGUAGE |
                                PLATFORM_CAP_LINK;
    platformInfo.capabilities |= PLATFORM_CAP_KEYBOARD;
    platformInfo.capabilities |= PLATFORM_CAP_RUNMAT;
  }
  if (platformInfo.model == 3) {
    platformInfo.capabilities |= PLATFORM_CAP_CG50;
  }
  platformInfoValid = 1;
}

void getPlatformInfo(PlatformInfo* info) {
  if (!platformInfoValid) refreshPlatformInfo();
  if (info != NULL) *info = platformInfo;
}

unsigned int getPlatformCapabilities() {
  if (!platformInfoValid) refreshPlatformInfo();
  return platformInfo.capabilities;
}

int platformHasCapability(unsigned int capability) {
  return (getPlatformCapabilities() & capability) == capability;
}

const char* platformModelName(int model) {
  switch (model) {
    case 1: return "fx-CG 10";
    case 2: return "fx-CG 20";
    case 3: return "fx-CG 50";
    default: return "Unknown";
  }
}

int platformGetDeviceID(char buffer[9]) {
  PlatformInfo info;
  if (buffer == NULL) return PLATFORM_ERR_INVALID_ARG;
  buffer[0] = 0;
  getPlatformInfo(&info);
  if (info.model == 0) return PLATFORM_ERR_UNSUPPORTED;
  getHardwareID(buffer);
  return PLATFORM_OK;
}

int platformGetSystemLanguageCode(char buffer[8]) {
  char localizedLanguage[96];
  if (buffer == NULL) return PLATFORM_ERR_INVALID_ARG;
  strcpy(buffer, "N/A");
  if (!platformHasCapability(PLATFORM_CAP_SYSTEM_LANGUAGE)) {
    return PLATFORM_ERR_UNSUPPORTED;
  }

  /* There is no stable public language getter in the Prizm ABI.  Message 1
   * is the native language name on the supported CG50 OS builds.  Convert
   * only the known names to ASCII so an unexpected locale never becomes
   * corrupt mini-font output on the dashboard. */
  memset(localizedLanguage, 0, sizeof(localizedLanguage));
  if (LocalizeMessage1(1, localizedLanguage) < 0 ||
      localizedLanguage[0] == 0) {
    return PLATFORM_ERR_UNSUPPORTED;
  }
  localizedLanguage[sizeof(localizedLanguage) - 1] = 0;
  if (!strcmp(localizedLanguage, "English")) {
    strcpy(buffer, "EN");
  } else if ((unsigned char)localizedLanguage[0] == 0xD6 &&
             (unsigned char)localizedLanguage[1] == 0xD0 &&
             (unsigned char)localizedLanguage[2] == 0xCE &&
             (unsigned char)localizedLanguage[3] == 0xC4) {
    strcpy(buffer, "CN");
  } else {
    strcpy(buffer, "??");
  }
  return PLATFORM_OK;
}

int platformReadKeyMatrix(unsigned short words[8]) {
  if (words == NULL) return PLATFORM_ERR_INVALID_ARG;
  if (!platformHasCapability(PLATFORM_CAP_KEYBOARD)) {
    return PLATFORM_ERR_UNSUPPORTED;
  }

  const volatile unsigned short* keyboardRegister =
    (const volatile unsigned short*)0xA44B0000;
  for (int i = 0; i < 8; i++) words[i] = keyboardRegister[i];
  return PLATFORM_OK;
}

int platformIsKeyPressed(int basicKeyCode) {
  int row = basicKeyCode % 10;
  int col = basicKeyCode / 10 - 1;
  if (basicKeyCode < 0 || row < 0 || row >= 16 || col < 0 || col >= 8) {
    return 0;
  }

  unsigned short matrix[8];
  if (platformReadKeyMatrix(matrix) != PLATFORM_OK) return 0;

  int word = row >> 1;
  int bit = col + 8 * (row & 1);
  return (matrix[word] & (1u << bit)) != 0;
}

int platformGetLegacyPasswordUsage(int* used, int* capacity) {
  if (used == NULL || capacity == NULL) return PLATFORM_ERR_INVALID_ARG;
  *used = 0;
  *capacity = 0;
  PlatformInfo info;
  getPlatformInfo(&info);
  if (info.model != 1 && info.model != 2) {
    return PLATFORM_ERR_UNSUPPORTED;
  }

  volatile unsigned char* flagPointer = (volatile unsigned char*)0x80BE0000;
  int remaining = PLATFORM_LEGACY_PASSWORD_CAPACITY;
  while (remaining > 0 && *flagPointer == 0x0F) {
    flagPointer += 0x40;
    *used += 0x40;
    remaining -= 0x40;
  }
  *capacity = PLATFORM_LEGACY_PASSWORD_CAPACITY;
  return PLATFORM_OK;
}

static int getFunctionKeyColorAddress(volatile unsigned char** address) {
  PlatformInfo info;
  if (address == NULL) return PLATFORM_ERR_INVALID_ARG;
  getPlatformInfo(&info);
  if (info.model == 0) return PLATFORM_ERR_UNSUPPORTED;
  *address = (volatile unsigned char*)0xFD8013E4;
  if (info.osMajor > 2 || (info.osMajor == 2 && info.osMinor >= 2)) {
    *address = (volatile unsigned char*)0xFD8016AC;
  }
  return PLATFORM_OK;
}

int platformGetFunctionKeyColor(unsigned char* color) {
  volatile unsigned char* address;
  if (color == NULL) return PLATFORM_ERR_INVALID_ARG;
  if (getFunctionKeyColorAddress(&address) != PLATFORM_OK) {
    return PLATFORM_ERR_UNSUPPORTED;
  }
  *color = *address;
  return PLATFORM_OK;
}

int platformSetFunctionKeyColor(unsigned char color) {
  volatile unsigned char* address;
  if (getFunctionKeyColorAddress(&address) != PLATFORM_OK) {
    return PLATFORM_ERR_UNSUPPORTED;
  }
  *address = color;
  return PLATFORM_OK;
}
