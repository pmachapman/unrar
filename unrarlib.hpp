/*  THIS FILE IS NOT ORIGINAL TO UNRAR, AND WAS CREATED BY IDAN LUDMIR
    IN ORDER TO ALLOW ARCHIVE UNPACKING IN MEMORY*/
#ifndef _RAR_LIB
#define _RAR_LIB

#ifdef _UNIX
#define CALLBACK
#define PASCAL
#define LONG long
#define HANDLE void *
#define INVALID_HANDLE_VALUE (HANDLE)-1
#define LPARAM long
#define UINT unsigned int
#endif

namespace UnrarLib {
HANDLE PASCAL OpenArchive(const uint8_t * buff, uint32_t size);
bool PASCAL ReadHeader(HANDLE hArc, uint32_t * size);
bool PASCAL UnpackEntry(HANDLE hArc, uint8_t * entryData);
void PASCAL CloseArchive(HANDLE hArc);
}

#endif
