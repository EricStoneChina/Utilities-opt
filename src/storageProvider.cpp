#include <fxcg/file.h>
#include <limits.h>
#include "chronoGUI.hpp"
#include "diagnosticsProvider.hpp"
#include "storageProvider.hpp"

static int storageGuardDepth = 0;
static int restartStubTimer = 0;

int storageOperationBegin(StorageGuard* guard) {
  if (guard == NULL || guard->active) return -1;
  guard->active = 1;
  if (storageGuardDepth == 0) {
    restartStubTimer = isStubTimerRunning();
    if (restartStubTimer) stopAndUninstallStubTimer();
    diagnosticsRecordEvent(DIAGNOSTIC_EVENT_STORAGE_GUARD, 1);
  }
  storageGuardDepth++;
  return 0;
}

void storageOperationEnd(StorageGuard* guard) {
  if (guard == NULL || !guard->active) return;
  guard->active = 0;
  if (storageGuardDepth > 0) storageGuardDepth--;
  if (storageGuardDepth == 0) {
    if (restartStubTimer) restartStubTimerFunction();
    restartStubTimer = 0;
    diagnosticsRecordEvent(DIAGNOSTIC_EVENT_STORAGE_GUARD, 0);
  }
}

void storageOperationRecover() {
  if (storageGuardDepth == 0) return;
  storageGuardDepth = 0;
  if (restartStubTimer) restartStubTimerFunction();
  restartStubTimer = 0;
  diagnosticsRecordEvent(DIAGNOSTIC_EVENT_STORAGE_GUARD, -1);
}

int storageOperationIsActive() {
  return storageGuardDepth != 0;
}

int storageWriteFile(int handle, const void* buffer, int size) {
  if (handle < 0 || buffer == NULL || size <= 0) return 0;
  StorageScope storageScope;
  int position = Bfile_TellFile_OS(handle);
  if (position < 0 || size > INT_MAX - position) return 0;
  // OS revisions return either a byte count or a position; check actual progress.
  if (Bfile_WriteFile_OS(handle, buffer, size) < 0) return 0;
  return Bfile_TellFile_OS(handle) == position + size;
}
