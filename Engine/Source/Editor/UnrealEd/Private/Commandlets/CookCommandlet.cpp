// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CookCommandlet.cpp: Commandlet for cooking content
=============================================================================*/

#include "UnrealEd.h"

#include "PackageHelperFunctions.h"
#include "DerivedDataCacheInterface.h"
#include "ISourceControlModule.h"
#include "GlobalShader.h"
#include "TargetPlatform.h"
#include "IConsoleManager.h"
#include "Developer/PackageDependencyInfo/Public/PackageDependencyInfo.h"
#include "IPlatformFileSandboxWrapper.h"
#include "Messaging.h"
#include "NetworkFileSystem.h"
#include "AssetRegistryModule.h"
#include "UnrealEdMessages.h"
#include "GameDelegates.h"
#include "ChunkManifestGenerator.h"

DEFINE_LOG_CATEGORY_STATIC(LogCookCommandlet, Log, All);


/** Helper to pass a recompile request to game thread */
struct FRecompileRequest
{
	FShaderRecompileData RecompileData;
	bool bComplete;
};

/* Static functions
 *****************************************************************************/

static FString GetPackageFilename( UPackage* Package )
{
	FString Filename;
	if (FPackageName::DoesPackageExist(Package->GetName(), NULL, &Filename))
	{
		Filename = FPaths::ConvertRelativePathToFull(Filename);
	}
	return Filename;
}


/* UCookCommandlet structors
 *****************************************************************************/

UCookCommandlet::UCookCommandlet( const class FPostConstructInitializeProperties& PCIP )
	: Super(PCIP)
{

	LogToConsole = false;
}


/* UCookCommandlet interface
 *****************************************************************************/

bool UCookCommandlet::CookOnTheFly( FGuid InstanceId, int32 Timeout, bool bForceClose )
{
	UCookOnTheFlyServer *CookOnTheFlyServer = ConstructObject<UCookOnTheFlyServer>( UCookOnTheFlyServer::StaticClass() );

	struct FScopeRootObject
	{
		UObject *Object;
		FScopeRootObject( UObject *InObject ) : Object( InObject )
		{
			Object->AddToRoot();
		}

		~FScopeRootObject()
		{
			Object->RemoveFromRoot();
		}
	};

	// make sure that the cookonthefly server doesn't get cleaned up while we are garbage collecting below :)
	FScopeRootObject S(CookOnTheFlyServer);

	CookOnTheFlyServer->Initialize( bCompressed, bIterativeCooking, bSkipEditorContent );

	bool BindAnyPort = InstanceId.IsValid();

	if ( CookOnTheFlyServer->StartNetworkFileServer(BindAnyPort) == false )
	{
		return false;
	}

	if ( InstanceId.IsValid() )
	{
		if ( CookOnTheFlyServer->BroadcastFileserverPresence(InstanceId) == false )
		{
			return false;
		}
	}

	// Garbage collection should happen when either
	//	1. We have cooked a map
	//	2. We have cooked non-map packages and...
	//		a. we have accumulated 50 of these since the last GC.
	//		b. we have been idle for 20 seconds.
	bool bShouldGC = true;

	// megamoth
	uint32 NonMapPackageCountSinceLastGC = 0;
	
	const int32 PackagesPerGC = 50;
	
	const double IdleTimeToGC = 20.0;
	double LastCookActionTime = FPlatformTime::Seconds();

	FDateTime LastConnectionTime = FDateTime::UtcNow();
	bool bHadConnection = false;

	bool bCookedAMapSinceLastGC = false;
	while (!GIsRequestingExit)
	{
		uint32 TickResults = 0;
		static const float CookOnTheSideTimeSlice = 10.0f;
		TickResults = CookOnTheFlyServer->TickCookOnTheSide(CookOnTheSideTimeSlice, NonMapPackageCountSinceLastGC);

		bCookedAMapSinceLastGC |= TickResults & UCookOnTheFlyServer::COSR_CookedMap;
		if ( TickResults & (UCookOnTheFlyServer::COSR_CookedMap | UCookOnTheFlyServer::COSR_CookedPackage))
		{
			LastCookActionTime = FPlatformTime::Seconds();
		}


		while ( (CookOnTheFlyServer->HasCookRequests() == false) && !GIsRequestingExit)
		{
				
			{
				if (NonMapPackageCountSinceLastGC > 0)
				{
					// We should GC if we have packages to collect and we've been idle for some time.
					bShouldGC = (NonMapPackageCountSinceLastGC > PackagesPerGC) || 
						((FPlatformTime::Seconds() - LastCookActionTime) >= IdleTimeToGC);
				}

				// delay the gc until we process some unsolicited packages
				if ( bCookedAMapSinceLastGC && (CookOnTheFlyServer->HasUnsolicitedCookRequests() == false) )
				{
					UE_LOG( LogCookCommandlet, Display, TEXT("Delaying map gc because we have unsolicited cook requests") );
					bShouldGC |= bCookedAMapSinceLastGC;
				}

				if (bShouldGC)
				{
					bShouldGC = false;
					bCookedAMapSinceLastGC = false;
					NonMapPackageCountSinceLastGC = 0;

					UE_LOG(LogCookCommandlet, Display, TEXT("GC..."));

					CollectGarbage( RF_Native );
				}
				else
				{
					CookOnTheFlyServer->TickRecompileShaderRequests();

					FPlatformProcess::Sleep(0.0f);
				}
			}

			// update task graph
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);

			// execute deferred commands
			for (int32 DeferredCommandsIndex = 0; DeferredCommandsIndex<GEngine->DeferredCommands.Num(); ++DeferredCommandsIndex)
			{
				GEngine->Exec( GWorld, *GEngine->DeferredCommands[DeferredCommandsIndex], *GLog);
			}

			GEngine->DeferredCommands.Empty();

			// handle server timeout
			if (InstanceId.IsValid() || bForceClose)
			{
				if (CookOnTheFlyServer->NumConnections() > 0)
				{
					bHadConnection = true;
					LastConnectionTime = FDateTime::UtcNow();
				}

				if ((FDateTime::UtcNow() - LastConnectionTime) > FTimespan::FromSeconds(Timeout))
				{
					uint32 Result = FMessageDialog::Open(EAppMsgType::YesNo, NSLOCTEXT("UnrealEd", "FileServerIdle", "The file server did not receive any connections in the past 3 minutes. Would you like to shut it down?"));

					if (Result == EAppReturnType::No && !bForceClose)
					{
						LastConnectionTime = FDateTime::UtcNow();
					}
					else
					{
						GIsRequestingExit = true;
					}
				}
				else if (bHadConnection && (CookOnTheFlyServer->NumConnections() == 0) && bForceClose) // immediately shut down if we previously had a connection and now do not
				{
					GIsRequestingExit = true;
				}
			}
		}
	}

	CookOnTheFlyServer->EndNetworkFileServer();
	return true;
}


FString UCookCommandlet::GetOutputDirectory( const FString& PlatformName ) const
{
	// Use SandboxFile to get the correct sandbox directory.
	FString OutputDirectory = SandboxFile->GetSandboxDirectory();

	return OutputDirectory.Replace(TEXT("[Platform]"), *PlatformName);
}


bool UCookCommandlet::GetPackageTimestamp( const FString& InFilename, FDateTime& OutDateTime )
{
	FPackageDependencyInfoModule& PDInfoModule = FModuleManager::LoadModuleChecked<FPackageDependencyInfoModule>("PackageDependencyInfo");
	FDateTime DependentTime;

	if (PDInfoModule.DeterminePackageDependentTimeStamp(*InFilename, DependentTime) == true)
	{
		OutDateTime = DependentTime;

		return true;
	}

	return false;
}

bool UCookCommandlet::ShouldCook(const FString& InFileName, const FString &InPlatformName)
{
	bool bDoCook = false;


	FString PkgFile;
	FString PkgFilename;
	FDateTime DependentTimeStamp = FDateTime::MinValue();

	if (bIterativeCooking && FPackageName::DoesPackageExist(InFileName, NULL, &PkgFile))
	{
		PkgFilename = PkgFile;

		if (GetPackageTimestamp(FPaths::GetBaseFilename(PkgFilename, false), DependentTimeStamp) == false)
		{
			UE_LOG(LogCookCommandlet, Display, TEXT("Failed to find dependency timestamp for: %s"), *PkgFilename);
		}
	}

	// Use SandboxFile to do path conversion to properly handle sandbox paths (outside of standard paths in particular).
	PkgFilename = SandboxFile->ConvertToAbsolutePathForExternalAppForWrite(*PkgFilename);

	ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();

	static const TArray<ITargetPlatform*> &ActiveTargetPlatforms = TPM.GetActiveTargetPlatforms();

	TArray<ITargetPlatform*> Platforms;

	if ( InPlatformName.Len() > 0 )
	{
		Platforms.Add( TPM.FindTargetPlatform( InPlatformName ) );
	}
	else 
	{
		Platforms = ActiveTargetPlatforms;
	}

	for (int32 Index = 0; Index < Platforms.Num() && !bDoCook; Index++)
	{
		ITargetPlatform* Target = Platforms[Index];
		FString PlatFilename = PkgFilename.Replace(TEXT("[Platform]"), *Target->PlatformName());

		// If we are not iterative cooking, then cook the package
		bool bCookPackage = (bIterativeCooking == false);

		if (bCookPackage == false)
		{
			// If the cooked package doesn't exist, or if the cooked is older than the dependent, re-cook it
			FDateTime CookedTimeStamp = IFileManager::Get().GetTimeStamp(*PlatFilename);
			int32 CookedTimespanSeconds = (CookedTimeStamp - DependentTimeStamp).GetTotalSeconds();
			bCookPackage = (CookedTimeStamp == FDateTime::MinValue()) || (CookedTimespanSeconds < 0);
		}
		bDoCook |= bCookPackage;
	}

	return bDoCook;
}


bool UCookCommandlet::SaveCookedPackage( UPackage* Package, uint32 SaveFlags, bool& bOutWasUpToDate ) 
{
	TArray<FString> TargetPlatformNames; 
	return SaveCookedPackage( Package, SaveFlags, bOutWasUpToDate, TargetPlatformNames );
}

bool UCookCommandlet::SaveCookedPackage( UPackage* Package, uint32 SaveFlags, bool& bOutWasUpToDate, TArray<FString> &TargetPlatformNames )
{
	bool bSavedCorrectly = true;

	FString Filename(GetPackageFilename(Package));

	if (Filename.Len())
	{
		FString PkgFilename;
		FDateTime DependentTimeStamp = FDateTime::MinValue();

		// We always want to use the dependent time stamp when saving a cooked package...
		// Iterative or not!
		FString PkgFile;
		FString Name = Package->GetPathName();

		if (bIterativeCooking && FPackageName::DoesPackageExist(Name, NULL, &PkgFile))
		{
			PkgFilename = PkgFile;

			if (GetPackageTimestamp(FPaths::GetBaseFilename(PkgFilename, false), DependentTimeStamp) == false)
			{
				UE_LOG(LogCookCommandlet, Display, TEXT("Failed to find dependency timestamp for: %s"), *PkgFilename);
			}
		}

		// Use SandboxFile to do path conversion to properly handle sandbox paths (outside of standard paths in particular).
		Filename = SandboxFile->ConvertToAbsolutePathForExternalAppForWrite(*Filename);

		uint32 OriginalPackageFlags = Package->PackageFlags;
 		UWorld* World = NULL;
 		EObjectFlags Flags = RF_NoFlags;
		bool bPackageFullyLoaded = false;

		if (bCompressed)
		{
			Package->PackageFlags |= PKG_StoreCompressed;
		}

		ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();

		static TArray<ITargetPlatform*> ActiveStartupPlatforms = TPM.GetActiveTargetPlatforms();

		TArray<ITargetPlatform*> Platforms;

		if ( TargetPlatformNames.Num() )
		{
			for ( int Index = 0; Index < TargetPlatformNames.Num(); ++Index )
			{
				const FString &TargetPlatformName = TargetPlatformNames[Index];


				const TArray<ITargetPlatform*>& TargetPlatforms = TPM.GetTargetPlatforms();	


				for ( int Index = 0; Index < TargetPlatforms.Num(); ++Index )
				{
					ITargetPlatform *TargetPlatform = TargetPlatforms[ Index ];
					if ( TargetPlatform->PlatformName() == TargetPlatformName )
					{
						Platforms.Add( TargetPlatform );
					}
				}
			}
		}
		else
		{
			Platforms = ActiveStartupPlatforms;

			for ( int Index = 0; Index < Platforms.Num(); ++Index )
			{
				TargetPlatformNames.Add( Platforms[Index]->PlatformName() );
			}
		}
		

		for (int32 Index = 0; Index < Platforms.Num(); Index++)
		{
			ITargetPlatform* Target = Platforms[Index];
			FString PlatFilename = Filename.Replace(TEXT("[Platform]"), *Target->PlatformName());

			// If we are not iterative cooking, then cook the package
			bool bCookPackage = (bIterativeCooking == false);

			if (bCookPackage == false)
			{
				// If the cooked package doesn't exist, or if the cooked is older than the dependent, re-cook it
				FDateTime CookedTimeStamp = IFileManager::Get().GetTimeStamp(*PlatFilename);
				int32 CookedTimespanSeconds = (CookedTimeStamp - DependentTimeStamp).GetTotalSeconds();
				bCookPackage = (CookedTimeStamp == FDateTime::MinValue()) || (CookedTimespanSeconds < 0);
			}

			// don't save Editor resources from the Engine if the target doesn't have editoronly data
			if (bSkipEditorContent && Name.StartsWith(TEXT("/Engine/Editor")) && !Target->HasEditorOnlyData())
			{
				bCookPackage = false;
			}

			if (bCookPackage == true)
			{
				if (bPackageFullyLoaded == false)
				{
					Package->FullyLoad();
					if (!Package->IsFullyLoaded())
					{
						UE_LOG(LogCookCommandlet, Warning, TEXT("Package %s supposed to be fully loaded but isn't. RF_WasLoaded is %s"), 
							*Package->GetName(), Package->HasAnyFlags(RF_WasLoaded) ? TEXT("set") : TEXT("not set"));
					}
					bPackageFullyLoaded = true;

					// If fully loading has caused a blueprint to be regenerated, make sure we eliminate all meta data outside the package
					UMetaData* MetaData = Package->GetMetaData();
					MetaData->RemoveMetaDataOutsidePackage();

					// look for a world object in the package (if there is one, there's a map)
					World = UWorld::FindWorldInPackage(Package);
					Flags = World ? RF_NoFlags : RF_Standalone;
				}

				UE_LOG(LogCookCommandlet, Display, TEXT("Cooking %s -> %s"), *Package->GetName(), *PlatFilename);

				bool bSwap = (!Target->IsLittleEndian()) ^ (!PLATFORM_LITTLE_ENDIAN);

				if (!Target->HasEditorOnlyData())
				{
					Package->PackageFlags |= PKG_FilterEditorOnly;
				}
				else
				{
					Package->PackageFlags &= ~PKG_FilterEditorOnly;
				}

				if (World)
				{
					World->PersistentLevel->OwningWorld = World;
					if ( !World->bIsWorldInitialized)
					{
						// we need to initialize the world - at least need physics scene since BP construction script runs during cooking, otherwise trace won't work
						World->InitWorld(UWorld::InitializationValues().RequiresHitProxies(false).ShouldSimulatePhysics(false).EnableTraceCollision(false).CreateNavigation(false).AllowAudioPlayback(false).CreatePhysicsScene(true).CreateWorldComposition(false));
					}
				}

				const FString FullFilename = FPaths::ConvertRelativePathToFull( PlatFilename );
				if( FullFilename.Len() >= PLATFORM_MAX_FILEPATH_LENGTH )
				{
					UE_LOG( LogCookCommandlet, Error, TEXT( "Couldn't save package, filename is too long :%s" ), *PlatFilename );
					bSavedCorrectly = false;
				}
				else
				{
					bSavedCorrectly &= GEditor->SavePackage( Package, World, Flags, *PlatFilename, GError, NULL, bSwap, false, SaveFlags, Target, FDateTime::MinValue() );
				}

				
				bOutWasUpToDate = false;
			}
			else
			{
				UE_LOG(LogCookCommandlet, Display, TEXT("Up to date: %s"), *PlatFilename);

				bOutWasUpToDate = true;
			}
		}

		Package->PackageFlags = OriginalPackageFlags;
	}

	// return success
	return bSavedCorrectly;
}

void UCookCommandlet::MaybeMarkPackageAsAlreadyLoaded(UPackage *Package)
{
	FString Name = Package->GetName();
	if (PackagesToNotReload.Contains(Name))
	{
		UE_LOG(LogCookCommandlet, Verbose, TEXT("Marking %s already loaded."), *Name);
		Package->PackageFlags |= PKG_ReloadingForCooker;
	}
}

/* UCommandlet interface
 *****************************************************************************/

int32 UCookCommandlet::Main(const FString& CmdLineParams)
{
	Params = CmdLineParams;
	ParseCommandLine(*Params, Tokens, Switches);

	bCookOnTheFly = Switches.Contains(TEXT("COOKONTHEFLY"));   // Prototype cook-on-the-fly server
	bCookAll = Switches.Contains(TEXT("COOKALL"));   // Cook everything
	bLeakTest = Switches.Contains(TEXT("LEAKTEST"));   // Test for UObject leaks
	bUnversioned = Switches.Contains(TEXT("UNVERSIONED"));   // Save all cooked packages without versions. These are then assumed to be current version on load. This is dangerous but results in smaller patch sizes.
	bGenerateStreamingInstallManifests = Switches.Contains(TEXT("MANIFESTS"));   // Generate manifests for building streaming install packages
	bCompressed = Switches.Contains(TEXT("COMPRESSED"));
	bIterativeCooking = Switches.Contains(TEXT("ITERATE"));
	bSkipEditorContent = Switches.Contains(TEXT("SKIPEDITORCONTENT")); // This won't save out any packages in Engine/COntent/Editor*

	if (bLeakTest)
	{
		for (FObjectIterator It; It; ++It)
		{
			LastGCItems.Add(FWeakObjectPtr(*It));
		}
	}


	if ( bCookOnTheFly )
	{
		// parse instance identifier
		FString InstanceIdString;
		bool bForceClose = Switches.Contains(TEXT("FORCECLOSE"));

		FGuid InstanceId;
		if (FParse::Value(*Params, TEXT("InstanceId="), InstanceIdString))
		{
			if (!FGuid::Parse(InstanceIdString, InstanceId))
			{
				UE_LOG(LogCookCommandlet, Warning, TEXT("Invalid InstanceId on command line: %s"), *InstanceIdString);
			}
		}

		int32 Timeout = 180;
		if (!FParse::Value(*Params, TEXT("timeout="), Timeout))
		{
			Timeout = 180;
		}

		CookOnTheFly( InstanceId, Timeout, bForceClose);
	}
	else
	{
		ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
		const TArray<ITargetPlatform*>& Platforms = TPM.GetActiveTargetPlatforms();
	
		// Local sandbox file wrapper. This will be used to handle path conversions,
		// but will not be used to actually write/read files so we can safely
		// use [Platform] token in the sandbox directory name and then replace it
		// with the actual platform name.
		SandboxFile = new FSandboxPlatformFile(false);
	
		// Output directory override.	
		FString OutputDirectory = GetOutputDirectoryOverride();

		// Use SandboxFile to do path conversion to properly handle sandbox paths (outside of standard paths in particular).
		SandboxFile->Initialize(&FPlatformFileManager::Get().GetPlatformFile(), *FString::Printf(TEXT("-sandbox=%s"), *OutputDirectory));

		CleanSandbox(Platforms);

		// allow the game to fill out the asset registry, as well as get a list of objects to always cook
		TArray<FString> FilesInPath;
		FGameDelegates::Get().GetCookModificationDelegate().ExecuteIfBound(FilesInPath);

		// always generate the asset registry before starting to cook, for either method
		GenerateAssetRegistry(Platforms);

		Cook(Platforms, FilesInPath);

	}
	
	return 0;
}


/* UCookCommandlet implementation
 *****************************************************************************/

FString UCookCommandlet::GetOutputDirectoryOverride() const
{
	FString OutputDirectory;
	// Output directory override.	
	if (!FParse::Value(*Params, TEXT("Output="), OutputDirectory))
	{
		OutputDirectory = TEXT("Cooked-[Platform]");
	}
	else if (!OutputDirectory.Contains(TEXT("[Platform]"), ESearchCase::IgnoreCase, ESearchDir::FromEnd) )
	{
		// Output directory needs to contain [Platform] token to be able to cook for multiple targets.
		OutputDirectory += TEXT("/Cooked-[Platform]");
	}
	FPaths::NormalizeDirectoryName(OutputDirectory);

	return OutputDirectory;
}

void UCookCommandlet::CleanSandbox(const TArray<ITargetPlatform*>& Platforms)
{
	double SandboxCleanTime = 0.0;
	{
		SCOPE_SECONDS_COUNTER(SandboxCleanTime);

		if (bIterativeCooking == false)
		{
			// for now we are going to wipe the cooked directory
			for (int32 Index = 0; Index < Platforms.Num(); Index++)
			{
				ITargetPlatform* Target = Platforms[Index];
				FString SandboxDirectory = GetOutputDirectory(Target->PlatformName());
				IFileManager::Get().DeleteDirectory(*SandboxDirectory, false, true);
			}
		}
		else
		{
			FPackageDependencyInfoModule& PDInfoModule = FModuleManager::LoadModuleChecked<FPackageDependencyInfoModule>("PackageDependencyInfo");
			
			// list of directories to skip
			TArray<FString> DirectoriesToSkip;
			TArray<FString> DirectoriesToNotRecurse;

			// See what files are out of date in the sandbox folder
			for (int32 Index = 0; Index < Platforms.Num(); Index++)
			{
				ITargetPlatform* Target = Platforms[Index];
				FString SandboxDirectory = GetOutputDirectory(Target->PlatformName());

				// use the timestamp grabbing visitor
				IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
				FLocalTimestampDirectoryVisitor Visitor(PlatformFile, DirectoriesToSkip, DirectoriesToNotRecurse, false);
			
				PlatformFile.IterateDirectory(*SandboxDirectory, Visitor);

				for (TMap<FString, FDateTime>::TIterator TimestampIt(Visitor.FileTimes); TimestampIt; ++TimestampIt)
				{
					FString CookedFilename = TimestampIt.Key();
					FDateTime CookedTimestamp = TimestampIt.Value();
					FString StandardCookedFilename = CookedFilename.Replace(*SandboxDirectory, *(FPaths::GetRelativePathToRoot()));
					FDateTime DependentTimestamp;

					if (PDInfoModule.DeterminePackageDependentTimeStamp(*(FPaths::GetBaseFilename(StandardCookedFilename, false)), DependentTimestamp) == true)
					{
						double Diff = (CookedTimestamp - DependentTimestamp).GetTotalSeconds();

						if (Diff < 0.0)
						{
							UE_LOG(LogCookCommandlet, Display, TEXT("Deleting out of date cooked file: %s"), *CookedFilename);

							IFileManager::Get().Delete(*CookedFilename);
						}
					}
				}
			}

			// Collect garbage to ensure we don't have any packages hanging around from dependent time stamp determination
			CollectGarbage(RF_Native);
		}
	}

	UE_LOG(LogCookCommandlet, Display, TEXT("Sandbox cleanup took %5.3f seconds"), SandboxCleanTime);
}

void UCookCommandlet::GenerateAssetRegistry(const TArray<ITargetPlatform*>& Platforms)
{
	// load the interface
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	double GenerateAssetRegistryTime = 0.0;
	{
		SCOPE_SECONDS_COUNTER(GenerateAssetRegistryTime);
		UE_LOG(LogCookCommandlet, Display, TEXT("Creating asset registry [is editor: %d]"), GIsEditor);

		// Perform a synchronous search of any .ini based asset paths (note that the per-game delegate may
		// have already scanned paths on its own)
		// We want the registry to be fully initialized when generating streaming manifests too.
		TArray<FString> ScanPaths;
		if (GConfig->GetArray(TEXT("AssetRegistry"), TEXT("PathsToScanForCook"), ScanPaths, GEngineIni) > 0)
		{
			AssetRegistry.ScanPathsSynchronous(ScanPaths);
		}
		else
		{
			AssetRegistry.SearchAllAssets(true);
		}
		
		// When not cooking on the fly the registry will be saved after the cooker has finished
		if (bCookOnTheFly)
		{
			// write it out to a memory archive
			FArrayWriter SerializedAssetRegistry;
			AssetRegistry.Serialize(SerializedAssetRegistry);
			UE_LOG(LogCookCommandlet, Display, TEXT("Generated asset registry size is %5.2fkb"), (float)SerializedAssetRegistry.Num() / 1024.f);

			// now save it in each cooked directory
			FString RegistryFilename = FPaths::GameDir() / TEXT("AssetRegistry.bin");
			// Use SandboxFile to do path conversion to properly handle sandbox paths (outside of standard paths in particular).
			FString SandboxFilename = SandboxFile->ConvertToAbsolutePathForExternalAppForWrite(*RegistryFilename);

			for (int32 Index = 0; Index < Platforms.Num(); Index++)
			{
				FString PlatFilename = SandboxFilename.Replace(TEXT("[Platform]"), *Platforms[Index]->PlatformName());
				FFileHelper::SaveArrayToFile(SerializedAssetRegistry, *PlatFilename);
			}
		}
	}
	UE_LOG(LogCookCommandlet, Display, TEXT("Done creating registry. It took %5.2fs."), GenerateAssetRegistryTime);
	
}

void UCookCommandlet::SaveGlobalShaderMapFiles(const TArray<ITargetPlatform*>& Platforms)
{
	for (int32 Index = 0; Index < Platforms.Num(); Index++)
	{
		// make sure global shaders are up to date!
		TArray<FString> Files;
		FShaderRecompileData RecompileData;
		RecompileData.PlatformName = Platforms[Index]->PlatformName();
		// Compile for all platforms
		RecompileData.ShaderPlatform = -1;
		RecompileData.ModifiedFiles = &Files;
		RecompileData.MeshMaterialMaps = NULL;

		check( IsInGameThread() );

		FString OutputDir = GetOutputDirectory(RecompileData.PlatformName);

		RecompileShadersForRemote
			(RecompileData.PlatformName, 
			RecompileData.ShaderPlatform == -1 ? SP_NumPlatforms : (EShaderPlatform)RecompileData.ShaderPlatform,
			OutputDir, 
			RecompileData.MaterialsToLoad, 
			RecompileData.SerializedShaderResources, 
			RecompileData.MeshMaterialMaps, 
			RecompileData.ModifiedFiles);
	}
}

void UCookCommandlet::CollectFilesToCook(TArray<FString>& FilesInPath)
{
	TArray<FString> MapList;

	// Add the default map section
	GEditor->LoadMapListFromIni(TEXT("AlwaysCookMaps"), MapList);

	// Add any map sections specified on command line
	GEditor->ParseMapSectionIni(*Params, MapList);
	for (int32 MapIdx = 0; MapIdx < MapList.Num(); MapIdx++)
	{
		if (UWorldComposition::CollectTilesToCook(MapList[MapIdx], FilesInPath))
		{
			// Entry has been handled by world composition, no further processing required
			continue;
		}
				
		FilesInPath.AddUnique(MapList[MapIdx]);
	}

	TArray<FString> CmdLineMapEntries;
	TArray<FString> CmdLineDirEntries;
	for (int32 SwitchIdx = 0; SwitchIdx < Switches.Num(); SwitchIdx++)
	{
		// Check for -MAP=<name of map> entries
		const FString& Switch = Switches[SwitchIdx];

		if (Switch.StartsWith(TEXT("MAP=")) == true)
		{
			FString MapToCook = Switch.Right(Switch.Len() - 4);
			// Allow support for -MAP=Map1+Map2+Map3 as well as -MAP=Map1 -MAP=Map2
			int32 PlusIdx = MapToCook.Find(TEXT("+"));
			while (PlusIdx != INDEX_NONE)
			{
				FString MapName = MapToCook.Left(PlusIdx);
				CmdLineMapEntries.Add(MapName);
				MapToCook = MapToCook.Right(MapToCook.Len() - (PlusIdx + 1));
				PlusIdx = MapToCook.Find(TEXT("+"));
			}
			CmdLineMapEntries.Add(MapToCook);
		}

		if (Switch.StartsWith(TEXT("COOKDIR=")) == true)
		{
			FString DirToCook = Switch.Right(Switch.Len() - 8);
			// Allow support for -COOKDIR=Dir1+Dir2+Dir3 as well as -COOKDIR=Dir1 -COOKDIR=Dir2
			int32 PlusIdx = DirToCook.Find(TEXT("+"));
			while (PlusIdx != INDEX_NONE)
			{
				FString DirName = DirToCook.Left(PlusIdx);
				CmdLineMapEntries.Add(DirName);
				DirToCook = DirToCook.Right(DirToCook.Len() - (PlusIdx + 1));
				PlusIdx = DirToCook.Find(TEXT("+"));
			}
			CmdLineDirEntries.Add(DirToCook);
		}
	}

	for (int32 CmdLineMapIdx = 0; CmdLineMapIdx < CmdLineMapEntries.Num(); CmdLineMapIdx++)
	{
		FString CurrEntry = CmdLineMapEntries[CmdLineMapIdx];
		
		// Check if this cmd entry is related to world composition
		if (UWorldComposition::CollectTilesToCook(CurrEntry, FilesInPath))
		{
			// Entry has been handled by world composition, no further processing required
			continue;
		}
		
		if (FPackageName::IsShortPackageName(CurrEntry))
		{
			if (FPackageName::SearchForPackageOnDisk(CurrEntry, NULL, &CurrEntry) == false)
			{
				UE_LOG(LogCookCommandlet, Warning, TEXT("Unable to find package for map %s."), *CurrEntry);
			}
			else
			{
				FilesInPath.AddUnique(CurrEntry);
			}
		}
		else
		{
			FilesInPath.AddUnique(CurrEntry);
		}
	}

	const FString ExternalMountPointName(TEXT("/Game/"));
	for (int32 CmdLineDirIdx = 0; CmdLineDirIdx < CmdLineDirEntries.Num(); CmdLineDirIdx++)
	{
		FString CurrEntry = CmdLineDirEntries[CmdLineDirIdx];

		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *CurrEntry, *(FString(TEXT("*")) + FPackageName::GetAssetPackageExtension()), true, false);
		for (int32 Index = 0; Index < Files.Num(); Index++)
		{
			FString StdFile = Files[Index];
			FPaths::MakeStandardFilename(StdFile);
			FilesInPath.AddUnique(StdFile);

			// this asset may not be in our currently mounted content directories, so try to mount a new one now
			FString LongPackageName;
			if(!FPackageName::IsValidLongPackageName(StdFile) && !FPackageName::TryConvertFilenameToLongPackageName(StdFile, LongPackageName))
			{
				FPackageName::RegisterMountPoint(ExternalMountPointName, CurrEntry);
			}
		}
	}

	if (FilesInPath.Num() == 0 || bCookAll)
	{
		Tokens.Empty(2);
		Tokens.Add(FString("*") + FPackageName::GetAssetPackageExtension());
		Tokens.Add(FString("*") + FPackageName::GetMapPackageExtension());

		uint8 PackageFilter = NORMALIZE_DefaultFlags | NORMALIZE_ExcludeEnginePackages;
		if ( Switches.Contains(TEXT("MAPSONLY")) )
		{
			PackageFilter |= NORMALIZE_ExcludeContentPackages;
		}

		if ( Switches.Contains(TEXT("NODEV")) )
		{
			PackageFilter |= NORMALIZE_ExcludeDeveloperPackages;
		}

		// assume the first token is the map wildcard/pathname
		TArray<FString> Unused;
		for ( int32 TokenIndex = 0; TokenIndex < Tokens.Num(); TokenIndex++ )
		{
			TArray<FString> TokenFiles;
			if ( !NormalizePackageNames( Unused, TokenFiles, Tokens[TokenIndex], PackageFilter) )
			{
				UE_LOG(LogCookCommandlet, Display, TEXT("No packages found for parameter %i: '%s'"), TokenIndex, *Tokens[TokenIndex]);
				continue;
			}

			for (int32 TokenFileIndex = 0; TokenFileIndex < TokenFiles.Num(); ++TokenFileIndex)
			{
				FilesInPath.AddUnique(TokenFiles[TokenFileIndex]);
			}
		}
	}

	// make sure we cook the default maps
	ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
	static const TArray<ITargetPlatform*>& Platforms =  TPM.GetTargetPlatforms();
	for (int32 Index = 0; Index < Platforms.Num(); Index++)
	{
		// load the platform specific ini to get its DefaultMap
		FConfigFile PlatformEngineIni;
		FConfigCacheIni::LoadLocalIniFile(PlatformEngineIni, TEXT("Engine"), true, *Platforms[Index]->IniPlatformName());

		// get the server and game default maps and cook them
		TArray<FString> DefaultMaps;
		FString Obj;
		if (PlatformEngineIni.GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GameDefaultMap"), Obj))
		{
			DefaultMaps.AddUnique(Obj);
		}
		if (PlatformEngineIni.GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("ServerDefaultMap"), Obj))
		{
			DefaultMaps.AddUnique(Obj);
		}
		if (PlatformEngineIni.GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GlobalDefaultGameMode"), Obj))
		{
			DefaultMaps.AddUnique(Obj);
		}
		if (PlatformEngineIni.GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GlobalDefaultServerGameMode"), Obj))
		{
			DefaultMaps.AddUnique(Obj);
		}

		for (FString DefaultMap : DefaultMaps)
		{
			// Check if this map is related to world composition
			if (UWorldComposition::CollectTilesToCook(DefaultMap, FilesInPath))
			{
				// Entry has been handled by world composition, no further processing required
				continue;
			}

			FilesInPath.AddUnique(DefaultMap);
		}
	}

	// make sure we cook any extra assets for the default touch interface
	// @todo need a better approach to cooking assets which are dynamically loaded by engine code based on settings
	FConfigFile InputIni;
	FString InterfaceFile;
	FConfigCacheIni::LoadLocalIniFile(InputIni, TEXT("Input"), true);
	if (InputIni.GetString(TEXT("/Script/Engine.InputSettings"), TEXT("DefaultTouchInterface"), InterfaceFile))
	{
		if (InterfaceFile != TEXT("None") && InterfaceFile != TEXT(""))
		{
			FilesInPath.AddUnique(InterfaceFile);
		}
	}

	//@todo SLATE: This is a hack to ensure all slate referenced assets get cooked.
	// Slate needs to be refactored to properly identify required assets at cook time.
	// Simply jamming everything in a given directory into the cook list is error-prone
	// on many levels - assets not required getting cooked/shipped; assets not put under 
	// the correct folder; etc.
	{
		TArray<FString> UIContentPaths;
		if (GConfig->GetArray(TEXT("UI"), TEXT("ContentDirectories"), UIContentPaths, GEditorIni) > 0)
		{
			for (int32 DirIdx = 0; DirIdx < UIContentPaths.Num(); DirIdx++)
			{
				FString ContentPath = FPackageName::LongPackageNameToFilename(UIContentPaths[DirIdx]);

				TArray<FString> Files;
				IFileManager::Get().FindFilesRecursive(Files, *ContentPath, *(FString(TEXT("*")) + FPackageName::GetAssetPackageExtension()), true, false);
				for (int32 Index = 0; Index < Files.Num(); Index++)
				{
					FString StdFile = Files[Index];
					FPaths::MakeStandardFilename(StdFile);
					FilesInPath.AddUnique(StdFile);
				}
			}
		}
	}
}

void UCookCommandlet::GenerateLongPackageNames(TArray<FString>& FilesInPath)
{
	TArray<FString> FilesInPathReverse;
	FilesInPathReverse.Reserve(FilesInPath.Num());
	for( int32 FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
	{
		const FString& FileInPath = FilesInPath[FilesInPath.Num() - FileIndex - 1];
		if (FPackageName::IsValidLongPackageName(FileInPath))
		{
			FilesInPathReverse.AddUnique(FileInPath);
		}
		else
		{
			FString LongPackageName;
			if (FPackageName::TryConvertFilenameToLongPackageName(FileInPath, LongPackageName))
			{
				FilesInPathReverse.AddUnique(LongPackageName);
			}
			else
			{
				UE_LOG(LogCookCommandlet, Warning, TEXT("Unable to generate long package name for %s"), *FileInPath);
			}
		}
	}
	Exchange(FilesInPathReverse, FilesInPath);
}

bool UCookCommandlet::Cook(const TArray<ITargetPlatform*>& Platforms, TArray<FString>& FilesInPath)
{
	// Subsets for parallel processing
	uint32 SubsetMod = 0;
	uint32 SubsetTarget = MAX_uint32;
	FParse::Value(*Params, TEXT("SubsetMod="), SubsetMod);
	FParse::Value(*Params, TEXT("SubsetTarget="), SubsetTarget);
	bool bDoSubset = SubsetMod > 0 && SubsetTarget < SubsetMod;

	FCoreDelegates::PackageCreatedForLoad.AddUObject(this, &UCookCommandlet::MaybeMarkPackageAsAlreadyLoaded);
	
	SaveGlobalShaderMapFiles(Platforms);

	CollectFilesToCook(FilesInPath);
	if (FilesInPath.Num() == 0)
	{
		UE_LOG(LogCookCommandlet, Warning, TEXT("No files found."));
	}

	GenerateLongPackageNames(FilesInPath);
	
	const int32 GCInterval = bLeakTest ? 1: 500;
	int32 NumProcessedSinceLastGC = GCInterval;
	bool bLastLoadWasMap = false;
	bool bLastLoadWasMapWithStreamingLevels = false;
	TSet<FString> CookedPackages;
	FString LastLoadedMapName;

	FChunkManifestGenerator ManifestGenerator(Platforms);
	// Always clean manifest directories so that there's no stale data
	ManifestGenerator.CleanManifestDirectories();
	ManifestGenerator.Initialize(bGenerateStreamingInstallManifests);

	for( int32 FileIndex = 0; ; FileIndex++ )
	{
		if (NumProcessedSinceLastGC >= GCInterval || bLastLoadWasMap || FileIndex < 0 || FileIndex >= FilesInPath.Num())
		{
			// since we are about to save, we need to resolve all string asset references now
			GRedirectCollector.ResolveStringAssetReference();
			TArray<UObject *> ObjectsInOuter;
			GetObjectsWithOuter(NULL, ObjectsInOuter, false);
			// save the cooked packages before collect garbage
			for( int32 Index = 0; Index < ObjectsInOuter.Num(); Index++ )
			{
				UPackage* Pkg = Cast<UPackage>(ObjectsInOuter[Index]);
				if (!Pkg)
				{
					continue;
				}

				FString Name = Pkg->GetPathName();
				FString Filename(GetPackageFilename(Pkg));

				if (!Filename.IsEmpty())
				{
					// Populate streaming install manifests
					FString SandboxFilename = SandboxFile->ConvertToAbsolutePathForExternalAppForWrite(*Filename);
					ManifestGenerator.AddPackageToChunkManifest(Pkg, SandboxFilename, LastLoadedMapName);
				}
					
				if (!CookedPackages.Contains(Filename))
				{
					CookedPackages.Add(Filename);

					bool bWasUpToDate = false;

					SaveCookedPackage(Pkg, SAVE_KeepGUID | SAVE_Async | (bUnversioned ? SAVE_Unversioned : 0), bWasUpToDate);

					PackagesToNotReload.Add(Pkg->GetName());
					Pkg->PackageFlags |= PKG_ReloadingForCooker;
					{
						TArray<UObject *> ObjectsInPackage;
						GetObjectsWithOuter(Pkg, ObjectsInPackage, true);
						for( int32 IndexPackage = 0; IndexPackage < ObjectsInPackage.Num(); IndexPackage++ )
						{
							ObjectsInPackage[IndexPackage]->CookerWillNeverCookAgain();
						}
					}
				}
			}

			if (NumProcessedSinceLastGC >= GCInterval)
			{
				UE_LOG(LogCookCommandlet, Display, TEXT("Full GC..."));

				CollectGarbage( RF_Native );
				NumProcessedSinceLastGC = 0;

				if (bLeakTest)
				{
					for (FObjectIterator It; It; ++It)
					{
						if (!LastGCItems.Contains(FWeakObjectPtr(*It)))
						{
							UE_LOG(LogCookCommandlet, Warning, TEXT("\tLeaked %s"), *(It->GetFullName()));
							LastGCItems.Add(FWeakObjectPtr(*It));
						}
					}
				}
			}
		}

		if (FileIndex < 0 || FileIndex >= FilesInPath.Num())
		{
			break;
		}
			
		// Attempt to find file for package name. THis is to make sure no short package
		// names are passed to LoadPackage.
		FString Filename;
		if (FPackageName::DoesPackageExist(FilesInPath[FileIndex], NULL, &Filename) == false)
		{
			UE_LOG(LogCookCommandlet, Warning, TEXT("Unable to find package file for: %s"), *FilesInPath[FileIndex]);
			
			continue;
		}
		Filename = FPaths::ConvertRelativePathToFull(Filename);

		if (bDoSubset)
		{
			const FString& PackageName = FPackageName::PackageFromPath(*Filename);
			if (FCrc::StrCrc_DEPRECATED(*PackageName.ToUpper()) % SubsetMod != SubsetTarget)
			{
				continue;
			}
		}

		if (CookedPackages.Contains(Filename))
		{
			UE_LOG(LogCookCommandlet, Display, TEXT("\tskipping %s, already cooked."), *Filename);
			continue;
		}

		bLastLoadWasMap = false;
		bLastLoadWasMapWithStreamingLevels = false;

		if (!ShouldCook(Filename))
		{
			UE_LOG(LogCookCommandlet, Display, TEXT("Up To Date: %s"), *Filename);
			NumProcessedSinceLastGC++;
			continue;
		}

		UE_LOG(LogCookCommandlet, Display, TEXT("Loading %s"), *Filename );

		if (bGenerateStreamingInstallManifests)
		{
			ManifestGenerator.PrepareToLoadNewPackage(Filename);
		}

		UPackage* Package = LoadPackage( NULL, *Filename, LOAD_None );

		if( Package == NULL )
		{
			UE_LOG(LogCookCommandlet, Warning, TEXT("Could not load %s!"), *Filename );
		}
		else
		{
			NumProcessedSinceLastGC++;
			if (Package->ContainsMap())
			{
				// load sublevels
				UWorld* World = UWorld::FindWorldInPackage(Package);
				check(Package);

				if (World->StreamingLevels.Num())
				{
					World->LoadSecondaryLevels(true, &CookedPackages);
				}
				// maps don't compile level script actors correctly unless we do FULL GC's, they may also hold weak pointer refs that need to be reset
				NumProcessedSinceLastGC = GCInterval; 

				LastLoadedMapName = Package->GetName();
				bLastLoadWasMap = true;
			}
			else
			{
				LastLoadedMapName.Empty();
			}
		}
	}

	IConsoleManager::Get().ProcessUserConsoleInput(TEXT("Tex.DerivedDataTimings"), *GWarn, NULL );
	UPackage::WaitForAsyncFileWrites();

	GetDerivedDataCacheRef().WaitForQuiescence(true);

	if (bGenerateStreamingInstallManifests)
	{
		ManifestGenerator.SaveManifests();
	}
	{
		// Save modified asset registry with all streaming chunk info generated during cook
		FString RegistryFilename = FPaths::GameDir() / TEXT("AssetRegistry.bin");
		FString SandboxRegistryFilename = SandboxFile->ConvertToAbsolutePathForExternalAppForWrite(*RegistryFilename);
		ManifestGenerator.SaveAssetRegistry(SandboxRegistryFilename);
	}

	return true;
}


