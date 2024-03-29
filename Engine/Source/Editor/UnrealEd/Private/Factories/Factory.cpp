// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


// Editor includes.
#include "UnrealEd.h"
#include "ObjectTools.h"
#include "AssetToolsModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogFactory, Log, All);

/*----------------------------------------------------------------------------
	UFactory.
----------------------------------------------------------------------------*/
FString UFactory::CurrentFilename(TEXT(""));


int32 UFactory::OverwriteYesOrNoToAllState = -1;

bool UFactory::bAllowOneTimeWarningMessages = true;

UFactory::UFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

}

void UFactory::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{	
	UFactory* This = CastChecked<UFactory>(InThis);
	UClass* SupportedClass = *This->SupportedClass;
	UClass* ContextClass = *This->ContextClass;
	Collector.AddReferencedObject( SupportedClass, This );
	Collector.AddReferencedObject( ContextClass, This );

	Super::AddReferencedObjects( This, Collector );
}


bool UFactory::FactoryCanImport( const FString& Filename )
{
	//check extension (only do the following for t3d)
	if (FPaths::GetExtension(Filename) == TEXT("t3d"))
	{
		//Open file
		FString Data;
		if( FFileHelper::LoadFileToString( Data, *Filename ) )
		{
			const TCHAR* Str= *Data;

			if( FParse::Command(&Str, TEXT("BEGIN") ) && FParse::Command(&Str, TEXT("OBJECT")) )
			{
				FString strClass;
				if (FParse::Value(Str, TEXT("CLASS="), strClass))
				{
					//we found the right syntax, so no error if we don't match
					if (strClass == SupportedClass->GetName())
					{
						return true;
					}
					return false;
				}
			}
			UE_LOG(LogFactory, Warning, TEXT("Factory import failed due to invalid format: %s"), *Filename);
		}
		else
		{
			UE_LOG(LogFactory, Warning, TEXT("Factory import failed due to inability to load file %s"), *Filename);
		}
	}

	return false;
}

bool UFactory::ShouldShowInNewMenu() const
{
	return bCreateNew;
}

FText UFactory::GetDisplayName() const
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	UClass* LocalSupportedClass = GetSupportedClass();
	if ( LocalSupportedClass )
	{
		TWeakPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(LocalSupportedClass);
		if ( AssetTypeActions.IsValid() )
		{
			// @todo AssetTypeActions should be returning a FText so we dont have to do this conversion here.
			return AssetTypeActions.Pin()->GetName();
		}

		// Factories whose classes do not have asset type actions should just display the sanitized class name
		return FText::FromString( FName::NameToDisplayString(*LocalSupportedClass->GetName(), false) );
	}

	// Factories that have no supported class have no display name.
	return FText();
}

uint32 UFactory::GetMenuCategories() const
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	UClass* LocalSupportedClass = GetSupportedClass();
	if ( LocalSupportedClass )
	{
		TWeakPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(LocalSupportedClass);
		if ( AssetTypeActions.IsValid() )
		{
			return AssetTypeActions.Pin()->GetCategories();
		}
	}

	// Factories whose classes do not have asset type actions fall in the misc category
	return EAssetTypeCategories::Misc;
}

FText UFactory::GetToolTip() const
{
	return GetSupportedClass()->GetToolTipText();
}

UClass* UFactory::GetSupportedClass() const
{
	return SupportedClass;
}


bool UFactory::DoesSupportClass(UClass * Class)
{
	return (Class == GetSupportedClass());
}

UClass* UFactory::ResolveSupportedClass()
{
	// This check forces factories which support multiple classes to overload this method.
	// In other words, you can't have a SupportedClass of NULL and not overload this method.
	check( SupportedClass );
	return SupportedClass;
}

void UFactory::ResetState()
{
	// Resets the state of the 'Yes To All / No To All' prompt for overwriting existing objects on import.
	// After the reset, the next import collision will always display the prompt.
	OverwriteYesOrNoToAllState = -1;

	// Resets the state of one time warning messages. This will allow the warning message to be shown.
	bAllowOneTimeWarningMessages = true;
}

UObject* UFactory::StaticImportObject
(
UClass*				Class,
UObject*			InOuter,
FName				Name,
EObjectFlags		Flags,
const TCHAR*		Filename,
UObject*			Context,
UFactory*			InFactory,
const TCHAR*		Parms,
FFeedbackContext*	Warn,
int32				MaxImportFileSize
)
{
	bool bOperationCanceled = false;
	return StaticImportObject(Class, InOuter, Name, Flags, bOperationCanceled, Filename, Context, InFactory, Parms, Warn, MaxImportFileSize);
}

UObject* UFactory::StaticImportObject
(
	UClass*				Class,
	UObject*			InOuter,
	FName				Name,
	EObjectFlags		Flags,
	bool&				bOutOperationCanceled,
	const TCHAR*		Filename,
	UObject*			Context,
	UFactory*			InFactory,
	const TCHAR*		Parms,
	FFeedbackContext*	Warn,
	int32				MaxImportFileSize
)
{
	check(Class);

	CurrentFilename = Filename;

	// Make list of all applicable factories.
	TArray<UFactory*> Factories;
	if( InFactory )
	{
		// Use just the specified factory.
		check( !InFactory->SupportedClass || Class->IsChildOf(InFactory->SupportedClass) );
		Factories.Add( InFactory );
	}
	else
	{
		// Try all automatic factories, sorted by priority.
		for( TObjectIterator<UClass> It; It; ++It )
		{
			if( It->IsChildOf( UFactory::StaticClass() ) )
			{
				UFactory* Default = It->GetDefaultObject<UFactory>();
				if( Class->IsChildOf( Default->SupportedClass ) && Default->AutoPriority >= 0 )
				{
					Factories.Add( ConstructObject<UFactory>( *It ) );
				}
			}
		}

		struct FCompareUFactoryAutoPriority
		{
			FORCEINLINE bool operator()(const UFactory& A, const UFactory& B) const { return A.AutoPriority < B.AutoPriority; }
		};
		Factories.Sort( FCompareUFactoryAutoPriority() );
	}

	bool bLoadedFile = false;

	// Try each factory in turn.
	for( int32 i=0; i<Factories.Num(); i++ )
	{
		UFactory* Factory = Factories[i];
		UObject* Result = NULL;
		if( Factory->bCreateNew )
		{
			if( FCString::Stricmp(Filename,TEXT(""))==0 )
			{
				UE_LOG(LogFactory, Log,  TEXT("FactoryCreateNew: %s with %s (%i %i %s)"), *Class->GetName(), *Factories[i]->GetClass()->GetName(), Factory->bCreateNew, Factory->bText, Filename );
				Factory->ParseParms( Parms );
				Result = Factory->FactoryCreateNew( Class, InOuter, Name, Flags, NULL, Warn );
			}
		}
		else if( FCString::Stricmp(Filename,TEXT(""))!=0 )
		{
			if( Factory->bText )
			{
				//UE_LOG(LogFactory, Log,  TEXT("FactoryCreateText: %s with %s (%i %i %s)"), *Class->GetName(), *Factories(i)->GetClass()->GetName(), Factory->bCreateNew, Factory->bText, Filename );
				FString Data;
				if( FFileHelper::LoadFileToString( Data, Filename ) )
				{
					bLoadedFile = true;
					const TCHAR* Ptr = *Data;
					Factory->ParseParms( Parms );
					Result = Factory->FactoryCreateText( Class, InOuter, Name, Flags, NULL, *FPaths::GetExtension(Filename), Ptr, Ptr+Data.Len(), Warn );
				}
			}
			else
			{
				UE_LOG(LogFactory, Log,  TEXT("FactoryCreateBinary: %s with %s (%i %i %s)"), *Class->GetName(), *Factories[i]->GetClass()->GetName(), Factory->bCreateNew, Factory->bText, Filename );
				
				// Sanity check the file size of the impending import and prompt the user if they wish to continue if the file size is very large
				const int32 FileSize = IFileManager::Get().FileSize( Filename );
				bool bValidFileSize = true;

				// File size was found
				if ( FileSize != INDEX_NONE )
				{
					if( ( MaxImportFileSize > 0 ) && ( FileSize > MaxImportFileSize ) )
					{
						// Prompt the user if they would like to proceed with large import, displaying the size of the file in MBs
						// (File Size >> 20) is the same as dividing by (1024*1024) to convert to MBs
						bValidFileSize = EAppReturnType::Yes == FMessageDialog::Open( EAppMsgType::YesNo,
							FText::Format( NSLOCTEXT("UnrealEd", "Warning_LargeFileImport", "Attempting to import a very large file, proceed?\nFile Size: {0} MB"), FText::AsNumber(FileSize >> 20) ) );
					}
				}
				else
				{
					UE_LOG(LogFactory, Error,TEXT("File '%s' does not exist"), Filename );
					bValidFileSize = false;
				}

				TArray<uint8> Data;
				if( bValidFileSize && FFileHelper::LoadFileToArray( Data, Filename ) )
				{
					bLoadedFile = true;
					Data.Add( 0 );
					const uint8* Ptr = &Data[ 0 ];
					Factory->ParseParms( Parms );
					Result = Factory->FactoryCreateBinary( Class, InOuter, Name, Flags, NULL, *FPaths::GetExtension(Filename), Ptr, Ptr+Data.Num()-1, Warn, bOutOperationCanceled );
				}
			}
		}
		if( Result )
		{
			// prevent UTextureCube created from UTextureFactory			check(Result->IsA(Class));
			Result->MarkPackageDirty();
			ULevel::LevelDirtiedEvent.Broadcast();
			Result->PostEditChange();

			CurrentFilename = TEXT("");
			return Result;
		}
	}

	if ( !bLoadedFile && !bOutOperationCanceled )
	{
		Warn->Logf( *FText::Format( NSLOCTEXT( "UnrealEd", "NoFindImport", "Can't find file '{0}' for import" ), FText::FromString( FString(Filename) ) ).ToString() );
	}

	CurrentFilename = TEXT("");

	return NULL;
}


bool UFactory::ValidForCurrentGame()
{
	if( ValidGameNames.Num() > 0 )
	{
		for( int32 Idx = 0; Idx < ValidGameNames.Num(); Idx++ )
		{
			if( FCString::Stricmp( FApp::GetGameName(), *ValidGameNames[Idx] ) == 0 )
			{
				return 1;
			}
		}

		return 0;
	}

	return 1;
}

void UFactory::GetSupportedFileExtensions(TArray<FString>& OutExtensions) const
{
	for ( int32 FormatIdx = 0; FormatIdx < Formats.Num(); ++FormatIdx )
	{
		const FString& Format = Formats[FormatIdx];
		const int32 DelimiterIdx = Format.Find(TEXT(";"));

		if ( DelimiterIdx != INDEX_NONE )
		{
			OutExtensions.Add( Format.Left(DelimiterIdx) );
		}
	}
}

bool UFactory::ImportUntypedBulkDataFromText(const TCHAR*& Buffer, FUntypedBulkData& BulkData)
{
	FString StrLine;
	int32 ElementCount = 0;
	int32 ElementSize = 0;
	bool bBulkDataIsLocked = false;

	while(FParse::Line(&Buffer,StrLine))
	{
		FString ParsedText;
		const TCHAR* Str = *StrLine;

		if (FParse::Value(Str, TEXT("ELEMENTCOUNT="), ParsedText))
		{
			/** Number of elements in bulk data array */
			ElementCount = FCString::Atoi(*ParsedText);
		}
		else
		if (FParse::Value(Str, TEXT("ELEMENTSIZE="), ParsedText))
		{
			/** Serialized flags for bulk data */
			ElementSize = FCString::Atoi(*ParsedText);
		}
		else
		if (FParse::Value(Str, TEXT("BEGIN "), ParsedText) && (ParsedText.ToUpper() == TEXT("BINARYBLOB")))
		{
			uint8* RawData = NULL;
			/** The bulk data... */
			while(FParse::Line(&Buffer,StrLine))
			{
				Str = *StrLine;

				if (FParse::Value(Str, TEXT("SIZE="), ParsedText))
				{
					int32 Size = FCString::Atoi(*ParsedText);

					check(Size == (ElementSize *ElementCount));

					BulkData.Lock(LOCK_READ_WRITE);
					void* RawBulkData = BulkData.Realloc(ElementCount);
					RawData = (uint8*)RawBulkData;
					bBulkDataIsLocked = true;
				}
				else
				if (FParse::Value(Str, TEXT("BEGIN "), ParsedText) && (ParsedText.ToUpper() == TEXT("BINARY")))
				{
					uint8* BulkDataPointer = RawData;
					while(FParse::Line(&Buffer,StrLine))
					{
						Str = *StrLine;
						TCHAR* ParseStr = (TCHAR*)(Str);

						if (FParse::Value(Str, TEXT("END "), ParsedText) && (ParsedText.ToUpper() == TEXT("BINARY")))
						{
							break;
						}

						// Clear whitespace
						while ((*ParseStr == L' ') || (*ParseStr == L'\t'))
						{
							ParseStr++;
						}

						// Parse the values into the bulk data...
						while ((*ParseStr != L'\n') && (*ParseStr != L'\r') && (*ParseStr != 0))
						{
							int32 Value;
							if (!FCString::Strnicmp(ParseStr, TEXT("0x"), 2))
							{
								ParseStr +=2;
							}
							Value = FParse::HexDigit(ParseStr[0]) * 16 + FParse::HexDigit(ParseStr[1]);
							*BulkDataPointer = (uint8)Value;
							BulkDataPointer++;
							ParseStr += 2;
							ParseStr++;
						}
					}
				}
				else
				if (FParse::Value(Str, TEXT("END "), ParsedText) && (ParsedText.ToUpper() == TEXT("BINARYBLOB")))
				{
					BulkData.Unlock();
					bBulkDataIsLocked = false;
					break;
				}
			}
		}
		else
		if (FParse::Value(Str, TEXT("END "), ParsedText) && (ParsedText.ToUpper() == TEXT("UNTYPEDBULKDATA")))
		{
			break;
		}
	}

	if (bBulkDataIsLocked == true)
	{
		BulkData.Unlock();
	}

	return true;
}

UObject* UFactory::CreateOrOverwriteAsset(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InTemplate) const
{
	// Creates an asset if it doesn't exist.
	UObject* ExistingAsset = StaticFindObject(NULL, InParent, *InName.ToString());
	if ( !ExistingAsset )
	{
		return StaticConstructObject(InClass, InParent, InName, InFlags, InTemplate);
	}

	// If it does exist then it overwrites it if possible.
	if ( ExistingAsset->GetClass()->IsChildOf(InClass) )
	{
		return StaticConstructObject(InClass, InParent, InName, InFlags, InTemplate);
	}
	
	// If it can not overwrite then it will delete and replace.
	if ( ObjectTools::DeleteSingleObject( ExistingAsset ) )
	{
		// Keep InPackage alive through the GC, in case ExistingAsset was the only reason it was around.
		const bool bRootedPackage = InParent->IsRooted();
		if ( !bRootedPackage )
		{
			InParent->AddToRoot();
		}

		// Force GC so we can cleanly create a new asset (and not do an 'in place' replacement)
		CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

		if ( !bRootedPackage )
		{
			InParent->RemoveFromRoot();
		}

		// Try to find the existing asset again now that the GC has occurred
		ExistingAsset = StaticFindObject(NULL, InParent, *InName.ToString());
		if ( ExistingAsset )
		{
			// Even after the delete and GC, the object is still around. Fail this operation.
			return NULL;
		}
		else
		{
			// We can now create the asset in the package
			return StaticConstructObject(InClass, InParent, InName, InFlags, InTemplate);
		}
	}
	else
	{
		// The delete did not succeed. There are still references to the old content.
		return NULL;
	}
}
