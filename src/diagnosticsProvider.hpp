#ifndef __DIAGNOSTICSPROVIDER_H
#define __DIAGNOSTICSPROVIDER_H

#define DIAGNOSTIC_EVENT_RUNMAT_ENTER       1
#define DIAGNOSTIC_EVENT_RUNMAT_RETURN      2
#define DIAGNOSTIC_EVENT_RUNMAT_ERROR       3
#define DIAGNOSTIC_EVENT_TIMER_START        4
#define DIAGNOSTIC_EVENT_TIMER_STOP         5
#define DIAGNOSTIC_EVENT_KEY_MATRIX         6
#define DIAGNOSTIC_EVENT_STORAGE_GUARD      7
#define DIAGNOSTICS_EVENT_HISTORY_SIZE      8

typedef struct {
  unsigned long sequence;
  unsigned short type;
  short detail;
} DiagnosticsEvent;

void diagnosticsRecordEvent(int type, int detail);
void diagnosticsRecordError(int errorCode, int detail);
int diagnosticsGetLastEvent(DiagnosticsEvent* event);
int diagnosticsGetRecentEvent(int offset, DiagnosticsEvent* event);
int diagnosticsGetLastError(int* errorCode, int* detail);
void diagnosticsClear();
const char* diagnosticsEventName(int type);

#endif
