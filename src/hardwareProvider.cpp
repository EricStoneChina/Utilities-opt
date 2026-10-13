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

#include "hardwareProvider.hpp"
#include "settingsProvider.hpp"

// START OF POWER MANAGEMENT CODE
#define LCDC *(unsigned int*)(0xB4000000)
int getRawBacklightSubLevel()
{
  Bdisp_DDRegisterSelect(0x5a1);
  return (LCDC & 0xFF) - 6;
}
void setRawBacklightSubLevel(int level)
{
  Bdisp_DDRegisterSelect(0x5a1);
  LCDC = (level & 0xFF) + 6;
}
// END OF POWER MANAGEMENT CODE 

void getHardwareID(char* buffer) {
  // Gets the calculator's unique ID (as told by Simon Lothar): 8 bytes starting at 0x8001FFD0
  // NOTE buffer must be at least 9 bytes long!
  for(int i = 0; i < 8; i++) buffer[i] = *((char*)0x8001FFD0+i);
  buffer[8] = 0;
}

int getHardwareModel() {
  /* Returns calculator model. There is a syscall for this, but with this we know exactly what we
   * are checking. Not to be confused with the "PCB model", which on the Prizm series appears to be
   * always 755D for CG 20 or 755A for CG 10.
   * Returns 0 on unknown/inconclusive model, 1 on fx-CG 10, 2 on fx-CG 20,
   * and 3 on fx-CG 50 (including the AU hardware variant).
   */
  const volatile unsigned char* byte1 = (const volatile unsigned char*)0x80000303;
  const volatile unsigned char* byte2 = (const volatile unsigned char*)0x80000305;
  const unsigned char modelByte1 = *byte1;
  const unsigned char modelByte2 = *byte2;
  if((modelByte1 == 0x41) && (modelByte2 == 0x5A)) return 1;
  if((modelByte1 == 0x44) && (modelByte2 == 0xAA)) return 2;
  if((modelByte1 == 0x41) && (modelByte2 == 0xAA)) return 3;
  return 0;
}

int getIsEmulated() {
  // CG50 emulators can reproduce the power register; it cannot prove hardware.
  int model = getHardwareModel();
  if (model == 3 || model == 0) return -1;
  int k = *(unsigned char*)P11DR;
  return !k;
}

void formatBatteryVoltage(char* buffer) {
  if (buffer == NULL) return;
  int centivolts = GetMainBatteryVoltage(1);
  if (centivolts < 0) {
    strcpy(buffer, "n/a");
    return;
  }
  sprintf(buffer, "%d.%02d V", centivolts / 100, centivolts % 100);
}

void setBrightnessToStartupSetting() {
  if (getSetting(SETTING_STARTUP_BRIGHTNESS) <= 249) {
    setRawBacklightSubLevel(getSetting(SETTING_STARTUP_BRIGHTNESS));
  }
}

// Legacy CPU clocking code (used only from PicoC scripts).
//
// The register sequence below belongs to the original fx-CG10/20 Prizm
// clocking interface.  The fx-CG50 has a different native clock and memory
// timing profile; never expose this sequence on CG50, emulators, or unknown
// hardware.  Keep the symbol for source compatibility with old scripts, but
// make unsupported calls a safe no-op.

void CPU_change_freq(int mult) { 
   const int model = getHardwareModel();
   if (model != 1 && model != 2) return;
   __asm__( 
      "mov r4, r0\n\t" 
      "and #0x3F, r0\n\t"  
      "shll16 r0\n\t"  
      "shll8 r0\n\t"  
      "mov.l frqcr, r1\n\t"   
      "mov.l pll_mask, r3\n\t"   
      "mov.l @r1, r2\n\t"   
      "and r3, r2\n\t"   
      "or r0, r2\n\t"  
      "mov.l r2, @r1\n\t"  
      "mov.l frqcr_kick_bit, r0\n\t"  
      "mov.l @r1, r2\n\t" 
      "or r0, r2\n\t" 
      "rts\n\t" 
      "mov.l r2, @r1\n\t" 
      ".align 4\n\t" 
      "frqcr_kick_bit: .long 0x80000000\n\t" 
      "pll_mask: .long 0xC0FFFFFF\n\t"  
      "frqcr: .long 0xA4150000\n\t" 
   ); 
}
