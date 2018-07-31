/*  THIS FILE IS NOT ORIGINAL TO UNRAR, AND WAS CREATED BY IDAN LUDMIR
    IN ORDER TO ALLOW ARCHIVE UNPACKING IN MEMORY*/
#include "rar.hpp"

File::File()
{
    FileData = 0;
    FileSize = 0;
    *FileName = 0;
    FilePos = 0;
    NewFile = false;
    LastWrite = false;
    HandleType = FILE_HANDLENORMAL;
    SkipClose = false;
    IgnoreReadErrors = false;
    ErrorType = FILE_SUCCESS;
    OpenShared = false;
    AllowDelete = true;
    AllowExceptions = true;
#ifdef _WIN_ALL
    NoSequentialRead = false;
    CreateMode = FMF_UNDEFINED;
#endif
}


File::~File()
{

}


void File::operator = (File & SrcFile)
{
    FileData = SrcFile.FileData;
    FileSize = SrcFile.FileSize;
    NewFile = SrcFile.NewFile;
    LastWrite = SrcFile.LastWrite;
    HandleType = SrcFile.HandleType;
    wcsncpyz(FileName, SrcFile.FileName, ASIZE(FileName));
    SrcFile.SkipClose = true;
}


bool File::Open(const wchar * Data, uint Size)
{
    FileData = reinterpret_cast<const byte *>(Data);
    FileSize = Size;
    return true;
}


#if !defined(SFX_MODULE)
void File::TOpen(const wchar * Name)
{
    if (!WOpen(Name)) {
        ErrHandler.Exit(RARX_OPEN);
    }
}
#endif


bool File::WOpen(const wchar * Name)
{
    return false;
}


bool File::Create(const wchar * Name, uint Mode)
{
    return false;
}


#if !defined(SFX_MODULE)
void File::TCreate(const wchar * Name, uint Mode)
{
    if (!WCreate(Name, Mode)) {
        ErrHandler.Exit(RARX_FATAL);
    }
}
#endif


bool File::WCreate(const wchar * Name, uint Mode)
{
    return false;
}


bool File::Close()
{
    return true;
}


bool File::Delete()
{
    return false;
}


bool File::Rename(const wchar * NewName)
{
    return false;
}


bool File::Write(const void * Data, size_t Size)
{
    return false;
}


int File::Read(void * Data, size_t Size)
{
    if (static_cast<uint64>(FilePos + Size) > FileSize) {
        return 0;
    }

    memcpy(Data, FileData + FilePos, Size);

    FilePos += Size;

    return static_cast<int>(Size);
}


// Returns -1 in case of error.
int File::DirectRead(void * Data, size_t Size)
{
    return -1;
}


void File::Seek(int64 Offset, int Method)
{
    if (!RawSeek(Offset, Method) && AllowExceptions) {
        ErrHandler.SeekError(FileName);
    }
}


bool File::RawSeek(int64 Offset, int Method)
{
    if (static_cast<uint64>(Offset) > FileSize) {
        return false;
    }

    FilePos = Offset;

    return true;
}


int64 File::Tell()
{
    return FilePos;
}


void File::Prealloc(int64 Size)
{
#ifdef _WIN_ALL
    if (RawSeek(Size, SEEK_SET)) {
        Truncate();
        Seek(0, SEEK_SET);
    }
#endif

#if defined(_UNIX) && defined(USE_FALLOCATE)
    // fallocate is rather new call. Only latest kernels support it.
    // So we are not using it by default yet.
    int fd = GetFD();
    if (fd >= 0) {
        fallocate(fd, 0, 0, Size);
    }
#endif
}


byte File::GetByte()
{
    byte Byte = 0;
    Read(&Byte, 1);
    return Byte;
}


void File::PutByte(byte Byte)
{
    Write(&Byte, 1);
}


bool File::Truncate()
{
    return false;
}


void File::Flush()
{

}


void File::SetOpenFileTime(RarTime * ftm, RarTime * ftc, RarTime * fta)
{

}


void File::SetCloseFileTime(RarTime * ftm, RarTime * fta)
{

}


void File::SetCloseFileTimeByName(const wchar * Name, RarTime * ftm,
                                  RarTime * fta)
{

}


void File::GetOpenFileTime(RarTime * ft)
{

}


int64 File::FileLength()
{
    return FileSize;
}


bool File::IsDevice()
{
    return false;
}


#ifndef SFX_MODULE
int64 File::Copy(File & Dest, int64 Length)
{
    return 0;
}
#endif
