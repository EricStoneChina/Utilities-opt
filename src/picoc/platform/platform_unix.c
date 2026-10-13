#ifdef UNIX_HOST

#include "../picoc.h"
#include "../interpreter.h"

#ifdef USE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

jmp_buf PicocExitBuf;

void PlatformCleanup()
{
}

char *PlatformGetLine(char *Buf, int MaxLen, const char *Prompt)
{
#ifdef USE_READLINE
    if (Prompt != NULL)
    {
        char *InLine = readline(Prompt);
        if (InLine == NULL) return NULL;
        Buf[MaxLen - 1] = '\0';
        strncpy(Buf, InLine, MaxLen - 2);
        strncat(Buf, "\n", MaxLen - 2);
        if (InLine[0] != '\0') add_history(InLine);
        free(InLine);
        return Buf;
    }
#endif
    if (Prompt != NULL) printf("%s", Prompt);
    fflush(stdout);
    return fgets(Buf, MaxLen, stdin);
}

int PlatformGetCharacter()
{
    fflush(stdout);
    return getchar();
}

void PlatformPutc(unsigned char OutCh, union OutputStreamInfo *Stream)
{
    (void)Stream;
    putchar(OutCh);
}

char *PlatformReadFile(const char *FileName)
{
    struct stat FileInfo;
    char *ReadText;
    FILE *InFile;
    size_t BytesRead;

    if (stat(FileName, &FileInfo))
        ProgramFail(NULL, "can't read file %s\n", FileName);
    ReadText = (char*)malloc(FileInfo.st_size + 1);
    if (ReadText == NULL) ProgramFail(NULL, "out of memory\n");
    InFile = fopen(FileName, "r");
    if (InFile == NULL) ProgramFail(NULL, "can't read file %s\n", FileName);
    BytesRead = fread(ReadText, 1, FileInfo.st_size, InFile);
    if (ferror(InFile)) ProgramFail(NULL, "can't read file %s\n", FileName);
    ReadText[BytesRead] = '\0';
    fclose(InFile);
    return ReadText;
}

void PicocPlatformScanFile(const char *FileName)
{
    char *SourceStr = PlatformReadFile(FileName);
    PicocParse(FileName, SourceStr, strlen(SourceStr), TRUE, FALSE, TRUE);
}

void PlatformExit(int RetVal)
{
    PicocExitValue = RetVal;
    longjmp(PicocExitBuf, 1);
}

#endif
