/*  THIS FILE IS NOT ORIGINAL TO UNRAR, AND WAS CREATED BY IDAN LUDMIR
    IN ORDER TO ALLOW ARCHIVE UNPACKING IN MEMORY*/
#include "rar.hpp"

struct UnrarArchive
{
    Archive Arc;
};

namespace UnrarLib {
void UnstoreFile(ComprDataIO & DataIO, int64 DestUnpSize)
{
    Array<byte> Buffer(File::CopyBufferSize());
    while (true) {
        int ReadSize = DataIO.UnpRead(&Buffer[0], Buffer.Size());
        if (ReadSize <= 0) {
            break;
        }
        int WriteSize = ReadSize < DestUnpSize ? ReadSize : (int)DestUnpSize;
        if (WriteSize > 0) {
            DataIO.UnpWrite(&Buffer[0], WriteSize);
            DestUnpSize -= WriteSize;
        }
    }
}

HANDLE PASCAL OpenArchive(const uint8_t * buff, uint32_t size)
{
    UnrarArchive * rarArc = NULL;
    try {
        rarArc = new UnrarArchive;
        rarArc->Arc.Open(reinterpret_cast<const wchar *>(buff), size);
        if (!rarArc->Arc.IsArchive(true)) {
            throw RARX_OPEN;
        }
    } catch (RAR_EXIT) {
        if (rarArc != NULL) {
            delete rarArc;
        }
        return NULL;
    } catch (std::bad_alloc &) { // Catch 'new' exception.
        if (rarArc != NULL) {
            delete rarArc;
        }
    }
    return rarArc;
}

bool PASCAL ReadHeader(HANDLE hArc, uint32_t * size)
{
    UnrarArchive * rarArc = (UnrarArchive *)hArc;
    try {
        size_t headerSize = rarArc->Arc.ReadHeader();
        if (headerSize <= 7) {
            return false;
        }

        *size = static_cast<uint32_t>(rarArc->Arc.FileHead.UnpSize);
    } catch (RAR_EXIT) {
        return false;
    }
    return true;
}

bool PASCAL UnpackEntry(HANDLE hArc, uint8_t * entryData)
{
    UnrarArchive * rarArc = (UnrarArchive *)hArc;
    try {
        ComprDataIO dataIO;
        dataIO.SetUnpackToMemory(entryData,
                                 static_cast<uint32_t>(rarArc->Arc.FileHead.UnpSize));
        dataIO.CurUnpRead = 0;
        dataIO.CurUnpWrite = 0;
        dataIO.UnpHash.Init(rarArc->Arc.FileHead.FileHash.Type, 1);
        dataIO.PackedDataHash.Init(rarArc->Arc.FileHead.FileHash.Type, 1);
        dataIO.SetPackedSizeToRead(rarArc->Arc.FileHead.PackSize);
        dataIO.SetFiles(&rarArc->Arc, NULL);
        Unpack unp(&dataIO);

        if (rarArc->Arc.FileHead.Method == 0) {
            UnstoreFile(dataIO, rarArc->Arc.FileHead.UnpSize);
        } else {
            unp.Init(rarArc->Arc.FileHead.WinSize, rarArc->Arc.FileHead.Solid);
            unp.SetDestSize(rarArc->Arc.FileHead.UnpSize);
            if (rarArc->Arc.Format != RARFMT50 && rarArc->Arc.FileHead.UnpVer <= 15) {
                unp.DoUnpack(15, rarArc->Arc.Solid);
            } else {
                unp.DoUnpack(rarArc->Arc.FileHead.UnpVer, rarArc->Arc.FileHead.Solid);
            }
        }
    } catch (std::bad_alloc &) {
        return false;
    } catch (RAR_EXIT) {
        return false;
    }

    return true;
}

void PASCAL CloseArchive(HANDLE hArc)
{
    UnrarArchive * rarArc = (UnrarArchive *)hArc;
    try {
        bool Success = rarArc == NULL ? false : rarArc->Arc.Close();
        delete rarArc;
        return;
    } catch (RAR_EXIT) {
        return;
    }
}
}