// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ApplePlatformFile.mm: Apple platform implementations of File functions
=============================================================================*/

#include "CorePrivate.h"

// make an FTimeSpan object that represents the "epoch" for time_t (from a stat struct)
const FDateTime MacEpoch(1970, 1, 1);

/** 
 * Mac file handle implementation which limits number of open files per thread. This
 * is to prevent running out of system file handles (250). Should not be neccessary when
 * using pak file (e.g., SHIPPING?) so not particularly optimized. Only manages
 * files which are opened READ_ONLY.
**/
#define MANAGE_FILE_HANDLES PLATFORM_MAC // !UE_BUILD_SHIPPING

class CORE_API FFileHandleApple : public IFileHandle
{
	enum {READWRITE_SIZE = 1024 * 1024};

public:
	FFileHandleApple(int32 InFileHandle, const TCHAR* InFilename, bool bIsReadOnly)
		: FileHandle(InFileHandle)
#if MANAGE_FILE_HANDLES
		, Filename(InFilename)
		, HandleSlot(-1)
		, FileOffset(0)
		, FileSize(0)
#endif
	{
		check(FileHandle > -1);

#if MANAGE_FILE_HANDLES
		// Only files opened for read will be managed
		if (bIsReadOnly)
		{
			ReserveSlot();
			ActiveHandles[HandleSlot] = this;
			struct stat FileInfo;
			fstat(FileHandle, &FileInfo);
			FileSize = FileInfo.st_size;
		}
#endif
	}
	virtual ~FFileHandleApple()
	{
#if MANAGE_FILE_HANDLES
		if( IsManaged() )
		{
			if( ActiveHandles[ HandleSlot ] == this )
			{
				close(FileHandle);
				ActiveHandles[ HandleSlot ] = NULL;
			}
		}
		else
#endif
		{
			close(FileHandle);
		}
		FileHandle = -1;
	}
	virtual int64 Tell() OVERRIDE
	{
#if MANAGE_FILE_HANDLES
		if( IsManaged() )
		{
			return FileOffset;
		}
		else
#endif
		{
			check(IsValid());
			return lseek(FileHandle, 0, SEEK_CUR);
		}
	}
	virtual bool Seek(int64 NewPosition) OVERRIDE
	{
		check(NewPosition >= 0);

#if MANAGE_FILE_HANDLES
		if( IsManaged() )
		{
			FileOffset = NewPosition >= FileSize ? FileSize - 1 : NewPosition;
			return ActiveHandles[ HandleSlot ] == this ? lseek(FileHandle, FileOffset, SEEK_SET) != -1 : true;
		}
		else
#endif
		{
			check(IsValid());
			return lseek(FileHandle, NewPosition, SEEK_SET) != -1;
		}
	}
	virtual bool SeekFromEnd(int64 NewPositionRelativeToEnd = 0) OVERRIDE
	{
		check(NewPositionRelativeToEnd <= 0);

#if MANAGE_FILE_HANDLES
		if( IsManaged() )
		{
			FileOffset = (NewPositionRelativeToEnd >= FileSize) ? 0 : ( FileSize + NewPositionRelativeToEnd - 1 );
			return ActiveHandles[ HandleSlot ] == this ? lseek(FileHandle, FileOffset, SEEK_SET) != -1 : true;
		}
		else
#endif
		{
			check(IsValid());
			return lseek(FileHandle, NewPositionRelativeToEnd, SEEK_END) != -1;
		}
	}
	virtual bool Read(uint8* Destination, int64 BytesToRead) OVERRIDE
	{
#if MANAGE_FILE_HANDLES
		if( IsManaged() )
		{
			ActivateSlot();
			int64 BytesRead = ReadInternal(Destination, BytesToRead);
			FileOffset += BytesRead;
			return BytesRead == BytesToRead;
		}
		else
#endif
		{
			return ReadInternal(Destination, BytesToRead) == BytesToRead;
		}
	}
	virtual bool Write(const uint8* Source, int64 BytesToWrite) OVERRIDE
	{
		check(IsValid());
		while (BytesToWrite)
		{
			check(BytesToWrite >= 0);
			int64 ThisSize = FMath::Min<int64>(READWRITE_SIZE, BytesToWrite);
			check(Source);
			if (write(FileHandle, Source, ThisSize) != ThisSize)
			{
				return false;
			}
			Source += ThisSize;
			BytesToWrite -= ThisSize;
		}
		return true;
	}
	virtual int64 Size() OVERRIDE
	{
#if MANAGE_FILE_HANDLES
		if( IsManaged() )
		{
			return FileSize;
		}
		else
#endif
		{
			struct stat FileInfo;
			fstat(FileHandle, &FileInfo);
			return FileInfo.st_size;
		}
	}

private:

#if MANAGE_FILE_HANDLES
	FORCEINLINE bool IsManaged()
	{
		return HandleSlot != -1;
	}

	void ActivateSlot()
	{
		if( IsManaged() )
		{
			if( ActiveHandles[ HandleSlot ] != this || (ActiveHandles[ HandleSlot ] && ActiveHandles[ HandleSlot ]->FileHandle == -1) )
			{
				ReserveSlot();

				FileHandle = open(TCHAR_TO_UTF8(*Filename), O_RDONLY);
				if( FileHandle != -1 )
				{
					lseek(FileHandle, FileOffset, SEEK_SET);
					ActiveHandles[ HandleSlot ] = this;
				}
			}
			else
			{
				AccessTimes[ HandleSlot ] = FPlatformTime::Seconds();
			}
		}
	}

	void ReserveSlot()
	{
		HandleSlot = -1;

		// Look for non-reserved slot
		for( int32 i = 0; i < ACTIVE_HANDLE_COUNT; ++i )
		{
			if( ActiveHandles[ i ] == NULL )
			{
				HandleSlot = i;
				break;
			}
		}

		// Take the oldest handle
		if( HandleSlot == -1 )
		{
			int32 Oldest = 0;
			for( int32 i = 1; i < ACTIVE_HANDLE_COUNT; ++i )
			{
				if( AccessTimes[ Oldest ] > AccessTimes[ i ] )
				{
					Oldest = i;
				}
			}

			close( ActiveHandles[ Oldest ]->FileHandle );
			ActiveHandles[ Oldest ]->FileHandle = -1;
			HandleSlot = Oldest;
		}

		ActiveHandles[ HandleSlot ] = NULL;
		AccessTimes[ HandleSlot ] = FPlatformTime::Seconds();
	}
#endif

	int64 ReadInternal(uint8* Destination, int64 BytesToRead)
	{
		check(IsValid());
		int64 BytesRead = 0;
		while (BytesToRead)
		{
			check(BytesToRead >= 0);
			int64 ThisSize = FMath::Min<int64>(READWRITE_SIZE, BytesToRead);
			check(Destination);
			int64 ThisRead = read(FileHandle, Destination, ThisSize);
			BytesRead += ThisRead;
			if (ThisRead != ThisSize)
			{
				return BytesRead;
			}
			Destination += ThisSize;
			BytesToRead -= ThisSize;
		}
		return BytesRead;
	}

	// Holds the internal file handle.
	int32 FileHandle;

#if MANAGE_FILE_HANDLES
	// Holds the name of the file that this handle represents. Kept around for possible reopen of file.
	FString Filename;

    // Most recent valid slot index for this handle; >=0 for handles which are managed.
    int32 HandleSlot;

    // Current file offset; valid if a managed handle.
    int64 FileOffset;

    // Cached file size; valid if a managed handle.
    int64 FileSize;

    // Each thread keeps a collection of active handles with access times.
    static const int32 ACTIVE_HANDLE_COUNT = 256;
    static __thread FFileHandleApple* ActiveHandles[ ACTIVE_HANDLE_COUNT ];
    static __thread double AccessTimes[ ACTIVE_HANDLE_COUNT ];
#endif

	FORCEINLINE bool IsValid()
	{
		return FileHandle != -1;
	}
};

#if MANAGE_FILE_HANDLES
__thread FFileHandleApple* FFileHandleApple::ActiveHandles[ FFileHandleApple::ACTIVE_HANDLE_COUNT ];
__thread double FFileHandleApple::AccessTimes[ FFileHandleApple::ACTIVE_HANDLE_COUNT ];
#endif

/**
 * Mac File I/O implementation
**/
FString FApplePlatformFile::NormalizeFilename(const TCHAR* Filename)
{
	FString Result(Filename);
	Result.ReplaceInline(TEXT("\\"), TEXT("/"));
	return Result;
}
FString FApplePlatformFile::NormalizeDirectory(const TCHAR* Directory)
{
	FString Result(Directory);
	Result.ReplaceInline(TEXT("\\"), TEXT("/"));
	if (Result.EndsWith(TEXT("/")))
	{
		Result.LeftChop(1);
	}
	return Result;
}

bool FApplePlatformFile::FileExists(const TCHAR* Filename)
{
	struct stat FileInfo;
	if (Stat(Filename, &FileInfo) != -1)
	{
		return S_ISREG(FileInfo.st_mode);
	}
	return false;
}
int64 FApplePlatformFile::FileSize(const TCHAR* Filename)
{
	struct stat FileInfo;
	FileInfo.st_size = -1;
	Stat(Filename, &FileInfo);
	// make sure to return -1 for directories
	if (S_ISDIR(FileInfo.st_mode))
	{
		FileInfo.st_size = -1;
	}
	return FileInfo.st_size;
}
bool FApplePlatformFile::DeleteFile(const TCHAR* Filename)
{
	return unlink(TCHAR_TO_UTF8(*NormalizeFilename(Filename))) == 0;
}
bool FApplePlatformFile::IsReadOnly(const TCHAR* Filename)
{
	if (access(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), F_OK) == -1)
	{
		return false; // file doesn't exist
	}
	if (access(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), W_OK) == -1)
	{
		return errno == EACCES;
	}
	return false;
}
bool FApplePlatformFile::MoveFile(const TCHAR* To, const TCHAR* From)
{
	int32 Result = rename(TCHAR_TO_UTF8(*NormalizeFilename(From)), TCHAR_TO_UTF8(*NormalizeFilename(To)));
	if (Result == -1 && errno == EXDEV)
	{
		// Copy the file if rename failed because To and From are on different file systems
		if (CopyFile(To, From))
		{
			DeleteFile(From);
			Result = 0;
		}
	}
	return Result != -1;
}
bool FApplePlatformFile::SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue)
{
	struct stat FileInfo;
	ANSICHAR FixedFilePath[MAX_PATH] = { 0 };
	if (Stat(Filename, &FileInfo, FixedFilePath, MAX_PATH) != -1)
	{
		if (bNewReadOnlyValue)
		{
			FileInfo.st_mode &= ~S_IWUSR;
		}
		else
		{
			FileInfo.st_mode |= S_IWUSR;
		}
		return chmod(*FixedFilePath ? FixedFilePath : TCHAR_TO_UTF8(*NormalizeFilename(Filename)), FileInfo.st_mode);
	}
	return false;
}


FDateTime FApplePlatformFile::GetTimeStamp(const TCHAR* Filename)
{
	// get file times
	struct stat FileInfo;
	if(Stat(Filename, &FileInfo) == -1)
	{
		return FDateTime::MinValue();
	}

	// convert _stat time to FDateTime
	FTimespan TimeSinceEpoch(0, 0, FileInfo.st_mtime);
	return MacEpoch + TimeSinceEpoch;

}

void FApplePlatformFile::SetTimeStamp(const TCHAR* Filename, const FDateTime DateTime)
{
	// get file times
	struct stat FileInfo;
	ANSICHAR FixedFilePath[MAX_PATH] = { 0 };
	if (Stat(Filename, &FileInfo, FixedFilePath, MAX_PATH) != -1)
	{
		return;
	}

	// change the modification time only
	struct utimbuf Times;
	Times.actime = FileInfo.st_atime;
	Times.modtime = (DateTime - MacEpoch).GetTotalSeconds();
	utime(*FixedFilePath ? FixedFilePath : TCHAR_TO_UTF8(*NormalizeFilename(Filename)), &Times);
}
FDateTime FApplePlatformFile::GetAccessTimeStamp(const TCHAR* Filename)
{
	// get file times
	struct stat FileInfo;
	if(Stat(Filename, &FileInfo) == -1)
	{
		return FDateTime::MinValue();
	}

	// convert _stat time to FDateTime
	FTimespan TimeSinceEpoch(0, 0, FileInfo.st_atime);
	return MacEpoch + TimeSinceEpoch;
}

IFileHandle* FApplePlatformFile::OpenRead(const TCHAR* Filename)
{
	int32 Handle = open(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), O_RDONLY);
	if (Handle != -1)
	{
#if MANAGE_FILE_HANDLES
		return new FFileHandleApple(Handle, *NormalizeDirectory(Filename), true);
#else
		return new FFileHandleApple(Handle, Filename, true);
#endif
	}
	return NULL;
}
IFileHandle* FApplePlatformFile::OpenWrite(const TCHAR* Filename, bool bAppend, bool bAllowRead)
{
	int Flags = O_CREAT;
	if (bAppend)
	{
		Flags |= O_APPEND;
	}
	else
	{
		Flags |= O_TRUNC;
	}
	if (bAllowRead)
	{
		Flags |= O_RDWR;
	}
	else
	{
		Flags |= O_WRONLY;
	}
	int32 Handle = open(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), Flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (Handle != -1)
	{
#if MANAGE_FILE_HANDLES
		FFileHandleApple* FileHandleApple = new FFileHandleApple(Handle, *NormalizeDirectory(Filename), false);
#else
		FFileHandleApple* FileHandleApple = new FFileHandleApple(Handle, Filename, false);
#endif
		if (bAppend)
		{
			FileHandleApple->SeekFromEnd(0);
		}
		return FileHandleApple;
	}
	return NULL;
}
bool FApplePlatformFile::DirectoryExists(const TCHAR* Directory)
{
	struct stat FileInfo;
	if (Stat(Directory, &FileInfo) != -1)
	{
		return S_ISDIR(FileInfo.st_mode);
	}
	return false;
}
bool FApplePlatformFile::CreateDirectory(const TCHAR* Directory)
{
	@autoreleasepool
	{
		CFStringRef CFDirectory = FPlatformString::TCHARToCFString(*NormalizeFilename(Directory));
		bool Result = [[NSFileManager defaultManager] createDirectoryAtPath:(NSString*)CFDirectory withIntermediateDirectories:true attributes:nil error:nil];
		CFRelease(CFDirectory);
		return Result;
	}
}
bool FApplePlatformFile::DeleteDirectory(const TCHAR* Directory)
{
	return rmdir(TCHAR_TO_UTF8(*NormalizeFilename(Directory)));
}
bool FApplePlatformFile::IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor)
{
	@autoreleasepool
	{
		bool Result = false;
		DIR* Handle = opendir(Directory[0] ? TCHAR_TO_UTF8(Directory) : ".");
		if (Handle)
		{
			Result = true;
			struct dirent *Entry;
			while ((Entry = readdir(Handle)) != NULL)
			{
				if (FCStringAnsi::Strcmp(Entry->d_name, ".") && FCStringAnsi::Strcmp(Entry->d_name, "..") && FCStringAnsi::Strcmp(Entry->d_name, ".DS_Store"))
				{
                    // Normalize any unicode forms so we match correctly
                    FString NormalizedFilename = UTF8_TO_TCHAR(([[[NSString stringWithUTF8String:Entry->d_name] precomposedStringWithCanonicalMapping] cStringUsingEncoding:NSUTF8StringEncoding]));
                    Result = Visitor.Visit(*(FString(Directory) / NormalizedFilename), Entry->d_type == DT_DIR);
				}
			}
			closedir(Handle);
		}
		return Result;
	}
}

int32 FApplePlatformFile::Stat(const TCHAR* Filename, struct stat* OutFileInfo, ANSICHAR* OutFixedPath, SIZE_T FixedPathSize)
{
	return stat(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), OutFileInfo);
}
