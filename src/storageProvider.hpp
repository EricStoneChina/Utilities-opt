#ifndef __STORAGEPROVIDER_H
#define __STORAGEPROVIDER_H

typedef struct {
  int active;
} StorageGuard;

int storageOperationBegin(StorageGuard* guard);
void storageOperationEnd(StorageGuard* guard);
void storageOperationRecover();
int storageOperationIsActive();
int storageWriteFile(int handle, const void* buffer, int size);

// Scope helper for low-level providers.  It makes early returns safe while
// preserving the existing nested StorageGuard API for GUI code.
class StorageScope {
  StorageGuard guard;
public:
  StorageScope() {
    guard.active = 0;
    storageOperationBegin(&guard);
  }
  ~StorageScope() {
    storageOperationEnd(&guard);
  }
};

#endif
