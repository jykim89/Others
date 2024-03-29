// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DerivedDataCacheCommandlet.cpp: Commandlet for DDC maintenence
=============================================================================*/
#include "UnrealEd.h"
#include "PackageHelperFunctions.h"
#include "DerivedDataCacheInterface.h"
#include "ISourceControlModule.h"
#include "GlobalShader.h"
#include "TargetPlatform.h"
#include "IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDerivedDataCacheCommandlet, Log, All);

UDerivedDataCacheCommandlet::UDerivedDataCacheCommandlet(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	LogToConsole = false;
}

void UDerivedDataCacheCommandlet::MaybeMarkPackageAsAlreadyLoaded(UPackage *Package)
{
	FString Name = Package->GetName();
	if (PackagesToNotReload.Contains(Name))
	{
		UE_LOG(LogDerivedDataCacheCommandlet, Verbose, TEXT("Marking %s already loaded."), *Name);
		Package->PackageFlags |= PKG_ReloadingForCooker;
	}
}

int32 UDerivedDataCacheCommandlet::Main( const FString& Params )
{
	TArray<FString> Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);

	bool bFillCache = Switches.Contains("FILL");   // do the equivalent of a "loadpackage -all" to fill the DDC
	bool bStartupOnly = Switches.Contains("STARTUPONLY");   // regardless of any other flags, do not iterate packages

	// Subsets for parallel processing
	uint32 SubsetMod = 0;
	uint32 SubsetTarget = MAX_uint32;
	FParse::Value(*Params, TEXT("SubsetMod="), SubsetMod);
	FParse::Value(*Params, TEXT("SubsetTarget="), SubsetTarget);
	bool bDoSubset = SubsetMod > 0 && SubsetTarget < SubsetMod;
	double FindProcessedPackagesTime = 0.0;
	double GCTime = 0.0;

	if (!bStartupOnly && bFillCache)
	{
		FCoreDelegates::PackageCreatedForLoad.AddUObject(this, &UDerivedDataCacheCommandlet::MaybeMarkPackageAsAlreadyLoaded);

		TArray<FString> FilesInPath;

		Tokens.Empty(2);
		Tokens.Add(FString("*") + FPackageName::GetAssetPackageExtension());
		Tokens.Add(FString("*") + FPackageName::GetMapPackageExtension());
		
		uint8 PackageFilter = NORMALIZE_DefaultFlags;
		if ( Switches.Contains(TEXT("MAPSONLY")) )
		{
			PackageFilter |= NORMALIZE_ExcludeContentPackages;
		}

		if ( !Switches.Contains(TEXT("DEV")) )
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
				UE_LOG(LogDerivedDataCacheCommandlet, Display, TEXT("No packages found for parameter %i: '%s'"), TokenIndex, *Tokens[TokenIndex]);
				continue;
			}

			FilesInPath += TokenFiles;
		}

		if ( FilesInPath.Num() == 0 )
		{
			UE_LOG(LogDerivedDataCacheCommandlet, Warning, TEXT("No files found."));
		}

		ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
		const TArray<ITargetPlatform*>& Platforms = TPM->GetActiveTargetPlatforms();

		for (int32 Index = 0; Index < Platforms.Num(); Index++)
		{
			TArray<FName> DesiredShaderFormats;
			Platforms[Index]->GetAllTargetedShaderFormats(DesiredShaderFormats);

			for (int32 FormatIndex = 0; FormatIndex < DesiredShaderFormats.Num(); FormatIndex++)
			{
				const EShaderPlatform TargetPlatform = ShaderFormatToLegacyShaderPlatform(DesiredShaderFormats[FormatIndex]);
				// Kick off global shader compiles for each target platform
				GetGlobalShaderMap(TargetPlatform);
			}
		}

		const int32 GCInterval = 100;
		int32 NumProcessedSinceLastGC = GCInterval;
		bool bLastPackageWasMap = true; // 'true' is to prime the ProcessedPackages list
		TSet<FString> ProcessedPackages;

		UE_LOG(LogDerivedDataCacheCommandlet, Display, TEXT("%d packages to load..."), FilesInPath.Num());

		for( int32 FileIndex = FilesInPath.Num() - 1; ; FileIndex-- )
		{
			// Keep track of which packages have already been processed along with the map.
			if (NumProcessedSinceLastGC >= GCInterval || bLastPackageWasMap || FileIndex == FilesInPath.Num() - 1)
			{
				const double FindProcessedPackagesStartTime = FPlatformTime::Seconds();
				TArray<UObject *> ObjectsInOuter;
				GetObjectsWithOuter(NULL, ObjectsInOuter, false);
				for( int32 Index = 0; Index < ObjectsInOuter.Num(); Index++ )
				{
					UPackage* Pkg = Cast<UPackage>(ObjectsInOuter[Index]);
					if (!Pkg)
					{
						continue;
					}
					FString Filename;
					if (FPackageName::DoesPackageExist(Pkg->GetName(), NULL, &Filename))
					{
						if (!ProcessedPackages.Contains(Filename))
						{
							ProcessedPackages.Add(Filename);

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
				}
				FindProcessedPackagesTime += FPlatformTime::Seconds() - FindProcessedPackagesStartTime;
			}

			if (NumProcessedSinceLastGC >= GCInterval || FileIndex < 0 || bLastPackageWasMap)
			{
				const double StartGCTime = FPlatformTime::Seconds();
				if (NumProcessedSinceLastGC >= GCInterval || FileIndex < 0)
				{
					UE_LOG(LogDerivedDataCacheCommandlet, Display, TEXT("GC (Full)..."));
					CollectGarbage( RF_Native );
					NumProcessedSinceLastGC = 0;
				}
				else
				{
					UE_LOG(LogDerivedDataCacheCommandlet, Display, TEXT("GC..."));
					CollectGarbage( RF_Native | RF_Standalone );
				}				
				GCTime += FPlatformTime::Seconds() - StartGCTime;

				bLastPackageWasMap = false;
			}
			if (FileIndex < 0)
			{
				break;
			}
			const FString& Filename = FilesInPath[FileIndex];
			if (ProcessedPackages.Contains(Filename))
			{
				continue;
			}
			if (bDoSubset)
			{
				const FString& PackageName = FPackageName::PackageFromPath(*Filename);
				if (FCrc::StrCrc_DEPRECATED(*PackageName.ToUpper()) % SubsetMod != SubsetTarget)
				{
					continue;
				}
			}

			UE_LOG(LogDerivedDataCacheCommandlet, Display, TEXT("Loading (%d) %s"), FilesInPath.Num() - FileIndex, *Filename );

			UPackage* Package = LoadPackage( NULL, *Filename, LOAD_None );
			if( Package == NULL )
			{
				UE_LOG(LogDerivedDataCacheCommandlet, Error, TEXT("Error loading %s!"), *Filename );
			}
			else
			{
				bLastPackageWasMap = Package->ContainsMap();
				NumProcessedSinceLastGC++;
			}
		}
	}

	IConsoleManager::Get().ProcessUserConsoleInput(TEXT("Tex.DerivedDataTimings"), *GWarn, NULL);
	UE_LOG(LogDerivedDataCacheCommandlet, Display, TEXT("Waiting for shaders to finish."));
	GShaderCompilingManager->FinishAllCompilation();
	UE_LOG(LogDerivedDataCacheCommandlet, Display, TEXT("Done waiting for shaders to finish."));
	GetDerivedDataCacheRef().WaitForQuiescence(true);

	UE_LOG(LogDerivedDataCacheCommandlet, Display, TEXT("%.2lfs spent looking for processed packages, %.2lfs spent on GC."), FindProcessedPackagesTime, GCTime);

	return 0;
}
