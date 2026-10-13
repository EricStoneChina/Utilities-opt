#ifndef __LOCKGUI_H
#define __LOCKGUI_H

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
int passwordInput(int x, int y, unsigned char* buffer);
int setPassword();
int unlockCalc();
int lockCalc();
void lockApp();
extern jmp_buf utilities_return;

#define RUNMAT_RESULT_NONE         0
#define RUNMAT_RESULT_SHIFT_EXIT   1
#define RUNMAT_RESULT_APP_RETURN   2
#define RUNMAT_RESULT_EXIT         3

#define RUNMAT_ERROR_NO_CAPABILITY -1
#define RUNMAT_ERROR_TIMER_INSTALL -2

typedef struct {
  int active;
  int timerId;
  unsigned int launchCount;
  unsigned int returnCount;
  int lastResult;
  int lastError;
} RunMatStatus;

void openRunMat();
void returnToUtilitiesHandler();
void finalizeRunMatReturn();
void getRunMatStatus(RunMatStatus* status);
#endif
