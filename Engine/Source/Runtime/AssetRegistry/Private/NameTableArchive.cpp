// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "AssetRegistryPCH.h"

FNameTableArchiveReader::FNameTableArchiveReader()
	: FArchive()
{
	ArIsLoading = true;
}

bool FNameTableArchiveReader::LoadFile(const TCHAR* Filename, int32 SerializationVersion)
{
	if ( FFileHelper::LoadFileToArray(Reader, Filename, FILEREAD_Silent) )
	{
		int32 MagicNumber = 0;
		*this << MagicNumber;

		if ( MagicNumber == PACKAGE_FILE_TAG )
		{
			int32 VersionNumber = 0;
			*this << VersionNumber;

			if (VersionNumber == SerializationVersion)
			{
				SerializeNameMap();
				return true;
			}
		}
	}

	return false;
}

void FNameTableArchiveReader::SerializeNameMap()
{
	int64 NameOffset = 0;
	*this << NameOffset;

	if( NameOffset > 0 )
	{
		int64 OriginalOffset = Tell();
		Seek( NameOffset );

		int32 NameCount = 0;
		*this << NameCount;

		for ( int32 NameMapIdx = 0; NameMapIdx < NameCount; ++NameMapIdx )
		{
			// Read the name entry from the file.
			FNameEntry NameEntry(ENAME_LinkerConstructor);
			*this << NameEntry;

			NameMap.Add( 
				NameEntry.IsWide() ? 
					FName(ENAME_LinkerConstructor, NameEntry.GetWideName()) : 
					FName(ENAME_LinkerConstructor, NameEntry.GetAnsiName())
				);
		}

		Seek( OriginalOffset );
	}
}

void FNameTableArchiveReader::Serialize( void* V, int64 Length )
{
	Reader.Serialize( V, Length );
}

bool FNameTableArchiveReader::Precache( int64 PrecacheOffset, int64 PrecacheSize )
{
	return Reader.Precache( PrecacheOffset, PrecacheSize );
}

void FNameTableArchiveReader::Seek( int64 InPos )
{
	Reader.Seek( InPos );
}

int64 FNameTableArchiveReader::Tell()
{
	return Reader.Tell();
}

int64 FNameTableArchiveReader::TotalSize()
{
	return Reader.TotalSize();
}

FArchive& FNameTableArchiveReader::operator<<( FName& Name )
{
	NAME_INDEX NameIndex;
	FArchive& Ar = *this;
	Ar << NameIndex;

	if( !NameMap.IsValidIndex(NameIndex) )
	{
		UE_LOG(LogAssetRegistry, Fatal, TEXT("Bad name index %i/%i"), NameIndex, NameMap.Num() );
	}

	// if the name wasn't loaded (because it wasn't valid in this context)
	if (NameMap[NameIndex] == NAME_None)
	{
		int32 TempNumber;
		Ar << TempNumber;
		Name = NAME_None;
	}
	else
	{
		int32 Number;
		Ar << Number;
		// simply create the name from the NameMap's name index and the serialized instance number
		Name = FName((EName)NameMap[NameIndex].GetIndex(), Number);
	}

	return *this;
}



FNameTableArchiveWriter::FNameTableArchiveWriter(int32 SerializationVersion)
	: FArchive()
{
	ArIsSaving = true;

	int32 MagicNumber = PACKAGE_FILE_TAG;
	*this << MagicNumber;

	int32 VersionToWrite = SerializationVersion;
	*this << VersionToWrite;

	// Just write a 0 for the name table offset for now. This will be overwritten later when we are done serializing
	NameOffsetLoc = Tell();
	int64 NameOffset = 0;
	*this << NameOffset;
}

bool FNameTableArchiveWriter::SaveToFile(const TCHAR* Filename)
{
	int64 ActualNameOffset = Tell();
	SerializeNameMap();
	Seek(NameOffsetLoc);
	*this << ActualNameOffset;

	// Save to a temp file first, then move to the destination to avoid corruption
	const FString TempFile = FString(Filename) + TEXT(".tmp");
	if ( FFileHelper::SaveArrayToFile(Writer, *TempFile) )
	{
		if ( IFileManager::Get().Move(Filename, *TempFile) )
		{
			return true;
		}
	}

	return false;
}

void FNameTableArchiveWriter::SerializeNameMap()
{
	int32 NameCount = NameMap.Num();
	*this << NameCount;
	if( NameCount > 0 )
	{
		for ( int32 NameMapIdx = 0; NameMapIdx < NameCount; ++NameMapIdx )
		{
			// Read the name entry
			*this << *const_cast<FNameEntry*>(FName::GetEntry( NameMap[NameMapIdx].GetIndex() ));
		}
	}
}

void FNameTableArchiveWriter::Serialize( void* V, int64 Length )
{
	Writer.Serialize( V, Length );
}

bool FNameTableArchiveWriter::Precache( int64 PrecacheOffset, int64 PrecacheSize )
{
	return Writer.Precache( PrecacheOffset, PrecacheSize );
}

void FNameTableArchiveWriter::Seek( int64 InPos )
{
	Writer.Seek( InPos );
}

int64 FNameTableArchiveWriter::Tell()
{
	return Writer.Tell();
}

int64 FNameTableArchiveWriter::TotalSize()
{
	return Writer.TotalSize();
}

FArchive& FNameTableArchiveWriter::operator<<( FName& Name )
{
	int32* NameIndexPtr = NameMapLookup.Find(Name);
	int32 NameIndex = NameIndexPtr ? *NameIndexPtr : INDEX_NONE;
	if ( NameIndex == INDEX_NONE )
	{
		NameIndex = NameMap.Add(Name);
		NameMapLookup.Add(Name, NameIndex);
	}

	FArchive& Ar = *this;
	Ar << NameIndex;

	if (Name == NAME_None)
	{
		int32 TempNumber = 0;
		Ar << TempNumber;
	}
	else
	{
		int32 Number = Name.GetNumber();
		Ar << Number;
	}

	return *this;
}