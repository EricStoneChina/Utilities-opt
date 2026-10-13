#include <fxcg/file.h>
#include <string.h>
#include <stdlib.h>

#include "appProvider.hpp"
#include "constantsProvider.hpp"
#include "diagnosticsProvider.hpp"
#include "storageProvider.hpp"

static const unsigned char g3aMagic[14] = {
  0xAA, 0xAC, 0xBD, 0xAF, 0x90, 0x88, 0x9A,
  0x8D, 0xD3, 0xFF, 0xFE, 0xFF, 0xFE, 0xFF
};

enum { STORAGE_COPY_BUFFER_SIZE = 4096 };

static unsigned long readBigEndian32(const unsigned char value[4]) {
  return ((unsigned long)value[0] << 24) |
         ((unsigned long)value[1] << 16) |
         ((unsigned long)value[2] << 8) |
         (unsigned long)value[3];
}

static int readAt(int handle, int offset, void* buffer, int size) {
  StorageScope storageScope;
  if (handle < 0 || buffer == NULL || offset < 0 || size <= 0) return 0;

  // Bfile_ReadFile_OS supports an absolute read position.  Using it here is
  // important: the G3A parser performs several independent header reads, and
  // relying on a prior Seek/current-pointer state made valid add-ins such as
  // the official 3DGraph.g3a appear as "Bad header" on some CG50 OS builds.
  return Bfile_ReadFile_OS(handle, buffer, size, offset) == size;
}

static int writeFileChunk(int handle, const void* buffer, int size,
                          unsigned long position) {
  if (handle < 0 || buffer == NULL || size <= 0) return 0;
  StorageScope storageScope;
  int currentPosition = Bfile_TellFile_OS(handle);
  if (currentPosition < 0 || (unsigned long)currentPosition != position) return 0;
  return storageWriteFile(handle, buffer, size);
}

static int readStringAt(int handle, int offset, char* buffer, int size) {
  if (buffer == NULL || size <= 0) return 0;
  memset(buffer, 0, size);
  if (!readAt(handle, offset, buffer, size - 1)) {
    buffer[0] = 0;
    return 0;
  }
  buffer[size - 1] = 0;
  return 1;
}

static int calculateG3AChecksum(int handle, unsigned long fileSize,
                                unsigned long* checksum) {
  StorageScope storageScope;
  // Keep the streaming buffer out of the small calculator call stack.  G3A
  // files can be large, while the checksum itself only needs a small window.
  static unsigned char buffer[4096];
  unsigned long position = 0;
  unsigned long sum = 0;

  if (checksum == NULL || fileSize < 4) return 0;
  while (position < fileSize) {
    unsigned long remaining = fileSize - position;
    int request = remaining > sizeof(buffer) ? sizeof(buffer) : (int)remaining;
    int received = Bfile_ReadFile_OS(handle, buffer, request, (int)position);
    if (received != request) return 0;
    for (int i = 0; i < received; i++) {
      unsigned long absolute = position + (unsigned long)i;
      if ((absolute >= 0x20 && absolute < 0x24) ||
          absolute >= fileSize - 4) {
        continue;
      }
      sum += buffer[i];
    }
    position += (unsigned long)received;
  }
  *checksum = sum;
  return 1;
}

static void initializeRecord(G3ARecord* record, const char* filename,
                             unsigned long fileSize) {
  memset(record, 0, sizeof(G3ARecord));
  strncpy(record->filename, filename, sizeof(record->filename) - 1);
  record->fileSize = fileSize;
  strncpy(record->displayName, filename, sizeof(record->displayName) - 1);
  record->status = APP_STATUS_UNREADABLE;
}

static int parseG3ARecord(const char* filename, unsigned long fileSize,
                          G3ARecord* record) {
  StorageScope storageScope;
  unsigned short path[MAX_FILENAME_SIZE + 1];
  char fullPath[MAX_FILENAME_SIZE + 1];
  unsigned char flags[2];
  unsigned char storedSizeBytes[4];
  unsigned char codeOffsetBytes[4];
  unsigned char storedChecksumBytes[4];
  unsigned char magic[14];
  int handle;

  if (filename == NULL || record == NULL) return 0;
  initializeRecord(record, filename, fileSize);
  strcpy(fullPath, SMEM_PREFIX);
  strncat(fullPath, filename, sizeof(fullPath) - strlen(fullPath) - 1);
  Bfile_StrToName_ncpy(path, fullPath, MAX_FILENAME_SIZE + 1);
  handle = Bfile_OpenFile_OS(path, READ, 0);
  if (handle < 0) {
    return 0;
  }

  // Directory enumeration and the open-file handle are separate OS paths.
  // Use the handle's size for all header checks so a stale/truncated
  // file_type_t entry cannot make a valid G3A look malformed.
  int actualFileSize = Bfile_GetFileSize_OS(handle);
  if (actualFileSize < 0) {
    Bfile_CloseFile_OS(handle);
    return 0;
  }
  fileSize = (unsigned long)actualFileSize;
  record->fileSize = fileSize;
  if (fileSize < 0x7004) {
    if (handle >= 0) Bfile_CloseFile_OS(handle);
    return 0;
  }

  if (!readAt(handle, 0x00, magic, sizeof(magic)) ||
      !readAt(handle, 0x24, flags, sizeof(flags)) ||
      !readAt(handle, 0x2E, codeOffsetBytes, sizeof(codeOffsetBytes)) ||
      !readAt(handle, 0x5C, storedSizeBytes, sizeof(storedSizeBytes)) ||
      !readAt(handle, 0x20, storedChecksumBytes, sizeof(storedChecksumBytes)) ||
      !readStringAt(handle, 0x006B, record->displayName,
                    sizeof(record->displayName)) ||
      !readStringAt(handle, 0x0060, record->internalName,
                    sizeof(record->internalName)) ||
      !readStringAt(handle, 0x0130, record->version, sizeof(record->version)) ||
      !readStringAt(handle, 0x013C, record->timestamp,
                    sizeof(record->timestamp))) {
    Bfile_CloseFile_OS(handle);
    return 0;
  }

  record->active = flags[0] == 1 && flags[1] == 1;
  record->codeOffset = readBigEndian32(codeOffsetBytes);
  record->checksumStored = readBigEndian32(storedChecksumBytes);
  record->iconPresent = fileSize >= 0x6E00;
  int codeOffsetValid = record->codeOffset <= fileSize - 0x7004UL;
  if (readBigEndian32(storedSizeBytes) != fileSize ||
      record->codeOffset < 4 ||
      !codeOffsetValid ||
      memcmp(magic, g3aMagic, sizeof(g3aMagic)) != 0 ||
      flags[0] != 1 || flags[1] != 1 ||
      record->displayName[0] == 0 || record->version[0] == 0) {
    record->status = APP_STATUS_BAD_HEADER;
    Bfile_CloseFile_OS(handle);
    return 1;
  }

  if (!calculateG3AChecksum(handle, fileSize, &record->checksumCalculated)) {
    record->status = APP_STATUS_UNREADABLE;
  } else if (record->checksumCalculated != record->checksumStored) {
    record->status = APP_STATUS_BAD_CHECKSUM;
  } else {
    record->status = APP_STATUS_OK;
  }
  Bfile_CloseFile_OS(handle);
  return 1;
}

int scanG3ARecords(G3ARecord records[], int maximum, int* truncated) {
  StorageScope storageScope;
  unsigned short path[MAX_FILENAME_SIZE + 1];
  unsigned short found[MAX_FILENAME_SIZE + 1];
  char pattern[MAX_FILENAME_SIZE + 1];
  char filename[MAX_FILENAME_SIZE + 1];
  file_type_t fileInfo;
  int findHandle;
  int result;
  int count = 0;

  if (truncated != NULL) *truncated = 0;
  if (records == NULL || maximum <= 0) return -1;
  strcpy(pattern, SMEM_PREFIX "*.g3a");
  Bfile_StrToName_ncpy(path, pattern, MAX_FILENAME_SIZE + 1);
  findHandle = -1;
  result = Bfile_FindFirst_NON_SMEM((const char*)path, &findHandle,
                                    (char*)found, &fileInfo);
  while (!result) {
    Bfile_NameToStr_ncpy(filename, found, MAX_FILENAME_SIZE + 1);
    if (fileInfo.fsize) {
      if (count < maximum) {
        parseG3ARecord(filename, fileInfo.fsize, &records[count]);
        count++;
      } else if (truncated != NULL) {
        *truncated = 1;
      }
    }
    result = Bfile_FindNext_NON_SMEM(findHandle, (char*)found,
                                     (char*)&fileInfo);
  }
  if (findHandle >= 0) Bfile_FindClose(findHandle);

  for (int i = 0; i < count; i++) {
    for (int j = i + 1; j < count; j++) {
      const char* left = records[i].internalName[0] ?
                         records[i].internalName : records[i].displayName;
      const char* right = records[j].internalName[0] ?
                          records[j].internalName : records[j].displayName;
      if (!strcmp(left, right)) {
        records[i].duplicate = 1;
        records[j].duplicate = 1;
      }
    }
  }
  return count;
}

int readG3AIcon(const G3ARecord* record, unsigned short pixels[],
               int pixelCount) {
  StorageScope storageScope;
  unsigned short path[MAX_FILENAME_SIZE + 1];
  char fullPath[MAX_FILENAME_SIZE + 1];
  unsigned char* raw = NULL;
  int handle = -1;
  int readResult;

  if (record == NULL || pixels == NULL ||
      pixelCount < G3A_ICON_WIDTH * G3A_ICON_HEIGHT ||
      !record->iconPresent) {
    return 0;
  }
  strcpy(fullPath, SMEM_PREFIX);
  strncat(fullPath, record->filename,
          sizeof(fullPath) - strlen(fullPath) - 1);
  Bfile_StrToName_ncpy(path, fullPath, MAX_FILENAME_SIZE + 1);
  handle = Bfile_OpenFile_OS(path, READ, 0);
  if (handle < 0) return 0;
  if (Bfile_GetFileSize_OS(handle) < 0x1000 + G3A_ICON_BYTES) {
    Bfile_CloseFile_OS(handle);
    return 0;
  }

  // G3A stores icons as big-endian RGB565.  Convert through byte-addressable
  // RAM; reading directly into an SH unsigned-short array swaps each pixel.
  raw = (unsigned char*)malloc(G3A_ICON_BYTES);
  if (raw == NULL) {
    Bfile_CloseFile_OS(handle);
    return 0;
  }
  readResult = Bfile_ReadFile_OS(handle, raw, G3A_ICON_BYTES, 0x1000);
  Bfile_CloseFile_OS(handle);
  if (readResult != G3A_ICON_BYTES) {
    free(raw);
    return 0;
  }
  for (int i = 0; i < G3A_ICON_WIDTH * G3A_ICON_HEIGHT; i++) {
    pixels[i] = (unsigned short)(((unsigned short)raw[i * 2] << 8) |
                                 (unsigned short)raw[i * 2 + 1]);
  }
  free(raw);
  return 1;
}

const char* g3aStatusName(int status) {
  switch (status) {
    case APP_STATUS_OK: return "OK";
    case APP_STATUS_BAD_HEADER: return "Bad header";
    case APP_STATUS_BAD_CHECKSUM: return "Checksum mismatch";
    default: return "Unreadable";
  }
}

static int buildG3ABackupPath(const char* filename, char* path, int pathSize) {
  const char* prefix = SMEM_PREFIX "UTILS-APP-";
  const char* extension;
  int filenameLength;
  int required;
  if (filename == NULL || path == NULL || pathSize <= 0) return 0;
  filenameLength = strlen(filename);
  if (filenameLength < 5 || filename[0] == '.') return 0;
  for (const char* cursor = filename; *cursor != 0; cursor++) {
    unsigned char character = (unsigned char)*cursor;
    if (character < 0x20 || *cursor == '\\' || *cursor == '/' ||
        *cursor == ':' || *cursor == '*' || *cursor == '?' ||
        *cursor == '"' || *cursor == '<' || *cursor == '>' ||
        *cursor == '|') return 0;
  }
  extension = strrchr(filename, '.');
  if (extension == NULL || extension == filename ||
      filenameLength - (int)(extension - filename) != 4 ||
      (extension[1] != 'g' && extension[1] != 'G') ||
      (extension[2] != '3') ||
      (extension[3] != 'a' && extension[3] != 'A') ||
      extension[4] != 0) return 0;
  required = strlen(prefix) + strlen(filename) + strlen(".bak") + 1;
  if (required > pathSize) return 0;
  strcpy(path, prefix);
  strcat(path, filename);
  strcat(path, ".bak");
  return 1;
}

/*
 * Copy an add-in to a new, never-overwritten backup before removing it from
 * the root directory. A failed or partial copy is deleted and the source is
 * left untouched.
 */
int quarantineG3ARecord(const G3ARecord* record) {
  StorageScope storageScope;
  unsigned short sourcePath[MAX_FILENAME_SIZE + 1];
  unsigned short backupPath[MAX_FILENAME_SIZE + 1];
  char sourceName[MAX_FILENAME_SIZE + 1];
  char backupName[MAX_FILENAME_SIZE + 1];
  unsigned char* buffer;
  int sourceHandle = -1;
  int backupHandle = -1;
  int backupCreated = 0;
  int result = APP_MANAGE_COPY_FAILED;
  unsigned long total = 0;

  if (record == NULL || record->filename[0] == 0 ||
      record->fileSize < 4) return APP_MANAGE_INVALID;
  if (!buildG3ABackupPath(record->filename, backupName,
                          sizeof(backupName))) {
    return APP_MANAGE_INVALID;
  }

  /*
   * Bfile_WriteFile_OS reads from caller-provided main RAM.  A static buffer
   * is not safe on every CG50 OS build, so keep the copy buffer writable and
   * operation-local on the heap.
   */
  buffer = (unsigned char*)malloc(STORAGE_COPY_BUFFER_SIZE);
  if (buffer == NULL) return APP_MANAGE_COPY_FAILED;

  strcpy(sourceName, SMEM_PREFIX);
  strncat(sourceName, record->filename,
          sizeof(sourceName) - strlen(sourceName) - 1);
  Bfile_StrToName_ncpy(sourcePath, sourceName, MAX_FILENAME_SIZE + 1);
  Bfile_StrToName_ncpy(backupPath, backupName, MAX_FILENAME_SIZE + 1);

  backupHandle = Bfile_OpenFile_OS(backupPath, READ, 0);
  if (backupHandle >= 0) {
    Bfile_CloseFile_OS(backupHandle);
    free(buffer);
    return APP_MANAGE_BACKUP_EXISTS;
  }

  sourceHandle = Bfile_OpenFile_OS(sourcePath, READ, 0);
  if (sourceHandle < 0) {
    free(buffer);
    return APP_MANAGE_COPY_FAILED;
  }
  int sourceSizeValue = Bfile_GetFileSize_OS(sourceHandle);
  if (sourceSizeValue < 0) {
    Bfile_CloseFile_OS(sourceHandle);
    free(buffer);
    return APP_MANAGE_COPY_FAILED;
  }
  unsigned long sourceSize = (unsigned long)sourceSizeValue;
  if (sourceSize != record->fileSize || sourceSize < 4) {
    Bfile_CloseFile_OS(sourceHandle);
    free(buffer);
    return APP_MANAGE_COPY_FAILED;
  }

  size_t backupSize = (size_t)sourceSize;
  if (Bfile_CreateEntry_OS(backupPath, CREATEMODE_FILE, &backupSize) < 0) {
    Bfile_CloseFile_OS(sourceHandle);
    free(buffer);
    return APP_MANAGE_COPY_FAILED;
  }
  backupCreated = 1;
  backupHandle = Bfile_OpenFile_OS(backupPath, READWRITE, 0);
  if (backupHandle < 0) goto copy_failed;
  if (Bfile_SeekFile_OS(backupHandle, 0) < 0) goto copy_failed;

  while (total < sourceSize) {
    int request = (sourceSize - total) > STORAGE_COPY_BUFFER_SIZE ?
                  STORAGE_COPY_BUFFER_SIZE : (int)(sourceSize - total);
    int received = Bfile_ReadFile_OS(sourceHandle, buffer, request, -1);
    if (received != request ||
        !writeFileChunk(backupHandle, buffer, received, total)) {
      goto copy_failed;
    }
    total += (unsigned long)received;
  }
  Bfile_CloseFile_OS(backupHandle);
  backupHandle = -1;
  Bfile_CloseFile_OS(sourceHandle);
  sourceHandle = -1;

  backupHandle = Bfile_OpenFile_OS(backupPath, READ, 0);
  if (backupHandle < 0 ||
      Bfile_GetFileSize_OS(backupHandle) != (int)sourceSize) {
    if (backupHandle >= 0) Bfile_CloseFile_OS(backupHandle);
    goto copy_failed;
  }
  Bfile_CloseFile_OS(backupHandle);
  backupHandle = -1;

  if (Bfile_DeleteEntry(sourcePath) < 0) {
    free(buffer);
    return APP_MANAGE_DELETE_FAILED;
  }
  free(buffer);
  return APP_MANAGE_OK;

copy_failed:
  if (backupHandle >= 0) Bfile_CloseFile_OS(backupHandle);
  if (sourceHandle >= 0) Bfile_CloseFile_OS(sourceHandle);
  if (backupCreated) Bfile_DeleteEntry(backupPath);
  free(buffer);
  return result;
}

int quarantineG3AByFilename(const char* filename) {
  StorageScope storageScope;
  unsigned short path[MAX_FILENAME_SIZE + 1];
  char backupName[MAX_FILENAME_SIZE + 1];
  char fullName[MAX_FILENAME_SIZE + 1];
  G3ARecord record;
  int handle;
  int fileSize;
  if (filename == NULL || filename[0] == 0 ||
      !buildG3ABackupPath(filename, backupName, sizeof(backupName))) {
    return APP_MANAGE_INVALID;
  }
  strcpy(fullName, SMEM_PREFIX);
  strncat(fullName, filename, sizeof(fullName) - strlen(fullName) - 1);
  Bfile_StrToName_ncpy(path, fullName, MAX_FILENAME_SIZE + 1);
  handle = Bfile_OpenFile_OS(path, READ, 0);
  if (handle < 0) return APP_MANAGE_INVALID;
  fileSize = Bfile_GetFileSize_OS(handle);
  Bfile_CloseFile_OS(handle);
  if (fileSize < 4) return APP_MANAGE_INVALID;
  memset(&record, 0, sizeof(record));
  strncpy(record.filename, filename, sizeof(record.filename) - 1);
  record.fileSize = (unsigned long)fileSize;
  return quarantineG3ARecord(&record);
}

static int copyStorageFileNoOverwrite(const char* sourceName,
                                      const char* destinationName,
                                      unsigned long expectedSize) {
  StorageScope storageScope;
  unsigned short sourcePath[MAX_FILENAME_SIZE + 1];
  unsigned short destinationPath[MAX_FILENAME_SIZE + 1];
  unsigned char* buffer;
  int sourceHandle = -1;
  int destinationHandle = -1;
  int destinationCreated = 0;
  unsigned long total = 0;

  if (sourceName == NULL || destinationName == NULL || expectedSize < 4)
    return APP_MANAGE_INVALID;
  Bfile_StrToName_ncpy(sourcePath, sourceName, MAX_FILENAME_SIZE + 1);
  Bfile_StrToName_ncpy(destinationPath, destinationName,
                       MAX_FILENAME_SIZE + 1);

  destinationHandle = Bfile_OpenFile_OS(destinationPath, READ, 0);
  if (destinationHandle >= 0) {
    Bfile_CloseFile_OS(destinationHandle);
    return APP_MANAGE_TARGET_EXISTS;
  }
  sourceHandle = Bfile_OpenFile_OS(sourcePath, READ, 0);
  if (sourceHandle < 0) return APP_MANAGE_COPY_FAILED;

  int sourceSizeValue = Bfile_GetFileSize_OS(sourceHandle);
  if (sourceSizeValue < 0 || (unsigned long)sourceSizeValue != expectedSize) {
    Bfile_CloseFile_OS(sourceHandle);
    return APP_MANAGE_COPY_FAILED;
  }
  buffer = (unsigned char*)malloc(STORAGE_COPY_BUFFER_SIZE);
  if (buffer == NULL) {
    Bfile_CloseFile_OS(sourceHandle);
    return APP_MANAGE_COPY_FAILED;
  }
  size_t destinationSize = (size_t)expectedSize;
  if (Bfile_CreateEntry_OS(destinationPath, CREATEMODE_FILE,
                            &destinationSize) < 0) {
    Bfile_CloseFile_OS(sourceHandle);
    free(buffer);
    return APP_MANAGE_COPY_FAILED;
  }
  destinationCreated = 1;
  destinationHandle = Bfile_OpenFile_OS(destinationPath, READWRITE, 0);
  if (destinationHandle < 0) goto restore_copy_failed;
  if (Bfile_SeekFile_OS(destinationHandle, 0) < 0)
    goto restore_copy_failed;

  while (total < expectedSize) {
    int request = (expectedSize - total) > STORAGE_COPY_BUFFER_SIZE ?
                  STORAGE_COPY_BUFFER_SIZE : (int)(expectedSize - total);
    int received = Bfile_ReadFile_OS(sourceHandle, buffer, request, -1);
    if (received != request ||
        !writeFileChunk(destinationHandle, buffer, received, total)) {
      goto restore_copy_failed;
    }
    total += (unsigned long)received;
  }
  Bfile_CloseFile_OS(destinationHandle);
  destinationHandle = -1;
  Bfile_CloseFile_OS(sourceHandle);
  sourceHandle = -1;

  destinationHandle = Bfile_OpenFile_OS(destinationPath, READ, 0);
  if (destinationHandle < 0 ||
      Bfile_GetFileSize_OS(destinationHandle) != (int)expectedSize) {
    if (destinationHandle >= 0) Bfile_CloseFile_OS(destinationHandle);
    goto restore_copy_failed;
  }
  Bfile_CloseFile_OS(destinationHandle);
  destinationHandle = -1;
  free(buffer);
  return APP_MANAGE_OK;

restore_copy_failed:
  if (destinationHandle >= 0) Bfile_CloseFile_OS(destinationHandle);
  if (sourceHandle >= 0) Bfile_CloseFile_OS(sourceHandle);
  if (destinationCreated) Bfile_DeleteEntry(destinationPath);
  free(buffer);
  return APP_MANAGE_COPY_FAILED;
}

int scanG3ABackups(G3ABackupRecord records[], int maximum, int* truncated) {
  StorageScope storageScope;
  unsigned short path[MAX_FILENAME_SIZE + 1];
  unsigned short found[MAX_FILENAME_SIZE + 1];
  char pattern[MAX_FILENAME_SIZE + 1];
  char filename[MAX_FILENAME_SIZE + 1];
  const char* prefix = "UTILS-APP-";
  const char* suffix = ".bak";
  file_type_t fileInfo;
  int findHandle = -1;
  int result;
  int count = 0;

  if (truncated != NULL) *truncated = 0;
  if (records == NULL || maximum <= 0) return -1;
  strcpy(pattern, SMEM_PREFIX);
  strcat(pattern, prefix);
  strcat(pattern, "*.bak");
  Bfile_StrToName_ncpy(path, pattern, MAX_FILENAME_SIZE + 1);
  result = Bfile_FindFirst_NON_SMEM((const char*)path, &findHandle,
                                    (char*)found, &fileInfo);
  while (!result) {
    Bfile_NameToStr_ncpy(filename, found, MAX_FILENAME_SIZE + 1);
    int filenameLength = strlen(filename);
    int prefixLength = strlen(prefix);
    int suffixLength = strlen(suffix);
    if (filenameLength > prefixLength + suffixLength &&
        !strncmp(filename, prefix, prefixLength) &&
        !strcmp(filename + filenameLength - suffixLength, suffix)) {
      int originalLength = filenameLength - prefixLength - suffixLength;
      if (count < maximum && originalLength < MAX_FILENAME_SIZE) {
        G3ABackupRecord* record = &records[count];
        memset(record, 0, sizeof(G3ABackupRecord));
        strncpy(record->backupFilename, filename,
                sizeof(record->backupFilename) - 1);
        record->fileSize = fileInfo.fsize;
        char originalName[MAX_FILENAME_SIZE];
        strncpy(originalName, filename + prefixLength, originalLength);
        originalName[originalLength] = 0;
        if (buildG3ABackupPath(originalName, pattern, sizeof(pattern))) {
          count++;
        }
      } else if (truncated != NULL) {
        *truncated = 1;
      }
    }
    result = Bfile_FindNext_NON_SMEM(findHandle, (char*)found,
                                     (char*)&fileInfo);
  }
  if (findHandle >= 0) Bfile_FindClose(findHandle);
  return count;
}

int g3aBackupOriginalFilename(const G3ABackupRecord* record,
                              char* filename, int filenameSize) {
  const char* prefix = "UTILS-APP-";
  const char* suffix = ".bak";
  int length;
  int prefixLength = strlen(prefix);
  int suffixLength = strlen(suffix);
  if (record == NULL || filename == NULL || filenameSize <= 0) return 0;
  filename[0] = 0;
  length = strlen(record->backupFilename);
  if (length <= prefixLength + suffixLength ||
      strncmp(record->backupFilename, prefix, prefixLength) ||
      strcmp(record->backupFilename + length - suffixLength, suffix)) {
    return 0;
  }
  length -= prefixLength + suffixLength;
  if (length <= 0 || length >= filenameSize) return 0;
  strncpy(filename, record->backupFilename + prefixLength, length);
  filename[length] = 0;
  return 1;
}

int restoreG3ABackup(const G3ABackupRecord* record) {
  StorageScope storageScope;
  char sourceName[MAX_FILENAME_SIZE + 1];
  char destinationName[MAX_FILENAME_SIZE + 1];
  char originalName[MAX_FILENAME_SIZE];
  if (record == NULL || record->backupFilename[0] == 0 ||
      record->fileSize < 4 ||
      !g3aBackupOriginalFilename(record, originalName,
                                  sizeof(originalName)) ||
      !buildG3ABackupPath(originalName, destinationName,
                          sizeof(destinationName))) {
    return APP_MANAGE_INVALID;
  }
  strcpy(sourceName, SMEM_PREFIX);
  strncat(sourceName, record->backupFilename,
          sizeof(sourceName) - strlen(sourceName) - 1);
  strcpy(destinationName, SMEM_PREFIX);
  strncat(destinationName, originalName,
          sizeof(destinationName) - strlen(destinationName) - 1);
  return copyStorageFileNoOverwrite(sourceName, destinationName,
                                    record->fileSize);
}
