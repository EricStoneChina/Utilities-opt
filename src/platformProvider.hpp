#ifndef __PLATFORMPROVIDER_H
#define __PLATFORMPROVIDER_H

#include <fxcg/keyboard.h>

/*
 * Hardware and syscall capability information used by the CG50 control
 * centre.  The SDK does not expose a safe runtime syscall probe, so the
 * capability mask describes the public Prizm interface that this build is
 * linked against and narrows the result when the hardware model is unknown.
 */
#define PLATFORM_CAP_TIMER       0x00000001u
#define PLATFORM_CAP_KEYBOARD    0x00000002u
#define PLATFORM_CAP_BFILE       0x00000004u
#define PLATFORM_CAP_MCS         0x00000008u
#define PLATFORM_CAP_BATTERY     0x00000010u
#define PLATFORM_CAP_RTC         0x00000020u
#define PLATFORM_CAP_SCREENSHOT  0x00000040u
#define PLATFORM_CAP_SYSTEM_APPS 0x00000080u
#define PLATFORM_CAP_RUNMAT      0x00000100u
#define PLATFORM_CAP_CG50        0x00000200u
#define PLATFORM_CAP_SYSTEM_LANGUAGE 0x00000400u
#define PLATFORM_CAP_LINK         0x00000800u
#define PLATFORM_LEGACY_PASSWORD_CAPACITY 0x1FFFF

#define PLATFORM_OK                 0
#define PLATFORM_ERR_INVALID_ARG   -1
#define PLATFORM_ERR_UNSUPPORTED   -2

typedef struct {
  int model;
  int isEmulated;
  unsigned char osMajor;
  unsigned char osMinor;
  short osBuild;
  short osRevision;
  unsigned int capabilities;
} PlatformInfo;

void getPlatformInfo(PlatformInfo* info);
unsigned int getPlatformCapabilities();
int platformHasCapability(unsigned int capability);
const char* platformModelName(int model);
/*
 * Return the device identifier only when the platform probe recognizes a
 * supported Prizm model. Unknown hardware must not be handed a legacy
 * fixed-address read by a UI page.
 */
int platformGetDeviceID(char buffer[9]);
// Return a short, render-safe code such as EN or CN.  The native locale
// string itself may be GB18030 and must not be sent to the mini font.
int platformGetSystemLanguageCode(char buffer[8]);

/*
 * The CG50 Run-Mat return path needs a non-blocking matrix read.  Keep the
 * MMIO address and key-matrix mapping in this platform layer so application
 * code does not contain model-specific register accesses.
 */
int platformReadKeyMatrix(unsigned short words[8]);
int platformIsKeyPressed(int basicKeyCode);
int platformGetLegacyPasswordUsage(int* used, int* capacity);
int platformGetFunctionKeyColor(unsigned char* color);
int platformSetFunctionKeyColor(unsigned char color);

#endif
