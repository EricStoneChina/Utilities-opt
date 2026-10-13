#ifndef __LINKPROVIDER_H
#define __LINKPROVIDER_H

// Open the calculator's native Link transfer dialog. Utilities does not
// implement a parallel USB protocol or take ownership of storage.
void linkCenter();

#ifdef DISABLED_EXPERIMENTAL_CODE
void endSerialComm(int error);
void serialTransferSingleFile(char* filename);
#endif

#endif
