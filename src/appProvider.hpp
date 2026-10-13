#ifndef __APPPROVIDER_H
#define __APPPROVIDER_H

#include "fileProvider.hpp"

#define APP_MAX_G3A_RECORDS 64
#define APP_MAX_G3A_BACKUPS 32
#define G3A_ICON_WIDTH 92
#define G3A_ICON_HEIGHT 64
#define G3A_ICON_BYTES (G3A_ICON_WIDTH * G3A_ICON_HEIGHT * 2)

#define APP_STATUS_OK              0
#define APP_STATUS_BAD_HEADER      1
#define APP_STATUS_BAD_CHECKSUM    2
#define APP_STATUS_UNREADABLE      3

#define APP_MANAGE_OK              0
#define APP_MANAGE_INVALID        -1
#define APP_MANAGE_BACKUP_EXISTS  -2
#define APP_MANAGE_COPY_FAILED    -3
#define APP_MANAGE_DELETE_FAILED  -4
#define APP_MANAGE_TARGET_EXISTS  -5

typedef struct {
  char filename[MAX_FILENAME_SIZE];
  char displayName[25];
  char internalName[12];
  char version[17];
  char timestamp[16];
  unsigned long fileSize;
  unsigned long codeOffset;
  unsigned long checksumStored;
  unsigned long checksumCalculated;
  int active;
  int iconPresent;
  int duplicate;
  int status;
} G3ARecord;

typedef struct {
  char backupFilename[MAX_FILENAME_SIZE];
  unsigned long fileSize;
} G3ABackupRecord;

int scanG3ARecords(G3ARecord records[], int maximum, int* truncated);
const char* g3aStatusName(int status);
int readG3AIcon(const G3ARecord* record, unsigned short pixels[],
                int pixelCount);
int quarantineG3ARecord(const G3ARecord* record);
int scanG3ABackups(G3ABackupRecord records[], int maximum, int* truncated);
int g3aBackupOriginalFilename(const G3ABackupRecord* record,
                              char* filename, int filenameSize);
int restoreG3ABackup(const G3ABackupRecord* record);
int quarantineG3AByFilename(const char* filename);

#endif
