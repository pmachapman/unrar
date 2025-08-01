#include "rar.hpp"

static int RarErrorToDll(RAR_EXIT ErrCode);

struct DataSet
{
  CommandData Cmd;
  Archive Arc;
  CmdExtract Extract;
  int OpenMode;
  int HeaderSize;

  DataSet():Arc(&Cmd),Extract(&Cmd) {};
};


HANDLE PASCAL RAROpenArchive(struct RAROpenArchiveData *r)
{
  RAROpenArchiveDataEx rx{};
  rx.ArcName=r->ArcName;
  rx.OpenMode=r->OpenMode;
  rx.CmtBuf=r->CmtBuf;
  rx.CmtBufSize=r->CmtBufSize;
  HANDLE hArc=RAROpenArchiveEx(&rx);
  r->OpenResult=rx.OpenResult;
  r->CmtSize=rx.CmtSize;
  r->CmtState=rx.CmtState;
  return hArc;
}


HANDLE PASCAL RAROpenArchiveEx(struct RAROpenArchiveDataEx *r)
{
  DataSet *Data=nullptr;
  try
  {
    ErrHandler.Clean();

    r->OpenResult=0;
    Data=new DataSet;
    Data->Cmd.DllError=0;
    Data->OpenMode=r->OpenMode;
    Data->Cmd.FileArgs.AddString(L"*");
    Data->Cmd.KeepBroken=(r->OpFlags&ROADOF_KEEPBROKEN)!=0;

    std::string AnsiArcName;
    if (r->ArcName!=nullptr)
    {
      AnsiArcName=r->ArcName;
#ifdef _WIN_ALL
      if (!AreFileApisANSI())
        OemToExt(r->ArcName,AnsiArcName);
#endif
    }

    std::wstring ArcName;
    if (r->ArcNameW!=nullptr && *r->ArcNameW!=0)
      ArcName=r->ArcNameW;
    else
      CharToWide(AnsiArcName,ArcName);

    Data->Cmd.AddArcName(ArcName);
    Data->Cmd.Overwrite=OVERWRITE_ALL;
    Data->Cmd.VersionControl=1;

    Data->Cmd.Callback=r->Callback;
    Data->Cmd.UserData=r->UserData;

    // Open shared mode is added by request of dll users, who need to
    // browse and unpack archives while downloading.
    Data->Cmd.OpenShared = true;
    if (!Data->Arc.Open(ArcName,FMF_OPENSHARED))
    {
      r->OpenResult=ERAR_EOPEN;
      delete Data;
      return nullptr;
    }
    if (!Data->Arc.IsArchive(true))
    {
      if (Data->Cmd.DllError!=0)
        r->OpenResult=Data->Cmd.DllError;
      else
      {
        RAR_EXIT ErrCode=ErrHandler.GetErrorCode();
        if (ErrCode!=RARX_SUCCESS && ErrCode!=RARX_WARNING)
          r->OpenResult=RarErrorToDll(ErrCode);
        else
          r->OpenResult=ERAR_BAD_ARCHIVE;
      }
      delete Data;
      return nullptr;
    }
    r->Flags=0;
    
    if (Data->Arc.Volume)
      r->Flags|=ROADF_VOLUME;
    if (Data->Arc.MainComment)
      r->Flags|=ROADF_COMMENT;
    if (Data->Arc.Locked)
      r->Flags|=ROADF_LOCK;
    if (Data->Arc.Solid)
      r->Flags|=ROADF_SOLID;
    if (Data->Arc.NewNumbering)
      r->Flags|=ROADF_NEWNUMBERING;
    if (Data->Arc.Signed)
      r->Flags|=ROADF_SIGNED;
    if (Data->Arc.Protected)
      r->Flags|=ROADF_RECOVERY;
    if (Data->Arc.Encrypted)
      r->Flags|=ROADF_ENCHEADERS;
    if (Data->Arc.FirstVolume)
      r->Flags|=ROADF_FIRSTVOLUME;

    std::wstring CmtDataW;
    if (r->CmtBufSize!=0 && Data->Arc.GetComment(CmtDataW))
    {
      if (r->CmtBufW!=nullptr)
      {
//        CmtDataW.push_back(0);
        size_t Size=wcslen(CmtDataW.data())+1;

        r->CmtState=Size>r->CmtBufSize ? ERAR_SMALL_BUF:1;
        r->CmtSize=(uint)Min(Size,r->CmtBufSize);
        memcpy(r->CmtBufW,CmtDataW.data(),(r->CmtSize-1)*sizeof(*r->CmtBufW));
        r->CmtBufW[r->CmtSize-1]=0;
      }
      else
        if (r->CmtBuf!=NULL)
        {
          std::vector<char> CmtData(CmtDataW.size()*4+1);
          WideToChar(&CmtDataW[0],&CmtData[0],CmtData.size()-1);
          size_t Size=strlen(CmtData.data())+1;

          r->CmtState=Size>r->CmtBufSize ? ERAR_SMALL_BUF:1;
          r->CmtSize=(uint)Min(Size,r->CmtBufSize);
          memcpy(r->CmtBuf,CmtData.data(),r->CmtSize-1);
          r->CmtBuf[r->CmtSize-1]=0;
        }
    }
    else
      r->CmtState=r->CmtSize=0;

#ifdef PROPAGATE_MOTW
    if (r->MarkOfTheWeb!=nullptr)
    {
      Data->Cmd.MotwAllFields=r->MarkOfTheWeb[0]=='1';
      const wchar *Sep=wcschr(r->MarkOfTheWeb,'=');
      if (r->MarkOfTheWeb[0]=='-')
        Data->Cmd.MotwList.Reset();
      else
        Data->Cmd.GetBriefMaskList(Sep==nullptr ? L"*":Sep+1,Data->Cmd.MotwList);
    }
#endif

    Data->Extract.ExtractArchiveInit(Data->Arc);
    return (HANDLE)Data;
  }
  catch (RAR_EXIT ErrCode)
  {
    if (Data!=NULL && Data->Cmd.DllError!=0)
      r->OpenResult=Data->Cmd.DllError;
    else
      r->OpenResult=RarErrorToDll(ErrCode);
    if (Data != NULL)
      delete Data;
    return NULL;
  }
  catch (std::bad_alloc&) // Catch 'new' exception.
  {
    r->OpenResult=ERAR_NO_MEMORY;
    if (Data != NULL)
      delete Data;
  }
  return NULL; // To make compilers happy.
}


int PASCAL RARCloseArchive(HANDLE hArcData)
{
  DataSet *Data=(DataSet *)hArcData;
  try
  {
    bool Success=Data==NULL ? false:Data->Arc.Close();
    delete Data;
    return Success ? ERAR_SUCCESS : ERAR_ECLOSE;
  }
  catch (RAR_EXIT ErrCode)
  {
    return Data->Cmd.DllError!=0 ? Data->Cmd.DllError : RarErrorToDll(ErrCode);
  }
}


int PASCAL RARReadHeader(HANDLE hArcData,struct RARHeaderData *D)
{
  struct RARHeaderDataEx X{};

  int Code=RARReadHeaderEx(hArcData,&X);

  strncpyz(D->ArcName,X.ArcName,ASIZE(D->ArcName));
  strncpyz(D->FileName,X.FileName,ASIZE(D->FileName));
  D->Flags=X.Flags;
  D->PackSize=X.PackSize;
  D->UnpSize=X.UnpSize;
  D->HostOS=X.HostOS;
  D->FileCRC=X.FileCRC;
  D->FileTime=X.FileTime;
  D->UnpVer=X.UnpVer;
  D->Method=X.Method;
  D->FileAttr=X.FileAttr;
  D->CmtSize=0;
  D->CmtState=0;

  return Code;
}


int PASCAL RARReadHeaderEx(HANDLE hArcData,struct RARHeaderDataEx *D)
{
  DataSet *Data=(DataSet *)hArcData;
  try
  {
    if ((Data->HeaderSize=(int)Data->Arc.SearchBlock(HEAD_FILE))<=0)
    {
      if (Data->Arc.Volume && Data->Arc.GetHeaderType()==HEAD_ENDARC &&
          Data->Arc.EndArcHead.NextVolume)
        if (MergeArchive(Data->Arc,NULL,false,'L'))
        {
          Data->Arc.Seek(Data->Arc.CurBlockPos,SEEK_SET);
          return RARReadHeaderEx(hArcData,D);
        }
        else
          return ERAR_EOPEN;

      if (Data->Arc.BrokenHeader)
        return ERAR_BAD_DATA;

      // Might be necessary if RARSetPassword is still called instead of
      // open callback for RAR5 archives and if password is invalid.
      if (Data->Arc.FailedHeaderDecryption)
        return ERAR_BAD_PASSWORD;
      
      return ERAR_END_ARCHIVE;
    }
    FileHeader *hd=&Data->Arc.FileHead;
    if (Data->OpenMode==RAR_OM_LIST && hd->SplitBefore)
    {
      int Code=RARProcessFile(hArcData,RAR_SKIP,NULL,NULL);
      if (Code==0)
        return RARReadHeaderEx(hArcData,D);
      else
        return Code;
    }
    wcsncpyz(D->ArcNameW,Data->Arc.FileName.c_str(),ASIZE(D->ArcNameW));
    WideToChar(D->ArcNameW,D->ArcName,ASIZE(D->ArcName));
    if (D->ArcNameEx!=nullptr)
      wcsncpyz(D->ArcNameEx,Data->Arc.FileName.c_str(),D->ArcNameExSize);

    wcsncpyz(D->FileNameW,hd->FileName.c_str(),ASIZE(D->FileNameW));
    WideToChar(D->FileNameW,D->FileName,ASIZE(D->FileName));
#ifdef _WIN_ALL
    CharToOemA(D->FileName,D->FileName);
#endif
    if (D->FileNameEx!=nullptr)
      wcsncpyz(D->FileNameEx,hd->FileName.c_str(),D->FileNameExSize);

    D->Flags=0;
    if (hd->SplitBefore)
      D->Flags|=RHDF_SPLITBEFORE;
    if (hd->SplitAfter)
      D->Flags|=RHDF_SPLITAFTER;
    if (hd->Encrypted)
      D->Flags|=RHDF_ENCRYPTED;
    if (hd->Solid)
      D->Flags|=RHDF_SOLID;
    if (hd->Dir)
      D->Flags|=RHDF_DIRECTORY;

    D->PackSize=uint(hd->PackSize & 0xffffffff);
    D->PackSizeHigh=uint(hd->PackSize>>32);
    D->UnpSize=uint(hd->UnpSize & 0xffffffff);
    D->UnpSizeHigh=uint(hd->UnpSize>>32);
    D->HostOS=hd->HSType==HSYS_WINDOWS ? HOST_WIN32:HOST_UNIX;
    D->UnpVer=Data->Arc.FileHead.UnpVer;
    D->FileCRC=hd->FileHash.CRC32;
    D->FileTime=hd->mtime.GetDos();
    
    uint64 MRaw=hd->mtime.GetWin();
    D->MtimeLow=(uint)MRaw;
    D->MtimeHigh=(uint)(MRaw>>32);
    uint64 CRaw=hd->ctime.GetWin();
    D->CtimeLow=(uint)CRaw;
    D->CtimeHigh=(uint)(CRaw>>32);
    uint64 ARaw=hd->atime.GetWin();
    D->AtimeLow=(uint)ARaw;
    D->AtimeHigh=(uint)(ARaw>>32);

    D->Method=hd->Method+0x30;
    D->FileAttr=hd->FileAttr;
    D->CmtSize=0;
    D->CmtState=0;

    D->DictSize=uint(hd->WinSize/1024);

    switch (hd->FileHash.Type)
    {
      case HASH_RAR14:
      case HASH_CRC32:
        D->HashType=RAR_HASH_CRC32;
        break;
      case HASH_BLAKE2:
        D->HashType=RAR_HASH_BLAKE2;
        memcpy(D->Hash,hd->FileHash.Digest,BLAKE2_DIGEST_SIZE);
        break;
      default:
        D->HashType=RAR_HASH_NONE;
        break;
    }

    D->RedirType=hd->RedirType;
    // RedirNameSize sanity check is useful in case some developer
    // did not initialize Reserved area with 0 as required in docs.
    // We have taken 'Redir*' fields from Reserved area. We may remove
    // this RedirNameSize check sometimes later.
    if (hd->RedirType!=FSREDIR_NONE && D->RedirName!=NULL &&
        D->RedirNameSize>0 && D->RedirNameSize<100000)
      wcsncpyz(D->RedirName,hd->RedirName.c_str(),D->RedirNameSize);
    D->DirTarget=hd->DirTarget;
  }
  catch (RAR_EXIT ErrCode)
  {
    return Data->Cmd.DllError!=0 ? Data->Cmd.DllError : RarErrorToDll(ErrCode);
  }
  return Data->Cmd.DllError!=0 ? Data->Cmd.DllError : RarErrorToDll(ErrHandler.GetErrorCode());
}


int PASCAL ProcessFile(HANDLE hArcData,int Operation,char *DestPath,char *DestName,wchar *DestPathW,wchar *DestNameW)
{
  DataSet *Data=(DataSet *)hArcData;
  try
  {
    Data->Cmd.DllError=0;
    if (Data->OpenMode==RAR_OM_LIST || Data->OpenMode==RAR_OM_LIST_INCSPLIT ||
        Operation==RAR_SKIP && !Data->Arc.Solid)
    {
      if (Data->Arc.Volume && Data->Arc.GetHeaderType()==HEAD_FILE &&
          Data->Arc.FileHead.SplitAfter)
        if (MergeArchive(Data->Arc,NULL,false,'L'))
        {
          Data->Arc.Seek(Data->Arc.CurBlockPos,SEEK_SET);
          return ERAR_SUCCESS;
        }
        else
          return ERAR_EOPEN;
      Data->Arc.SeekToNext();
    }
    else
    {
      Data->Cmd.DllOpMode=Operation;

      Data->Cmd.ExtrPath.clear();
      Data->Cmd.DllDestName.clear();

      if (DestPath!=NULL)
      {
        std::string ExtrPathA=DestPath;
#ifdef _WIN_ALL
        // We must not apply OemToCharBuffA directly to DestPath,
        // because we do not know DestPath length and OemToCharBuffA
        // does not stop at 0.
        OemToExt(ExtrPathA,ExtrPathA);
#endif
        CharToWide(ExtrPathA,Data->Cmd.ExtrPath);
        AddEndSlash(Data->Cmd.ExtrPath);
      }
      if (DestName!=NULL)
      {
        std::string DestNameA=DestName;
#ifdef _WIN_ALL
        // We must not apply OemToCharBuffA directly to DestName,
        // because we do not know DestName length and OemToCharBuffA
        // does not stop at 0.
        OemToExt(DestNameA,DestNameA);
#endif
        CharToWide(DestNameA,Data->Cmd.DllDestName);
      }

      if (DestPathW!=NULL)
      {
        Data->Cmd.ExtrPath=DestPathW;
        AddEndSlash(Data->Cmd.ExtrPath);
      }

      if (DestNameW!=NULL)
        Data->Cmd.DllDestName=DestNameW;

      Data->Cmd.Command=Operation==RAR_EXTRACT ? L"X":L"T";
      Data->Cmd.Test=Operation!=RAR_EXTRACT;
      bool Repeat=false;
      Data->Extract.ExtractCurrentFile(Data->Arc,Data->HeaderSize,Repeat);

      // Now we process extra file information if any.
      // It is important to do it in the same ProcessFile(), because caller
      // app can rely on this behavior, for example, to overwrite
      // the extracted Mark of the Web with propagated from archive
      // immediately after ProcessFile() call.
      //
      // Archive can be closed if we process volumes, next volume is missing
      // and current one is already removed or deleted. So we need to check
      // if archive is still open to avoid calling file operations on
      // the invalid file handle. Some of our file operations like Seek()
      // process such invalid handle correctly, some not.
      while (Data->Arc.IsOpened() && Data->Arc.ReadHeader()!=0 && 
             Data->Arc.GetHeaderType()==HEAD_SERVICE)
      {
        Data->Extract.ExtractCurrentFile(Data->Arc,Data->HeaderSize,Repeat);
        Data->Arc.SeekToNext();
      }
      Data->Arc.Seek(Data->Arc.CurBlockPos,SEEK_SET);
    }
  }
  catch (std::bad_alloc&)
  {
    return ERAR_NO_MEMORY;
  }
  catch (RAR_EXIT ErrCode)
  {
    return Data->Cmd.DllError!=0 ? Data->Cmd.DllError : RarErrorToDll(ErrCode);
  }
  return Data->Cmd.DllError!=0 ? Data->Cmd.DllError : RarErrorToDll(ErrHandler.GetErrorCode());
}


int PASCAL RARProcessFile(HANDLE hArcData,int Operation,char *DestPath,char *DestName)
{
  return ProcessFile(hArcData,Operation,DestPath,DestName,NULL,NULL);
}


int PASCAL RARProcessFileW(HANDLE hArcData,int Operation,wchar *DestPath,wchar *DestName)
{
  return ProcessFile(hArcData,Operation,NULL,NULL,DestPath,DestName);
}


void PASCAL RARSetChangeVolProc(HANDLE hArcData,CHANGEVOLPROC ChangeVolProc)
{
  DataSet *Data=(DataSet *)hArcData;
  Data->Cmd.ChangeVolProc=ChangeVolProc;
}


void PASCAL RARSetCallback(HANDLE hArcData,UNRARCALLBACK Callback,LPARAM UserData)
{
  DataSet *Data=(DataSet *)hArcData;
  Data->Cmd.Callback=Callback;
  Data->Cmd.UserData=UserData;
}


void PASCAL RARSetProcessDataProc(HANDLE hArcData,PROCESSDATAPROC ProcessDataProc)
{
  DataSet *Data=(DataSet *)hArcData;
  Data->Cmd.ProcessDataProc=ProcessDataProc;
}


void PASCAL RARSetPassword(HANDLE hArcData,char *Password)
{
#ifndef RAR_NOCRYPT
  DataSet *Data=(DataSet *)hArcData;
  wchar PasswordW[MAXPASSWORD];
  CharToWide(Password,PasswordW,ASIZE(PasswordW));
  Data->Cmd.Password.Set(PasswordW);
  cleandata(PasswordW,sizeof(PasswordW));
#endif
}


int PASCAL RARGetDllVersion()
{
  return RAR_DLL_VERSION;
}


static int RarErrorToDll(RAR_EXIT ErrCode)
{
  switch(ErrCode)
  {
    case RARX_FATAL:
    case RARX_READ:
      return ERAR_EREAD;
    case RARX_CRC:
      return ERAR_BAD_DATA;
    case RARX_WRITE:
      return ERAR_EWRITE;
    case RARX_OPEN:
      return ERAR_EOPEN;
    case RARX_CREATE:
      return ERAR_ECREATE;
    case RARX_MEMORY:
      return ERAR_NO_MEMORY;
    case RARX_BADPWD:
      return ERAR_BAD_PASSWORD;
    case RARX_SUCCESS:
      return ERAR_SUCCESS; // 0.
    case RARX_BADARC:
      return ERAR_BAD_ARCHIVE;
    default:
      return ERAR_UNKNOWN;
  }
}
