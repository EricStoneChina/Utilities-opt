#include <stddef.h>

#include "diagnosticsProvider.hpp"

static volatile DiagnosticsEvent lastEvent = {0, 0, 0};
static volatile DiagnosticsEvent eventHistory[DIAGNOSTICS_EVENT_HISTORY_SIZE];
static volatile unsigned char eventHistoryHead = 0;
static volatile unsigned char eventHistoryCount = 0;
static volatile int lastErrorCode = 0;
static volatile int lastErrorDetail = 0;
static volatile unsigned long nextSequence = 1;

void diagnosticsRecordEvent(int type, int detail) {
  unsigned long sequence = nextSequence++;
  lastEvent.type = (unsigned short)type;
  lastEvent.detail = (short)detail;
  lastEvent.sequence = sequence;
  eventHistory[eventHistoryHead].type = (unsigned short)type;
  eventHistory[eventHistoryHead].detail = (short)detail;
  eventHistory[eventHistoryHead].sequence = sequence;
  eventHistoryHead = (unsigned char)
      ((eventHistoryHead + 1) % DIAGNOSTICS_EVENT_HISTORY_SIZE);
  if (eventHistoryCount < DIAGNOSTICS_EVENT_HISTORY_SIZE) {
    eventHistoryCount++;
  }
}

void diagnosticsRecordError(int errorCode, int detail) {
  lastErrorCode = errorCode;
  lastErrorDetail = detail;
  diagnosticsRecordEvent(DIAGNOSTIC_EVENT_RUNMAT_ERROR, errorCode);
}

int diagnosticsGetLastEvent(DiagnosticsEvent* event) {
  if (event == NULL || lastEvent.sequence == 0) return 0;
  event->sequence = lastEvent.sequence;
  event->type = lastEvent.type;
  event->detail = lastEvent.detail;
  return 1;
}

int diagnosticsGetRecentEvent(int offset, DiagnosticsEvent* event) {
  int index;
  if (event == NULL || offset < 0 || offset >= eventHistoryCount) return 0;
  index = eventHistoryHead - 1 - offset;
  while (index < 0) index += DIAGNOSTICS_EVENT_HISTORY_SIZE;
  event->sequence = eventHistory[index].sequence;
  event->type = eventHistory[index].type;
  event->detail = eventHistory[index].detail;
  return 1;
}

int diagnosticsGetLastError(int* errorCode, int* detail) {
  if (errorCode == NULL || detail == NULL || lastErrorCode == 0) return 0;
  *errorCode = lastErrorCode;
  *detail = lastErrorDetail;
  return 1;
}

void diagnosticsClear() {
  lastEvent.sequence = 0;
  lastEvent.type = 0;
  lastEvent.detail = 0;
  eventHistoryHead = 0;
  eventHistoryCount = 0;
  lastErrorCode = 0;
  lastErrorDetail = 0;
}

const char* diagnosticsEventName(int type) {
  switch (type) {
    case DIAGNOSTIC_EVENT_RUNMAT_ENTER: return "Run-Mat entered";
    case DIAGNOSTIC_EVENT_RUNMAT_RETURN: return "Run-Mat returned";
    case DIAGNOSTIC_EVENT_RUNMAT_ERROR: return "Run-Mat error";
    case DIAGNOSTIC_EVENT_TIMER_START: return "Timer started";
    case DIAGNOSTIC_EVENT_TIMER_STOP: return "Timer stopped";
    case DIAGNOSTIC_EVENT_KEY_MATRIX: return "Key matrix sampled";
    case DIAGNOSTIC_EVENT_STORAGE_GUARD: return "Storage guard";
    default: return "Unknown event";
  }
}
