// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderCore.h: Shader core module implementation.
=============================================================================*/

#include "ShaderCore.h"
#include "SecureHash.h"
#include "Shader.h"
#include "VertexFactory.h"
#include "ModuleManager.h"

FSHAHash GGlobalShaderMapHash;

void UpdateShaderDevelopmentMode()
{
	// Keep LogShaders verbosity in sync with r.ShaderDevelopmentMode
	// r.ShaderDevelopmentMode==1 results in all LogShaders log messages being displayed
	static const auto CVar0 = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ShaderDevelopmentMode"));
	bool bLogShadersUnsuppressed = UE_LOG_ACTIVE(LogShaders, Log);
	bool bDesiredLogShadersUnsuppressed = CVar0->GetValueOnGameThread() == 1;

	if (bLogShadersUnsuppressed != bDesiredLogShadersUnsuppressed)
	{
		if (bDesiredLogShadersUnsuppressed)
		{
			UE_SET_LOG_VERBOSITY(LogShaders, Log);
		}
		else
		{
			UE_SET_LOG_VERBOSITY(LogShaders, Error);
		}
	}
}

class FShaderCoreModule : public FDefaultModuleImpl
{
public:

	/**
	 * Called right after the module DLL has been loaded and the module object has been created
	 */
	virtual void StartupModule()
	{
		// Create the global shader map hash
		{
			FSHA1 HashState;
			const TCHAR* GlobalShaderString = TEXT("GlobalShaderMap");
			HashState.UpdateWithString(GlobalShaderString, FCString::Strlen(GlobalShaderString));
			HashState.Final();
			HashState.GetHash(&GGlobalShaderMapHash.Hash[0]);
		}

		IConsoleManager::Get().RegisterConsoleVariableSink(FConsoleCommandDelegate::CreateStatic(&UpdateShaderDevelopmentMode));
	}
};

IMPLEMENT_MODULE( FShaderCoreModule, ShaderCore );

//
// Shader stats
//


DEFINE_STAT(STAT_ShaderCompiling_MaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_GlobalShaders);
DEFINE_STAT(STAT_ShaderCompiling_RHI);
DEFINE_STAT(STAT_ShaderCompiling_HashingShaderFiles);
DEFINE_STAT(STAT_ShaderCompiling_LoadingShaderFiles);
DEFINE_STAT(STAT_ShaderCompiling_HLSLTranslation);
DEFINE_STAT(STAT_ShaderCompiling_DDCLoading);
DEFINE_STAT(STAT_ShaderCompiling_MaterialLoading);
DEFINE_STAT(STAT_ShaderCompiling_MaterialCompiling);

DEFINE_STAT(STAT_ShaderCompiling_NumTotalMaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumSpecialMaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumParticleMaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumSkinnedMaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumLitMaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumUnlitMaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumTransparentMaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumOpaqueMaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumMaskedMaterialShaders);


DEFINE_STAT(STAT_Shaders_NumShadersLoaded);
DEFINE_STAT(STAT_Shaders_NumShaderResourcesLoaded);
DEFINE_STAT(STAT_Shaders_NumShaderMaps);
DEFINE_STAT(STAT_Shaders_RTShaderLoadTime);
DEFINE_STAT(STAT_Shaders_NumShadersUsedForRendering);
DEFINE_STAT(STAT_Shaders_TotalRTShaderInitForRenderingTime);
DEFINE_STAT(STAT_Shaders_FrameRTShaderInitForRenderingTime);
DEFINE_STAT(STAT_Shaders_ShaderMemory);
DEFINE_STAT(STAT_Shaders_ShaderResourceMemory);
DEFINE_STAT(STAT_Shaders_ShaderMapMemory);

/** Protects GShaderFileCache from simultaneous access by multiple threads. */
FCriticalSection FileCacheCriticalSection;

/** The shader file cache, used to minimize shader file reads */
TMap<FString, FString> GShaderFileCache;

/** The shader file hash cache, used to minimize loading and hashing shader files */
TMap<FString, FSHAHash> GShaderHashCache;

/** Returns true if debug viewmodes are allowed for the current platform. */
bool AllowDebugViewmodes()
{
	static bool bAllowDebugViewmodesOnConsoles = false;
	static bool bReadFromIni = false;
	if (!bReadFromIni)
	{
		bReadFromIni = true;
		GConfig->GetBool( TEXT("/Script/Engine.Engine"), TEXT("bAllowDebugViewmodesOnConsoles"), bAllowDebugViewmodesOnConsoles, GEngineIni );
	}

	// To use debug viewmodes on consoles, bAllowDebugViewmodesOnConsoles in the engine ini must be set to true, 
	// And EngineDebugMaterials must be in the StartupPackages for the target platform.
	return bAllowDebugViewmodesOnConsoles || (!IsRunningCommandlet() && !FPlatformProperties::RequiresCookedData());
}

bool FShaderParameterMap::FindParameterAllocation(const TCHAR* ParameterName,uint16& OutBufferIndex,uint16& OutBaseIndex,uint16& OutSize) const
{
	const FParameterAllocation* Allocation = ParameterMap.Find(ParameterName);
	if(Allocation)
	{
		OutBufferIndex = Allocation->BufferIndex;
		OutBaseIndex = Allocation->BaseIndex;
		OutSize = Allocation->Size;
		Allocation->bBound = true;
		return true;
	}
	else
	{
		return false;
	}
}

bool FShaderParameterMap::ContainsParameterAllocation(const TCHAR* ParameterName) const
{
	return ParameterMap.Find(ParameterName) != NULL;
}

void FShaderParameterMap::AddParameterAllocation(const TCHAR* ParameterName,uint16 BufferIndex,uint16 BaseIndex,uint16 Size)
{
	FParameterAllocation Allocation;
	Allocation.BufferIndex = BufferIndex;
	Allocation.BaseIndex = BaseIndex;
	Allocation.Size = Size;
	ParameterMap.Add(ParameterName,Allocation);
}

void FShaderCompilerOutput::GenerateOutputHash()
{
	FSHA1 HashState;
	HashState.Update(Code.GetData(), Code.Num() * Code.GetTypeSize());
	ParameterMap.UpdateHash(HashState);
	HashState.Final();
	HashState.GetHash(&OutputHash.Hash[0]);
}


/**
* Add a new entry to the list of shader source files
* Only unique entries which can be loaded are added as well as their #include files
*
* @param ShaderSourceFiles - [out] list of shader source files to add to
* @param ShaderFilename - shader file to add
*/
static void AddShaderSourceFileEntry( TArray<FString>& ShaderSourceFiles, const FString& ShaderFilename)
{
	FString ShaderFilenameBase( FPaths::GetBaseFilename(ShaderFilename) );

	// get the filename for the the vertex factory type
	if( !ShaderSourceFiles.Contains(ShaderFilenameBase) )
	{
		ShaderSourceFiles.Add(ShaderFilenameBase);

		TArray<FString> ShaderIncludes;
		GetShaderIncludes(*ShaderFilenameBase,ShaderIncludes);
		for( int32 IncludeIdx=0; IncludeIdx < ShaderIncludes.Num(); IncludeIdx++ )
		{
			ShaderSourceFiles.AddUnique(ShaderIncludes[IncludeIdx]);
		}
	}
}

/**
* Generate a list of shader source files that engine needs to load
*
* @param ShaderSourceFiles - [out] list of shader source files to add to
*/
static void GetAllShaderSourceFiles( TArray<FString>& ShaderSourceFiles )
{
	// add all shader source files for hashing
	for( TLinkedList<FVertexFactoryType*>::TIterator FactoryIt(FVertexFactoryType::GetTypeList()); FactoryIt; FactoryIt.Next() )
	{
		FVertexFactoryType* VertexFactoryType = *FactoryIt;
		if( VertexFactoryType )
		{
			FString ShaderFilename(VertexFactoryType->GetShaderFilename());
			AddShaderSourceFileEntry(ShaderSourceFiles,ShaderFilename);
		}
	}
	for( TLinkedList<FShaderType*>::TIterator ShaderIt(FShaderType::GetTypeList()); ShaderIt; ShaderIt.Next() )
	{
		FShaderType* ShaderType = *ShaderIt;
		if(ShaderType)
		{
			FString ShaderFilename(ShaderType->GetShaderFilename());
			AddShaderSourceFileEntry(ShaderSourceFiles,ShaderFilename);
		}
	}
	// also always add the MaterialTemplate.usf shader file
	AddShaderSourceFileEntry(ShaderSourceFiles,FString(TEXT("MaterialTemplate")));
	AddShaderSourceFileEntry(ShaderSourceFiles,FString(TEXT("Common")));
	AddShaderSourceFileEntry(ShaderSourceFiles,FString(TEXT("Definitions")));	
}

/**
* Kick off SHA verification for all shader source files
*/
void VerifyShaderSourceFiles()
{
	if (!FPlatformProperties::RequiresCookedData())
	{
		// get the list of shader files that can be used
		TArray<FString> ShaderSourceFiles;
		GetAllShaderSourceFiles(ShaderSourceFiles);
		for( int32 ShaderFileIdx=0; ShaderFileIdx < ShaderSourceFiles.Num(); ShaderFileIdx++ )
		{
			FString FileContents;
			// load each shader source file. This will cache the shader source data after it has been verified
			LoadShaderSourceFileChecked(*ShaderSourceFiles[ShaderFileIdx], FileContents);
		}
	}
}

FString GetRelativeShaderFilename(const FString& InFilename)
{
	FString ShaderDir = FString(FPlatformProcess::ShaderDir());
	ShaderDir.ReplaceInline(TEXT("\\"), TEXT("/"));
	int32 CharIndex = ShaderDir.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd, ShaderDir.Len() - 1);
	if (CharIndex != INDEX_NONE)
	{
		ShaderDir = ShaderDir.Right(ShaderDir.Len() - CharIndex);
	}

	FString RelativeFilename = InFilename.Replace(TEXT("\\"), TEXT("/"));
	RelativeFilename = IFileManager::Get().ConvertToRelativePath(*RelativeFilename);
	CharIndex = RelativeFilename.Find(ShaderDir);
	if (CharIndex != INDEX_NONE)
	{
		CharIndex += ShaderDir.Len();
		if (RelativeFilename.GetCharArray()[CharIndex] == TEXT('/'))
		{
			CharIndex++;
		}
		if (RelativeFilename.Contains(TEXT("WorkingDirectory")) )
		{
			const int32 NumDirsToSkip = 3;
			int32 NumDirsSkipped = 0;
			int32 NewCharIndex = CharIndex;

			do 
			{
				NewCharIndex = RelativeFilename.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, CharIndex);
				CharIndex = (NewCharIndex == INDEX_NONE) ? CharIndex : NewCharIndex + 1;
			}
			while (NewCharIndex != INDEX_NONE && ++NumDirsSkipped < NumDirsToSkip);
		}
		RelativeFilename = RelativeFilename.Mid(CharIndex, RelativeFilename.Len() - CharIndex);
	}
	return RelativeFilename;
}

bool LoadShaderSourceFile(const TCHAR* Filename, FString& OutFileContents)
{
	// it's not expected that cooked platforms get here, but if they do, this is the final out
	if (FPlatformProperties::RequiresCookedData())
	{
		return false;
	}

	bool bResult = false;

	STAT(double ShaderFileLoadingTime = 0);
	{
		SCOPE_SECONDS_COUNTER(ShaderFileLoadingTime);

		// Load the specified file from the System/Shaders directory.
		FString ShaderFilename = FPaths::Combine(FPlatformProcess::BaseDir(), FPlatformProcess::ShaderDir(), Filename);

		if (FPaths::GetExtension(ShaderFilename) == TEXT(""))
		{
			ShaderFilename += TEXT(".usf");
		}
		// Protect GShaderFileCache from simultaneous access by multiple threads
		FScopeLock ScopeLock(&FileCacheCriticalSection);

		FString* CachedFile = GShaderFileCache.Find(ShaderFilename);

		//if this file has already been loaded and cached, use that
		if (CachedFile)
		{
			OutFileContents = *CachedFile;
			bResult = true;
		}
		else
		{
			// verify SHA hash of shader files on load. missing entries trigger an error
			if (FFileHelper::LoadFileToString(OutFileContents, *ShaderFilename, FFileHelper::EHashOptions::EnableVerify|FFileHelper::EHashOptions::ErrorMissingHash) )
			{
				//update the shader file cache
				GShaderFileCache.Add(ShaderFilename, *OutFileContents);
				bResult = true;
			}
		}
	}
	INC_FLOAT_STAT_BY(STAT_ShaderCompiling_LoadingShaderFiles,(float)ShaderFileLoadingTime);

	return bResult;
}

void LoadShaderSourceFileChecked(const TCHAR* Filename, FString& OutFileContents)
{
	if (!LoadShaderSourceFile(Filename, OutFileContents))
	{
		UE_LOG(LogShaders, Fatal,TEXT("Couldn't load shader file \'%s\'"),Filename);
	}
}

/**
 * Walks InStr until we find either an end-of-line or TargetChar.
 */
const TCHAR* SkipToCharOnCurrentLine(const TCHAR* InStr, TCHAR TargetChar)
{
	const TCHAR* Str = InStr;
	if (Str)
	{
		while (*Str && *Str != TargetChar && *Str != TEXT('\n'))
		{
			++Str;
		}
		if (*Str != TargetChar)
		{
			Str = NULL;
		}
	}
	return Str;
}

/**
 * Recursively populates IncludeFilenames with the unique include filenames found in the shader file named Filename.
 */
void GetShaderIncludes(const TCHAR* Filename, TArray<FString>& IncludeFilenames, uint32 DepthLimit)
{
	FString FileContents;
	LoadShaderSourceFileChecked(Filename, FileContents);

	//avoid an infinite loop with a 0 length string
	if (FileContents.Len() > 0)
	{
		//find the first include directive
		const TCHAR* IncludeBegin = FCString::Strstr(*FileContents, TEXT("#include "));

		uint32 SearchCount = 0;
		const uint32 MaxSearchCount = 20;
		//keep searching for includes as long as we are finding new ones and haven't exceeded the fixed limit
		while (IncludeBegin != NULL && SearchCount < MaxSearchCount && DepthLimit > 0)
		{
			//find the first double quotation after the include directive
			const TCHAR* IncludeFilenameBegin = SkipToCharOnCurrentLine(IncludeBegin, TEXT('\"'));

			if (IncludeFilenameBegin)
			{
				//find the trailing double quotation
				const TCHAR* IncludeFilenameEnd = SkipToCharOnCurrentLine(IncludeFilenameBegin + 1, TEXT('\"'));

				if (IncludeFilenameEnd)
				{
					//construct a string between the double quotations
					FString ExtractedIncludeFilename(FString((int32)(IncludeFilenameEnd - IncludeFilenameBegin - 1), IncludeFilenameBegin + 1));

					//CRC the template, not the filled out version so that this shader's CRC will be independent of which material references it.
					if (ExtractedIncludeFilename == TEXT("Material.usf"))
					{
						ExtractedIncludeFilename = TEXT("MaterialTemplate.usf");
					}

					// Ignore uniform buffer and vertex factory includes
					bool bIgnoreInclude = ExtractedIncludeFilename == TEXT("VertexFactory.usf")
						|| ExtractedIncludeFilename == TEXT("GeneratedUniformBuffers.usf")
						|| ExtractedIncludeFilename.StartsWith(TEXT("UniformBuffers/"));
			
					// Some headers aren't required to be found (platforms that the user doesn't have access to)
					// @todo: Is there some way to generalize this"
					const bool bIsOptionalInclude = (ExtractedIncludeFilename == TEXT("PS4/PS4Common.usf") || ExtractedIncludeFilename == TEXT("PS4/PostProcessHMDMorpheus.usf"));
					// ignore the header if it's optional and doesn't exist
					if (bIsOptionalInclude)
					{
						FString ShaderFilename = FPaths::Combine(FPlatformProcess::BaseDir(), FPlatformProcess::ShaderDir(), *ExtractedIncludeFilename);
						if (!FPaths::FileExists(ShaderFilename))
						{
							bIgnoreInclude = true;
						}
					}

					//vertex factories need to be handled separately
					if (!bIgnoreInclude)
					{
						GetShaderIncludes(*ExtractedIncludeFilename, IncludeFilenames, DepthLimit - 1);
						// maintain subdirectory info, but strip the extension
						ExtractedIncludeFilename = FPaths::GetBaseFilename(ExtractedIncludeFilename, false);
						IncludeFilenames.AddUnique(ExtractedIncludeFilename);
					}
				}
			}

			// Skip to the end of the line.
			IncludeBegin = SkipToCharOnCurrentLine(IncludeBegin, TEXT('\n'));
		
			//find the next include directive
			if (IncludeBegin && *IncludeBegin != 0)
			{
				IncludeBegin = FCString::Strstr(IncludeBegin + 1, TEXT("#include "));
			}
			SearchCount++;
		}
	}
}

/**
 * Calculates a Hash for the given filename and its includes if it does not already exist in the Hash cache.
 * @param Filename - shader file to Hash
 */
const FSHAHash& GetShaderFileHash(const TCHAR* Filename)
{
	// Make sure we are only accessing GShaderHashCache from one thread
	check(IsInGameThread());
	STAT(double HashTime = 0);
	{
		SCOPE_SECONDS_COUNTER(HashTime);

		FSHAHash* CachedHash = GShaderHashCache.Find(Filename);

		// If a hash for this filename has been cached, use that
		if (CachedHash)
		{
			return *CachedHash;
		}

		// Get the list of includes this file contains
		TArray<FString> IncludeFilenames;
		GetShaderIncludes(Filename, IncludeFilenames);

		FSHA1 HashState;
		for (int32 IncludeIndex = 0; IncludeIndex < IncludeFilenames.Num(); IncludeIndex++)
		{
			// Load the include file and hash it
			FString IncludeFileContents;
			LoadShaderSourceFileChecked(*IncludeFilenames[IncludeIndex], IncludeFileContents);
			HashState.UpdateWithString(*IncludeFileContents, IncludeFileContents.Len());
		}

		// Load the source file and hash it
		FString FileContents;
		LoadShaderSourceFileChecked(Filename, FileContents);
		HashState.UpdateWithString(*FileContents, FileContents.Len());
		HashState.Final();

		// Update the hash cache
		FSHAHash& NewHash = GShaderHashCache.Add(*FString(Filename), FSHAHash());
		HashState.GetHash(&NewHash.Hash[0]);
		return NewHash;
	}
	INC_FLOAT_STAT_BY(STAT_ShaderCompiling_HashingShaderFiles,(float)HashTime);
}

void BuildShaderFileToUniformBufferMap(TMap<FString, TArray<const TCHAR*> >& ShaderFileToUniformBufferVariables)
{
	if (!FPlatformProperties::RequiresCookedData())
	{
		TArray<FString> ShaderSourceFiles;
		GetAllShaderSourceFiles(ShaderSourceFiles);

		for (int32 FileIndex = 0; FileIndex < ShaderSourceFiles.Num(); FileIndex++)
		{
			FString ShaderFileContents;

			LoadShaderSourceFileChecked(*ShaderSourceFiles[FileIndex], ShaderFileContents);

			TArray<const TCHAR*>& ReferencedUniformBuffers = ShaderFileToUniformBufferVariables.FindOrAdd(ShaderSourceFiles[FileIndex]);

			for (TLinkedList<FUniformBufferStruct*>::TIterator StructIt(FUniformBufferStruct::GetStructList()); StructIt; StructIt.Next())
			{
				// Searching for the uniform buffer shader variable being accessed with '.'
				if (ShaderFileContents.Contains(FString(StructIt->GetShaderVariableName()) + TEXT(".")))
				{
					ReferencedUniformBuffers.AddUnique(StructIt->GetShaderVariableName());
				}
			}
		}
	}
}

void InitializeShaderTypes()
{
	TMap<FString, TArray<const TCHAR*> > ShaderFileToUniformBufferVariables;
	BuildShaderFileToUniformBufferMap(ShaderFileToUniformBufferVariables);

	FShaderType::Initialize(ShaderFileToUniformBufferVariables);
	FVertexFactoryType::Initialize(ShaderFileToUniformBufferVariables);
}

void UninitializeShaderTypes()
{
	FShaderType::Uninitialize();
	FVertexFactoryType::Uninitialize();
}

/**
 * Flushes the shader file and CRC cache, and regenerates the binary shader files if necessary.
 * Allows shader source files to be re-read properly even if they've been modified since startup.
 */
void FlushShaderFileCache()
{
	GShaderHashCache.Empty();
	GShaderFileCache.Empty();

	if (!FPlatformProperties::RequiresCookedData())
	{
		TMap<FString, TArray<const TCHAR*> > ShaderFileToUniformBufferVariables;
		BuildShaderFileToUniformBufferMap(ShaderFileToUniformBufferVariables);

		for(TLinkedList<FShaderType*>::TIterator It(FShaderType::GetTypeList()); It; It.Next())
		{
			It->FlushShaderFileCache(ShaderFileToUniformBufferVariables);
		}

		for(TLinkedList<FVertexFactoryType*>::TIterator It(FVertexFactoryType::GetTypeList()); It; It.Next())
		{
			It->FlushShaderFileCache(ShaderFileToUniformBufferVariables);
		}
	}
}

void GenerateReferencedUniformBuffers(
	const TCHAR* SourceFilename, 
	const TCHAR* ShaderTypeName, 
	const TMap<FString, TArray<const TCHAR*> >& ShaderFileToUniformBufferVariables,
	TMap<const TCHAR*,FCachedUniformBufferDeclaration>& UniformBufferEntries)
{
	TArray<FString> FilesToSearch;
	GetShaderIncludes(SourceFilename, FilesToSearch);
	FilesToSearch.Add(SourceFilename);

	for (int32 FileIndex = 0; FileIndex < FilesToSearch.Num(); FileIndex++)
	{
		const TArray<const TCHAR*>& FoundUniformBufferVariables = ShaderFileToUniformBufferVariables.FindChecked(FilesToSearch[FileIndex]);

		for (int32 VariableIndex = 0; VariableIndex < FoundUniformBufferVariables.Num(); VariableIndex++)
		{
			UniformBufferEntries.Add(FoundUniformBufferVariables[VariableIndex], FCachedUniformBufferDeclaration());
		}
	}
}

void SerializeUniformBufferInfo(FShaderSaveArchive& Ar, const TMap<const TCHAR*,FCachedUniformBufferDeclaration>& UniformBufferEntries)
{
	for (TMap<const TCHAR*,FCachedUniformBufferDeclaration>::TConstIterator It(UniformBufferEntries); It; ++It)
	{
		for (TLinkedList<FUniformBufferStruct*>::TIterator StructIt(FUniformBufferStruct::GetStructList()); StructIt; StructIt.Next())
		{
			if (It.Key() == StructIt->GetShaderVariableName())
			{
				// Serialize information about the struct layout so we can detect when it changes
				const FUniformBufferStruct& Struct = **StructIt;
				const TArray<FUniformBufferStruct::FMember>& Members = Struct.GetMembers();

				int32 NumMembers = Members.Num();
				// Serializing with NULL so that FShaderSaveArchive will record the length without causing an actual data serialization
				Ar.Serialize(NULL, NumMembers);

				for (int32 MemberIndex = 0; MemberIndex < Members.Num(); MemberIndex++)
				{
					const FUniformBufferStruct::FMember& Member = Members[MemberIndex];

					// Note: Only comparing number of floats used by each member and type, so this can be tricked (eg. swapping two equal size and type members)
					int32 MemberSize = Member.GetNumColumns() * Member.GetNumRows();
					Ar.Serialize(NULL, MemberSize);
					int32 MemberType = (int32)Member.GetBaseType();
					Ar.Serialize(NULL, MemberType);
				}
				break;
			}
		}
	}
}
