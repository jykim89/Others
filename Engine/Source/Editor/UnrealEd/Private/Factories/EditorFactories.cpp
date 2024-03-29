// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EditorFactories.cpp: Editor class factories.
=============================================================================*/

#include "UnrealEd.h"
#include "Factories.h"
#include "SoundDefinitions.h"
#include "BlueprintUtilities.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "BmpImageSupport.h"
#include "ScopedTransaction.h"
#include "BusyCursor.h"
#include "BSPOps.h"
#include "LevelUtils.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "SSkeletonWidget.h"

#include "DDSLoader.h"
#include "HDRLoader.h"
#include "IESLoader.h"
#include "ImageWrapper.h"

#include "FbxImporter.h"

#include "AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "SClassPickerDialog.h"
#include "MessageLog.h"
#include "EnumEditorUtils.h"
#include "StructureEditorUtils.h"

#if PLATFORM_WINDOWS
// Needed for DDS support.
#include "AllowWindowsPlatformTypes.h"
	#include <ddraw.h>
#include "HideWindowsPlatformTypes.h"
#endif

#if WITH_EDITOR
#include "CubemapUnwrapUtils.h"			// FMipLevelBatchedElementParameters
#endif

DEFINE_LOG_CATEGORY_STATIC(LogEditorFactories, Log, All);

#define LOCTEXT_NAMESPACE "EditorFactories"

/*------------------------------------------------------------------------------
	UTexture2DFactoryNew implementation.
------------------------------------------------------------------------------*/
UTexture2DFactoryNew::UTexture2DFactoryNew(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SupportedClass		= UTexture2D::StaticClass();
	bCreateNew			= true;
	bEditAfterNew		= true;

	Width = 256;
	Height = 256;
}

bool UTexture2DFactoryNew::ShouldShowInNewMenu() const
{
	// You may not create texture2d assets in the content browser
	return false;
}

UObject* UTexture2DFactoryNew::FactoryCreateNew( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn )
{
	// Do not create a texture with bad dimensions.
	if((Width & (Width - 1)) || (Height & (Height - 1)))
	{
		return NULL;
	}

	UTexture2D* Object = CastChecked<UTexture2D>(StaticConstructObject(InClass,InParent,InName,Flags) );

	Object->Source.Init2DWithMipChain(Width, Height, TSF_BGRA8);

	//Set the source art to be white as default.
	if( Object->Source.IsValid() )
	{
		TArray<uint8> TexturePixels;
		Object->Source.GetMipData( TexturePixels, 0 );

		uint8* DestData = Object->Source.LockMip(0);
		FMemory::MemSet(DestData, TexturePixels.Num() * sizeof( uint8 ) );
		Object->Source.UnlockMip(0);

		Object->PostEditChange();
	}
	return Object;
}

/*------------------------------------------------------------------------------
	UMaterialInstanceConstantFactoryNew implementation.
------------------------------------------------------------------------------*/
UMaterialInstanceConstantFactoryNew::UMaterialInstanceConstantFactoryNew(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = UMaterialInstanceConstant::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UMaterialInstanceConstantFactoryNew::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn)
{
	UMaterialInstanceConstant* MIC = CastChecked<UMaterialInstanceConstant>(StaticConstructObject(Class,InParent,Name,Flags));
	
	if ( MIC )
	{
		MIC->InitResources();

		if ( InitialParent )
		{
			MIC->SetParentEditorOnly(InitialParent);
		}
	}

	return MIC;
}

/*------------------------------------------------------------------------------
	UMaterialFactoryNew implementation.
------------------------------------------------------------------------------*/
UMaterialFactoryNew::UMaterialFactoryNew(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = UMaterial::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}


UObject* UMaterialFactoryNew::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn)
{
	UMaterial* NewMaterial = ConstructObject<UMaterial>(Class, InParent, Name, Flags);

	if ( InitialTexture != NULL )
	{
		// An initial texture was specified, add it and assign it to the BaseColor
		UMaterialExpressionTextureSample* Expression = ConstructObject<UMaterialExpressionTextureSample>( UMaterialExpressionTextureSample::StaticClass(), NewMaterial );
		NewMaterial->Expressions.Add( Expression );

		NewMaterial->BaseColor.Expression = Expression;
		Expression->Texture = InitialTexture;

		TArray<FExpressionOutput> Outputs;
		Outputs = Expression->GetOutputs();
		FExpressionOutput* Output = Outputs.GetTypedData();
		NewMaterial->BaseColor.Mask = Output->Mask;
		NewMaterial->BaseColor.MaskR = Output->MaskR;
		NewMaterial->BaseColor.MaskG = Output->MaskG;
		NewMaterial->BaseColor.MaskB = Output->MaskB;
		NewMaterial->BaseColor.MaskA = Output->MaskA;

		NewMaterial->PostEditChange();
	}

	return NewMaterial;
}


/*------------------------------------------------------------------------------
	UMaterialFunctionFactoryNew implementation.
------------------------------------------------------------------------------*/
UMaterialFunctionFactoryNew::UMaterialFunctionFactoryNew(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = UMaterialFunction::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UMaterialFunctionFactoryNew::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn)
{
	return StaticConstructObject(Class,InParent,Name,Flags);
}


/*------------------------------------------------------------------------------
	UMaterialParameterCollectionFactoryNew implementation.
------------------------------------------------------------------------------*/
UMaterialParameterCollectionFactoryNew::UMaterialParameterCollectionFactoryNew(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = UMaterialParameterCollection::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UMaterialParameterCollectionFactoryNew::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn)
{
	return StaticConstructObject(Class,InParent,Name,Flags);
}

/*------------------------------------------------------------------------------
	ULevelFactory.
------------------------------------------------------------------------------*/

/**
 * Iterates over an object's properties making sure that any UObjectProperty properties
 * that refer to non-NULL actors refer to valid actors.
 *
 * @return		false if no object references were NULL'd out, true otherwise.
 */
static bool ForceValidActorRefs(UStruct* Struct, uint8* Data)
{
	bool bChangedObjectPointer = false;

	//@todo DB: Optimize this!!
	for( TFieldIterator<UProperty> PropertyIt(Struct); PropertyIt; ++PropertyIt )
	{
		for( int32 i=0; i<PropertyIt->ArrayDim; i++ )
		{
			uint8* Value = PropertyIt->ContainerPtrToValuePtr<uint8>(Data, i);
			UObjectPropertyBase* Prop = Cast<UObjectPropertyBase>(*PropertyIt);
			if(Prop)
			{
				UObject* Obj = Prop->GetObjectPropertyValue(Value);
				AActor* SearchActor = Cast<AActor>(Obj);
				if( SearchActor && !Obj->HasAnyFlags(RF_ArchetypeObject|RF_ClassDefaultObject) )
				{
					bool bFound = false;
					for( FActorIterator ActorIt(SearchActor->GetWorld()); ActorIt; ++ActorIt )
					{
						AActor* Actor = *ActorIt;
						if( Actor == SearchActor )
						{
							bFound = true;
							break;
						}
					}
					
					if( !bFound )
					{
						UE_LOG(LogEditorFactories, Log,  TEXT("Usurped %s"), *Obj->GetClass()->GetName() );
						Prop->SetObjectPropertyValue(Value, NULL);
						bChangedObjectPointer = true;
					}
				}
			}
			else if( Cast<UStructProperty>(*PropertyIt) )
			{
				bChangedObjectPointer |= ForceValidActorRefs( ((UStructProperty*)*PropertyIt)->Struct, Value );
			}
		}
	}

	return bChangedObjectPointer;
}

ULevelFactory::ULevelFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SupportedClass = UWorld::StaticClass();
	Formats.Add(TEXT("t3d;Unreal World"));

	bCreateNew = false;
	bText = true;
	bEditorImport = false;
}

UObject* ULevelFactory::FactoryCreateText
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	EObjectFlags		Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const TCHAR*&		Buffer,
	const TCHAR*		BufferEnd,
	FFeedbackContext*	Warn
)
{
	FEditorDelegates::OnAssetPreImport.Broadcast(this, Class, InParent, Name, Type);

	UWorld* World = GWorld;
	//@todo locked levels - if lock state is persistent, do we need to check for whether the level is locked?
#ifdef MULTI_LEVEL_IMPORT
	// this level is the current level for pasting. If we get a named level, not for pasting, we will look up the level, and overwrite this
	ULevel*				OldCurrentLevel = World->GetCurrentLevel();
	check(OldCurrentLevel);
#endif

	UPackage* RootMapPackage = Cast<UPackage>(InParent);
	TMap<FString, UPackage*> MapPackages;
	TMap<AActor*, AActor*> MapActors;
	// Assumes data is being imported over top of a new, valid map.
	FParse::Next( &Buffer );
	if (GetBEGIN(&Buffer, TEXT("MAP")))
	{
		if (RootMapPackage)
		{
			FString MapName;
			if (FParse::Value(Buffer, TEXT("Name="), MapName))
			{
				// Advance the buffer
				Buffer += FCString::Strlen(TEXT("Name="));
				Buffer += MapName.Len();
				// Check to make sure that there are no naming conflicts
				if( RootMapPackage->Rename(*MapName, NULL, REN_Test | REN_ForceNoResetLoaders) )
				{
					// Rename it!
					RootMapPackage->Rename(*MapName, NULL, REN_ForceNoResetLoaders);
				}
				else
				{
					Warn->Logf(ELogVerbosity::Warning, TEXT("The Root map package name : '%s', conflicts with the existing object : '%s'"), *RootMapPackage->GetFullName(), *MapName);
					FEditorDelegates::OnAssetPostImport.Broadcast( this, NULL );
					return NULL;
				}
				
				// Stick it in the package map
				MapPackages.Add(MapName, RootMapPackage);
			}
		}
	}
	else
	{
		return World;
	}

	bool bIsExpectingNewMapTag = false;

	// Unselect all actors.
	GEditor->SelectNone( false, false );

	// Mark us importing a T3D (only from a file, not from copy/paste).
	GEditor->IsImportingT3D = (FCString::Stricmp(Type,TEXT("paste")) != 0) && (FCString::Stricmp(Type,TEXT("move")) != 0);
	GIsImportingT3D = GEditor->IsImportingT3D;

	// We need to detect if the .t3d file is the entire level or just selected actors, because we
	// don't want to replace the WorldSettings and BuildBrush if they already exist. To know if we
	// can skip the WorldSettings and BuilderBrush (which will always be the first two actors if the entire
	// level was exported), we make sure the first actor is a WorldSettings, if it is, and we already had
	// a WorldSettings, then we skip the builder brush
	// In other words, if we are importing a full level into a full level, we don't want to import
	// the WorldSettings and BuildBrush
	bool bShouldSkipImportSpecialActors = false;
	bool bHitLevelToken = false;

	FString MapPackageText;

	int32 ActorIndex = 0;

	//@todo locked levels - what needs to happen here?


	// Maintain a list of a new actors and the text they were created from.
	TMap<AActor*,FString> NewActorMap;
	TMap< FString, AGroupActor* > NewGroups; // Key=The orig actor's group's name, Value=The new actor's group.
	
	// Maintain a lookup for the new actors, keyed by their source FName.
	TMap<FName, AActor*> NewActorsFNames;

	// Maintain a lookup of the new actors to their parent and socket attachment if provided.
	struct FAttachmentDetail
	{
		const FName ParentName;
		const FName SocketName;
		FAttachmentDetail(const FName InParentName, const FName InSocketName) : ParentName(InParentName), SocketName(InSocketName) {}
	};
	TMap<AActor*,FAttachmentDetail> NewActorsAttachmentMap;

	FString StrLine;
	while( FParse::Line(&Buffer,StrLine) )
	{
		const TCHAR* Str = *StrLine;

		// If we're still waiting to see a 'MAP' tag, then check for that
		if( bIsExpectingNewMapTag )
		{
			if( GetBEGIN( &Str, TEXT("MAP")) )
			{
				bIsExpectingNewMapTag = false;
			}
			else
			{
				// Not a new map tag, so continue on
			}
		}
		else if( GetEND(&Str,TEXT("MAP")) )
		{
			// End of brush polys.
			bIsExpectingNewMapTag = true;
		}
		else if( GetBEGIN(&Str,TEXT("LEVEL")) )
		{
			bHitLevelToken = true;
#ifdef MULTI_LEVEL_IMPORT
			// try to look up the named level. if this fails, we will need to create a new level
			if (ParseObject<ULevel>(Str, TEXT("NAME="), World->GetCurrentLevel(), World->GetOuter()) == false)
			{
				// get the name
				FString LevelName;
				// if there is no name, that means we are pasting, so just put this guy into the CurrentLevel - don't make a new one
				if (FParse::Value(Str, TEXT("NAME="), LevelName))
				{
					// create a new named level
					World->SetCurrentLevel( new(World->GetOuter(), *LevelName)ULevel(FPostConstructInitializeProperties(),FURL(NULL)) );
				}
			}
#endif
		}
		else if( GetEND(&Str,TEXT("LEVEL")) )
		{
#ifdef MULTI_LEVEL_IMPORT
			// any actors outside of a level block go into the current level
			World->SetCurrentLevel( OldCurrentLevel );
#endif
		}
		else if( GetBEGIN(&Str,TEXT("ACTOR")) )
		{
			UClass* TempClass;
			if( ParseObject<UClass>( Str, TEXT("CLASS="), TempClass, ANY_PACKAGE ) )
			{
				// Get actor name.
				FName ActorUniqueName(NAME_None);
				FName ActorSourceName(NAME_None);
				FParse::Value( Str, TEXT("NAME="), ActorSourceName );
				ActorUniqueName = ActorSourceName;
				// Make sure this name is unique.
				AActor* Found=NULL;
				if( ActorUniqueName!=NAME_None )
				{
					// look in the current level for the same named actor
					Found = FindObject<AActor>( World->GetCurrentLevel(), *ActorUniqueName.ToString() );
				}
				if( Found )
				{
					ActorUniqueName = MakeUniqueObjectName( World->GetCurrentLevel(), TempClass, ActorUniqueName );
				}

				// Get parent name for attachment.
				FName ActorParentName(NAME_None);
				FParse::Value( Str, TEXT("ParentActor="), ActorParentName );

				// Get socket name for attachment.
				FName ActorParentSocket(NAME_None);
				FParse::Value( Str, TEXT("SocketName="), ActorParentSocket );

				// if an archetype was specified in the Begin Object block, use that as the template for the ConstructObject call.
				FString ArchetypeName;
				AActor* Archetype = NULL;
				if (FParse::Value(Str, TEXT("Archetype="), ArchetypeName))
				{
					// if given a name, break it up along the ' so separate the class from the name
					FString ObjectClass;
					FString ObjectPath;
					if ( FPackageName::ParseExportTextPath(ArchetypeName, &ObjectClass, &ObjectPath) )
					{
						// find the class
						UClass* ArchetypeClass = (UClass*)StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *ObjectClass);
						if ( ArchetypeClass )
						{
							if ( ArchetypeClass->IsChildOf(AActor::StaticClass()) )
							{
								// if we had the class, find the archetype
								Archetype = Cast<AActor>(StaticFindObject(ArchetypeClass, ANY_PACKAGE, *ObjectPath));
							}
							else
							{
								Warn->Logf(ELogVerbosity::Warning, TEXT("Invalid archetype specified in subobject definition '%s': %s is not a child of Actor"),
									Str, *ObjectClass);
							}
						}
					}
				}

				if (TempClass->IsChildOf(AWorldSettings::StaticClass()))
				{
					// if we see a WorldSettings, then we are importing an entire level, so if we
					// are importing into an existing level, then we should not import the next actor
					// which will be the builder brush
					check(ActorIndex == 0);

					// if we have any actors, then we are importing into an existing level
					if (World->GetCurrentLevel()->Actors.Num())
					{
						check(World->GetCurrentLevel()->Actors[0]->IsA(AWorldSettings::StaticClass()));

						// full level into full level, skip the first two actors
						bShouldSkipImportSpecialActors = true;
					}
				}

				// Get property text.
				FString PropText, PropertyLine;
				while
				(	GetEND( &Buffer, TEXT("ACTOR") )==0
				&&	FParse::Line( &Buffer, PropertyLine ) )
				{
					PropText += *PropertyLine;
					PropText += TEXT("\r\n");
				}

				// If we need to skip the WorldSettings and BuilderBrush, skip the first two actors.  Note that
				// at this point, we already know that we have a WorldSettings and BuilderBrush in the .t3d.
				if ( FLevelUtils::IsLevelLocked(World->GetCurrentLevel()) )
				{
					UE_LOG(LogEditorFactories, Warning, TEXT("Import actor: The requested operation could not be completed because the level is locked."));
					FEditorDelegates::OnAssetPostImport.Broadcast( this, NULL );
					return NULL;
				}
				else if ( !(bShouldSkipImportSpecialActors && ActorIndex < 2) )
				{
					// Don't import the default physics volume, as it doesn't have a UModel associated with it
					// and thus will not import properly.
					if ( !TempClass->IsChildOf(ADefaultPhysicsVolume::StaticClass()) )
					{
						// Create a new actor.
						FActorSpawnParameters SpawnInfo;
						SpawnInfo.Name = ActorUniqueName;
						SpawnInfo.Template = Archetype;
						SpawnInfo.bNoCollisionFail = true;
						AActor* NewActor = World->SpawnActor( TempClass, NULL, NULL, SpawnInfo );
						
						if( NewActor )
						{
							if( GEditor->bGroupingActive && !Cast<AGroupActor>(NewActor) )
							{
								bool bGrouped = false;

								AGroupActor** tmpNewGroup = NULL;
								// We need to add all the objects we selected into groups with new objects that were in their group before.
								FString GroupName;
								if(FParse::Value(Str, TEXT("GroupActor="), GroupName))
								{
									tmpNewGroup = NewGroups.Find(GroupName);
									bGrouped = true;
								}

								// Does the group exist?
								if(tmpNewGroup)
								{
									AGroupActor* NewActorGroup = *tmpNewGroup;

									// Add it to the group.
									NewActorGroup->Add(*NewActor);
								}
								else if(bGrouped)
								{
									// Create a new group and add the actor.
									AGroupActor* SpawnedGroupActor = NewActor->GetWorld()->SpawnActor<AGroupActor>();
									SpawnedGroupActor->Add(*NewActor);

									// Place the group in the map so we can find it later.
									NewGroups.Add( GroupName, SpawnedGroupActor);
									GEditor->SetActorLabelUnique(SpawnedGroupActor, GroupName);
								}

								// If we're copying a sub-group, add add duplicated group to original parent
								// If we're just copying an actor, only append it to the original parent group if unlocked
								if (Found)
								{
									AGroupActor* FoundParent = AGroupActor::GetParentForActor(Found);
									if(FoundParent && (Found->IsA(AGroupActor::StaticClass()) || !FoundParent->IsLocked()) )
									{
										FoundParent->Add(*NewActor);
									}
								}
							}

							// Store the new actor and the text it should be initialized with.
							NewActorMap.Add( NewActor, *PropText );

							// Store the copy to original actor mapping
							MapActors.Add(NewActor, Found);

							// Store the new actor against its source actor name (not the one that may have been made unique)
							if( ActorSourceName!=NAME_None )
							{
								NewActorsFNames.Add( ActorSourceName, NewActor );
							}

							// Store the new actor with its parent's FName, and socket FName if applicable
							if( ActorParentName!=NAME_None )
							{
								NewActorsAttachmentMap.Add( NewActor, FAttachmentDetail( ActorParentName, ActorParentSocket ) );
							}
						}
					}
				}

				// increment the number of actors we imported
				ActorIndex++;
			}
		}
		else if( GetBEGIN(&Str,TEXT("SURFACE")) )
		{
			UMaterialInterface* SrcMaterial = NULL;
			FVector SrcBase, SrcTextureU, SrcTextureV, SrcNormal;
			uint32 SrcPolyFlags = PF_DefaultFlags;
			int32 SurfacePropertiesParsed = 0;

			SrcBase = FVector::ZeroVector;
			SrcTextureU = FVector::ZeroVector;
			SrcTextureV = FVector::ZeroVector;
			SrcNormal = FVector::ZeroVector;

			bool bJustParsedTextureName = false;
			bool bFoundSurfaceEnd = false;
			bool bParsedLineSuccessfully = false;

			do
			{
				if( GetEND( &Buffer, TEXT("SURFACE") ) )
				{
					bFoundSurfaceEnd = true;
					bParsedLineSuccessfully = true;
				}
				else if( FParse::Command(&Buffer,TEXT("TEXTURE")) )
				{
					Buffer++;	// Move past the '=' sign

					FString TextureName;
					bParsedLineSuccessfully = FParse::Line(&Buffer, TextureName, true);
					if ( TextureName != TEXT("None") )
					{
						SrcMaterial = Cast<UMaterialInterface>(StaticLoadObject( UMaterialInterface::StaticClass(), NULL, *TextureName, NULL, LOAD_NoWarn, NULL ));
					}
					bJustParsedTextureName = true;
					SurfacePropertiesParsed++;
				}
				else if( FParse::Command(&Buffer,TEXT("BASE")) )
				{
					GetFVECTOR( Buffer, SrcBase );
					SurfacePropertiesParsed++;
				}
				else if( FParse::Command(&Buffer,TEXT("TEXTUREU")) )
				{
					GetFVECTOR( Buffer, SrcTextureU );
					SurfacePropertiesParsed++;
				}
				else if( FParse::Command(&Buffer,TEXT("TEXTUREV")) )
				{
					GetFVECTOR( Buffer, SrcTextureV );
					SurfacePropertiesParsed++;
				}
				else if( FParse::Command(&Buffer,TEXT("NORMAL")) )
				{
					GetFVECTOR( Buffer, SrcNormal );
					SurfacePropertiesParsed++;
				}
				else if( FParse::Command(&Buffer,TEXT("POLYFLAGS")) )
				{
					FParse::Value( Buffer, TEXT("="), SrcPolyFlags );
					SurfacePropertiesParsed++;
				}

				// Parse to the next line only if the texture name wasn't just parsed or if the 
				// end of surface isn't parsed. Don't parse to the next line for the texture 
				// name because a FParse::Line() is called when retrieving the texture name. 
				// Doing another FParse::Line() would skip past a necessary surface property.
				if( !bJustParsedTextureName && !bFoundSurfaceEnd )
				{
					FString DummyLine;
					bParsedLineSuccessfully = FParse::Line( &Buffer, DummyLine );
				}

				// Reset this bool so that we can parse lines starting during next iteration.
				bJustParsedTextureName = false;
			}
			while( !bFoundSurfaceEnd && bParsedLineSuccessfully );

			// There are 6 BSP surface properties exported via T3D. If there wasn't 6 properties 
			// successfully parsed, the parsing failed. This surface isn't valid then.
			if( SurfacePropertiesParsed == 6 )
			{
				const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "PasteTextureToSurface", "Paste Texture to Surface") );

				for( int32 j = 0; j < World->GetNumLevels(); ++j )
				{
					ULevel* CurrentLevel = World->GetLevel(j);
					for( int32 i = 0 ; i < CurrentLevel->Model->Surfs.Num() ; i++ )
					{
						FBspSurf* DstSurf = &CurrentLevel->Model->Surfs[i];

						if( DstSurf->PolyFlags & PF_Selected )
						{
							CurrentLevel->Model->ModifySurf( i, 1 );

							const FVector DstNormal = CurrentLevel->Model->Vectors[ DstSurf->vNormal ];

							// Need to compensate for changes in the polygon normal.
							const FRotator SrcRot = SrcNormal.Rotation();
							const FRotator DstRot = DstNormal.Rotation();
							const FRotationMatrix RotMatrix( DstRot - SrcRot );

							FVector NewBase	= RotMatrix.TransformPosition( SrcBase );
							FVector NewTextureU = RotMatrix.TransformVector( SrcTextureU );
							FVector NewTextureV = RotMatrix.TransformVector( SrcTextureV );

							DstSurf->Material = SrcMaterial;
							DstSurf->pBase = FBSPOps::bspAddPoint( CurrentLevel->Model, &NewBase, 1 );
							DstSurf->vTextureU = FBSPOps::bspAddVector( CurrentLevel->Model, &NewTextureU, 0 );
							DstSurf->vTextureV = FBSPOps::bspAddVector( CurrentLevel->Model, &NewTextureV, 0 );
							DstSurf->PolyFlags = SrcPolyFlags;

							DstSurf->PolyFlags &= ~PF_Selected;

							CurrentLevel->MarkPackageDirty();

							GEditor->polyUpdateMaster( CurrentLevel->Model, i, 1 );
						}
					}
				}
			}
		}
		else if (GetBEGIN(&Str,TEXT("MAPPACKAGE")))
		{
			// Get all the text.
			while ((GetEND(&Buffer, TEXT("MAPPACKAGE") )==0) && FParse::Line(&Buffer, StrLine))
			{
				MapPackageText += *StrLine;
				MapPackageText += TEXT("\r\n");
			}
		}
	}

	// Import actor properties.
	// We do this after creating all actors so that actor references can be matched up.
	AWorldSettings* WorldSettings = World->GetWorldSettings();

	if (GIsImportingT3D && (MapPackageText.Len() > 0))
	{
		UPackageFactory* PackageFactory = new UPackageFactory(FPostConstructInitializeProperties());
		check(PackageFactory);

		FName NewPackageName(*(RootMapPackage->GetName()));

		const TCHAR* MapPkg_BufferStart = *MapPackageText;
		const TCHAR* MapPkg_BufferEnd = MapPkg_BufferStart + MapPackageText.Len();
		PackageFactory->FactoryCreateText(UPackage::StaticClass(), NULL, NewPackageName, RF_NoFlags, 0, TEXT("T3D"), MapPkg_BufferStart, MapPkg_BufferEnd, Warn);
	}

	// Pass 1: Sort out all the properties on the individual actors
	bool bIsMoveToStreamingLevel =(FCString::Stricmp(Type, TEXT("move")) == 0);
	for( FActorIterator It(World); It; ++It )
	{
		AActor* Actor = *It;

		// Import properties if the new actor is 
		bool		bActorChanged = false;
		FString*	PropText = NewActorMap.Find(Actor);
		if( PropText )
		{
			if ( Actor->ShouldImport(PropText, bIsMoveToStreamingLevel) )
			{
				Actor->PreEditChange(NULL);
				ImportObjectProperties( (uint8*)Actor, **PropText, Actor->GetClass(), Actor, Actor, Warn, 0 );
				bActorChanged = true;

				GEditor->SelectActor( Actor, true, false, true );
			}
			else // This actor is new, but rejected to import its properties, so just delete...
			{
				Actor->Destroy();
			}
		}
		else
		if( !Actor->IsA(AInstancedFoliageActor::StaticClass()) )
		{
			// This actor is old
		}

		// If this is a newly imported static brush, validate it.  If it's a newly imported dynamic brush, rebuild it.
		// Previously, this just called bspValidateBrush.  However, that caused the dynamic brushes which require a valid BSP tree
		// to be built to break after being duplicated.  Calling RebuildBrush will rebuild the BSP tree from the imported polygons.
		ABrush* Brush = Cast<ABrush>(Actor);
		if( bActorChanged && Brush && Brush->Brush )
		{
			const bool bIsStaticBrush = Brush->IsStaticBrush();
			if( bIsStaticBrush )
			{
				FBSPOps::bspValidateBrush( Brush->Brush, true, false );
			}
			else
			{
				FBSPOps::RebuildBrush( Brush->Brush );
			}
		}

		// Make sure all references to actors are valid.
		// if they don't belong to same level
		UWorld * ActorWorld = Actor->GetTypedOuter<UWorld>();
		if( ActorWorld != World )
		{
			const bool bFixedUpObjectRefs = ForceValidActorRefs( Actor->GetClass(), (uint8*)Actor );

			// Aactor references were fixed up, so treat the actor as having been changed.
			if ( bFixedUpObjectRefs )
			{
				bActorChanged = true;
			}
		}

		// Copy brushes' model pointers over to their BrushComponent, to keep compatibility with old T3Ds.
		if( Brush && bActorChanged )
		{
			if( Brush->BrushComponent ) // Should always be the case, but not asserting so that old broken content won't crash.
			{
				Brush->BrushComponent->Brush = Brush->Brush;

				// We need to avoid duplicating default/ builder brushes. This is done by destroying all brushes that are CSG_Active and are not
				// the default brush in their respective levels.
				if( Brush->IsStaticBrush() && Brush->BrushType==Brush_Default )
				{
					bool bIsDefaultBrush = false;
					
					// Iterate over all levels and compare current actor to the level's default brush.
					for( int32 LevelIndex=0; LevelIndex<World->GetNumLevels(); LevelIndex++ )
					{
						ULevel* Level = World->GetLevel(LevelIndex);
						if( Level->GetBrush() == Brush )
						{
							bIsDefaultBrush = true;
							break;
						}
					}

					// Destroy actor if it's a builder brush but not the default brush in any of the currently loaded levels.
					if( !bIsDefaultBrush )
					{
						World->DestroyActor( Brush );

						// Since the actor has been destroyed, skip the rest of this iteration of the loop.
						continue;
					}
				}
			}
		}
		
		// If the actor was imported . . .
		if( bActorChanged )
		{
			// Let the actor deal with having been imported, if desired.
			Actor->PostEditImport();

			// Notify actor its properties have changed.
			Actor->PostEditChange();
		}
	}

	// Pass 2: Sort out any attachment parenting on the new actors now that all actors have the correct properties set
	for( auto It = MapActors.CreateIterator(); It; ++It )
	{
		AActor* const Actor = It.Key();

		// Fixup parenting
		FAttachmentDetail* ActorAttachmentDetail = NewActorsAttachmentMap.Find( Actor );
		if( ActorAttachmentDetail != NULL )
		{
			AActor* ActorParent = NULL;
			// Try to find the new copy of the parent
			AActor** NewActorParent = NewActorsFNames.Find( ActorAttachmentDetail->ParentName );
			if ( NewActorParent != NULL )
			{
				ActorParent = *NewActorParent;
			}
			// Try to find an already existing parent
			if( ActorParent == NULL )
			{
				ActorParent = FindObject<AActor>( World->GetCurrentLevel(), *ActorAttachmentDetail->ParentName.ToString() );
			}
			// Parent the actors
			if( ActorParent != NULL )
			{
				// Make sure our parent isn't selected (would cause GEditor->ParentActors to fail)
				const bool bParentWasSelected = ActorParent->IsSelected();
				if(bParentWasSelected)
				{
					GEditor->SelectActor(ActorParent, false, false, true);
				}

				GEditor->ParentActors( ActorParent, Actor, ActorAttachmentDetail->SocketName );

				if(bParentWasSelected)
				{
					GEditor->SelectActor(ActorParent, true, false, true);
				}
			}
		}
	}

	// Go through all the groups we added and finalize them.
	for(TMap< FString, AGroupActor* >::TIterator It(NewGroups); It; ++It)
	{
		It.Value()->CenterGroupLocation();
		It.Value()->Lock();
	}

	// Mark us as no longer importing a T3D.
	GEditor->IsImportingT3D = 0;
	GIsImportingT3D = false;

	FEditorDelegates::OnAssetPostImport.Broadcast( this, World );

	return World;
}

/*-----------------------------------------------------------------------------
	UPackageFactory.
-----------------------------------------------------------------------------*/
UPackageFactory::UPackageFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SupportedClass = UPackage::StaticClass();
	Formats.Add(TEXT("T3DPKG;Unreal Package"));

	bCreateNew = false;
	bText = true;
	bEditorImport = false;
}

UObject* UPackageFactory::FactoryCreateText( UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn )
{
	FEditorDelegates::OnAssetPreImport.Broadcast(this, Class, InParent, Name, Type);

	bool bSavedImportingT3D = GIsImportingT3D;
	// Mark us as importing a T3D.
	GEditor->IsImportingT3D = true;
	GIsImportingT3D = true;

	if (InParent != NULL)
	{
		return NULL;
	}

	TMap<FString, UPackage*> MapPackages;
	bool bImportingMapPackage = false;

	UPackage* TopLevelPackage = NULL;
	UPackage* RootMapPackage = NULL;
	UWorld* World = GWorld;
	if (World)
	{
		RootMapPackage = World->GetOutermost();
	}

	if (RootMapPackage)
	{
		if (RootMapPackage->GetName() == Name.ToString())
		{
			// Loading into the Map package!
			MapPackages.Add(RootMapPackage->GetName(), RootMapPackage);
			TopLevelPackage = RootMapPackage;
			bImportingMapPackage = true;
		}
	}

	// Unselect all actors.
	GEditor->SelectNone( false, false );

	// Mark us importing a T3D (only from a file, not from copy/paste).
	GEditor->IsImportingT3D = FCString::Stricmp(Type,TEXT("paste")) != 0;
	GIsImportingT3D = GEditor->IsImportingT3D;

	// Maintain a list of a new package objects and the text they were created from.
	TMap<UObject*,FString> NewPackageObjectMap;

	FString StrLine;
	while( FParse::Line(&Buffer,StrLine) )
	{
		const TCHAR* Str = *StrLine;

		if (GetBEGIN(&Str, TEXT("TOPLEVELPACKAGE")) && !bImportingMapPackage)
		{
			//Begin TopLevelPackage Class=Package Name=ExportTest_ORIG Archetype=Package'Core.Default__Package'
			UClass* TempClass;
			if( ParseObject<UClass>( Str, TEXT("CLASS="), TempClass, ANY_PACKAGE ) )
			{
				// Get actor name.
				FName PackageName(NAME_None);
				FParse::Value( Str, TEXT("NAME="), PackageName );

				if (FindObject<UPackage>(ANY_PACKAGE, *(PackageName.ToString())))
				{
					UE_LOG(LogEditorFactories, Warning, TEXT("Package factory can only handle the map package or new packages!"));
					return NULL;
				}
				TopLevelPackage = CreatePackage(NULL, *(PackageName.ToString()));
				TopLevelPackage->SetFlags(RF_Standalone|RF_Public);
				MapPackages.Add(TopLevelPackage->GetName(), TopLevelPackage);

				// if an archetype was specified in the Begin Object block, use that as the template for the ConstructObject call.
				FString ArchetypeName;
				AActor* Archetype = NULL;
				if (FParse::Value(Str, TEXT("Archetype="), ArchetypeName))
				{
				}
			}
		}
		else if (GetBEGIN(&Str,TEXT("PACKAGE")))
		{
			FString ParentPackageName;
			FParse::Value(Str, TEXT("PARENTPACKAGE="), ParentPackageName);
			UClass* PkgClass;
			if (ParseObject<UClass>(Str, TEXT("CLASS="), PkgClass, ANY_PACKAGE))
			{
				// Get the name of the object.
				FName NewPackageName(NAME_None);
				FParse::Value(Str, TEXT("NAME="), NewPackageName);

				// if an archetype was specified in the Begin Object block, use that as the template for the ConstructObject call.
				FString ArchetypeName;
				UPackage* Archetype = NULL;
				if (FParse::Value(Str, TEXT("Archetype="), ArchetypeName))
				{
					// if given a name, break it up along the ' so separate the class from the name
					FString ObjectClass;
					FString ObjectPath;
					if ( FPackageName::ParseExportTextPath(ArchetypeName, &ObjectClass, &ObjectPath) )
					{
						// find the class
						UClass* ArchetypeClass = (UClass*)StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *ObjectClass);
						if (ArchetypeClass)
						{
							if (ArchetypeClass->IsChildOf(UPackage::StaticClass()))
							{
								// if we had the class, find the archetype
								Archetype = Cast<UPackage>(StaticFindObject(ArchetypeClass, ANY_PACKAGE, *ObjectPath));
							}
							else
							{
								Warn->Logf(ELogVerbosity::Warning, TEXT("Invalid archetype specified in subobject definition '%s': %s is not a child of Package"),
									Str, *ObjectClass);
							}
						}
					}

					UPackage* ParentPkg = NULL;
					UPackage** ppParentPkg = MapPackages.Find(ParentPackageName);
					if (ppParentPkg)
					{
						ParentPkg = *ppParentPkg;
					}
					check(ParentPkg);

					UPackage* NewPackage = Cast<UPackage>(StaticConstructObject(UPackage::StaticClass(), ParentPkg, NewPackageName, RF_NoFlags, Archetype));
					check(NewPackage);
					NewPackage->SetFlags(RF_Standalone|RF_Public);
					MapPackages.Add(NewPackageName.ToString(), NewPackage);
				}
			}
		}
	}

	for (FObjectIterator ObjIt; ObjIt; ++ObjIt)
	{
		UObject* LoadObject = *ObjIt;

		if (LoadObject)
		{
			bool bModifiedObject = false;

			FString* PropText = NewPackageObjectMap.Find(LoadObject);
			if (PropText)
			{
				LoadObject->PreEditChange(NULL);
				ImportObjectProperties((uint8*)LoadObject, **PropText, LoadObject->GetClass(), LoadObject, LoadObject, Warn, 0 );
				bModifiedObject = true;
			}

			if (bModifiedObject)
			{
				// Let the actor deal with having been imported, if desired.
				LoadObject->PostEditImport();
				// Notify actor its properties have changed.
				LoadObject->PostEditChange();
				LoadObject->SetFlags(RF_Standalone | RF_Public);
				LoadObject->MarkPackageDirty();
			}
		}
	}

	// Mark us as no longer importing a T3D.
	GEditor->IsImportingT3D = bSavedImportingT3D;
	GIsImportingT3D = bSavedImportingT3D;

	FEditorDelegates::OnAssetPostImport.Broadcast( this, TopLevelPackage );

	return TopLevelPackage;
}

/*-----------------------------------------------------------------------------
	UPolysFactory.
-----------------------------------------------------------------------------*/

UPolysFactory::UPolysFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = UPolys::StaticClass();
	Formats.Add(TEXT("t3d;Unreal brush text"));
	bCreateNew = false;
	bText = true;
}

UObject* UPolysFactory::FactoryCreateText
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	EObjectFlags		Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const TCHAR*&		Buffer,
	const TCHAR*		BufferEnd,
	FFeedbackContext*	Warn
)
{
	FEditorDelegates::OnAssetPreImport.Broadcast(this, Class, InParent, Name, Type);

	// Create polys.
	UPolys* Polys = Context ? CastChecked<UPolys>(Context) : new(InParent,Name,Flags)UPolys(FPostConstructInitializeProperties());

	// Eat up if present.
	GetBEGIN( &Buffer, TEXT("POLYLIST") );

	// Parse all stuff.
	int32 First=1, GotBase=0;
	FString StrLine, ExtraLine;
	FPoly Poly;
	while( FParse::Line( &Buffer, StrLine ) )
	{
		const TCHAR* Str = *StrLine;
		if( GetEND(&Str,TEXT("POLYLIST")) )
		{
			// End of brush polys.
			break;
		}
		//
		//
		// AutoCad - DXF File
		//
		//
		else if( FCString::Strstr(Str,TEXT("ENTITIES")) && First )
		{
			UE_LOG(LogEditorFactories, Log, TEXT("Reading Autocad DXF file"));
			int32 Started=0, NumPts=0, IsFace=0;
			FVector PointPool[4096];
			FPoly NewPoly; NewPoly.Init();

			while
			(	FParse::Line( &Buffer, StrLine, 1 )
			&&	FParse::Line( &Buffer, ExtraLine, 1 ) )
			{
				// Handle the line.
				Str = *ExtraLine;
				int32 Code = FCString::Atoi(*StrLine);
				if( Code==0 )
				{
					// Finish up current poly.
					if( Started )
					{
						if( NewPoly.Vertices.Num() == 0 )
						{
							// Got a vertex definition.
							NumPts++;
						}
						else if( NewPoly.Vertices.Num()>=3 )
						{
							// Got a poly definition.
							if( IsFace ) NewPoly.Reverse();
							NewPoly.Base = NewPoly.Vertices[0];
							NewPoly.Finalize(NULL,0);
							new(Polys->Element)FPoly( NewPoly );
						}
						else
						{
							// Bad.
							Warn->Logf( TEXT("DXF: Bad vertex count %i"), NewPoly.Vertices.Num() );
						}
						
						// Prepare for next.
						NewPoly.Init();
					}
					Started=0;

					if( FParse::Command(&Str,TEXT("VERTEX")) )
					{
						// Start of new vertex.
						PointPool[NumPts] = FVector::ZeroVector;
						Started = 1;
						IsFace  = 0;
					}
					else if( FParse::Command(&Str,TEXT("3DFACE")) )
					{
						// Start of 3d face definition.
						Started = 1;
						IsFace  = 1;
					}
					else if( FParse::Command(&Str,TEXT("SEQEND")) )
					{
						// End of sequence.
						NumPts=0;
					}
					else if( FParse::Command(&Str,TEXT("EOF")) )
					{
						// End of file.
						break;
					}
				}
				else if( Started )
				{
					// Replace commas with periods to handle european dxf's.
					//for( TCHAR* Stupid = FCString::Strchr(*ExtraLine,','); Stupid; Stupid=FCString::Strchr(Stupid,',') )
					//	*Stupid = '.';

					// Handle codes.
					if( Code>=10 && Code<=19 )
					{
						// X coordinate.
						int32 VertexIndex = Code-10;
						if( IsFace && VertexIndex >= NewPoly.Vertices.Num() )
						{
							NewPoly.Vertices.AddZeroed(VertexIndex - NewPoly.Vertices.Num() + 1);
						}
						NewPoly.Vertices[VertexIndex].X = PointPool[NumPts].X = FCString::Atof(*ExtraLine);
					}
					else if( Code>=20 && Code<=29 )
					{
						// Y coordinate.
						int32 VertexIndex = Code-20;
						NewPoly.Vertices[VertexIndex].Y = PointPool[NumPts].Y = FCString::Atof(*ExtraLine);
					}
					else if( Code>=30 && Code<=39 )
					{
						// Z coordinate.
						int32 VertexIndex = Code-30;
						NewPoly.Vertices[VertexIndex].Z = PointPool[NumPts].Z = FCString::Atof(*ExtraLine);
					}
					else if( Code>=71 && Code<=79 && (Code-71)==NewPoly.Vertices.Num() )
					{
						int32 iPoint = FMath::Abs(FCString::Atoi(*ExtraLine));
						if( iPoint>0 && iPoint<=NumPts )
							new(NewPoly.Vertices) FVector(PointPool[iPoint-1]);
						else UE_LOG(LogEditorFactories, Warning, TEXT("DXF: Invalid point index %i/%i"), iPoint, NumPts );
					}
				}
			}
		}
		//
		//
		// 3D Studio MAX - ASC File
		//
		//
		else if( FCString::Strstr(Str,TEXT("Tri-mesh,")) && First )
		{
			UE_LOG(LogEditorFactories, Log,  TEXT("Reading 3D Studio ASC file") );
			FVector PointPool[4096];

			AscReloop:
			int32 NumVerts = 0, TempNumPolys=0, TempVerts=0;
			while( FParse::Line( &Buffer, StrLine ) )
			{
				Str = *StrLine;

				FString VertText = FString::Printf( TEXT("Vertex %i:"), NumVerts );
				FString FaceText = FString::Printf( TEXT("Face %i:"), TempNumPolys );
				if( FCString::Strstr(Str,*VertText) )
				{
					PointPool[NumVerts].X = FCString::Atof(FCString::Strstr(Str,TEXT("X:"))+2);
					PointPool[NumVerts].Y = FCString::Atof(FCString::Strstr(Str,TEXT("Y:"))+2);
					PointPool[NumVerts].Z = FCString::Atof(FCString::Strstr(Str,TEXT("Z:"))+2);
					NumVerts++;
					TempVerts++;
				}
				else if( FCString::Strstr(Str,*FaceText) )
				{
					Poly.Init();
					new(Poly.Vertices)FVector(PointPool[FCString::Atoi(FCString::Strstr(Str,TEXT("A:"))+2)]);
					new(Poly.Vertices)FVector(PointPool[FCString::Atoi(FCString::Strstr(Str,TEXT("B:"))+2)]);
					new(Poly.Vertices)FVector(PointPool[FCString::Atoi(FCString::Strstr(Str,TEXT("C:"))+2)]);
					Poly.Base = Poly.Vertices[0];
					Poly.Finalize(NULL,0);
					new(Polys->Element)FPoly(Poly);
					TempNumPolys++;
				}
				else if( FCString::Strstr(Str,TEXT("Tri-mesh,")) )
					goto AscReloop;
			}
			UE_LOG(LogEditorFactories, Log,  TEXT("Imported %i vertices, %i faces"), TempVerts, Polys->Element.Num() );
		}
		//
		//
		// T3D FORMAT
		//
		//
		else if( GetBEGIN(&Str,TEXT("POLYGON")) )
		{
			// Init to defaults and get group/item and texture.
			Poly.Init();
			FParse::Value( Str, TEXT("LINK="), Poly.iLink );
			FParse::Value( Str, TEXT("ITEM="), Poly.ItemName );
			FParse::Value( Str, TEXT("FLAGS="), Poly.PolyFlags );
			FParse::Value( Str, TEXT("LightMapScale="), Poly.LightMapScale );
			Poly.PolyFlags &= ~PF_NoImport;

			FString TextureName;
			// only load the texture if it was present
			if (FParse::Value( Str, TEXT("TEXTURE="), TextureName ))
			{
				Poly.Material = Cast<UMaterialInterface>(StaticFindObject( UMaterialInterface::StaticClass(), ANY_PACKAGE, *TextureName ) );
/***
				if (Poly.Material == NULL)
				{
					Poly.Material = Cast<UMaterialInterface>(StaticLoadObject( UMaterialInterface::StaticClass(), NULL, *TextureName, NULL,  LOAD_NoWarn, NULL ) );
				}
***/
			}
		}
		else if( FParse::Command(&Str,TEXT("PAN")) )
		{
			int32	PanU = 0,
				PanV = 0;

			FParse::Value( Str, TEXT("U="), PanU );
			FParse::Value( Str, TEXT("V="), PanV );

			Poly.Base += Poly.TextureU * PanU;
			Poly.Base += Poly.TextureV * PanV;
		}
		else if( FParse::Command(&Str,TEXT("ORIGIN")) )
		{
			GotBase=1;
			GetFVECTOR( Str, Poly.Base );
		}
		else if( FParse::Command(&Str,TEXT("VERTEX")) )
		{
			FVector TempVertex;
			GetFVECTOR( Str, TempVertex );
			new(Poly.Vertices) FVector(TempVertex);
		}
		else if( FParse::Command(&Str,TEXT("TEXTUREU")) )
		{
			GetFVECTOR( Str, Poly.TextureU );
		}
		else if( FParse::Command(&Str,TEXT("TEXTUREV")) )
		{
			GetFVECTOR( Str, Poly.TextureV );
		}
		else if( GetEND(&Str,TEXT("POLYGON")) )
		{
			if( !GotBase )
				Poly.Base = Poly.Vertices[0];
			if( Poly.Finalize(NULL,1)==0 )
				new(Polys->Element)FPoly(Poly);
			GotBase=0;
		}
	}

	FEditorDelegates::OnAssetPostImport.Broadcast( this, Polys );

	// Success.
	return Polys;
}

/*-----------------------------------------------------------------------------
	UModelFactory.
-----------------------------------------------------------------------------*/
UModelFactory::UModelFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SupportedClass = UModel::StaticClass();
	Formats.Add(TEXT("t3d;Unreal model text"));
	bCreateNew = false;
	bText = true;
}

UObject* UModelFactory::FactoryCreateText
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	EObjectFlags		Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const TCHAR*&		Buffer,
	const TCHAR*		BufferEnd,
	FFeedbackContext*	Warn
)
{
	FEditorDelegates::OnAssetPreImport.Broadcast(this, Class, InParent, Name, Type);

	ABrush* TempOwner = (ABrush*)Context;
	UModel* Model = new( InParent, Name, Flags )UModel( FPostConstructInitializeProperties(),TempOwner, 1 );

	const TCHAR* StrPtr;
	FString StrLine;
	if( TempOwner )
	{
		TempOwner->InitPosRotScale();
		GEditor->GetSelectedActors()->Deselect( TempOwner );
	}
	while( FParse::Line( &Buffer, StrLine ) )
	{
		StrPtr = *StrLine;
		if( GetEND(&StrPtr,TEXT("BRUSH")) )
		{
			break;
		}
		else if( GetBEGIN (&StrPtr,TEXT("POLYLIST")) )
		{
			UPolysFactory* PolysFactory = new UPolysFactory(FPostConstructInitializeProperties());
			Model->Polys = (UPolys*)PolysFactory->FactoryCreateText(UPolys::StaticClass(),Model,NAME_None,RF_Transactional,NULL,Type,Buffer,BufferEnd,Warn);
			check(Model->Polys);
		}
		if( TempOwner )
		{
			if(FParse::Command(&StrPtr,TEXT("PREPIVOT"	))) 
			{
				FVector TempPrePivot(0.f);
				GetFVECTOR 	(StrPtr,TempPrePivot);
				TempOwner->SetPrePivot(TempPrePivot);
			}
			else if (FParse::Command(&StrPtr,TEXT("LOCATION"	))) 
			{
				FVector NewLocation(0.f);
				GetFVECTOR	(StrPtr,NewLocation);
				TempOwner->SetActorLocation(NewLocation, false);
			}
			else if (FParse::Command(&StrPtr,TEXT("ROTATION"	))) 
			{
				FRotator NewRotation;
				GetFROTATOR  (StrPtr,NewRotation,1);
				TempOwner->SetActorRotation(NewRotation);
			}
			if( FParse::Command(&StrPtr,TEXT("SETTINGS")) )
			{
				uint8 BrushType = (uint8)TempOwner->BrushType;
				FParse::Value( StrPtr, TEXT("BRUSHTYPE="), BrushType);
				TempOwner->BrushType = EBrushType(BrushType);
				FParse::Value( StrPtr, TEXT("POLYFLAGS="), TempOwner->PolyFlags );
			}
		}
	}

	FEditorDelegates::OnAssetPostImport.Broadcast( this, Model );

	return Model;
}

void InsertSoundNode( USoundCue* SoundCue, UClass* NodeClass, int32 NodeIndex )
{
	USoundNode* SoundNode = SoundCue->ConstructSoundNode<USoundNode>( NodeClass );

	// If this node allows >0 children but by default has zero - create a connector for starters
	if( SoundNode->GetMaxChildNodes() > 0 && SoundNode->ChildNodes.Num() == 0 )
	{
		SoundNode->CreateStartingConnectors();
	}

	SoundNode->GraphNode->NodePosX = -150 * NodeIndex - 100;
	SoundNode->GraphNode->NodePosY = -35;

	// Link the node to the cue.
	SoundNode->ChildNodes[ 0 ] = SoundCue->FirstNode;

	// Link the attenuation node to root.
	SoundCue->FirstNode = SoundNode;

	SoundCue->LinkGraphNodesFromSoundNodes();
}

void CreateSoundCue( USoundWave* Sound, UObject* InParent, EObjectFlags Flags, bool bIncludeAttenuationNode, bool bIncludeModulatorNode, bool bIncludeLoopingNode, float CueVolume )
{
	// then first create the actual sound cue
	FString SoundCueName = FString::Printf( TEXT( "%s_Cue" ), *Sound->GetName() );

	// Create sound cue and wave player
	USoundCue* SoundCue = ConstructObject<USoundCue>( USoundCue::StaticClass(), InParent, *SoundCueName, Flags );
	USoundNodeWavePlayer* WavePlayer = SoundCue->ConstructSoundNode<USoundNodeWavePlayer>(); 

	int32 NodeIndex = ( int32 )bIncludeAttenuationNode + ( int32 )bIncludeModulatorNode + ( int32 )bIncludeLoopingNode;

	WavePlayer->GraphNode->NodePosX = -150 * NodeIndex - 100;
	WavePlayer->GraphNode->NodePosY = -35;

	// Apply the initial volume.
	SoundCue->VolumeMultiplier = CueVolume;

	WavePlayer->SoundWave = Sound;
	SoundCue->FirstNode = WavePlayer;
	SoundCue->LinkGraphNodesFromSoundNodes();

	if( bIncludeLoopingNode )
	{
		WavePlayer->bLooping = true;
	}

	if( bIncludeModulatorNode )
	{
		InsertSoundNode( SoundCue, USoundNodeModulator::StaticClass(), --NodeIndex );
	}

	if( bIncludeAttenuationNode )
	{
		InsertSoundNode( SoundCue, USoundNodeAttenuation::StaticClass(), --NodeIndex );
	}

	// Make sure the content browser finds out about this newly-created object.  This is necessary when sound
	// cues are created automatically after creating a sound node wave.  See use of bAutoCreateCue in USoundTTSFactory.
	if( ( Flags & ( RF_Public | RF_Standalone ) ) != 0 )
	{
		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(SoundCue);
	}
}

/*-----------------------------------------------------------------------------
	UReverbEffectFactory.
-----------------------------------------------------------------------------*/
UReverbEffectFactory::UReverbEffectFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = UReverbEffect::StaticClass();

	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* UReverbEffectFactory::FactoryCreateNew( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn )
{
	UReverbEffect* ReverbEffect = ConstructObject<UReverbEffect>( UReverbEffect::StaticClass(), InParent, InName, Flags );

	return ReverbEffect;
}

/*-----------------------------------------------------------------------------
	USoundFactory.
-----------------------------------------------------------------------------*/

static bool bSoundFactorySuppressImportOverwriteDialog = false;

USoundFactory::USoundFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{


	SupportedClass = USoundWave::StaticClass();
	Formats.Add(TEXT("wav;Sound"));

	bCreateNew = false;
	bAutoCreateCue = false;
	bIncludeAttenuationNode = false;
	bIncludeModulatorNode = false;
	bIncludeLoopingNode = false;
	CueVolume = 0.75f;
	CuePackageSuffix = TEXT("_Cue");
	bEditorImport = true;
}


UObject* USoundFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	EObjectFlags		Flags,
	UObject*			Context,
	const TCHAR*		FileType,
	const uint8*&		Buffer,
	const uint8*			BufferEnd,
	FFeedbackContext*	Warn
)
{
	FEditorDelegates::OnAssetPreImport.Broadcast(this, Class, InParent, Name, FileType);

	if(	FCString::Stricmp( FileType, TEXT( "WAV" ) ) == 0 )
	{
		// create the group name for the cue
		const FString GroupName = InParent->GetFullGroupName( false );
		FString CuePackageName = InParent->GetOutermost()->GetName();
		CuePackageName += CuePackageSuffix;
		if( GroupName.Len() > 0 && GroupName != TEXT( "None" ) )
		{
			CuePackageName += TEXT( "." );
			CuePackageName += GroupName;
		}

		// validate the cue's group
		FText Reason;
		const bool bCuePathIsValid = FName( *CuePackageSuffix ).IsValidGroupName( Reason );
		const bool bMoveCue = CuePackageSuffix.Len() > 0 && bCuePathIsValid && bAutoCreateCue;
		if( bAutoCreateCue )
		{
			if( !bCuePathIsValid )
			{
				FMessageDialog::Open( EAppMsgType::Ok, FText::Format(NSLOCTEXT("UnrealEd", "Error_ImportFailed_f", "Import failed for {0}: {1}"), FText::FromString(CuePackageName), Reason) );
				FEditorDelegates::OnAssetPostImport.Broadcast( this, NULL );
				return NULL;
			}
		}	

		// if we are creating the cue move it when necessary
		UPackage* CuePackage = bMoveCue ? CreatePackage( NULL, *CuePackageName ) : NULL;

		// if the sound already exists, remember the user settings
		USoundWave* ExistingSound = FindObject<USoundWave>( InParent, *Name.ToString() );

		// TODO - Audio Threading. This needs to be sent to the audio device and wait on stopping the sounds
		TArray<UAudioComponent*> ComponentsToRestart;
		FAudioDevice* AudioDevice = GEngine->GetAudioDevice();
		if (AudioDevice && ExistingSound)
		{
			AudioDevice->StopSoundsForReimport(ExistingSound, ComponentsToRestart);
		}

		bool bUseExistingSettings = bSoundFactorySuppressImportOverwriteDialog;

		if( ExistingSound && !bSoundFactorySuppressImportOverwriteDialog && !GIsAutomationTesting )
		{
			// Prompt the user for what to do if a 'To All' response wasn't already given.
			if( OverwriteYesOrNoToAllState != EAppReturnType::YesAll && OverwriteYesOrNoToAllState != EAppReturnType::NoAll )
			{
				OverwriteYesOrNoToAllState = FMessageDialog::Open( EAppMsgType::YesNoYesAllNoAllCancel, FText::Format(
						NSLOCTEXT("UnrealEd", "ImportedSoundAlreadyExists_F", "You are about to import '{0}' over an existing sound. Would you like to overwrite the existing settings?\n\nYes or Yes to All: Overwrite the existing settings.\nNo or No to All: Preserve the existing settings.\nCancel: Abort the operation." ),
						FText::FromName(Name) ) );
			}

			switch( OverwriteYesOrNoToAllState )
			{

			case EAppReturnType::Yes:
			case EAppReturnType::YesAll:
				{
					// Overwrite existing settings
					bUseExistingSettings = false;
					break;
				}
			case EAppReturnType::No:
			case EAppReturnType::NoAll:
				{
					// Preserve existing settings
					bUseExistingSettings = true;
					break;
				}
			default:
				{
					FEditorDelegates::OnAssetPostImport.Broadcast( this, NULL );
					return NULL;
				}
			}
		}

		// Reset the flag back to false so subsequent imports are not suppressed unless the code explicitly suppresses it
		bSoundFactorySuppressImportOverwriteDialog = false;

		TArray<uint8> RawWaveData;
		RawWaveData.Empty( BufferEnd - Buffer );
		RawWaveData.AddUninitialized( BufferEnd - Buffer );
		FMemory::Memcpy( RawWaveData.GetData(), Buffer, RawWaveData.Num() );

		// Read the wave info and make sure we have valid wave data
		FWaveModInfo WaveInfo;
		FString ErrorMessage;
		if( WaveInfo.ReadWaveInfo( RawWaveData.GetTypedData(), RawWaveData.Num(), &ErrorMessage ) )
		{
			if( *WaveInfo.pBitsPerSample != 16 )
			{
				WaveInfo.ReportImportFailure();
				Warn->Logf(ELogVerbosity::Error, TEXT( "Currently, only 16 bit WAV files are supported (%s)." ), *Name.ToString() );
				FEditorDelegates::OnAssetPostImport.Broadcast( this, NULL );
				return NULL;
			}

			if( *WaveInfo.pChannels != 1 && *WaveInfo.pChannels != 2 )
			{
				WaveInfo.ReportImportFailure();
				Warn->Logf(ELogVerbosity::Error, TEXT("Currently, only mono or stereo WAV files are supported (%s)."), *Name.ToString());
				FEditorDelegates::OnAssetPostImport.Broadcast( this, NULL );
				return NULL;
			}
		}
		else
		{
			Warn->Logf(ELogVerbosity::Error, TEXT( "Unable to read wave file '%s' - \"%s\""), *Name.ToString(), *ErrorMessage );
			FEditorDelegates::OnAssetPostImport.Broadcast( this, NULL );
			return NULL;
		}

		// Use pre-existing sound if it exists and we want to keep settings,
		// otherwise create new sound and import raw data.
		USoundWave* Sound = (bUseExistingSettings && ExistingSound) ? ExistingSound : new( InParent, Name, Flags ) USoundWave(FPostConstructInitializeProperties());
		
		if (bUseExistingSettings && ExistingSound)
		{
			// Clear resources so that if it's already been played, it will reload the wave data
			Sound->FreeResources();
			Sound->DecompressionType = DTYPE_Setup;
			Sound->bDecompressedFromOgg = 0;
			Sound->RawPCMDataSize = 0;
		}
		
		// Store the current file path and timestamp for re-import purposes
		Sound->SourceFilePath = FReimportManager::SanitizeImportFilename( *CurrentFilename, Sound );
		Sound->SourceFileTimestamp = IFileManager::Get().GetTimeStamp(*CurrentFilename).ToString();

		// Compressed data is now out of date.
		Sound->InvalidateCompressedData();

		Sound->RawData.Lock( LOCK_READ_WRITE );
		void* LockedData = Sound->RawData.Realloc( BufferEnd-Buffer );		
		FMemory::Memcpy( LockedData, Buffer, BufferEnd-Buffer ); 
		Sound->RawData.Unlock();
		
		// Calculate duration.
		int32 DurationDiv = *WaveInfo.pChannels * *WaveInfo.pBitsPerSample * *WaveInfo.pSamplesPerSec;  
		if( DurationDiv ) 
		{
			Sound->Duration = *WaveInfo.pWaveDataSize * 8.0f / DurationDiv;
		}
		else
		{
			Sound->Duration = 0.0f;
		}
	
		Sound->SampleRate = *WaveInfo.pSamplesPerSec;
		Sound->NumChannels = *WaveInfo.pChannels;

		FEditorDelegates::OnAssetPostImport.Broadcast( this, Sound );

		if( ExistingSound && bUseExistingSettings )
		{
			// Call PostEditChange() to update text to speech
			Sound->PostEditChange();
		}

		// if we're auto creating a default cue
		if( bAutoCreateCue )
		{
			CreateSoundCue( Sound, bMoveCue ? CuePackage : InParent, Flags, bIncludeAttenuationNode, bIncludeModulatorNode, bIncludeLoopingNode, CueVolume );
		}

		for(int32 ComponentIndex = 0; ComponentIndex < ComponentsToRestart.Num(); ++ComponentIndex)
		{
			ComponentsToRestart[ComponentIndex]->Play();
		}

		return Sound;
	}
	else
	{
		// Unrecognized.
		Warn->Logf(ELogVerbosity::Error, TEXT("Unrecognized sound format '%s' in %s"), FileType, *Name.ToString() );
		FEditorDelegates::OnAssetPostImport.Broadcast( this, NULL );
		return NULL;
	}
}

void USoundFactory::SuppressImportOverwriteDialog()
{
	bSoundFactorySuppressImportOverwriteDialog = true;
}

/*-----------------------------------------------------------------------------
	UDialogueVoiceFactory.
-----------------------------------------------------------------------------*/

UDialogueVoiceFactory::UDialogueVoiceFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SupportedClass = UDialogueVoice::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UDialogueVoiceFactory::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn)
{
	return StaticConstructObject(Class,InParent,Name,Flags);
}

/*-----------------------------------------------------------------------------
	UDialogueWaveFactory.
-----------------------------------------------------------------------------*/

UDialogueWaveFactory::UDialogueWaveFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SupportedClass = UDialogueWave::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UDialogueWaveFactory::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn)
{
	return StaticConstructObject(Class,InParent,Name,Flags);
}

/*-----------------------------------------------------------------------------
	UReimportSoundFactory.
-----------------------------------------------------------------------------*/
UReimportSoundFactory::UReimportSoundFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = USoundWave::StaticClass();
	Formats.Add(TEXT("wav;Sound"));

	bCreateNew = false;
	bAutoCreateCue = false;
	bIncludeAttenuationNode = false;
	bIncludeModulatorNode = false;
	bIncludeLoopingNode = false;
	CueVolume = 0.75f;
}

bool UReimportSoundFactory::CanReimport( UObject* Obj, TArray<FString>& OutFilenames )
{	
	USoundWave* SoundWave = Cast<USoundWave>(Obj);
	if(SoundWave && SoundWave->NumChannels < 3)
	{
		OutFilenames.Add(FReimportManager::ResolveImportFilename(SoundWave->SourceFilePath, SoundWave));
		return true;
	}
	return false;
}

void UReimportSoundFactory::SetReimportPaths( UObject* Obj, const TArray<FString>& NewReimportPaths )
{	
	USoundWave* SoundWave = Cast<USoundWave>(Obj);
	if(SoundWave && ensure(NewReimportPaths.Num() == 1))
	{
		SoundWave->SourceFilePath = FReimportManager::ResolveImportFilename(NewReimportPaths[0], SoundWave);
	}
}

EReimportResult::Type UReimportSoundFactory::Reimport( UObject* Obj )
{
	// Only handle valid sound node waves
	if( !Obj || !Obj->IsA( USoundWave::StaticClass() ) )
	{
		return EReimportResult::Failed;
	}

	USoundWave* SoundWave = Cast<USoundWave>( Obj );

	const FString Filename = FReimportManager::ResolveImportFilename(SoundWave->SourceFilePath, SoundWave);
	const FString FileExtension = FPaths::GetExtension(Filename);
	const bool bIsWav = ( FCString::Stricmp( *FileExtension, TEXT("WAV") ) == 0 );

	// Only handle WAV files
	if ( !bIsWav )
	{
		return EReimportResult::Failed;
	}
	// If there is no file path provided, can't reimport from source
	if ( !Filename.Len() )
	{
		// Since this is a new system most sound node waves don't have paths, so logging has been commented out
		//UE_LOG(LogEditorFactories, Warning, TEXT("-- cannot reimport: sound node wave resource does not have path stored."));
		return EReimportResult::Failed;
	}

	UE_LOG(LogEditorFactories, Log, TEXT("Performing atomic reimport of [%s]"), *Filename);

	// Ensure that the file provided by the path exists
	if (IFileManager::Get().FileSize(*Filename) == INDEX_NONE)
	{
		UE_LOG(LogEditorFactories, Warning, TEXT("-- cannot reimport: source file cannot be found.") );
		return EReimportResult::Failed;
	}

	// Suppress the import overwrite dialog, we want to keep existing settings when re-importing
	USoundFactory::SuppressImportOverwriteDialog();

	if( UFactory::StaticImportObject( SoundWave->GetClass(), SoundWave->GetOuter(), *SoundWave->GetName(), RF_Public|RF_Standalone, *Filename, NULL, this ) )
	{
		UE_LOG(LogEditorFactories, Log, TEXT("-- imported successfully") );

		// Mark the package dirty after the successful import
		SoundWave->MarkPackageDirty();
	}
	else
	{
		UE_LOG(LogEditorFactories, Warning, TEXT("-- import failed") );
	}

	return EReimportResult::Succeeded;
}


/*-----------------------------------------------------------------------------
	USoundSurroundFactory.
-----------------------------------------------------------------------------*/

const FString SurroundSpeakerLocations[SPEAKER_Count] =
{
	TEXT( "_fl" ),			// SPEAKER_FrontLeft
	TEXT( "_fr" ),			// SPEAKER_FrontRight
	TEXT( "_fc" ),			// SPEAKER_FrontCenter
	TEXT( "_lf" ),			// SPEAKER_LowFrequency
	TEXT( "_sl" ),			// SPEAKER_SideLeft
	TEXT( "_sr" ),			// SPEAKER_SideRight
	TEXT( "_bl" ),			// SPEAKER_BackLeft
	TEXT( "_br" )			// SPEAKER_BackRight
};

USoundSurroundFactory::USoundSurroundFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = USoundWave::StaticClass();
	Formats.Add(TEXT("WAV;Multichannel Sound"));

	bCreateNew = false;
	CueVolume = 0.75f;
	bEditorImport = true;
}

bool USoundSurroundFactory::FactoryCanImport( const FString& Filename )
{
	// Find the root name
	FString RootName = FPaths::GetBaseFilename(Filename);
	FString SpeakerLocation = RootName.Right( 3 ).ToLower();

	// Find which channel this refers to		
	for( int32 SpeakerIndex = 0; SpeakerIndex < SPEAKER_Count; SpeakerIndex++ )
	{
		if( SpeakerLocation == SurroundSpeakerLocations[SpeakerIndex] )
		{
			return( true );
		}
	}

	return( false );
}

UObject* USoundSurroundFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	EObjectFlags		Flags,
	UObject*			Context,
	const TCHAR*		FileType,
	const uint8*&		Buffer,
	const uint8*			BufferEnd,
	FFeedbackContext*	Warn
 )
{
	FEditorDelegates::OnAssetPreImport.Broadcast(this, Class, InParent, Name, FileType);

	int32		SpeakerIndex, i;

	// Only import wavs
	if(	FCString::Stricmp( FileType, TEXT( "WAV" ) ) == 0 )
	{
		// Find the root name
		FString RootName = Name.GetPlainNameString();
		FString SpeakerLocation = RootName.Right( 3 ).ToLower();
		FName BaseName = FName( *RootName.LeftChop( 3 ) );

		// Find which channel this refers to		
		for( SpeakerIndex = 0; SpeakerIndex < SPEAKER_Count; SpeakerIndex++ )
		{
			if( SpeakerLocation == SurroundSpeakerLocations[SpeakerIndex] )
			{
				break;
			}
		}

		if( SpeakerIndex == SPEAKER_Count )
		{
			Warn->Logf(ELogVerbosity::Error, TEXT( "Failed to find speaker location; valid extensions are _fl, _fr, _fc, _lf, _sl, _sr, _bl, _br." ) );
			FEditorDelegates::OnAssetPostImport.Broadcast( this, NULL );
			return( NULL );
		}

		// Find existing soundwave
		USoundWave* Sound = FindObject<USoundWave>(InParent, *BaseName.ToString());

		// Create new sound if necessary
		if( Sound == NULL )
		{
			// If This is a single asset package, then create package so that its name will be identical to the asset.
			if (PackageTools::IsSingleAssetPackage(InParent->GetName()))
			{
				InParent = CreatePackage(NULL, *InParent->GetName().LeftChop(3) );

				// Make sure the destination package is loaded
				CastChecked<UPackage>(InParent)->FullyLoad();

				Sound = FindObject<USoundWave>(InParent, *BaseName.ToString());
			}

			if (Sound == NULL)
			{
				Sound = new( InParent, BaseName, Flags ) USoundWave(FPostConstructInitializeProperties());
			}
		}

		// Presize the offsets array, in case the sound was new or the original sound data was stripped by cooking.
		if ( Sound->ChannelOffsets.Num() != SPEAKER_Count )
		{
			Sound->ChannelOffsets.Empty( SPEAKER_Count );
			Sound->ChannelOffsets.AddZeroed( SPEAKER_Count );
		}
		// Presize the sizes array, in case the sound was new or the original sound data was stripped by cooking.
		if ( Sound->ChannelSizes.Num() != SPEAKER_Count )
		{
			Sound->ChannelSizes.Empty( SPEAKER_Count );
			Sound->ChannelSizes.AddZeroed( SPEAKER_Count );
		}

		// Store the current file path and timestamp for re-import purposes
		Sound->SourceFilePath = FReimportManager::SanitizeImportFilename( *CurrentFilename, Sound );
		Sound->SourceFileTimestamp = IFileManager::Get().GetTimeStamp(*CurrentFilename).ToString();

		// Compressed data is now out of date.
		Sound->InvalidateCompressedData();

		// Delete the old version of the wave from the bulk data
		uint8 * RawWaveData[SPEAKER_Count] = { NULL };
		uint8 * RawData = ( uint8 * )Sound->RawData.Lock( LOCK_READ_WRITE );
		int32 RawDataOffset = 0;
		int32 TotalSize = 0;

		// Copy off the still used waves
		for( i = 0; i < SPEAKER_Count; i++ )
		{
			if( i != SpeakerIndex && Sound->ChannelSizes[ i ] )
			{
				RawWaveData[i] = new uint8 [Sound->ChannelSizes[ i ]];
				FMemory::Memcpy( RawWaveData[i], RawData + Sound->ChannelOffsets[ i ], Sound->ChannelSizes[ i ] );
				TotalSize += Sound->ChannelSizes[ i ];
			}
		}

		// Copy them back without the one that will be updated
		RawData = ( uint8 * )Sound->RawData.Realloc( TotalSize );

		for( i = 0; i < SPEAKER_Count; i++ )
		{
			if( RawWaveData[i] )
			{
				FMemory::Memcpy( RawData + RawDataOffset, RawWaveData[i], Sound->ChannelSizes[ i ] );
				Sound->ChannelOffsets[ i ] = RawDataOffset;
				RawDataOffset += Sound->ChannelSizes[ i ];

				delete [] RawWaveData[i];
			}
		}

		uint32 RawDataSize = BufferEnd - Buffer;
		uint8* LockedData = ( uint8 * )Sound->RawData.Realloc( RawDataOffset + RawDataSize );		
		LockedData += RawDataOffset;
		FMemory::Memcpy( LockedData, Buffer, RawDataSize ); 

		Sound->ChannelOffsets[ SpeakerIndex ] = RawDataOffset;
		Sound->ChannelSizes[ SpeakerIndex ] = RawDataSize;

		Sound->RawData.Unlock();

		// Calculate duration.
		FWaveModInfo WaveInfo;
		FString ErrorReason;
		if( WaveInfo.ReadWaveInfo( LockedData, RawDataSize, &ErrorReason ) )
		{
			// Calculate duration in seconds
			int32 DurationDiv = *WaveInfo.pChannels * *WaveInfo.pBitsPerSample * *WaveInfo.pSamplesPerSec;  
			if( DurationDiv ) 
			{
				Sound->Duration = *WaveInfo.pWaveDataSize * 8.0f / DurationDiv;
			}
			else
			{
				Sound->Duration = 0.0f;
			}

			if( *WaveInfo.pBitsPerSample != 16 )
			{
				Warn->Logf(ELogVerbosity::Error, TEXT( "Currently, only 16 bit WAV files are supported (%s)." ), *Name.ToString() );
				Sound->MarkPendingKill();
				Sound = NULL;
			}

			if( *WaveInfo.pChannels != 1 )
			{
				Warn->Logf(ELogVerbosity::Error, TEXT( "Currently, only mono WAV files can be imported as channels of surround audio (%s)." ), *Name.ToString() );
				Sound->MarkPendingKill();
				Sound = NULL;
			}
		}
		else
		{
			Warn->Logf(ELogVerbosity::Error, TEXT( "Unable to read wave file '%s' - \"%s\""), *Name.ToString(), *ErrorReason );
			Sound->MarkPendingKill();
			Sound = NULL;
		}
		if (Sound)
		{
			Sound->NumChannels = 0;
			for( i = 0; i < SPEAKER_Count; i++ )
			{
				if( Sound->ChannelSizes[ i ] )
				{
					Sound->NumChannels++;
				}
			}
		}

		FEditorDelegates::OnAssetPostImport.Broadcast( this, Sound );

		return( Sound );
	}
	else
	{
		// Unrecognized.
		Warn->Logf(ELogVerbosity::Error, TEXT("Unrecognized sound extension '%s' in %s"), FileType, *Name.ToString() );
		FEditorDelegates::OnAssetPostImport.Broadcast( this, NULL );
	}

	return( NULL );
}

/*-----------------------------------------------------------------------------
	UReimportSoundSurroundFactory.
-----------------------------------------------------------------------------*/
UReimportSoundSurroundFactory::UReimportSoundSurroundFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = USoundWave::StaticClass();
	Formats.Add(TEXT("wav;Sound"));

	bCreateNew = false;
	CueVolume = 0.75f;
}

bool UReimportSoundSurroundFactory::CanReimport( UObject* Obj, TArray<FString>& OutFilenames )
{	
	USoundWave* SoundWave = Cast<USoundWave>(Obj);
	if(SoundWave && SoundWave->NumChannels > 2)
	{
		bool bGeneratedFilenames = false;

		if (!SoundWave->SourceFilePath.IsEmpty())
		{
			// Convert to a FString to check it has correct formatting
			const FString SourceFilename = FReimportManager::ResolveImportFilename(SoundWave->SourceFilePath, SoundWave);
			if (FactoryCanImport(SourceFilename))
			{
				// Get filename with speaker location removed
				FString BaseFilename = FPaths::GetBaseFilename(SourceFilename).LeftChop(3);
				FString FileExtension = FPaths::GetExtension(SourceFilename, true);
				FString FilePath = FPaths::GetPath(SourceFilename);

				// Add a filename for each speaker location we have Channel Size data for
				for (int32 ChannelIndex = 0; ChannelIndex < SoundWave->ChannelSizes.Num(); ++ChannelIndex)
				{
					if (SoundWave->ChannelSizes[ChannelIndex])
					{
						OutFilenames.Add(FilePath + TEXT("//") + BaseFilename + SurroundSpeakerLocations[ChannelIndex] + FileExtension);
					}
				}

				bGeneratedFilenames = true;
			}
		}

		if (!bGeneratedFilenames)
		{
			// We failed to generate possible filenames, fill the array with a blank string for each channel
			for (int32 ChannelIndex = 0; ChannelIndex < SoundWave->NumChannels; ++ChannelIndex)
			{
				OutFilenames.Add(FString());
			}
		}

		// Store these for later use
		ReimportPaths = OutFilenames;

		return true;
	}
	return false;
}

void UReimportSoundSurroundFactory::SetReimportPaths( UObject* Obj, const TArray<FString>& NewReimportPaths )
{	
	USoundWave* SoundWave = Cast<USoundWave>(Obj);
	if(SoundWave)
	{
		ReimportPaths = NewReimportPaths;
	}
}

EReimportResult::Type UReimportSoundSurroundFactory::Reimport( UObject* Obj )
{
	// Only handle valid sound node waves
	if( !Obj || !Obj->IsA( USoundWave::StaticClass() ) )
	{
		return EReimportResult::Failed;
	}

	USoundWave* SoundWave = Cast<USoundWave>( Obj );

	// Holds the warnings for the message log.
	FMessageLog EditorErrors("EditorErrors");
	FText NameText = FText::FromString(SoundWave->GetName());

	bool bSourceReimported = false;

	for (int32 PathIndex = 0; PathIndex < ReimportPaths.Num(); ++PathIndex)
	{
		FString Filename(ReimportPaths[PathIndex]);

		// If there is no file path provided, can't reimport from source
		if (!Filename.Len())
		{
			// Since this is a new system most sound node waves don't have paths, so logging has been commented out
			//UE_LOG(LogEditorFactories, Warning, TEXT("-- cannot reimport: sound node wave resource does not have path stored."));
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("NameText"), NameText);
			EditorErrors.Warning(FText::Format(LOCTEXT("SurroundWarningNoFilename", "{NameText}: Attempt to reimport empty file name."), Arguments));
			continue;
		}

		FText FilenameText = FText::FromString(Filename);

		const FString FileExtension = FPaths::GetExtension(Filename);
		const bool bIsWav = ( FCString::Stricmp( *FileExtension, TEXT("WAV") ) == 0 );

		// Only handle WAV files
		if (!bIsWav)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("NameText"), NameText);
			Arguments.Add(TEXT("FilenameText"), FilenameText);
			EditorErrors.Warning(FText::Format(LOCTEXT("SurroundWarningFormat", "{NameText}: Incorrect File Format - {FilenameText}"), Arguments));
			continue;
		}

		UE_LOG(LogEditorFactories, Log, TEXT("Performing atomic reimport of [%s]"), *Filename);

		// Ensure that the file provided by the path exists
		if (IFileManager::Get().FileSize(*Filename) == INDEX_NONE)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("NameText"), NameText);
			Arguments.Add(TEXT("FilenameText"), FilenameText);
			EditorErrors.Warning(FText::Format(LOCTEXT("SurroundWarningNoFile", "{NameText}: Source file cannot be found - {FilenameText}"), Arguments));
			continue;
		}

		FString SpeakerLocation = FPaths::GetBaseFilename(Filename).Right(3);
		FName ImportName = *(SoundWave->GetName() + SpeakerLocation);

		if (UFactory::StaticImportObject(SoundWave->GetClass(), SoundWave->GetOuter(), ImportName, RF_Public|RF_Standalone, *Filename, NULL, this ))
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("NameText"), NameText);
			Arguments.Add(TEXT("FilenameText"), FilenameText);
			EditorErrors.Info(FText::Format(LOCTEXT("SurroundWarningImportSucceeded", "{NameText}: Import successful - {FilenameText}"), Arguments));

			// Mark the package dirty after the successful import
			SoundWave->MarkPackageDirty();

			bSourceReimported = true;
		}
		else
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("NameText"), NameText);
			Arguments.Add(TEXT("FilenameText"), FilenameText);
			EditorErrors.Warning(FText::Format(LOCTEXT("SurroundWarningImportFailed", "{NameText}: Import failed - {FilenameText}"), Arguments));
		}
	}

	EditorErrors.Notify(LOCTEXT("SurroundWarningDescription", "Some files could not be reimported."), EMessageSeverity::Warning);

	return bSourceReimported ? EReimportResult::Succeeded : EReimportResult::Failed;
}


/*------------------------------------------------------------------------------
	USoundCueFactoryNew.
------------------------------------------------------------------------------*/
USoundCueFactoryNew::USoundCueFactoryNew(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = USoundCue::StaticClass();

	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* USoundCueFactoryNew::FactoryCreateNew( UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn )
{
	USoundCue* SoundCue = ConstructObject<USoundCue>( USoundCue::StaticClass(), InParent, Name, Flags );

	if (InitialSoundWave)
	{
		USoundNodeWavePlayer* WavePlayer = SoundCue->ConstructSoundNode<USoundNodeWavePlayer>();
		SoundCue->FirstNode = WavePlayer;
		SoundCue->LinkGraphNodesFromSoundNodes();
		WavePlayer->SoundWave = InitialSoundWave;
		WavePlayer->GraphNode->NodePosX = -250;
		WavePlayer->GraphNode->NodePosY = -35;
	}

	return SoundCue;
}

/*-----------------------------------------------------------------------------
	USoundMixFactory.
-----------------------------------------------------------------------------*/
USoundMixFactory::USoundMixFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = USoundMix::StaticClass();

	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* USoundMixFactory::FactoryCreateNew( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn )
{
	USoundMix* Mix = ConstructObject<USoundMix>( USoundMix::StaticClass(), InParent, InName, Flags );

	return Mix;
}


/*-----------------------------------------------------------------------------
	USoundClassFactory.
-----------------------------------------------------------------------------*/
USoundClassFactory::USoundClassFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = USoundClass::StaticClass();
	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* USoundClassFactory::FactoryCreateNew( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn )
{
	USoundClass* SoundClass = ConstructObject<USoundClass>( USoundClass::StaticClass(), InParent, InName, Flags );
	
	FAudioDevice* AudioDevice = GEngine ? GEngine->GetAudioDevice() : NULL;
	if (AudioDevice)
	{
		AudioDevice->InitSoundClasses();
	}

	return( SoundClass );
}

/*------------------------------------------------------------------------------
	USoundAttenuationFactory.
------------------------------------------------------------------------------*/
USoundAttenuationFactory::USoundAttenuationFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = USoundAttenuation::StaticClass();
	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* USoundAttenuationFactory::FactoryCreateNew( UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn )
{
	return ConstructObject<USoundAttenuation>( USoundAttenuation::StaticClass(), InParent, Name, Flags );
}

/*------------------------------------------------------------------------------
	UParticleSystemFactoryNew.
------------------------------------------------------------------------------*/
UParticleSystemFactoryNew::UParticleSystemFactoryNew(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = UParticleSystem::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UParticleSystemFactoryNew::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn)
{
	return StaticConstructObject(Class,InParent,Name,Flags);
}

/*------------------------------------------------------------------------------
	UPhysicalMaterialFactoryNew.
------------------------------------------------------------------------------*/
UPhysicalMaterialFactoryNew::UPhysicalMaterialFactoryNew(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = UPhysicalMaterial::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UPhysicalMaterialFactoryNew::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn)
{
	return StaticConstructObject(Class,InParent,Name,Flags);
}

/*------------------------------------------------------------------------------
	UInterpDataFactoryNew.
------------------------------------------------------------------------------*/
UInterpDataFactoryNew::UInterpDataFactoryNew(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = UInterpData::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UInterpDataFactoryNew::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn)
{
	return StaticConstructObject(Class,InParent,Name,Flags);
}

/*-----------------------------------------------------------------------------
	UTextureRenderTargetFactoryNew
-----------------------------------------------------------------------------*/

UTextureRenderTargetFactoryNew::UTextureRenderTargetFactoryNew(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SupportedClass = UTextureRenderTarget2D::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
	bEditorImport = false;

	Width = 256;
	Height = 256;
	Format = 0;
}


UObject* UTextureRenderTargetFactoryNew::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn)
{
	// create the new object
	UTextureRenderTarget2D* Result = CastChecked<UTextureRenderTarget2D>( StaticConstructObject(Class,InParent,Name,Flags) );
	// initialize the resource
	Result->InitAutoFormat( Width, Height );
	return( Result );
}


/*-----------------------------------------------------------------------------
	UTextureRenderTargetCubeFactoryNew
-----------------------------------------------------------------------------*/
UTextureRenderTargetCubeFactoryNew::UTextureRenderTargetCubeFactoryNew(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = UTextureRenderTargetCube::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
	bEditorImport = false;

	Width = 256;
	Format = 0;
}

UObject* UTextureRenderTargetCubeFactoryNew::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	// create the new object
	UTextureRenderTargetCube* Result = CastChecked<UTextureRenderTargetCube>(StaticConstructObject(Class, InParent, Name, Flags));

	// initialize the resource
	Result->InitAutoFormat(Width);

	return (Result);
}


/*-----------------------------------------------------------------------------
	UTextureFactory.
-----------------------------------------------------------------------------*/

// .PCX file header.
#pragma pack(push,1)
class FPCXFileHeader
{
public:
	uint8	Manufacturer;		// Always 10.
	uint8	Version;			// PCX file version.
	uint8	Encoding;			// 1=run-length, 0=none.
	uint8	BitsPerPixel;		// 1,2,4, or 8.
	uint16	XMin;				// Dimensions of the image.
	uint16	YMin;				// Dimensions of the image.
	uint16	XMax;				// Dimensions of the image.
	uint16	YMax;				// Dimensions of the image.
	uint16	XDotsPerInch;		// Horizontal printer resolution.
	uint16	YDotsPerInch;		// Vertical printer resolution.
	uint8	OldColorMap[48];	// Old colormap info data.
	uint8	Reserved1;			// Must be 0.
	uint8	NumPlanes;			// Number of color planes (1, 3, 4, etc).
	uint16	BytesPerLine;		// Number of bytes per scanline.
	uint16	PaletteType;		// How to interpret palette: 1=color, 2=gray.
	uint16	HScreenSize;		// Horizontal monitor size.
	uint16	VScreenSize;		// Vertical monitor size.
	uint8	Reserved2[54];		// Must be 0.
	friend FArchive& operator<<( FArchive& Ar, FPCXFileHeader& H )
	{
		Ar << H.Manufacturer << H.Version << H.Encoding << H.BitsPerPixel;
		Ar << H.XMin << H.YMin << H.XMax << H.YMax << H.XDotsPerInch << H.YDotsPerInch;
		for( int32 i=0; i<ARRAY_COUNT(H.OldColorMap); i++ )
			Ar << H.OldColorMap[i];
		Ar << H.Reserved1 << H.NumPlanes;
		Ar << H.BytesPerLine << H.PaletteType << H.HScreenSize << H.VScreenSize;
		for( int32 i=0; i<ARRAY_COUNT(H.Reserved2); i++ )
			Ar << H.Reserved2[i];
		return Ar;
	}
};

struct FTGAFileHeader
{
	uint8 IdFieldLength;
	uint8 ColorMapType;
	uint8 ImageTypeCode;		// 2 for uncompressed RGB format
	uint16 ColorMapOrigin;
	uint16 ColorMapLength;
	uint8 ColorMapEntrySize;
	uint16 XOrigin;
	uint16 YOrigin;
	uint16 Width;
	uint16 Height;
	uint8 BitsPerPixel;
	uint8 ImageDescriptor;
	friend FArchive& operator<<( FArchive& Ar, FTGAFileHeader& H )
	{
		Ar << H.IdFieldLength << H.ColorMapType << H.ImageTypeCode;
		Ar << H.ColorMapOrigin << H.ColorMapLength << H.ColorMapEntrySize;
		Ar << H.XOrigin << H.YOrigin << H.Width << H.Height << H.BitsPerPixel;
		Ar << H.ImageDescriptor;
		return Ar;
	}
};

struct FTGAFileFooter
{
	uint32 ExtensionAreaOffset;
	uint32 DeveloperDirectoryOffset;
	uint8 Signature[16];
	uint8 TrailingPeriod;
	uint8 NullTerminator;
};

struct FPSDFileHeader
{                                                           
	int32     Signature;      // 8BPS
	int16   Version;        // Version
	int16   nChannels;      // Number of Channels (3=RGB) (4=RGBA)
	int32     Height;         // Number of Image Rows
	int32     Width;          // Number of Image Columns
	int16   Depth;          // Number of Bits per Channel
	int16   Mode;           // Image Mode (0=Bitmap)(1=Grayscale)(2=Indexed)(3=RGB)(4=CYMK)(7=Multichannel)
	uint8    Pad[6];         // Padding

	/**
	 * @return Whether file has a valid signature
	 */
	bool IsValid( void )
	{
		// Fail on bad signature
		if (Signature != 0x38425053)
			return false;

		return true;
	}

	/**
	 * @return Whether file has a supported version
	 */
	bool IsSupported( void )
	{
		// Fail on bad version
		if( Version != 1 )
			return false;   
		// Fail on anything other than 3 or 4 channels
		if ((nChannels!=3) && (nChannels!=4))
			return false;
		// Fail on anything other than 8 Bits/channel or 16 Bits/channel  
		if ((Depth != 8) && (Depth != 16))
			return false;
		// Fail on anything other than RGB
		// We can add support for indexed later if needed.
		if (Mode!=3)
			return false;

		return true;
	}
};

#pragma pack(pop)


static bool psd_ReadData( uint8* pOut, const uint8*& pBuffer, FPSDFileHeader& Info )
{
	const uint8* pPlane = NULL;
	const uint8* pRowTable = NULL;
	int32         iPlane;
	int16       CompressionType;
	int32         iPixel;
	int32         iRow;
	int32         CompressedBytes;
	int32         iByte;
	int32         Count;
	uint8        Value;

	// Double check to make sure this is a valid request
	if (!Info.IsValid() || !Info.IsSupported())
	{
		return false;
	}

	const uint8* pCur = pBuffer + sizeof(FPSDFileHeader);
	int32         NPixels = Info.Width * Info.Height;

	int32  ClutSize =  ((int32)pCur[ 0] << 24) +
		((int32)pCur[ 1] << 16) +
		((int32)pCur[ 2] <<  8) +
		((int32)pCur[ 3] <<  0);
	pCur+=4;
	pCur += ClutSize;    

	// Skip Image Resource Section
	int32 ImageResourceSize = ((int32)pCur[ 0] << 24) +
		((int32)pCur[ 1] << 16) +
		((int32)pCur[ 2] <<  8) +
		((int32)pCur[ 3] <<  0);
	pCur += 4+ImageResourceSize;

	// Skip Layer and Mask Section
	int32 LayerAndMaskSize =  ((int32)pCur[ 0] << 24) +
		((int32)pCur[ 1] << 16) +
		((int32)pCur[ 2] <<  8) +
		((int32)pCur[ 3] <<  0);
	pCur += 4+LayerAndMaskSize;

	// Determine number of bytes per pixel
	int32 BytesPerPixel = 3;
	const int32 BytesPerChannel = (Info.Depth / 8);
	switch( Info.Mode )
	{
	case 2:        
		BytesPerPixel = 1;        
		return false;  // until we support indexed...
		break;
	case 3:
		if( Info.nChannels == 3 )                  
			BytesPerPixel = 3 * BytesPerChannel;
		else                   
			BytesPerPixel = 4 * BytesPerChannel;       
		break;
	default:
		return false;
		break;
	}

	// Get Compression Type
	CompressionType = ((int32)pCur[0] <<  8) + ((int32)pCur[1] <<  0);    
	pCur += 2;

	// Fail on 16 Bits/channel with RLE. This can occur when the file is not saved with 'Maximize Compatibility'. Compression doesn't appear to be standard.
	if(CompressionType == 1 && Info.Depth == 16)
	{
		return false;
	}

	// If no alpha channel, set alpha to opaque (255 or 65536).
	if( Info.nChannels == 3)
	{
		if(Info.Depth == 8)
		{
			const uint32 Channels = 4;
			const uint32 BufferSize = Info.Width * Info.Height * Channels * sizeof(uint8);
			FMemory::Memset(pOut, 0xff, BufferSize);
		}
		else if(Info.Depth == 16)
		{
			const uint32 Channels = 4;
			const uint32 BufferSize = Info.Width * Info.Height * Channels * sizeof(uint16);
			FMemory::Memset(pOut, 0xff, BufferSize);
		}
	}

	// Uncompressed?
	if( CompressionType == 0 )
	{
		if(Info.Depth == 8)
		{
			FColor* Dest = (FColor*)pOut;
			for(int32 Pixel=0; Pixel < NPixels; Pixel++ )
			{
				Dest[Pixel].R = pCur[ NPixels*0+Pixel ];
				Dest[Pixel].G = pCur[ NPixels*1+Pixel ];
				Dest[Pixel].B = pCur[ NPixels*2+Pixel ];
				if (Info.nChannels == 4)
				{
					Dest[Pixel].A = pCur[ NPixels*3+Pixel ];
				}
			}
		}
		else if (Info.Depth == 16)
		{
			uint32 SrcOffset = 0;
			
			// Loop through the planes	
			for(iPlane=0 ; iPlane<Info.nChannels ; iPlane++)
			{
				uint16* Dest = (uint16*)pOut;
				uint32 ChannelOffset = iPlane;
				
				for(int32 Pixel=0; Pixel < NPixels; Pixel++)
				{
					Dest[ChannelOffset] = ((pCur[SrcOffset] << 8) + (pCur[SrcOffset+1] << 0));

					//Increment offsets
					ChannelOffset +=4;
					SrcOffset += BytesPerChannel;
				}

			}
		}
	}
	// RLE?
	else if( CompressionType == 1 )
	{
		// Setup RowTable
		pRowTable = pCur;
		pCur += Info.nChannels*Info.Height*2;

		FColor* Dest = (FColor*)pOut;

		// Loop through the planes
		for( iPlane=0 ; iPlane<Info.nChannels ; iPlane++ )
		{
			int32 iWritePlane = iPlane;
			if( iWritePlane > BytesPerPixel-1 ) iWritePlane = BytesPerPixel-1;

			// Loop through the rows
			for( iRow=0 ; iRow<Info.Height ; iRow++ )
			{
				// Load a row
				CompressedBytes = (pRowTable[(iPlane*Info.Height+iRow)*2  ] << 8) +
					(pRowTable[(iPlane*Info.Height+iRow)*2+1] << 0);

				// Setup Plane
				pPlane = pCur;
				pCur += CompressedBytes;

				// Decompress Row
				iPixel = 0;
				iByte = 0;
				while( (iPixel < Info.Width) && (iByte < CompressedBytes) )
				{
					int8 code = (int8)pPlane[iByte++];

					// Is it a repeat?
					if( code < 0 )
					{
						Count = -(int32)code + 1;
						Value = pPlane[iByte++];
						while( Count-- > 0 )
						{
							int32 idx = (iPixel) + (iRow*Info.Width);
							switch(iWritePlane)
							{
							case 0: Dest[idx].R = Value; break;
							case 1: Dest[idx].G = Value; break;
							case 2: Dest[idx].B = Value; break;
							case 3: Dest[idx].A = Value; break;
							}                            
							iPixel++;
						}
					}
					// Must be a literal then
					else
					{
						Count = (int32)code + 1;
						while( Count-- > 0 )
						{
							Value = pPlane[iByte++];
							int32 idx = (iPixel) + (iRow*Info.Width);

							switch(iWritePlane)
							{
							case 0: Dest[idx].R = Value; break;
							case 1: Dest[idx].G = Value; break;
							case 2: Dest[idx].B = Value; break;
							case 3: Dest[idx].A = Value; break;
							}  
							iPixel++;
						}
					}
				}

				// Confirm that we decoded the right number of bytes
				check( iByte  == CompressedBytes );
				check( iPixel == Info.Width );
			}
		}
	}
	else
		return false;

	// Success!
	return( true );
}

static void psd_GetPSDHeader( const uint8* Buffer, FPSDFileHeader& Info )
{
	Info.Signature      =   ((int32)Buffer[ 0] << 24) +
		((int32)Buffer[ 1] << 16) +
		((int32)Buffer[ 2] <<  8) +
		((int32)Buffer[ 3] <<  0);
	Info.Version        =   ((int32)Buffer[ 4] <<  8) +
		((int32)Buffer[ 5] <<  0);
	Info.nChannels      =   ((int32)Buffer[12] <<  8) +
		((int32)Buffer[13] <<  0);
	Info.Height         =   ((int32)Buffer[14] << 24) +
		((int32)Buffer[15] << 16) +
		((int32)Buffer[16] <<  8) +
		((int32)Buffer[17] <<  0);
	Info.Width          =   ((int32)Buffer[18] << 24) +
		((int32)Buffer[19] << 16) +
		((int32)Buffer[20] <<  8) +
		((int32)Buffer[21] <<  0);
	Info.Depth          =   ((int32)Buffer[22] <<  8) +
		((int32)Buffer[23] <<  0);
	Info.Mode           =   ((int32)Buffer[24] <<  8) +
		((int32)Buffer[25] <<  0);
}

bool UTextureFactory::bSuppressImportOverwriteDialog = false;
bool UTextureFactory::bSuppressImportResolutionWarnings = false;

UTextureFactory::UTextureFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SupportedClass = UTexture::StaticClass();

	Formats.Add( TEXT( "bmp;Texture" ) );
	Formats.Add( TEXT( "pcx;Texture" ) );
	Formats.Add( TEXT( "tga;Texture" ) );
	Formats.Add( TEXT( "float;Texture" ) );
	Formats.Add( TEXT( "psd;Texture" ) );
	Formats.Add( TEXT( "dds;Texture" ) );
	Formats.Add( TEXT( "hdr;Cubemap Texture (LongLat unwrap)" ) );
	Formats.Add( TEXT( "ies;IES Texture (Standard light profiles)" ) );
	Formats.Add( TEXT( "png;Texture" ) );
	Formats.Add( TEXT( "jpg;Texture" ) );
	Formats.Add( TEXT( "jpeg;Texture" ) );

	bCreateNew = false;
	bEditorImport = true;
}

void UTextureFactory::PostInitProperties()
{
	Super::PostInitProperties();
	MipGenSettings = TextureMipGenSettings(0);
	bool bFlipNormalMapGreenChannelSetting = false;
	GConfig->GetBool(TEXT("/Script/UnrealEd.EditorEngine"), TEXT("FlipNormalMapGreenChannel"), bFlipNormalMapGreenChannelSetting, GEngineIni);
	bFlipNormalMapGreenChannel = bFlipNormalMapGreenChannelSetting;
}

UTexture2D* UTextureFactory::CreateTexture2D( UObject* InParent, FName Name, EObjectFlags Flags )
{
	UTexture2D* NewTexture = CastChecked<UTexture2D>( CreateOrOverwriteAsset(UTexture2D::StaticClass(),InParent,Name,Flags) );
	return NewTexture;
}

UTextureCube* UTextureFactory::CreateTextureCube( UObject* InParent, FName Name, EObjectFlags Flags )
{
	UTextureCube* NewTextureCube = CastChecked<UTextureCube>( CreateOrOverwriteAsset(UTextureCube::StaticClass(), InParent, Name, Flags) );
	return NewTextureCube;
}

void UTextureFactory::SuppressImportOverwriteDialog()
{
	bSuppressImportOverwriteDialog = true;
}

void UTextureFactory::SuppressImportResolutionWarningDialog()
{
	bSuppressImportResolutionWarnings = true;
}

/**
 * This fills any pixels of a texture with have an alpha value of zero,
 * with an RGB from the nearest neighboring pixel which has non-zero alpha.
*/
template<typename PixelDataType, typename ColorDataType, int32 RIdx, int32 GIdx, int32 BIdx, int32 AIdx> class PNGDataFill
{
public:

	PNGDataFill( const FTextureSource& TextureSource, uint8* SourceTextureData )
		: SourceData( reinterpret_cast<PixelDataType*>(SourceTextureData) )
		, TextureWidth( TextureSource.GetSizeX() )
		, TextureHeight( TextureSource.GetSizeY() )
	{
	}

	void ProcessData()
	{
		ClearZeroAlphaData();
		HorizontalPass( 1 );
		HorizontalPass( -1 );
		VerticalPass( 1 );
		VerticalPass( -1 );
	}

	void ClearZeroAlphaData()
	{
		for( int32 Y = 0; Y < TextureHeight; ++Y )
		{
			for( int32 X = 0; X < TextureWidth; ++X )
			{
				PixelDataType* PixelData = SourceData + (Y * TextureWidth + X) * 4;

				if( PixelData[ AIdx ] == 0 )
				{
					ColorDataType* ColorData = reinterpret_cast<ColorDataType*>(PixelData);
					*ColorData = 0;
				}
			}
		}
	}

	void HorizontalPass( int32 XStep )
	{
		const int32 XStart = XStep > 0 ? 0 : TextureWidth - 1;
		const int32 XEnd = XStep > 0 ? TextureWidth : -1;

		for( int32 Y = 0; Y < TextureHeight; ++Y )
		{
			uint8 FillRed = 0, FillGreen = 0, FillBlue = 0;
			bool bHaveFillColor = false;

			for( int32 X = XStart; X != XEnd; X += XStep )
			{
				PixelDataType* PixelData = SourceData + (Y * TextureWidth + X) * 4;
				const ColorDataType* ColorData = reinterpret_cast<const ColorDataType*>(PixelData);

				if( *ColorData == 0 )
				{
					if( bHaveFillColor )
					{
						PixelData[ RIdx ] = FillRed;
						PixelData[ GIdx ] = FillGreen;
						PixelData[ BIdx ] = FillBlue;
					}
				}
				else
				{
					bHaveFillColor = true;
					FillRed		= PixelData[ RIdx ];
					FillGreen	= PixelData[ GIdx ];
					FillBlue	= PixelData[ BIdx ];
				}
			}
		}
	}

	void VerticalPass( int32 YStep )
	{
		const int32 YStart = YStep > 0 ? 0 : TextureHeight - 1;
		const int32 YEnd = YStep > 0 ? TextureHeight : -1;

		for( int32 X = 0; X < TextureWidth; ++X )
		{
			uint8 FillRed = 0, FillGreen = 0, FillBlue = 0;
			bool bHaveFillColor = false;

			for( int32 Y = YStart; Y != YEnd; Y += YStep )
			{
				PixelDataType* PixelData = SourceData + (Y * TextureWidth + X) * 4;
				const ColorDataType* ColorData = reinterpret_cast<const ColorDataType*>(PixelData);

				if( *ColorData == 0 )
				{
					if( bHaveFillColor )
					{
						PixelData[ RIdx ] = FillRed;
						PixelData[ GIdx ] = FillGreen;
						PixelData[ BIdx ] = FillBlue;
					}
				}
				else
				{
					bHaveFillColor = true;
					FillRed		= PixelData[ RIdx ];
					FillGreen	= PixelData[ GIdx ];
					FillBlue	= PixelData[ BIdx ];
				}
			}
		}
	}

	PixelDataType* SourceData;
	int32 TextureWidth;
	int32 TextureHeight;
};

/**
 * For PNG texture importing, this ensures that any pixels with an alpha value of zero have an RGB
 * assigned to them from a neighboring pixel which has non-zero alpha.
 * This is needed as PNG exporters tend to turn pixels that are RGBA = (x,x,x,0) to (1,1,1,0)
 * and this produces artifacts when drawing the texture with bilinear filtering. 
 *
 * @param TextureSource - The source texture
 * @param SourceData - The source texture data
*/
void FillZeroAlphaPNGData( const FTextureSource& TextureSource, uint8* SourceData )
{
	const ETextureSourceFormat SourceFormat = TextureSource.GetFormat();

	switch( SourceFormat )
	{
		case TSF_BGRA8:
		{
			PNGDataFill<uint8, uint32, 2, 1, 0, 3> PNGFill( TextureSource, SourceData );
			PNGFill.ProcessData();
			break;
		}

		case TSF_RGBA16:
		{
			PNGDataFill<uint16, uint64, 0, 1, 2, 3> PNGFill( TextureSource, SourceData );
			PNGFill.ProcessData();
			break;
		}
	}
}

extern ENGINE_API bool GUseBilinearLightmaps;

UTexture* UTextureFactory::ImportTexture(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn)
{
	bool bAllowNonPowerOfTwo = false;
	GConfig->GetBool( TEXT("TextureImporter"), TEXT("AllowNonPowerOfTwoTextures"), bAllowNonPowerOfTwo, GEditorIni );

	// Validate it.
	const int32 Length = BufferEnd - Buffer;

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>( FName( "ImageWrapper" ) );

	//
	// PNG
	//
	IImageWrapperPtr PngImageWrapper = ImageWrapperModule.CreateImageWrapper( EImageFormat::PNG );
	if ( PngImageWrapper.IsValid() && PngImageWrapper->SetCompressed( Buffer, Length ) )
	{
		if ( !IsImportResolutionValid( PngImageWrapper->GetWidth(), PngImageWrapper->GetHeight(), bAllowNonPowerOfTwo, Warn ) )
		{
			return nullptr;
		}
		
		// Select the texture's source format
		ETextureSourceFormat TextureFormat = TSF_Invalid;
		int32 BitDepth = PngImageWrapper->GetBitDepth();
		ERGBFormat::Type Format = PngImageWrapper->GetFormat();
		if (Format == ERGBFormat::Gray)
		{
			if (BitDepth <= 8)
			{
				TextureFormat = TSF_G8;
				Format = ERGBFormat::Gray;
				BitDepth = 8;
			}
			else if (BitDepth == 16)
			{
				// TODO: TSF_G16?
				TextureFormat = TSF_RGBA16;
				Format = ERGBFormat::RGBA;
				BitDepth = 16;
			}
		}
		else if (Format == ERGBFormat::RGBA || Format == ERGBFormat::BGRA)
		{
			if (BitDepth <= 8)
			{
				TextureFormat = TSF_BGRA8;
				Format = ERGBFormat::BGRA;
				BitDepth = 8;
			}
			else if (BitDepth == 16)
			{
				TextureFormat = TSF_RGBA16;
				Format = ERGBFormat::RGBA;
				BitDepth = 16;
			}
		}

		if(TextureFormat == TSF_Invalid)
		{
			Warn->Logf(ELogVerbosity::Error, TEXT("PNG file contains data in an unsupported format."));
			return NULL;
		}

		UTexture2D* Texture = CreateTexture2D( InParent, Name, Flags );
		if ( Texture )
		{
			Texture->Source.Init(
				PngImageWrapper->GetWidth(),
				PngImageWrapper->GetHeight(),
				/*NumSlices=*/ 1,
				/*NumMips=*/ 1,
				TextureFormat
				);
			Texture->SRGB = true;
			const TArray<uint8>* RawPNG = nullptr;
			if ( PngImageWrapper->GetRaw( Format, BitDepth, RawPNG ) )
			{
				uint8* MipData = Texture->Source.LockMip(0);
				FMemory::Memcpy( MipData, RawPNG->GetTypedData(), RawPNG->Num() );

				// Replace the pixels with 0.0 alpha with a color value from the nearest neighboring color which has a non-zero alpha
				FillZeroAlphaPNGData( Texture->Source, MipData );
			}
			else
			{
				Warn->Logf(ELogVerbosity::Error, TEXT("Failed to decode PNG.") );
				Texture->Source.UnlockMip(0);
				Texture->MarkPendingKill();
				return nullptr;
			}
			Texture->Source.UnlockMip(0);
		}

		return Texture;
	}
	//
	// JPEG
	//
	IImageWrapperPtr JpegImageWrapper = ImageWrapperModule.CreateImageWrapper( EImageFormat::JPEG );
	if ( JpegImageWrapper.IsValid() && JpegImageWrapper->SetCompressed( Buffer, Length ) )
	{
		if ( !IsImportResolutionValid( JpegImageWrapper->GetWidth(), JpegImageWrapper->GetHeight(), bAllowNonPowerOfTwo, Warn ) )
		{
			return nullptr;
		}

		// Select the texture's source format
		ETextureSourceFormat TextureFormat = TSF_Invalid;
		int32 BitDepth = JpegImageWrapper->GetBitDepth();
		ERGBFormat::Type Format = JpegImageWrapper->GetFormat();

		if ( Format == ERGBFormat::Gray )
		{
			if ( BitDepth <= 8 )
			{
				TextureFormat = TSF_G8;
				Format = ERGBFormat::Gray;
				BitDepth = 8;
			}
		}
		else if ( Format == ERGBFormat::RGBA )
		{
			if ( BitDepth <= 8 )
			{
				TextureFormat = TSF_BGRA8;
				Format = ERGBFormat::BGRA;
				BitDepth = 8;
			}
		}

		if ( TextureFormat == TSF_Invalid )
		{
			Warn->Logf( ELogVerbosity::Error, TEXT( "JPEG file contains data in an unsupported format." ) );
			return nullptr;
		}

		UTexture2D* Texture = CreateTexture2D( InParent, Name, Flags );
		if ( Texture )
		{
			const TArray<uint8>* RawJPEG = nullptr;
			if ( JpegImageWrapper->GetRaw( Format, BitDepth, RawJPEG ) )
			{
				Texture->Source.Init(
					JpegImageWrapper->GetWidth(),
					JpegImageWrapper->GetHeight(),
					/*NumSlices=*/ 1,
					/*NumMips=*/ 1,
					TextureFormat
					);
				Texture->SRGB = true;
			
				uint8* MipData = Texture->Source.LockMip( 0 );
				FMemory::Memcpy( MipData, RawJPEG->GetTypedData(), RawJPEG->Num() );
				Texture->Source.UnlockMip( 0 );
			}
			else
			{
				Warn->Logf( ELogVerbosity::Error, TEXT( "Failed to decode JPEG." ) );
				Texture->MarkPendingKill();

				return nullptr;
			}
		}

		return Texture;
	}
	//
	// BMP
	//
	const FBitmapInfoHeader* bmhdr = (FBitmapInfoHeader *)(Buffer + sizeof(FBitmapFileHeader));
	const FBitmapFileHeader* bmf   = (FBitmapFileHeader *)(Buffer + 0);
	if( (Length>=sizeof(FBitmapFileHeader)+sizeof(FBitmapInfoHeader)) && Buffer[0]=='B' && Buffer[1]=='M' )
	{
		UTexture2D* Texture = 0;

		// Check the resolution of the imported texture to ensure validity
		if ( !IsImportResolutionValid(bmhdr->biWidth, bmhdr->biHeight, bAllowNonPowerOfTwo, Warn) )
		{
			return NULL;
		}
		if( bmhdr->biCompression != BCBI_RGB )
		{
			Warn->Logf(ELogVerbosity::Error, TEXT("RLE compression of BMP images not supported") );
			return NULL;
		}
		if( bmhdr->biPlanes==1 && bmhdr->biBitCount==8 )
		{
			Texture = CreateTexture2D( InParent, Name, Flags );
			if ( Texture )
			{
				// Do palette.
				const uint8* bmpal = (uint8*)Buffer + sizeof(FBitmapFileHeader) + sizeof(FBitmapInfoHeader);

				// Set texture properties.
				Texture->Source.Init(
					bmhdr->biWidth,
					bmhdr->biHeight,
					/*NumSlices=*/ 1,
					/*NumMips=*/ 1,
					TSF_BGRA8
					);
				FColor* MipData = (FColor*)Texture->Source.LockMip(0);

				// If the number for color palette entries is 0, we need to default to 2^biBitCount entries.  In this case 2^8 = 256
				int32 clrPaletteCount = bmhdr->biClrUsed ? bmhdr->biClrUsed : 256;
				TArray<FColor>	Palette;
				for( int32 i=0; i<clrPaletteCount; i++ )
					Palette.Add(FColor( bmpal[i*4+2], bmpal[i*4+1], bmpal[i*4+0], 255 ));
				while( Palette.Num()<256 )
					Palette.Add(FColor(0,0,0,255));

				// Copy upside-down scanlines.
				int32 SizeX = Texture->Source.GetSizeX();
				int32 SizeY = Texture->Source.GetSizeY();
				for(uint32 Y = 0;Y < bmhdr->biHeight;Y++)
				{
					for(uint32 X = 0;X < bmhdr->biWidth;X++)
					{
						MipData[(SizeY - Y - 1) * SizeX + X] = Palette[*((uint8*)Buffer + bmf->bfOffBits + Y * Align(bmhdr->biWidth,4) + X)];
					}
				}
				Texture->Source.UnlockMip(0);
			}
		}
		else if( bmhdr->biPlanes==1 && bmhdr->biBitCount==24 )
		{
			Texture = CreateTexture2D( InParent, Name, Flags );
			if ( Texture )
			{
				// Set texture properties.
				Texture->Source.Init(
					bmhdr->biWidth,
					bmhdr->biHeight,
					/*NumSlices=*/ 1,
					/*NumMips=*/ 1,
					TSF_BGRA8
					);
				uint8* MipData = Texture->Source.LockMip(0);

				// Copy upside-down scanlines.
				const uint8* Ptr = (uint8*)Buffer + bmf->bfOffBits;
				for( int32 y=0; y<(int32)bmhdr->biHeight; y++ ) 
				{
					uint8* DestPtr = &MipData[(bmhdr->biHeight - 1 - y) * bmhdr->biWidth * 4];
					uint8* SrcPtr = (uint8*) &Ptr[y * Align(bmhdr->biWidth*3,4)];
					for( int32 x=0; x<(int32)bmhdr->biWidth; x++ )
					{
						*DestPtr++ = *SrcPtr++;
						*DestPtr++ = *SrcPtr++;
						*DestPtr++ = *SrcPtr++;
						*DestPtr++ = 0xFF;
					}
				}
				Texture->Source.UnlockMip(0);
			}
		}
		else if( bmhdr->biPlanes==1 && bmhdr->biBitCount==32 )
		{
			Texture = CreateTexture2D( InParent, Name, Flags );
			if ( Texture )
			{
				// Set texture properties.
				Texture->Source.Init(
					bmhdr->biWidth,
					bmhdr->biHeight,
					/*NumSlices=*/ 1,
					/*NumMips=*/ 1,
					TSF_BGRA8
					);
				uint8* MipData = Texture->Source.LockMip(0);

				// Copy upside-down scanlines.
				const uint8* Ptr = (uint8*)Buffer + bmf->bfOffBits;
				for( int32 y=0; y<(int32)bmhdr->biHeight; y++ ) 
				{
					uint8* DestPtr = &MipData[(bmhdr->biHeight - 1 - y) * bmhdr->biWidth * 4];
					uint8* SrcPtr = (uint8*) &Ptr[y * bmhdr->biWidth * 4];
					for( int32 x=0; x<(int32)bmhdr->biWidth; x++ )
					{
						*DestPtr++ = *SrcPtr++;
						*DestPtr++ = *SrcPtr++;
						*DestPtr++ = *SrcPtr++;
						*DestPtr++ = *SrcPtr++;
					}
				}
				Texture->Source.UnlockMip(0);
			}
		}
		else if( bmhdr->biPlanes==1 && bmhdr->biBitCount==16 )
		{
			Warn->Logf(ELogVerbosity::Error, TEXT("BMP 16 bit format no longer supported. Use terrain tools for importing/exporting heightmaps.") );
			return NULL;
		}
		else
		{
			Warn->Logf(ELogVerbosity::Error, TEXT("BMP uses an unsupported format (%i/%i)"), bmhdr->biPlanes, bmhdr->biBitCount );
			return NULL;
		}

		return Texture;
	}
	//
	// PCX
	//
	const FPCXFileHeader*    PCX   = (FPCXFileHeader *)Buffer;
	if( Length >= sizeof(FPCXFileHeader) && PCX->Manufacturer==10 )
	{
		int32 NewU = PCX->XMax + 1 - PCX->XMin;
		int32 NewV = PCX->YMax + 1 - PCX->YMin;

		UTexture2D* Texture = 0;

		// Check the resolution of the imported texture to ensure validity
		if ( !IsImportResolutionValid(NewU, NewV, bAllowNonPowerOfTwo, Warn) )
		{
			return NULL;
		}
		else if( PCX->NumPlanes==1 && PCX->BitsPerPixel==8 )
		{
			Texture = CreateTexture2D( InParent, Name, Flags );
			if ( Texture )
			{
				// Set texture properties.
				Texture->Source.Init(
					NewU,
					NewV,
					/*NumSlices=*/ 1,
					/*NumMips=*/ 1,
					TSF_BGRA8
					);
				FColor* DestPtr = (FColor*)Texture->Source.LockMip(0);

				// Import the palette.
				uint8* PCXPalette = (uint8 *)(BufferEnd - 256 * 3);
				TArray<FColor>	Palette;
				for(uint32 i=0; i<256; i++ )
					Palette.Add(FColor(PCXPalette[i*3+0],PCXPalette[i*3+1],PCXPalette[i*3+2],i == 0 ? 0 : 255));

				// Import it.
				FColor* DestEnd	= DestPtr + NewU * NewV;
				Buffer += 128;
				while( DestPtr < DestEnd )
				{
					uint8 Color = *Buffer++;
					if( (Color & 0xc0) == 0xc0 )
					{
						uint32 RunLength = Color & 0x3f;
						Color = *Buffer++;
						checkf( (DestPtr+RunLength)<DestEnd, TEXT("RLE going off the end of buffer") );
						for(uint32 Index = 0;Index < RunLength;Index++)
							*DestPtr++ = Palette[Color];
					}
					else *DestPtr++ = Palette[Color];
				}
				Texture->Source.UnlockMip(0);
			}
		}
		else if( PCX->NumPlanes==3 && PCX->BitsPerPixel==8 )
		{
			Texture = CreateTexture2D( InParent, Name, Flags );
			if ( Texture )
			{
				// Set texture properties.
				Texture->Source.Init(
					NewU,
					NewV,
					/*NumSlices=*/ 1,
					/*NumMips=*/ 1,
					TSF_BGRA8
					);
				uint8* Dest = Texture->Source.LockMip(0);

				// Copy upside-down scanlines.
				Buffer += 128;
				int32 CountU = FMath::Min<int32>(PCX->BytesPerLine,NewU);
				for( int32 i=0; i<NewV; i++ )
				{

					// We need to decode image one line per time building RGB image color plane by color plane.
					int32 RunLength, Overflow=0;
					uint8 Color=0;
					for( int32 ColorPlane=2; ColorPlane>=0; ColorPlane-- )
					{
						for( int32 j=0; j<CountU; j++ )
						{
							if(!Overflow)
							{
								Color = *Buffer++;
								if((Color & 0xc0) == 0xc0)
								{
									RunLength=FMath::Min((Color&0x3f), CountU-j);
									Overflow=(Color&0x3f)-RunLength;
									Color=*Buffer++;
								}
								else
									RunLength = 1;
							}
							else
							{
								RunLength=FMath::Min(Overflow, CountU-j);
								Overflow=Overflow-RunLength;
							}
	
							checkf( ((i*NewU+RunLength)*4+ColorPlane) < (Texture->Source.CalcMipSize(0)), 
								TEXT("RLE going off the end of buffer") );
							for( int32 k=j; k<j+RunLength; k++ )
							{
								Dest[ (i*NewU+k)*4 + ColorPlane ] = Color;
							}
							j+=RunLength-1;
						}
					}				
				}
				Texture->Source.UnlockMip(0);
			}
		}
		else
		{
			Warn->Logf(ELogVerbosity::Error, TEXT("PCX uses an unsupported format (%i/%i)"), PCX->NumPlanes, PCX->BitsPerPixel );
			return NULL;
		}

		return Texture;
	}
	//
	// TGA
	//
	// Support for alpha stored as pseudo-color 8-bit TGA
	const FTGAFileHeader*    TGA   = (FTGAFileHeader *)Buffer;
	if (Length >= sizeof(FTGAFileHeader) &&
			 ((TGA->ColorMapType == 0 && TGA->ImageTypeCode == 2) ||
			  (TGA->ColorMapType == 0 && TGA->ImageTypeCode == 10) ||
			  (TGA->ColorMapType == 1 && TGA->ImageTypeCode == 1 && TGA->BitsPerPixel == 8)))
	{
		UTexture2D* Texture = 0;

		// Check the resolution of the imported texture to ensure validity
		if ( !IsImportResolutionValid(TGA->Width, TGA->Height, bAllowNonPowerOfTwo, Warn) )
		{
			return NULL;
		}
		if(TGA->ImageTypeCode == 10) // 10 = RLE compressed 
		{
			// RLE compression: CHUNKS: 1 -byte header, high bit 0 = raw, 1 = compressed
			// bits 0-6 are a 7-bit count; count+1 = number of raw pixels following, or rle pixels to be expanded. 
			if(TGA->BitsPerPixel == 32)
			{
				Texture = CreateTexture2D( InParent, Name, Flags );
				if ( Texture )
				{
					Texture->Source.Init(
						TGA->Width,
						TGA->Height,
						/*NumSlices=*/ 1,
						/*NumMips=*/ 1,
						TSF_BGRA8
						);
					uint32* TextureData = (uint32*)Texture->Source.LockMip(0);

					uint8*	IdData		= (uint8*)TGA + sizeof(FTGAFileHeader); 
					uint8*	ColorMap	= IdData + TGA->IdFieldLength;
					uint8*	ImageData	= (uint8*) (ColorMap + (TGA->ColorMapEntrySize + 4) / 8 * TGA->ColorMapLength);				
					uint32	Pixel		= 0;
					int32     RLERun		= 0;
					int32     RAWRun		= 0;

					for(int32 Y = TGA->Height-1; Y >=0; Y--) // Y-flipped.
					{					
						for(int32 X = 0;X < TGA->Width;X++)
						{						
							if( RLERun > 0 )
							{
								RLERun--;  // reuse current Pixel data.
							}
							else if( RAWRun == 0 ) // new raw pixel or RLE-run.
							{
								uint8 RLEChunk = *(ImageData++);							
								if( RLEChunk & 0x80 )
								{
									RLERun = ( RLEChunk & 0x7F ) + 1;
									RAWRun = 1;
								}
								else
								{
									RAWRun = ( RLEChunk & 0x7F ) + 1;
								}
							}							
							// Retrieve new pixel data - raw run or single pixel for RLE stretch.
							if( RAWRun > 0 )
							{
								Pixel = *(uint32*)ImageData; // RGBA 32-bit dword.
								ImageData += 4;
								RAWRun--;
								RLERun--;
							}
							// Store.
							*( (TextureData + Y*TGA->Width)+X ) = Pixel;
						}
					}
					Texture->Source.UnlockMip(0);
				}
			}
			else if( TGA->BitsPerPixel == 24 )
			{	
				Texture = CreateTexture2D( InParent, Name, Flags );
				if ( Texture )
				{
					Texture->Source.Init(
						TGA->Width,
						TGA->Height,
						/*NumSlices=*/ 1,
						/*NumMips=*/ 1,
						TSF_BGRA8
						);
					uint32* TextureData = (uint32*)Texture->Source.LockMip(0);

					uint8*	IdData = (uint8*)TGA + sizeof(FTGAFileHeader); 
					uint8*	ColorMap = IdData + TGA->IdFieldLength;
					uint8*	ImageData = (uint8*) (ColorMap + (TGA->ColorMapEntrySize + 4) / 8 * TGA->ColorMapLength);
					uint8    Pixel[4];
					int32     RLERun = 0;
					int32     RAWRun = 0;
				
					for(int32 Y = TGA->Height-1; Y >=0; Y--) // Y-flipped.
					{					
						for(int32 X = 0;X < TGA->Width;X++)
						{						
							if( RLERun > 0 )
								RLERun--;  // reuse current Pixel data.
							else if( RAWRun == 0 ) // new raw pixel or RLE-run.
							{
								uint8 RLEChunk = *(ImageData++);
								if( RLEChunk & 0x80 )
								{
									RLERun = ( RLEChunk & 0x7F ) + 1;
									RAWRun = 1;
								}
								else
								{
									RAWRun = ( RLEChunk & 0x7F ) + 1;
								}
							}							
							// Retrieve new pixel data - raw run or single pixel for RLE stretch.
							if( RAWRun > 0 )
							{
								Pixel[0] = *(ImageData++);
								Pixel[1] = *(ImageData++);
								Pixel[2] = *(ImageData++);
								Pixel[3] = 255;
								RAWRun--;
								RLERun--;
							}
							// Store.
							*( (TextureData + Y*TGA->Width)+X ) = *(uint32*)&Pixel;
						}
					}
					Texture->Source.UnlockMip(0);
				}
			}
			else if( TGA->BitsPerPixel == 16 )
			{
				Texture = CreateTexture2D( InParent, Name, Flags );
				if ( Texture )
				{
					Texture->Source.Init(
						TGA->Width,
						TGA->Height,
						/*NumSlices=*/ 1,
						/*NumMips=*/ 1,
						TSF_BGRA8
						);
					uint32* TextureData = (uint32*)Texture->Source.LockMip(0);
				
					uint8*	IdData = (uint8*)TGA + sizeof(FTGAFileHeader);
					uint8*	ColorMap = IdData + TGA->IdFieldLength;				
					uint16*	ImageData = (uint16*) (ColorMap + (TGA->ColorMapEntrySize + 4) / 8 * TGA->ColorMapLength);
					uint16  FilePixel = 0;
					uint32	TexturePixel = 0;
					int32     RLERun = 0;
					int32     RAWRun = 0;

					for(int32 Y = TGA->Height-1; Y >=0; Y--) // Y-flipped.
					{					
						for( int32 X=0;X<TGA->Width;X++ )
						{						
							if( RLERun > 0 )
								RLERun--;  // reuse current Pixel data.
							else if( RAWRun == 0 ) // new raw pixel or RLE-run.
							{
								uint8 RLEChunk =  *((uint8*)ImageData);
								ImageData = (uint16*)(((uint8*)ImageData)+1);
								if( RLEChunk & 0x80 )
								{
									RLERun = ( RLEChunk & 0x7F ) + 1;
									RAWRun = 1;
								}
								else
								{
									RAWRun = ( RLEChunk & 0x7F ) + 1;
								}
							}							
							// Retrieve new pixel data - raw run or single pixel for RLE stretch.
							if( RAWRun > 0 )
							{ 
								FilePixel = *(ImageData++);
								RAWRun--;
								RLERun--;
							}
							// Convert file format A1R5G5B5 into pixel format B8G8R8B8
							TexturePixel = (FilePixel & 0x001F) << 3;
							TexturePixel |= (FilePixel & 0x03E0) << 6;
							TexturePixel |= (FilePixel & 0x7C00) << 9;
							TexturePixel |= (FilePixel & 0x8000) << 16;
							// Store.
							*( (TextureData + Y*TGA->Width)+X ) = TexturePixel;
						}
					}
					Texture->Source.UnlockMip(0);
				}
			}
			else
			{
				Warn->Logf(ELogVerbosity::Error, TEXT("TGA uses an unsupported rle-compressed bit-depth: %u"),TGA->BitsPerPixel);
				return NULL;
			}
		}
		else if(TGA->ImageTypeCode == 2) // 2 = Uncompressed RGB
		{
			if(TGA->BitsPerPixel == 32)
			{
				Texture = CreateTexture2D( InParent, Name, Flags );
				if ( Texture )
				{
					Texture->Source.Init(
						TGA->Width,
						TGA->Height,
						/*NumSlices=*/ 1,
						/*NumMips=*/ 1,
						TSF_BGRA8
						);
					uint32* TextureData = (uint32*)Texture->Source.LockMip(0);

					uint8*	IdData = (uint8*)TGA + sizeof(FTGAFileHeader);
					uint8*	ColorMap = IdData + TGA->IdFieldLength;
					uint32*	ImageData = (uint32*) (ColorMap + (TGA->ColorMapEntrySize + 4) / 8 * TGA->ColorMapLength);

					for(int32 Y = 0;Y < TGA->Height;Y++)
					{
						FMemory::Memcpy(TextureData + Y * TGA->Width,ImageData + (TGA->Height - Y - 1) * TGA->Width,TGA->Width * 4);
					}
					Texture->Source.UnlockMip(0);
				}
			}
			else if(TGA->BitsPerPixel == 16)
			{
				Texture = CreateTexture2D( InParent, Name, Flags );
				Texture->Source.Init(
					TGA->Width,
					TGA->Height,
					/*NumSlices=*/ 1,
					/*NumMips=*/ 1,
					TSF_BGRA8
					);
				uint32* TextureData = (uint32*)Texture->Source.LockMip(0);

				uint8*	IdData = (uint8*)TGA + sizeof(FTGAFileHeader);
				uint8*	ColorMap = IdData + TGA->IdFieldLength;
				uint16*	ImageData = (uint16*) (ColorMap + (TGA->ColorMapEntrySize + 4) / 8 * TGA->ColorMapLength);
				uint16    FilePixel = 0;
				uint32	TexturePixel = 0;

				for (int32 Y = TGA->Height - 1; Y >= 0; Y--)
				{					
					for (int32 X = 0; X<TGA->Width; X++)
					{
						FilePixel = *ImageData++;
						// Convert file format A1R5G5B5 into pixel format B8G8R8A8
						TexturePixel = (FilePixel & 0x001F) << 3;
						TexturePixel |= (FilePixel & 0x03E0) << 6;
						TexturePixel |= (FilePixel & 0x7C00) << 9;
						TexturePixel |= (FilePixel & 0x8000) << 16;
						// Store.
						*((TextureData + Y*TGA->Width) + X) = TexturePixel;						
					}
				}
	   
				Texture->Source.UnlockMip(0);
			}            
			else if(TGA->BitsPerPixel == 24)
			{
				Texture = CreateTexture2D( InParent, Name, Flags );
				if ( Texture )
				{
					Texture->Source.Init(
						TGA->Width,
						TGA->Height,
						/*NumSlices=*/ 1,
						/*NumMips=*/ 1,
						TSF_BGRA8
						);
					uint32* TextureData = (uint32*)Texture->Source.LockMip(0);

					uint8*	IdData = (uint8*)TGA + sizeof(FTGAFileHeader);
					uint8*	ColorMap = IdData + TGA->IdFieldLength;
					uint8*	ImageData = (uint8*) (ColorMap + (TGA->ColorMapEntrySize + 4) / 8 * TGA->ColorMapLength);
					uint8    Pixel[4];

					for(int32 Y = 0;Y < TGA->Height;Y++)
					{
						for(int32 X = 0;X < TGA->Width;X++)
						{
							Pixel[0] = *(( ImageData+( TGA->Height-Y-1 )*TGA->Width*3 )+X*3+0);
							Pixel[1] = *(( ImageData+( TGA->Height-Y-1 )*TGA->Width*3 )+X*3+1);
							Pixel[2] = *(( ImageData+( TGA->Height-Y-1 )*TGA->Width*3 )+X*3+2);
							Pixel[3] = 255;
							*((TextureData+Y*TGA->Width)+X) = *(uint32*)&Pixel;
						}
					}
					Texture->Source.UnlockMip(0);
				}
			}
			else
			{
				Warn->Logf(ELogVerbosity::Error, TEXT("TGA uses an unsupported bit-depth: %u"),TGA->BitsPerPixel);
				return NULL;
			}
		}
		// Support for alpha stored as pseudo-color 8-bit TGA
		else if(TGA->ColorMapType == 1 && TGA->ImageTypeCode == 1 && TGA->BitsPerPixel == 8)
		{
			// Notes: The Scaleform GFx exporter (dll) strips all font glyphs into a single 8-bit texture.
			// The targa format uses this for a palette index; GFx uses a palette of (i,i,i,i) so the index
			// is also the alpha value.
			//
			// We store the image as PF_G8, where it will be used as alpha in the Glyph shader.

			Texture = CreateTexture2D( InParent, Name, Flags );
			if ( Texture )
			{
				Texture->Source.Init(
					TGA->Width,
					TGA->Height,
					/*NumSlices=*/ 1,
					/*NumMips=*/ 1,
					TSF_G8
					);
				uint8* TextureData = Texture->Source.LockMip(0);
				Texture->CompressionSettings = TC_Grayscale;

				const uint8*  const IdData = (uint8*)TGA + sizeof(FTGAFileHeader);
				const uint8*  const ColorMap = IdData + TGA->IdFieldLength;
				const uint8*  const ImageData = (uint8*) (ColorMap + (TGA->ColorMapEntrySize + 4) / 8 * TGA->ColorMapLength);

				int32 RevY = 0;
				for (int32 Y = TGA->Height-1; Y >= 0; --Y)
				{
					const uint8* ImageCol = ImageData + (Y * TGA->Width); 
					uint8* TextureCol = TextureData + (RevY++ * TGA->Width);
					FMemory::Memcpy(TextureCol, ImageCol, TGA->Width);
				}
				Texture->Source.UnlockMip(0);
			}
		}
		else
		{
			Warn->Logf(ELogVerbosity::Error, TEXT("TGA is an unsupported type: %u"),TGA->ImageTypeCode);
			return NULL;
		}

		// Flip the image data if the flip bits are set in the TGA header.
		bool FlipX = (TGA->ImageDescriptor & 0x10) ? 1 : 0;
		bool FlipY = (TGA->ImageDescriptor & 0x20) ? 1 : 0;
		if(FlipY || FlipX)
		{
			TArray<uint8> FlippedData;
			FlippedData.AddUninitialized(Texture->Source.CalcMipSize(0));

			int32 NumBlocksX = Texture->Source.GetSizeX();
			int32 NumBlocksY = Texture->Source.GetSizeY();
			int32 BlockBytes = Texture->Source.GetBytesPerPixel();

			uint8* MipData = Texture->Source.LockMip(0);
			for(int32 Y = 0;Y < NumBlocksY;Y++)
			{
				for(int32 X  = 0;X < NumBlocksX;X++)
				{
					int32 DestX = FlipX ? (NumBlocksX - X - 1) : X;
					int32 DestY = FlipY ? (NumBlocksY - Y - 1) : Y;
					FMemory::Memcpy(
						&FlippedData[(DestX + DestY * NumBlocksX) * BlockBytes],
						&MipData[(X + Y * NumBlocksX) * BlockBytes],
						BlockBytes
						);
				}
			}
			FMemory::Memcpy(MipData,FlippedData.GetData(),FlippedData.Num());
			Texture->Source.UnlockMip(0);
		}

		return Texture;
	}
	//
	// PSD File
	//
	FPSDFileHeader			 psdhdr;
	if (Length > sizeof(FPSDFileHeader))
	{
		psd_GetPSDHeader( Buffer, psdhdr );
	}
	if (psdhdr.IsValid())
	{
		// Check the resolution of the imported texture to ensure validity
		if ( !IsImportResolutionValid(psdhdr.Width, psdhdr.Height, bAllowNonPowerOfTwo, Warn) )
		{
			return NULL;
		}
		if (!psdhdr.IsSupported())
		{
			Warn->Logf( TEXT("Format of this PSD is not supported") );
			return NULL;
		}

		// Select the texture's source format
		ETextureSourceFormat TextureFormat = TSF_Invalid;
		if(psdhdr.Depth == 8)
		{
			TextureFormat = TSF_BGRA8;
		}
		else if(psdhdr.Depth == 16)
		{
			TextureFormat = TSF_RGBA16;
		}

		if(TextureFormat == TSF_Invalid)
		{
			Warn->Logf(ELogVerbosity::Error, TEXT("PSD file contains data in an unsupported format."));
			return NULL;
		}

		UTexture2D* Texture = CreateTexture2D( InParent, Name, Flags );
		if ( Texture )
		{
			// The psd is supported. Load it up.        
			Texture->Source.Init(
				psdhdr.Width,
				psdhdr.Height,
				/*NumSlices=*/ 1,
				/*NumMips=*/ 1,
				TextureFormat
				);
			uint8* Dst = (uint8*)Texture->Source.LockMip(0);

			if (!psd_ReadData( Dst, Buffer, psdhdr ))
			{
				Warn->Logf( TEXT("Failed to read this PSD") );
				Texture->Source.UnlockMip(0);
				Texture->MarkPendingKill();
				return NULL;
			}
			Texture->Source.UnlockMip(0);
		}

		return Texture;
	}
	//
	// DDS Cubemap
	//
	FDDSLoadHelper           DDSLoadHelper(Buffer, Length);
	if(DDSLoadHelper.IsValidCubemapTexture())
	{
		if(!IsImportResolutionValid(DDSLoadHelper.DDSHeader->dwWidth, DDSLoadHelper.DDSHeader->dwHeight, bAllowNonPowerOfTwo, Warn))
		{
			Warn->Logf(ELogVerbosity::Error, TEXT("DDS uses an unsupported format"));
			return NULL;
		}

		int32 NumMips = DDSLoadHelper.ComputeMipMapCount();
		ETextureSourceFormat Format = DDSLoadHelper.ComputeSourceFormat();
		if (Format == TSF_Invalid)
		{
			Warn->Logf(ELogVerbosity::Error, TEXT("DDS file contains data in an unsupported format."));
			return NULL;
		}

		// create the cube texture
		UTextureCube* TextureCube = CreateTextureCube( InParent, Name, Flags );

		if ( TextureCube )
		{
			TextureCube->Source.Init(
				DDSLoadHelper.DDSHeader->dwWidth, 
				DDSLoadHelper.DDSHeader->dwHeight,
				/*NumSlices=*/ 6,
				NumMips,
				Format
				);
			if(Format == TSF_RGBA16F)
			{
				TextureCube->CompressionSettings = TC_HDR;
			}

			uint8* DestMipData[MAX_TEXTURE_MIP_COUNT] = {0};
			int32 MipSize[MAX_TEXTURE_MIP_COUNT] = {0};
			for (int32 MipIndex = 0; MipIndex < NumMips; ++MipIndex)
			{
				DestMipData[MipIndex] = TextureCube->Source.LockMip(MipIndex);
				MipSize[MipIndex] = TextureCube->Source.CalcMipSize(MipIndex) / 6;
			}

			for (int32 SliceIndex = 0; SliceIndex < 6; ++SliceIndex)
			{
				const uint8* SrcMipData = DDSLoadHelper.GetDDSDataPointer((ECubeFace)SliceIndex);
				for (int32 MipIndex = 0; MipIndex < NumMips; ++MipIndex)
				{
					FMemory::Memcpy(DestMipData[MipIndex] + MipSize[MipIndex] * SliceIndex, SrcMipData, MipSize[MipIndex]);
					SrcMipData += MipSize[MipIndex];
				}
			}

			for (int32 MipIndex = 0; MipIndex < NumMips; ++MipIndex)
			{
				TextureCube->Source.UnlockMip(MipIndex);
			}

			// for now we don't support mip map generation on cubemaps
			TextureCube->MipGenSettings = TMGS_LeaveExistingMips;

			// generates the mips from the source art
			TextureCube->PostEditChange();
		}

		return TextureCube;
	}
	//
	// DDS Texture
	//
	if(DDSLoadHelper.IsValid2DTexture())
	{
		// DDS 2d texture
		if(!IsImportResolutionValid(DDSLoadHelper.DDSHeader->dwWidth, DDSLoadHelper.DDSHeader->dwHeight, bAllowNonPowerOfTwo, Warn))
		{
			Warn->Logf(ELogVerbosity::Error, TEXT("DDS uses an unsupported format"));
			return NULL;
		}
		
		ETextureSourceFormat SourceFormat = DDSLoadHelper.ComputeSourceFormat();
		uint32 MipMapCount = DDSLoadHelper.ComputeMipMapCount();
		if (SourceFormat != TSF_Invalid && MipMapCount > 0)
		{
			UTexture2D* Texture = CreateTexture2D( InParent, Name, Flags );
			if ( Texture )
			{
				Texture->Source.Init(
					DDSLoadHelper.DDSHeader->dwWidth,
					DDSLoadHelper.DDSHeader->dwHeight,
					/*NumSlices=*/ 1,
					MipMapCount,
					SourceFormat,
					DDSLoadHelper.GetDDSDataPointer()
					);

				if(MipMapCount > 1)
				{
					// if the source has mips we keep the mips by default, unless the user changes that
					MipGenSettings = TMGS_LeaveExistingMips;
				}				

				if(Texture->HasHDRSource())
				{
					Texture->CompressionSettings = TC_HDR;
				}

				// generates the mips from the source art
				Texture->PostEditChange();
			}

			return Texture;
		}
	}
	//
	// HDR File
	//
	FHDRLoadHelper           HDRLoadHelper(Buffer, Length);
	if(HDRLoadHelper.IsValid())
	{
		TArray<uint8> DDSFile;
		HDRLoadHelper.ExtractDDSInRGBE(DDSFile);
		FDDSLoadHelper HDRDDSLoadHelper(DDSFile.GetData(), DDSFile.Num());

		// create the cube texture
		UTextureCube* TextureCube = CreateTextureCube(InParent, Name, Flags);
		if ( TextureCube )
		{
			TextureCube->Source.Init(
				HDRDDSLoadHelper.DDSHeader->dwWidth,
				HDRDDSLoadHelper.DDSHeader->dwHeight,
				/*NumSlices=*/ 1,
				/*NumMips=*/ 1,
				TSF_BGRE8,
				HDRDDSLoadHelper.GetDDSDataPointer()
				);
			TextureCube->CompressionSettings = TC_HDR;
			TextureCube->PostEditChange();
		}

		return TextureCube;
	}
	//
	// IES File (usually measured real world light profiles)
	//
	if(!FCString::Stricmp(Type, TEXT("ies")))
	{
		// checks for .IES extension to avoid wasting loading large assets just to reject them during header parsing
		FIESLoadHelper IESLoadHelper(Buffer, Length);

		if(IESLoadHelper.IsValid())
		{
			TArray<uint8> RAWData;
			float Multiplier = IESLoadHelper.ExtractInRGBA16F(RAWData);

			UTextureLightProfile* Texture = Cast<UTextureLightProfile>( CreateOrOverwriteAsset(UTextureLightProfile::StaticClass(), InParent, Name, Flags) );
			if ( Texture )
			{
				Texture->Source.Init(
					IESLoadHelper.GetWidth(),
					IESLoadHelper.GetHeight(),
					/*NumSlices=*/ 1,
					1,
					TSF_RGBA16F,
					RAWData.GetData()
					);

				Texture->AddressX = TA_Clamp;
				Texture->AddressY = TA_Clamp;
				Texture->CompressionSettings = TC_HDR;
				MipGenSettings = TMGS_NoMipmaps;
				Texture->Brightness = IESLoadHelper.GetBrightness();
				Texture->TextureMultiplier = Multiplier;
				Texture->PostEditChange();
			}

			return Texture;
		}
	}

	return NULL;
}

bool UTextureFactory::DoesSupportClass(UClass* Class)
{
	return Class == UTexture2D::StaticClass() || Class == UTextureCube::StaticClass();
}

UObject* UTextureFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	EObjectFlags		Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const uint8*&		Buffer,
	const uint8*			BufferEnd,
	FFeedbackContext*	Warn
)
{
	check(Type);

	FEditorDelegates::OnAssetPreImport.Broadcast(this, Class, InParent, Name, Type);

	// if the texture already exists, remember the user settings
	UTexture* ExistingTexture = FindObject<UTexture>( InParent, *Name.ToString() );
	UTexture2D* ExistingTexture2D = FindObject<UTexture2D>( InParent, *Name.ToString() );

	TextureAddress						ExistingAddressX	= TA_Wrap;
	TextureAddress						ExistingAddressY	= TA_Wrap;
	TextureFilter						ExistingFilter		= TF_Default;
	TextureGroup						ExistingLODGroup	= TEXTUREGROUP_World;
	TextureCompressionSettings			ExistingCompressionSettings = TC_Default;
	int32									ExistingLODBias		= 0;
	int32									ExistingNumCinematicMipLevels = 0;
	bool								ExistingNeverStream = false;
	bool								ExistingSRGB		= false;
	bool								ExistingPreserveBorder = false;
	bool								ExistingNoCompression = false;
	bool								ExistingNoAlpha = false;
	bool								ExistingDeferCompression = false;
	bool 								ExistingDitherMipMapAlpha = false;
	bool 								ExistingFlipGreenChannel = false;
	float								ExistingAdjustBrightness = 1.0f;
	float								ExistingAdjustBrightnessCurve = 1.0f;
	float								ExistingAdjustVibrance = 0.0f;
	float								ExistingAdjustSaturation = 1.0f;
	float								ExistingAdjustRGBCurve = 1.0f;
	float								ExistingAdjustHue = 0.0f;
	float								ExistingAdjustMinAlpha = 0.0f;
	float								ExistingAdjustMaxAlpha = 1.0f;
	TextureMipGenSettings				ExistingMipGenSettings = TextureMipGenSettings(0);

	bUsingExistingSettings = bSuppressImportOverwriteDialog;

	if(ExistingTexture && !bSuppressImportOverwriteDialog)
	{
		// Prompt the user for what to do if a 'To All' response wasn't already given.
		if( OverwriteYesOrNoToAllState != EAppReturnType::YesAll && OverwriteYesOrNoToAllState != EAppReturnType::NoAll )
		{
			OverwriteYesOrNoToAllState = FMessageDialog::Open(EAppMsgType::YesNoYesAllNoAllCancel,
				NSLOCTEXT("UnrealEd", "ImportedTextureAlreadyExists_F", "The specified texture already exists and will be overwritten.  Do you want to keep the existing texture's settings?" ) );
		}

		switch( OverwriteYesOrNoToAllState )
		{

		case EAppReturnType::Yes:
		case EAppReturnType::YesAll:
			{
				// Preserve existing settings
				bUsingExistingSettings = true;
				break;
			}
		case EAppReturnType::No:
		case EAppReturnType::NoAll:
			{
				// Overwrite existing settings
				bUsingExistingSettings = false;
				break;
			}
		case EAppReturnType::Cancel:
		default:
			{
				FEditorDelegates::OnAssetPostImport.Broadcast( this, NULL );
				return NULL;
			}
		}
	}

	// Don't suppress future textures from checking for overwrites unless the calling code explicitly asks for it
	bSuppressImportOverwriteDialog = false;
	
	if (ExistingTexture && bUsingExistingSettings)
	{
		// save settings
		if(ExistingTexture2D)
		{
			ExistingAddressX	= ExistingTexture2D->AddressX;
			ExistingAddressY	= ExistingTexture2D->AddressY;
		}
		ExistingFilter		= ExistingTexture->Filter;
		ExistingLODGroup	= ExistingTexture->LODGroup;
		ExistingCompressionSettings = ExistingTexture->CompressionSettings;
		ExistingLODBias		= ExistingTexture->LODBias;
		ExistingNumCinematicMipLevels = ExistingTexture->NumCinematicMipLevels;
		ExistingNeverStream = ExistingTexture->NeverStream;
		ExistingSRGB		= ExistingTexture->SRGB;
		ExistingPreserveBorder = ExistingTexture->bPreserveBorder;
		ExistingNoCompression = ExistingTexture->CompressionNone;
		ExistingNoAlpha = ExistingTexture->CompressionNoAlpha;
		ExistingDeferCompression = ExistingTexture->DeferCompression;
		ExistingFlipGreenChannel = ExistingTexture->bFlipGreenChannel;
		ExistingDitherMipMapAlpha = ExistingTexture->bDitherMipMapAlpha;
		ExistingAdjustBrightness = ExistingTexture->AdjustBrightness;
		ExistingAdjustBrightnessCurve = ExistingTexture->AdjustBrightnessCurve;
		ExistingAdjustVibrance = ExistingTexture->AdjustVibrance;
		ExistingAdjustSaturation = ExistingTexture->AdjustSaturation;
		ExistingAdjustRGBCurve = ExistingTexture->AdjustRGBCurve;
		ExistingAdjustHue = ExistingTexture->AdjustHue;
		ExistingAdjustMinAlpha = ExistingTexture->AdjustMinAlpha;
		ExistingAdjustMaxAlpha = ExistingTexture->AdjustMaxAlpha;
		ExistingMipGenSettings = ExistingTexture->MipGenSettings;
	}

	if (ExistingTexture2D)
	{
		// Update with new settings, which should disable streaming...
		ExistingTexture2D->UpdateResource();
	}

	if (ExistingTexture)
	{
		// Release the existing resource so the new texture can get a fresh one. 
		ExistingTexture->ReleaseResource();
	}

	UTexture* Texture = ImportTexture(Class, InParent, Name, Flags, Type, Buffer, BufferEnd, Warn);

	if(!Texture)
	{
		if (ExistingTexture)
		{
			// We failed to import over the existing texture. Make sure the resource is ready in the existing texture.
			ExistingTexture->UpdateResource();
		}

		Warn->Logf(ELogVerbosity::Error, TEXT("Texture import failed") );
		FEditorDelegates::OnAssetPostImport.Broadcast( this, NULL );
		return NULL;
	}

	// Start with the value that the loader suggests.
	CompressionSettings = Texture->CompressionSettings;

	// Figure out whether we're using a normal map LOD group.
	bool bIsNormalMapLODGroup = false;
	if( LODGroup == TEXTUREGROUP_WorldNormalMap 
	||	LODGroup == TEXTUREGROUP_CharacterNormalMap
	||	LODGroup == TEXTUREGROUP_VehicleNormalMap
	||	LODGroup == TEXTUREGROUP_WeaponNormalMap )
	{
		// Change from default to normal map.
		if( CompressionSettings == TC_Default )
		{
			CompressionSettings = TC_Normalmap;
		}
		bIsNormalMapLODGroup = true;
	}

	// Propagate options.
	Texture->CompressionSettings	= CompressionSettings;

	// Packed normal map
	if( Texture->IsNormalMap() )
	{
		Texture->SRGB = 0;
		if( !bIsNormalMapLODGroup )
		{
			LODGroup = TEXTUREGROUP_WorldNormalMap;
		}
	}

	if(!FCString::Stricmp(Type, TEXT("ies")))
	{
		LODGroup = TEXTUREGROUP_IESLightProfile;
	}

	Texture->LODGroup				= LODGroup;

	// Revert the LODGroup to the default if it was forcibly set by the texture being a normal map.
	// This handles the case where multiple textures are being imported consecutively and
	// LODGroup unexpectedly changes because some textures were normal maps and others weren't.
	if ( LODGroup == TEXTUREGROUP_WorldNormalMap && !bIsNormalMapLODGroup )
	{
		LODGroup = TEXTUREGROUP_World;
	}

	Texture->CompressionNone		= NoCompression;
	Texture->CompressionNoAlpha		= NoAlpha;
	Texture->DeferCompression		= bDeferCompression;
	Texture->bDitherMipMapAlpha		= bDitherMipMapAlpha;
	Texture->MipGenSettings			= MipGenSettings;
	Texture->bPreserveBorder		= bPreserveBorder;

	Texture->SourceFilePath         = FReimportManager::SanitizeImportFilename(CurrentFilename, Texture);
	Texture->SourceFileTimestamp	= IFileManager::Get().GetTimeStamp(*CurrentFilename).ToString();

	UTexture2D* Texture2D = Cast<UTexture2D>(Texture);

	// Restore user set options
	if (ExistingTexture && bUsingExistingSettings)
	{
		if(Texture2D)
		{
			Texture2D->AddressX		= ExistingAddressX;
			Texture2D->AddressY		= ExistingAddressY;
		}

		Texture->Filter			= ExistingFilter;
		Texture->LODGroup		= ExistingLODGroup;
		Texture->CompressionSettings = ExistingCompressionSettings;
		Texture->LODBias		= ExistingLODBias;
		Texture->NumCinematicMipLevels = ExistingNumCinematicMipLevels;
		Texture->NeverStream	= ExistingNeverStream;
		Texture->SRGB			= ExistingSRGB;
		Texture->bPreserveBorder = ExistingPreserveBorder;
		Texture->CompressionNone = ExistingNoCompression;
		Texture->CompressionNoAlpha = ExistingNoAlpha;
		Texture->DeferCompression = ExistingDeferCompression;
		Texture->bDitherMipMapAlpha = ExistingDitherMipMapAlpha;
		Texture->bFlipGreenChannel = ExistingFlipGreenChannel;
		Texture->AdjustBrightness = ExistingAdjustBrightness;
		Texture->AdjustBrightnessCurve = ExistingAdjustBrightnessCurve;
		Texture->AdjustVibrance = ExistingAdjustVibrance;
		Texture->AdjustSaturation = ExistingAdjustSaturation;
		Texture->AdjustRGBCurve = ExistingAdjustRGBCurve;
		Texture->AdjustHue = ExistingAdjustHue;
		Texture->AdjustMinAlpha = ExistingAdjustMinAlpha;
		Texture->AdjustMaxAlpha = ExistingAdjustMaxAlpha;
		Texture->MipGenSettings = ExistingMipGenSettings;
	}
	else
	{
		Texture->bFlipGreenChannel = (bFlipNormalMapGreenChannel && Texture->IsNormalMap());
		// save user option
		GConfig->SetBool( TEXT("/Script/UnrealEd.EditorEngine"), TEXT("FlipNormalMapGreenChannel"), bFlipNormalMapGreenChannel, GEngineIni );
	}

	if(Texture2D)
	{
		// The texture has been imported and has no editor specific changes applied so we clear the painted flag.
		Texture2D->bHasBeenPaintedInEditor = false;
	}

	FEditorDelegates::OnAssetPostImport.Broadcast( this, Texture );

	// Invalidate any materials using the newly imported texture. (occurs if you import over an existing texture)
	Texture->PostEditChange();

	// If we are automatically creating a material for this texture...
	if( bCreateMaterial )
	{
		// Create the package for the material
		const FString MaterialName = FString::Printf( TEXT("%s_Mat"), *Name.ToString() );
		const FString MaterialPackageName = FPackageName::GetLongPackagePath(InParent->GetName()) + TEXT("/") + MaterialName;
		UPackage* MaterialPackage = CreatePackage(NULL, *MaterialPackageName);

		// Create the material
		UMaterialFactoryNew* Factory = new UMaterialFactoryNew(FPostConstructInitializeProperties());
		UMaterial* Material = (UMaterial*)Factory->FactoryCreateNew( UMaterial::StaticClass(), MaterialPackage, *MaterialName, Flags, Context, Warn );

		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(Material);

		// Create a texture reference for the texture we just imported and hook it up to the diffuse channel
		UMaterialExpression* Expression = ConstructObject<UMaterialExpression>( UMaterialExpressionTextureSample::StaticClass(), Material );
		Material->Expressions.Add( Expression );
		TArray<FExpressionOutput> Outputs;

		// If the user hasn't turned on any of the link checkboxes, default "bRGBToBaseColor" to being on.
		if( !bRGBToBaseColor && !bRGBToEmissive && !bAlphaToRoughness && !bAlphaToEmissive && !bAlphaToOpacity && !bAlphaToOpacityMask )
		{
			bRGBToBaseColor = 1;
		}

		// Set up the links the user asked for
		if( bRGBToBaseColor )
		{
			Material->BaseColor.Expression = Expression;
			((UMaterialExpressionTextureSample*)Material->BaseColor.Expression)->Texture = Texture;

			Outputs = Material->BaseColor.Expression->GetOutputs();
			FExpressionOutput* Output = Outputs.GetTypedData();
			Material->BaseColor.Mask = Output->Mask;
			Material->BaseColor.MaskR = Output->MaskR;
			Material->BaseColor.MaskG = Output->MaskG;
			Material->BaseColor.MaskB = Output->MaskB;
			Material->BaseColor.MaskA = Output->MaskA;
		}

		if( bRGBToEmissive )
		{
			Material->EmissiveColor.Expression = Expression;
			((UMaterialExpressionTextureSample*)Material->EmissiveColor.Expression)->Texture = Texture;

			Outputs = Material->EmissiveColor.Expression->GetOutputs();
			FExpressionOutput* Output = Outputs.GetTypedData();
			Material->EmissiveColor.Mask = Output->Mask;
			Material->EmissiveColor.MaskR = Output->MaskR;
			Material->EmissiveColor.MaskG = Output->MaskG;
			Material->EmissiveColor.MaskB = Output->MaskB;
			Material->EmissiveColor.MaskA = Output->MaskA;
		}

		if( bAlphaToRoughness )
		{
			Material->Roughness.Expression = Expression;
			((UMaterialExpressionTextureSample*)Material->Roughness.Expression)->Texture = Texture;

			Outputs = Material->Roughness.Expression->GetOutputs();
			FExpressionOutput* Output = Outputs.GetTypedData();
			Material->Roughness.Mask = Output->Mask;
			Material->Roughness.MaskR = 0;
			Material->Roughness.MaskG = 0;
			Material->Roughness.MaskB = 0;
			Material->Roughness.MaskA = 1;
		}

		if( bAlphaToEmissive )
		{
			Material->EmissiveColor.Expression = Expression;
			((UMaterialExpressionTextureSample*)Material->EmissiveColor.Expression)->Texture = Texture;

			Outputs = Material->EmissiveColor.Expression->GetOutputs();
			FExpressionOutput* Output = Outputs.GetTypedData();
			Material->EmissiveColor.Mask = Output->Mask;
			Material->EmissiveColor.MaskR = 0;
			Material->EmissiveColor.MaskG = 0;
			Material->EmissiveColor.MaskB = 0;
			Material->EmissiveColor.MaskA = 1;
		}

		if( bAlphaToOpacity )
		{
			Material->Opacity.Expression = Expression;
			((UMaterialExpressionTextureSample*)Material->Opacity.Expression)->Texture = Texture;

			Outputs = Material->Opacity.Expression->GetOutputs();
			FExpressionOutput* Output = Outputs.GetTypedData();
			Material->Opacity.Mask = Output->Mask;
			Material->Opacity.MaskR = 0;
			Material->Opacity.MaskG = 0;
			Material->Opacity.MaskB = 0;
			Material->Opacity.MaskA = 1;
		}

		if( bAlphaToOpacityMask )
		{
			Material->OpacityMask.Expression = Expression;
			((UMaterialExpressionTextureSample*)Material->OpacityMask.Expression)->Texture = Texture;

			Outputs = Material->OpacityMask.Expression->GetOutputs();
			FExpressionOutput* Output = Outputs.GetTypedData();
			Material->OpacityMask.Mask = Output->Mask;
			Material->OpacityMask.MaskR = 0;
			Material->OpacityMask.MaskG = 0;
			Material->OpacityMask.MaskB = 0;
			Material->OpacityMask.MaskA = 1;
		}

		Material->TwoSided	= bTwoSided;
		Material->BlendMode = Blending;
		Material->SetLightingModel(LightingModel);

		Material->PostEditChange();
	}
	return Texture;
}


bool UTextureFactory::IsImportResolutionValid(int32 Width, int32 Height, bool bAllowNonPowerOfTwo, FFeedbackContext* Warn)
{
	// Calculate the maximum supported resolution utilizing the global max texture mip count
	// (Note, have to subtract 1 because 1x1 is a valid mip-size; this means a GMaxTextureMipCount of 4 means a max resolution of 8x8, not 2^4 = 16x16)
	const int32 MaximumSupportedResolution = 1 << (GMaxTextureMipCount - 1);

	bool bValid = true;

	// Check if the texture is above the supported resolution and prompt the user if they wish to continue if it is
	if ( Width > MaximumSupportedResolution || Height > MaximumSupportedResolution )
	{
		if ( EAppReturnType::Yes != FMessageDialog::Open( EAppMsgType::YesNo, FText::Format(
				NSLOCTEXT("UnrealEd", "Warning_LargeTextureImport", "Attempting to import {0} x {1} texture, proceed?\nLargest supported texture size: {2} x {3}"),
				FText::AsNumber(Width), FText::AsNumber(Height), FText::AsNumber(MaximumSupportedResolution), FText::AsNumber(MaximumSupportedResolution)) ) )
		{
			bValid = false;
		}
	}

	const bool bIsPowerOfTwo = FMath::IsPowerOfTwo( Width ) && FMath::IsPowerOfTwo( Height );
	// Check if the texture dimensions are powers of two
	if ( !bAllowNonPowerOfTwo && !bIsPowerOfTwo )
	{
		Warn->Logf(ELogVerbosity::Error, *NSLOCTEXT("UnrealEd", "Warning_TextureNotAPowerOfTwo", "Cannot import texture with non-power of two dimensions").ToString() );
		bValid = false;
	}

	// If we are allowed to warn about NPT textures and the texture is not a power of two, display a warning.
	if( bAllowOneTimeWarningMessages && !bSuppressImportResolutionWarnings && bAllowNonPowerOfTwo && !bIsPowerOfTwo && bValid )
	{
		bAllowOneTimeWarningMessages = false;
		if( EAppReturnType::Yes != FMessageDialog::Open( EAppMsgType::YesNo, NSLOCTEXT("UnrealEd", "Warning_NPTTexture", "The texture you are importing is not a power of two.  Non power of two textures are never streamed and have no mipmaps. Proceed?") ) )
		{
			bValid = false;
		}
	}

	// Reset the suppression so that future imports can still warn
	bSuppressImportResolutionWarnings = false;
	
	return bValid;
}


/*------------------------------------------------------------------------------
	UTextureExporterPCX implementation.
------------------------------------------------------------------------------*/
UTextureExporterPCX::UTextureExporterPCX(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SupportedClass = UTexture2D::StaticClass();
	PreferredFormatIndex = 0;
	FormatExtension.Add(TEXT("PCX"));
	FormatDescription.Add(TEXT("PCX File"));

}

bool UTextureExporterPCX::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex, uint32 PortFlags )
{
	UTexture2D* Texture = CastChecked<UTexture2D>( Object );

	if( !Texture->Source.IsValid() || Texture->Source.GetFormat() != TSF_BGRA8 )
	{
		return false;
	}

	int32 SizeX = Texture->Source.GetSizeX();
	int32 SizeY = Texture->Source.GetSizeY();
	TArray<uint8> RawData;
	Texture->Source.GetMipData(RawData, 0);

	// Set all PCX file header properties.
	FPCXFileHeader PCX;
	FMemory::Memzero( &PCX, sizeof(PCX) );
	PCX.Manufacturer	= 10;
	PCX.Version			= 05;
	PCX.Encoding		= 1;
	PCX.BitsPerPixel	= 8;
	PCX.XMin			= 0;
	PCX.YMin			= 0;
	PCX.XMax			= SizeX-1;
	PCX.YMax			= SizeY-1;
	PCX.XDotsPerInch	= SizeX;
	PCX.YDotsPerInch	= SizeY;
	PCX.BytesPerLine	= SizeX;
	PCX.PaletteType		= 0;
	PCX.HScreenSize		= 0;
	PCX.VScreenSize		= 0;

	// Copy all RLE bytes.
	uint8 RleCode=0xc1;

	PCX.NumPlanes = 3;
	Ar << PCX;
	for( int32 Line=0; Line<SizeY; Line++ )
	{
		for( int32 ColorPlane = 2; ColorPlane >= 0; ColorPlane-- )
		{
			uint8* ScreenPtr = RawData.GetTypedData() + (Line * SizeX * 4) + ColorPlane;
			for( int32 Row=0; Row<SizeX; Row++ )
			{
				if( (*ScreenPtr&0xc0)==0xc0 )
					Ar << RleCode;
				Ar << *ScreenPtr;
				ScreenPtr += 4;
			}
		}
	}

	return true;
}

/*------------------------------------------------------------------------------
	UTextureExporterBMP implementation.
------------------------------------------------------------------------------*/
UTextureExporterBMP::UTextureExporterBMP(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SupportedClass = UTexture2D::StaticClass();
	PreferredFormatIndex = 0;
	FormatExtension.Add(TEXT("BMP"));
	FormatDescription.Add(TEXT("Windows Bitmap"));

}

bool UTextureExporterBMP::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex, uint32 PortFlags )
{
	UTexture2D* Texture = CastChecked<UTexture2D>( Object );

	if( !Texture->Source.IsValid() || Texture->Source.GetFormat() != TSF_BGRA8 )
	{
		return false;
	}

	int32 SizeX = Texture->Source.GetSizeX();
	int32 SizeY = Texture->Source.GetSizeY();
	TArray<uint8> RawData;
	Texture->Source.GetMipData(RawData, 0);

	FBitmapFileHeader bmf;
	FBitmapInfoHeader bmhdr;

	// File header.
	bmf.bfType      = 'B' + (256*(int32)'M');
	bmf.bfReserved1 = 0;
	bmf.bfReserved2 = 0;
	int32 biSizeImage	= SizeX * SizeY * 3;
	bmf.bfOffBits   = sizeof(FBitmapFileHeader) + sizeof(FBitmapInfoHeader);
	bmhdr.biBitCount= 24;

	bmf.bfSize		= bmf.bfOffBits + biSizeImage;
	Ar << bmf;

	// Info header.
	bmhdr.biSize          = sizeof(FBitmapInfoHeader);
	bmhdr.biWidth         = SizeX;
	bmhdr.biHeight        = SizeY;
	bmhdr.biPlanes        = 1;
	bmhdr.biCompression   = BCBI_RGB;
	bmhdr.biSizeImage     = biSizeImage;
	bmhdr.biXPelsPerMeter = 0;
	bmhdr.biYPelsPerMeter = 0;
	bmhdr.biClrUsed       = 0;
	bmhdr.biClrImportant  = 0;
	Ar << bmhdr;


	// Upside-down scanlines.
	for( int32 i=SizeY-1; i>=0; i-- )
	{
		uint8* ScreenPtr = &RawData[i*SizeX*4];
		for( int32 j=SizeX; j>0; j-- )
		{
			Ar << *ScreenPtr++;
			Ar << *ScreenPtr++;
			Ar << *ScreenPtr++;
			ScreenPtr++;
		}
	}
	return true;
}

/*------------------------------------------------------------------------------
	HDR file format helper.
------------------------------------------------------------------------------*/
class FHDRExportHelper
{
	void WriteScanLine(FArchive& Ar, const TArray<uint8>& ScanLine)
	{
		const uint8* LineEnd    = ScanLine.GetTypedData() + ScanLine.Num();
		const uint8* LineSource = ScanLine.GetTypedData();
		TArray<uint8> Output;
		Output.Reserve(ScanLine.Num() * 2);
		while (LineSource < LineEnd)
		{
			int32 CurrentPos = 0;
			int32 NextPos = 0;
			int32 CurrentRunLength = 0;
			while (CurrentRunLength <= 4 && NextPos < 128 && LineSource + NextPos < LineEnd)
			{
				CurrentPos = NextPos;
				CurrentRunLength = 0;
				while (CurrentRunLength < 127 && CurrentPos + CurrentRunLength < 128 && LineSource + NextPos < LineEnd && LineSource[CurrentPos] == LineSource[NextPos])
				{
					NextPos++;
					CurrentRunLength++;
				}
			}

			if (CurrentRunLength > 4)
			{
				// write a non run: LineSource[0] to LineSource[CurrentPos]
				if (CurrentPos > 0)
				{
					Output.Add(CurrentPos);
					for (int32 i = 0 ; i < CurrentPos ; i++)
					{
						Output.Add(LineSource[i]);
					}
				}
				Output.Add((uint8)(128 + CurrentRunLength));
				Output.Add(LineSource[CurrentPos]);
			}
			else
			{
				// write a non run: LineSource[0] to LineSource[NextPos]
				Output.Add((uint8)(NextPos));
				for (int32 i = 0 ; i < NextPos ; i++)
				{
					Output.Add((uint8)(LineSource[i]));
				}
			}
			LineSource += NextPos;
		}
		Ar.Serialize(Output.GetTypedData(), Output.Num());
	}

	static FColor ToRGBEDithered(const FLinearColor& ColorIN, const FRandomStream& Rand)
	{
		const float R = ColorIN.R;
		const float G = ColorIN.G;
		const float B = ColorIN.B;
		const float Primary = FMath::Max3(R, G, B);
		FColor	ReturnColor;

		if (Primary < 1E-32)
		{
			ReturnColor = FColor(0, 0, 0, 0);
		}
		else
		{
			int32 Exponent;
			const float Scale	 = frexp(Primary, &Exponent) / Primary * 255.f;

			ReturnColor.R = FMath::Clamp(FMath::TruncToInt((R* Scale) + Rand.GetFraction()), 0, 255);
			ReturnColor.G = FMath::Clamp(FMath::TruncToInt((G* Scale) + Rand.GetFraction()), 0, 255);
			ReturnColor.B = FMath::Clamp(FMath::TruncToInt((B* Scale) + Rand.GetFraction()), 0, 255);
			ReturnColor.A = FMath::Clamp(FMath::TruncToInt(Exponent), -128, 127) + 128;
		}

		return ReturnColor;
	}

	template<typename TSourceColorType> void WriteHDRBits(FArchive& Ar, TSourceColorType* SourceTexels)
	{
		const FRandomStream RandomStream(0xA1A1);
		const int32 NumChannels = 4;
		const int32 SizeX = Size.X;
		const int32 SizeY = Size.Y;
		TArray<uint8> ScanLine[NumChannels];
		for (int32 Channel = 0; Channel < NumChannels; Channel++)
		{
			ScanLine[Channel].Reserve(SizeX);
		}

		for (int32 y = 0 ; y < SizeY ; y++)
		{
			// write RLE header
			uint8 RLEheader[4];
			RLEheader[0] = 2;
			RLEheader[1] = 2;
			RLEheader[2] = SizeX >> 8;
			RLEheader[3] = SizeX & 0xFF;
			Ar.Serialize(&RLEheader[0], sizeof(RLEheader));

			for (int32 Channel = 0; Channel < NumChannels; Channel++)
			{
				ScanLine[Channel].Reset();
			}

			for (int32 x = 0 ; x < SizeX ; x++)
			{
				FLinearColor LinearColor(*SourceTexels);
				FColor RGBEColor = ToRGBEDithered(LinearColor, RandomStream);

				FLinearColor lintest = RGBEColor.FromRGBE();
				ScanLine[0].Add(RGBEColor.R);
				ScanLine[1].Add(RGBEColor.G);
				ScanLine[2].Add(RGBEColor.B);
				ScanLine[3].Add(RGBEColor.A);
				SourceTexels++;
			}

			for (int32 Channel = 0; Channel < NumChannels; Channel++)
			{
				WriteScanLine(Ar, ScanLine[Channel]);
			}
		}
	}

	void WriteHDRHeader(FArchive& Ar)
	{
		const int32 MaxHeaderSize = 256;
		char Header[MAX_SPRINTF];
		FCStringAnsi::Sprintf(Header, "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y %d +X %d\n", Size.Y, Size.X);
		Header[MaxHeaderSize - 1] = 0;
		int32 Len = FMath::Min(FCStringAnsi::Strlen(Header), MaxHeaderSize);
		Ar.Serialize(Header, Len);
	}

	/**
	* Returns data containing the pixmap of the passed in rendertarget.
	* @param TexRT - The 2D rendertarget from which to read pixmap data.
	* @param RawData - an array to be filled with pixel data.
	* @return true if RawData has been successfully filled.
	*/
	bool GetRawData(UTextureRenderTarget2D* TexRT, TArray<uint8>& RawData)
	{
		FRenderTarget* RenderTarget = TexRT->GameThread_GetRenderTargetResource();
		int32 ImageBytes = CalculateImageBytes(TexRT->SizeX, TexRT->SizeY, 0, Format);
		RawData.AddUninitialized(ImageBytes);
		bool bReadSuccess = false;
		switch (Format)
		{
			case PF_FloatRGBA:
			{
				TArray<FFloat16Color> FloatColors;
				bReadSuccess = RenderTarget->ReadFloat16Pixels(FloatColors);
				FMemory::Memcpy(RawData.GetTypedData(), FloatColors.GetTypedData(), ImageBytes);
			}
			break;
			case PF_B8G8R8A8:
				bReadSuccess = RenderTarget->ReadPixelsPtr((FColor*)RawData.GetTypedData());
			break;
		}
		if (bReadSuccess == false)
		{
			RawData.Empty();
		}
		return bReadSuccess;
	}

	void WriteHDRImage(const TArray<uint8>& RawData, FArchive& Ar)
	{
		WriteHDRHeader(Ar);
		if (Format == PF_FloatRGBA)
		{
			WriteHDRBits(Ar, (FFloat16Color*)RawData.GetTypedData());
		}
		else
		{
			WriteHDRBits(Ar, (FColor*)RawData.GetTypedData());
		}
	}

	FIntPoint Size;
	EPixelFormat Format;
public:
	/**
	* Writes HDR format image to an FArchive
	* @param TexRT - A 2D source render target to read from.
	* @param Ar - Archive object to write HDR data to.
	* @return true on successful export.
	*/
	bool ExportHDR(UTextureRenderTarget2D* TexRT, FArchive& Ar)
	{
		check(TexRT != NULL);
		FRenderTarget* RenderTarget = TexRT->GameThread_GetRenderTargetResource();
		Size = RenderTarget->GetSizeXY();
		Format = TexRT->GetFormat();

		TArray<uint8> RawData;
		bool bReadSuccess = GetRawData(TexRT, RawData);
		if (bReadSuccess)
		{
			WriteHDRImage(RawData, Ar);
			return true;
		}
		return false;
	}

	/**
	* Writes HDR format image to an FArchive
	* This function unwraps the cube image on to a 2D surface.
	* @param TexCube - A cube source (render target or cube texture) to read from.
	* @param Ar - Archive object to write HDR data to.
	* @return true on successful export.
	*/
	template <typename TCubeTextureType>
	bool ExportHDR(TCubeTextureType* TexCube, FArchive& Ar)
	{
		check(TexCube != NULL);

		// Generate 2D image.
		TArray<uint8> RawData;
		bool bUnwrapSuccess = CubemapHelpers::GenerateLongLatUnwrap(TexCube, RawData, Size, Format);
		bool bAcceptableFormat = (Format == PF_B8G8R8A8 || Format == PF_FloatRGBA);
		if (bUnwrapSuccess == false || bAcceptableFormat == false)
		{
			return false;
		}

		WriteHDRImage(RawData, Ar);

		return true;
	}
};

/*------------------------------------------------------------------------------
	UTextureExporterHDR implementation.
	Exports render targets.
------------------------------------------------------------------------------*/
UTextureExporterHDR::UTextureExporterHDR(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SupportedClass = UTextureRenderTarget::StaticClass();
	PreferredFormatIndex = 0;
	FormatExtension.Add(TEXT("HDR"));
	FormatDescription.Add(TEXT("HDR"));
}

bool UTextureExporterHDR::ExportBinary(UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex, uint32 PortFlags)
{
	UTextureRenderTarget2D* TexRT2D = Cast<UTextureRenderTarget2D>(Object);
	UTextureRenderTargetCube* TexRTCube = Cast<UTextureRenderTargetCube>(Object);

	FHDRExportHelper Exporter;
	if (TexRT2D != NULL)
	{
		return Exporter.ExportHDR(TexRT2D, Ar);
	}
	else if (TexRTCube != NULL)
	{
		return Exporter.ExportHDR(TexRTCube, Ar);
	}
	return false;
}

/*------------------------------------------------------------------------------
	UTextureCubeExporterHDR implementation.
	Export UTextureCubes as .HDR
------------------------------------------------------------------------------*/
UTextureCubeExporterHDR::UTextureCubeExporterHDR(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SupportedClass = UTextureCube::StaticClass();
	PreferredFormatIndex = 0;
	FormatExtension.Add(TEXT("HDR"));
	FormatDescription.Add(TEXT("HDR"));
}

bool UTextureCubeExporterHDR::ExportBinary(UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex, uint32 PortFlags)
{
	UTextureCube* TexCube = Cast<UTextureCube>(Object);

	FHDRExportHelper Exporter;
	if (TexCube != NULL)
	{
		return Exporter.ExportHDR(TexCube, Ar);
	}
	return false;
}

/*------------------------------------------------------------------------------
	UTextureExporterTGA implementation.
------------------------------------------------------------------------------*/
UTextureExporterTGA::UTextureExporterTGA(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = UTexture2D::StaticClass();
	PreferredFormatIndex = 0;
	FormatExtension.Add(TEXT("TGA"));
	FormatDescription.Add(TEXT("Targa"));
}

bool UTextureExporterTGA::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex, uint32 PortFlags )
{
	UTexture2D* Texture = CastChecked<UTexture2D>( Object );

	if( !Texture->Source.IsValid() || Texture->Source.GetFormat() != TSF_BGRA8 )
	{
		return false;
	}

	int32 SizeX = Texture->Source.GetSizeX();
	int32 SizeY = Texture->Source.GetSizeY();
	TArray<uint8> RawData;
	Texture->Source.GetMipData(RawData, 0);

	// If we should export the file with no alpha info.  
	// If the texture is compressed with no alpha we should definitely not export an alpha channel
	bool bExportWithAlpha = !Texture->CompressionNoAlpha;
	if( bExportWithAlpha )
	{
		// If the texture isn't compressed with no alpha scan the texture to see if the alpha values are all 255 which means we can skip exporting it.
		// This is a relatively slow process but we are just exporting textures 
		bExportWithAlpha = false;
		for( int32 Y = SizeY - 1; Y >= 0; --Y )
		{
			uint8* Color = &RawData[Y * SizeX * 4];
			for( int32 X = SizeX; X > 0; --X )
			{
				// Skip color info
				Color+=3;
				// Get Alpha value then increment the pointer past it for the next pixel
				uint8 Alpha = *Color++;
				if( Alpha != 255 )
				{
					// When a texture is imported with no alpha, the alpha bits are set to 255
					// So if the texture has non 255 alpha values, the texture is a valid alpha channel
					bExportWithAlpha = true;
					break;
				}
			}
			if( bExportWithAlpha )
			{
				break;
			}
		}
	}

	const int32 OriginalWidth = SizeX;
	const int32 OriginalHeight = SizeY;		

	FTGAFileHeader TGA;
	FMemory::Memzero( &TGA, sizeof(TGA) );
	TGA.ImageTypeCode = 2;
	TGA.BitsPerPixel = bExportWithAlpha ? 32 : 24 ;
	TGA.Height = OriginalHeight;
	TGA.Width = OriginalWidth;
	Ar.Serialize( &TGA, sizeof(TGA) );

	if( bExportWithAlpha )
	{
		for( int32 Y=0;Y < OriginalHeight;Y++ )
		{
			// If we aren't skipping alpha channels we can serialize each line
			Ar.Serialize( &RawData[ (OriginalHeight - Y - 1) * OriginalWidth * 4 ], OriginalWidth * 4 );
		}
	}
	else
	{
		// Serialize each pixel
		for( int32 Y = OriginalHeight - 1; Y >= 0; --Y )
		{
			uint8* Color = &RawData[Y * OriginalWidth * 4];
			for( int32 X = OriginalWidth; X > 0; --X )
			{
				Ar << *Color++;
				Ar << *Color++;
				Ar << *Color++;
				// Skip alpha channel since we are exporting with no alpha
				Color++;
			}
		}
	}

	FTGAFileFooter Ftr;
	FMemory::Memzero( &Ftr, sizeof(Ftr) );
	FMemory::Memcpy( Ftr.Signature, "TRUEVISION-XFILE", 16 );
	Ftr.TrailingPeriod = '.';
	Ar.Serialize( &Ftr, sizeof(Ftr) );
	return true;
}

/*------------------------------------------------------------------------------
	UFontFactory.
------------------------------------------------------------------------------*/

//
//	Fast pixel-lookup.
//
static inline uint8 AT( const uint8* Screen, uint32 SXL, uint32 X, uint32 Y )
{
	return Screen[X+Y*SXL];
}

//
// Codepage 850 -> Latin-1 mapping table:
//
uint8 FontRemap[256] = 
{
	0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
	48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,

	64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
	80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
	96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
	112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,

	000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,
	032,173,184,156,207,190,124,245,034,184,166,174,170,196,169,238,
	248,241,253,252,239,230,244,250,247,251,248,175,172,171,243,168,

	183,181,182,199,142,143,146,128,212,144,210,211,222,214,215,216,
	209,165,227,224,226,229,153,158,157,235,233,234,154,237,231,225,
	133,160,131,196,132,134,145,135,138,130,136,137,141,161,140,139,
	208,164,149,162,147,228,148,246,155,151,163,150,129,236,232,152,
};

//
//	Find the border around a font glyph that starts at x,y (it's upper
//	left hand corner).  If it finds a glyph box, it returns 0 and the
//	glyph 's length (xl,yl).  Otherwise returns -1.
//
static bool ScanFontBox( const uint8* Data, int32 X, int32 Y, int32& XL, int32& YL, int32 SizeX )
{
	int32 FontXL = SizeX;

	// Find x-length.
	int32 NewXL = 1;
	while ( AT(Data,FontXL,X+NewXL,Y)==255 && AT(Data,FontXL,X+NewXL,Y+1)!=255 )
	{
		NewXL++;
	}

	if( AT(Data,FontXL,X+NewXL,Y)!=255 )
	{
		return 0;
	}

	// Find y-length.
	int32 NewYL = 1;
	while( AT(Data,FontXL,X,Y+NewYL)==255 && AT(Data,FontXL,X+1,Y+NewYL)!=255 )
	{
		NewYL++;
	}

	if( AT(Data,FontXL,X,Y+NewYL)!=255 )
	{
		return 0;
	}

	XL = NewXL - 1;
	YL = NewYL - 1;

	return 1;
}

#define NUM_FONT_CHARS 256

UFontFactory::UFontFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SupportedClass = UFont::StaticClass();
	bEditorImport = false;
	LODGroup = TEXTUREGROUP_UI;
}

UObject* UFontFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	EObjectFlags		Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const uint8*&		Buffer,
	const uint8*			BufferEnd,
	FFeedbackContext*	Warn
)
{
	FEditorDelegates::OnAssetPreImport.Broadcast(this, Class, InParent, Name, Type);

	check(Type);

	check(Class==UFont::StaticClass());
	UFont* Font = new( InParent, Name, Flags )UFont(FPostConstructInitializeProperties());
	// note RF_Public because font textures can be referenced direclty by material expressions
	UTexture2D* Tex = CastChecked<UTexture2D>( UTextureFactory::FactoryCreateBinary( 
		UTexture2D::StaticClass(), Font, NAME_None, RF_Public, Context, Type, Buffer, BufferEnd, Warn ) );

	if( Tex != NULL )
	{
		Tex->LODGroup = TEXTUREGROUP_UI;  // set the LOD group otherwise this will be in the World Group

		// Also, we never want to stream in font textures since that always looks awful
		Tex->NeverStream = true;

		Font->Textures.Add(Tex);

		// Init.
		uint8* TextureData = (uint8*)Tex->Source.LockMip(0);
		int32 TexSizeX = Tex->Source.GetSizeX();
		int32 TexSizeY = Tex->Source.GetSizeY();
		Font->Characters.AddZeroed( NUM_FONT_CHARS );

		// Scan in all fonts, starting at glyph 32.
		uint32 i = 32;
		int32 Y = 0;
		do
		{
			int32 X = 0;
			while( AT(TextureData,TexSizeX,X,Y)!=255 && Y<TexSizeY )
			{
				X++;
				if( X >= TexSizeX )
				{
					X = 0;
					if( ++Y >= TexSizeY )
						break;
				}
			}

			// Scan all glyphs in this row.
			if( Y < TexSizeY )
			{
				int32 XL=0, YL=0, MaxYL=0;
				while( i<(uint32)Font->Characters.Num() && ScanFontBox(TextureData,X,Y,XL,YL,TexSizeX) )
				{
					Font->Characters[i].StartU = X+1;
					Font->Characters[i].StartV = Y+1;
					Font->Characters[i].USize  = XL;
					Font->Characters[i].VSize  = YL;
					Font->Characters[i].TextureIndex = 0;
					Font->Characters[i].VerticalOffset = 0;
					X += XL + 1;
					i++;
					if( YL > MaxYL )
						MaxYL = YL;
				}
				Y += MaxYL + 1;
			}
		} while( i<(uint32)Font->Characters.Num() && Y<TexSizeY );

		// Cleanup font data.
		int32 MipBytes = Tex->Source.CalcMipSize(0);
		for( int32 j=0; j<MipBytes; j++ )
		{
			if( TextureData[j]==255 )
			{
				TextureData[j] = 0;
			}
		}
		Tex->Source.UnlockMip(0);
		Tex->PostEditChange();

		// Remap old fonts.
		TArray<FFontCharacter> Old = Font->Characters;
		for( i=0; i<(uint32)Font->Characters.Num(); i++ )
		{
			Font->Characters[i] = Old[FontRemap[i]];
		}

		Font->CacheCharacterCountAndMaxCharHeight();

		FEditorDelegates::OnAssetPostImport.Broadcast(this, Font);

		return Font;
	}
	else 
	{
		Font->MarkPendingKill();
		FEditorDelegates::OnAssetPostImport.Broadcast( this, NULL );
		return NULL;
	}
}

/*------------------------------------------------------------------------------
FCustomizableTextObjectFactory implementation.
------------------------------------------------------------------------------*/

// Util to ensure that InName is a valid name for a new object within InParent. Will rename any existing object within InParent if it is called InName.
void FCustomizableTextObjectFactory::ClearObjectNameUsage(UObject* InParent, FName InName)
{
	// Make sure this name is unique within the scope of InParent.
	UObject* Found=NULL;
	if( (InName != NAME_None) && (InParent != NULL) )
	{
		Found = FindObject<UObject>( InParent, *InName.ToString() );
	}

	// If there is already another object in the same scope with this name, rename it.
	if( Found )
	{
		check(Found->GetOuter() == InParent);

		Found->Rename(NULL, NULL, REN_None);
	}
}

// Constructor for the factory; takes a context for emitting warnings such as GWarn
FCustomizableTextObjectFactory::FCustomizableTextObjectFactory(FFeedbackContext* InWarningContext)
	: WarningContext(InWarningContext)
{
}

// Parses a text buffer and factories objects from it, subject to the restrictions imposed by CanCreateClass()
void FCustomizableTextObjectFactory::ProcessBuffer(UObject* InParent, EObjectFlags Flags, const FString& TextBuffer)
{
	ProcessBuffer(InParent, Flags, *TextBuffer);
}

void FCustomizableTextObjectFactory::ProcessBuffer(UObject* InParent, EObjectFlags Flags, const TCHAR* Buffer)
{
	// We keep a mapping of new, empty sequence objects to their property text.
	// We want to create all new SequenceObjects first before importing their properties (which will create links)
	TArray<UObject*>			NewObjects;
	TMap<UObject*,FString>		PropMap;

	FParse::Next( &Buffer );

	FString StrLine;
	while( FParse::Line(&Buffer,StrLine) )
	{
		const TCHAR* Str = *StrLine;
		if( GetBEGIN(&Str,TEXT("OBJECT")) )
		{
			UClass* ObjClass;
			if( ParseObject<UClass>( Str, TEXT("CLASS="), ObjClass, ANY_PACKAGE ) )
			{
				if (!CanCreateClass(ObjClass))
				{
					continue;
				}

				FName ObjName(NAME_None);
				FParse::Value( Str, TEXT("NAME="), ObjName );

				// Setup archetype
				FString ObjArchetypeName;
				FParse::Value( Str, TEXT("ARCHETYPE="), ObjArchetypeName );
				UObject* ObjArchetype;
				ObjArchetype = LoadObject<UObject>(NULL, *ObjArchetypeName, NULL, LOAD_None, NULL);

				// Make sure this name is not used by anything else. Will rename other stuff if necessary
				ClearObjectNameUsage(InParent, ObjName);

				// Spawn the object and reset it's archetype
				UObject* CreatedObject = ConstructObject<UObject>( ObjClass, InParent, ObjName, Flags, ObjArchetype, !!InParent, &InstanceGraph );

				// Get property text for the new object.
				FString PropText, PropLine;
				FString SubObjText;
				int32 ObjDepth = 1;
				while ( FParse::Line( &Buffer, PropLine ) )
				{
					const TCHAR* PropStr = *PropLine;

					// Track how deep we are in contained sets of sub-objects.
					bool bEndLine = false;
					if( GetBEGIN(&PropStr, TEXT("OBJECT")) )
					{
						ObjDepth++;
					}
					else if( GetEND(&PropStr, TEXT("OBJECT")) )
					{
						bEndLine = true;

						// When close out our initial BEGIN OBJECT, we are done with this object.
						if(ObjDepth == 1)
						{
							break;
						}
					}

					PropText += *PropLine;
					PropText += TEXT("\r\n");

					if(bEndLine)
					{
						ObjDepth--;
					}
				}

				// Save property text and possibly sub-object text.
				PropMap.Add(CreatedObject, *PropText);
				NewObjects.Add(CreatedObject);
			}
		}
	}

	// Apply the property text to each of the created objects
	for (int32 i = 0; i < NewObjects.Num(); ++i)
	{
		UObject* CreatedObject = NewObjects[i];
		const FString& PropText = PropMap.FindChecked(CreatedObject);

		// Import the properties and give the derived factory a shot at it
		ImportObjectProperties((uint8*)CreatedObject, *PropText, CreatedObject->GetClass(), CreatedObject, CreatedObject, WarningContext, 0, 0, &InstanceGraph);
		ProcessConstructedObject(CreatedObject);
	}
}


bool FCustomizableTextObjectFactory::CanCreateObjectsFromText( const FString& TextBuffer ) const
{
	bool bCanCreate = false;

	const TCHAR* Buffer = *TextBuffer;
	const TCHAR* BufferEnd = Buffer + FCString::Strlen(Buffer);

	FParse::Next( &Buffer );

	FString StrLine;
	while( FParse::Line(&Buffer,StrLine) )
	{
		const TCHAR* Str = *StrLine;
		if( GetBEGIN(&Str,TEXT("OBJECT")) )
		{
			UClass* ObjClass;
			if( ParseObject<UClass>( Str, TEXT("CLASS="), ObjClass, ANY_PACKAGE ) )
			{
				if(CanCreateClass(ObjClass))
				{
					bCanCreate = true;
					break;
				}
			}
		}
	}
	return bCanCreate;
}

/** Return true if the an object of type ObjectClass is allowed to be created; If false is returned, the object and subobjects will be ignored. */
bool FCustomizableTextObjectFactory::CanCreateClass(UClass* ObjectClass) const
{
	return false;
}
		
/** This is called on each created object after PreEditChange and the property text is imported, but before PostEditChange */
void FCustomizableTextObjectFactory::ProcessConstructedObject(UObject* CreatedObject)
{
}

/*-----------------------------------------------------------------------------
UReimportTextureFactory.
-----------------------------------------------------------------------------*/
UReimportTextureFactory::UReimportTextureFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = UTexture::StaticClass();

	bCreateNew = false;
}

UTexture2D* UReimportTextureFactory::CreateTexture2D( UObject* InParent, FName Name, EObjectFlags Flags )
{
	UTexture2D* pTex2D = Cast<UTexture2D>(pOriginalTex);

	if (pTex2D)
	{
		// Release the existing resource so the new texture can get a fresh one. Otherwise if the next call to Init changes the format
		// of the texture and UpdateResource is called the editor will crash in RenderThread
		pTex2D->ReleaseResource();
		return pTex2D;
	}
	else
	{
		return Super::CreateTexture2D( InParent, Name, Flags );
	}
}

UTextureCube* UReimportTextureFactory::CreateTextureCube( UObject* InParent, FName Name, EObjectFlags Flags )
{
	UTextureCube* pTexCube = Cast<UTextureCube>(pOriginalTex);
	if (pTexCube)
	{
		// Release the existing resource so the new texture can get a fresh one. Otherwise if the next call to Init changes the format
		// of the texture and UpdateResource is called the editor will crash in RenderThread
		pTexCube->ReleaseResource();
		return pTexCube;
	}
	else
	{
		return Super::CreateTextureCube( InParent, Name, Flags );
	}
}

bool UReimportTextureFactory::CanReimport( UObject* Obj, TArray<FString>& OutFilenames )
{	
	UTexture* pTex = Cast<UTexture>(Obj);
	if(pTex)
	{
		OutFilenames.Add(FReimportManager::ResolveImportFilename(pTex->SourceFilePath, pTex));
		return true;
	}
	return false;
}

void UReimportTextureFactory::SetReimportPaths( UObject* Obj, const TArray<FString>& NewReimportPaths )
{	
	UTexture* pTex = Cast<UTexture>(Obj);
	if(pTex && ensure(NewReimportPaths.Num() == 1))
	{
		pTex->SourceFilePath = FReimportManager::SanitizeImportFilename(NewReimportPaths[0], Obj);
	}
}

/**
* Reimports specified texture from its source material, if the meta-data exists
*/
EReimportResult::Type UReimportTextureFactory::Reimport( UObject* Obj )
{
	if(!Obj || !Obj->IsA(UTexture::StaticClass()))
	{
		return EReimportResult::Failed;
	}

	UTexture* pTex = Cast<UTexture>(Obj);
	
	TGuardValue<UTexture*> OriginalTexGuardValue(pOriginalTex, pTex);

	const FString ResolvedSourceFilePath = FReimportManager::ResolveImportFilename(pTex->SourceFilePath, pTex);
	if (!ResolvedSourceFilePath.Len())
	{
		// Since this is a new system most textures don't have paths, so logging has been commented out
		//UE_LOG(LogEditorFactories, Warning, TEXT("-- cannot reimport: texture resource does not have path stored."));
		return EReimportResult::Failed;
	}

	UTexture2D* pTex2D = Cast<UTexture2D>(Obj);
	// Check if this texture has been modified by the paint tool.
	// If so, prompt the user to see if they'll continue with reimporting, returning if they decline.
	if( pTex2D && pTex2D->bHasBeenPaintedInEditor && EAppReturnType::Yes != FMessageDialog::Open( EAppMsgType::YesNo,
		FText::Format(NSLOCTEXT("UnrealEd", "Import_TextureHasBeenPaintedInEditor", "The texture '{0}' has been painted on by the Mesh Paint tool.\nReimporting it will override any changes.\nWould you like to continue?"),
			FText::FromString(pTex2D->GetName()))) )
	{
		return EReimportResult::Failed;
	}

	UE_LOG(LogEditorFactories, Log, TEXT("Performing atomic reimport of [%s]"),*ResolvedSourceFilePath);

	// Ensure that the file provided by the path exists
	if (IFileManager::Get().FileSize(*ResolvedSourceFilePath) == INDEX_NONE)
	{
		UE_LOG(LogEditorFactories, Warning, TEXT("-- cannot reimport: source file cannot be found."));
		return EReimportResult::Failed;
	}

	// We use this reimport factory to skip the object creation process
	// which obliterates all of the properties of the texture.
	// Also preset the factory with the settings of the current texture.
	// These will be used during the import and compression process.        
	CompressionSettings   = pTex->CompressionSettings;
	NoCompression         = pTex->CompressionNone;
	NoAlpha               = pTex->CompressionNoAlpha;
	bDeferCompression     = pTex->DeferCompression;
	MipGenSettings		  = pTex->MipGenSettings;

	// Suppress the import overwrite dialog because we know that for explicitly re-importing we want to preserve existing settings
	UTextureFactory::SuppressImportOverwriteDialog();

	if (UFactory::StaticImportObject(pTex->GetClass(), pTex->GetOuter(), *pTex->GetName(), RF_Public|RF_Standalone, *ResolvedSourceFilePath, NULL, this))
	{
		UE_LOG(LogEditorFactories, Log, TEXT("-- imported successfully") );
		// Try to find the outer package so we can dirty it up
		if (pTex->GetOuter())
		{
			pTex->GetOuter()->MarkPackageDirty();
		}
		else
		{
			pTex->MarkPackageDirty();
		}
	}
	else
	{
		UE_LOG(LogEditorFactories, Warning, TEXT("-- import failed") );
	}
	
	return EReimportResult::Succeeded;
}


/*-----------------------------------------------------------------------------
UReimportFbxStaticMeshFactory.
-----------------------------------------------------------------------------*/
UReimportFbxStaticMeshFactory::UReimportFbxStaticMeshFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = UStaticMesh::StaticClass();
	Formats.Add(TEXT("fbx;FBX static meshes"));

	bCreateNew = false;
	bText = false;
}

bool UReimportFbxStaticMeshFactory::CanReimport( UObject* Obj, TArray<FString>& OutFilenames )
{	
	UStaticMesh* Mesh = Cast<UStaticMesh>(Obj);
	if(Mesh)
	{
		if ( Mesh->AssetImportData )
		{
			OutFilenames.Add(FReimportManager::ResolveImportFilename(Mesh->AssetImportData->SourceFilePath, Mesh));
		}
		else
		{
			OutFilenames.Add(TEXT(""));
		}
		return true;
	}
	return false;
}

void UReimportFbxStaticMeshFactory::SetReimportPaths( UObject* Obj, const TArray<FString>& NewReimportPaths )
{	
	UStaticMesh* Mesh = Cast<UStaticMesh>(Obj);
	if(Mesh && ensure(NewReimportPaths.Num() == 1))
	{
		UFbxStaticMeshImportData* ImportData = UFbxStaticMeshImportData::GetImportDataForStaticMesh(Mesh, ImportUI->StaticMeshImportData);

		ImportData->SourceFilePath = FReimportManager::SanitizeImportFilename(NewReimportPaths[0], Mesh);
	}
}

EReimportResult::Type UReimportFbxStaticMeshFactory::Reimport( UObject* Obj )
{
	UStaticMesh* Mesh = Cast<UStaticMesh>(Obj);
	if( !Mesh )
	{
		return EReimportResult::Failed;
	}
	
	UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
	UnFbx::FBXImportOptions* ImportOptions = FFbxImporter->GetImportOptions();

	UFbxStaticMeshImportData* ImportData = Cast<UFbxStaticMeshImportData>(Mesh->AssetImportData);
	
	UFbxImportUI* ReimportUI = ConstructObject<UFbxImportUI>(UFbxImportUI::StaticClass());
	ReimportUI->MeshTypeToImport = FBXIT_StaticMesh;
	ReimportUI->bOverrideFullName = false;
	ReimportUI->bCombineMeshes = true;

	if( ImportData )
	{
		// Import data already exists, apply it to the fbx import options
		ReimportUI->StaticMeshImportData = ImportData;
		ApplyImportUIToImportOptions(ReimportUI, *ImportOptions);
	}
	else
	{
		// An existing import data object was not found, make one here and show the options dialog
		ImportData = UFbxStaticMeshImportData::GetImportDataForStaticMesh(Mesh, ImportUI->StaticMeshImportData);
		Mesh->AssetImportData = ImportData;
		ReimportUI->StaticMeshImportData = ImportData;
		
		bool bOperationCanceled = false;
		bool bShowOption = true;
		bool bForceImportType = true;

		GetImportOptions( FFbxImporter, ReimportUI, bShowOption, Obj->GetPathName(), bOperationCanceled, bForceImportType, FBXIT_StaticMesh );
	}

	if( !bOperationCanceled && ensure(ImportData) )
	{
		const FString Filename = FReimportManager::ResolveImportFilename(ImportData->SourceFilePath, Mesh);
		const FString FileExtension = FPaths::GetExtension(Filename);
		const bool bIsValidFile = FileExtension.Equals( TEXT("fbx"), ESearchCase::IgnoreCase ) || FileExtension.Equals( "obj",  ESearchCase::IgnoreCase );

		if ( !bIsValidFile )
		{
			return EReimportResult::Failed;
		}

		if(!(Filename.Len()))
		{
			// Since this is a new system most static meshes don't have paths, so logging has been commented out
			//UE_LOG(LogEditorFactories, Warning, TEXT("-- cannot reimport: static mesh resource does not have path stored."));
			return EReimportResult::Failed;
		}

		UE_LOG(LogEditorFactories, Log, TEXT("Performing atomic reimport of [%s]"), *Filename);

		// Ensure that the file provided by the path exists
		if (IFileManager::Get().FileSize(*Filename) == INDEX_NONE)
		{
			UE_LOG(LogEditorFactories, Warning, TEXT("-- cannot reimport: source file cannot be found."));
			return EReimportResult::Failed;
		}

		CurrentFilename = Filename;

		if ( FFbxImporter->ImportFromFile( *Filename, FPaths::GetExtension( Filename ) ) )
		{
			if (FFbxImporter->ReimportStaticMesh(Mesh, ImportData))
			{
				UE_LOG(LogEditorFactories, Log, TEXT("-- imported successfully") );

				// Try to find the outer package so we can dirty it up
				if (Mesh->GetOuter())
				{
					Mesh->GetOuter()->MarkPackageDirty();
				}
				else
				{
					Mesh->MarkPackageDirty();
				}
			}
			else
			{
				UE_LOG(LogEditorFactories, Warning, TEXT("-- import failed") );
			}
		}
		else
		{
			UE_LOG(LogEditorFactories, Warning, TEXT("-- import failed") );
		}

		FFbxImporter->ReleaseScene(); 

		return EReimportResult::Succeeded;
	}
	else
	{
		return EReimportResult::Cancelled;
	}
}



/*-----------------------------------------------------------------------------
UReimportFbxSkeletalMeshFactory
-----------------------------------------------------------------------------*/ 
UReimportFbxSkeletalMeshFactory::UReimportFbxSkeletalMeshFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = USkeletalMesh::StaticClass();
	Formats.Add(TEXT("fbx;FBX skeletal meshes"));

	bCreateNew = false;
	bText = false;
}

bool UReimportFbxSkeletalMeshFactory::CanReimport( UObject* Obj, TArray<FString>& OutFilenames )
{	
	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Obj);
	if(SkeletalMesh && !Obj->IsA( UDestructibleMesh::StaticClass() ))
	{
		if ( SkeletalMesh->AssetImportData )
		{
			OutFilenames.Add(FReimportManager::ResolveImportFilename(SkeletalMesh->AssetImportData->SourceFilePath, SkeletalMesh));
		}
		else
		{
			OutFilenames.Add(TEXT(""));
		}
		return true;
	}
	return false;
}

void UReimportFbxSkeletalMeshFactory::SetReimportPaths( UObject* Obj, const TArray<FString>& NewReimportPaths )
{	
	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Obj);
	if(SkeletalMesh && ensure(NewReimportPaths.Num() == 1))
	{
		UFbxSkeletalMeshImportData* ImportData = UFbxSkeletalMeshImportData::GetImportDataForSkeletalMesh(SkeletalMesh, ImportUI->SkeletalMeshImportData);

		ImportData->SourceFilePath = FReimportManager::SanitizeImportFilename(NewReimportPaths[0], SkeletalMesh);
	}
}

EReimportResult::Type UReimportFbxSkeletalMeshFactory::Reimport( UObject* Obj )
{
	// Only handle valid skeletal meshes that aren't destructible meshes
	if( !Obj || !Obj->IsA( USkeletalMesh::StaticClass() ) || Obj->IsA( UDestructibleMesh::StaticClass() ) )
	{
		return EReimportResult::Failed;
	}

	USkeletalMesh* SkeletalMesh = CastChecked<USkeletalMesh>( Obj );

	if( !ImportUI )
	{
		ImportUI = ConstructObject<UFbxImportUI>( UFbxImportUI::StaticClass(), this, NAME_None, RF_Public );
	}

	UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
	UnFbx::FBXImportOptions* ImportOptions = FFbxImporter->GetImportOptions();

	UFbxSkeletalMeshImportData* ImportData = Cast<UFbxSkeletalMeshImportData>(SkeletalMesh->AssetImportData);
	
	// Prepare the import options
	UFbxImportUI* ReimportUI = ConstructObject<UFbxImportUI>(UFbxImportUI::StaticClass());
	ReimportUI->MeshTypeToImport = FBXIT_SkeletalMesh;
	ReimportUI->bOverrideFullName = false;
	ReimportUI->Skeleton = SkeletalMesh->Skeleton;
	ReimportUI->bCreatePhysicsAsset = false;
	ReimportUI->PhysicsAsset = SkeletalMesh->PhysicsAsset;
	ReimportUI->bImportAnimations = false;
	ReimportUI->AnimationName = TEXT("");
	ReimportUI->bImportRigidMesh = false;
	ReimportUI->bUseDefaultSampleRate = false;

	bool bSuccess = false;

	if( ImportData )
	{
		// Import data already exists, apply it to the fbx import options
		ReimportUI->SkeletalMeshImportData = ImportData;
		ApplyImportUIToImportOptions(ReimportUI, *ImportOptions);
	}
	else
	{
		// An existing import data object was not found, make one here and show the options dialog
		ImportData = UFbxSkeletalMeshImportData::GetImportDataForSkeletalMesh(SkeletalMesh, ImportUI->SkeletalMeshImportData);
		SkeletalMesh->AssetImportData = ImportData;
		ReimportUI->SkeletalMeshImportData = ImportData;

		bool bOperationCanceled = false;
		bool bShowOption = true;
		bool bForceImportType = true;

		// arggg... hate this different option class to confuse everybody
		// @hack to make sure skeleton is set before opening the dialog
		ImportOptions->SkeletonForAnimation = SkeletalMesh->Skeleton;
		ImportOptions->bCreatePhysicsAsset = false;
		ImportOptions->PhysicsAsset = SkeletalMesh->PhysicsAsset;

		ImportOptions = GetImportOptions( FFbxImporter, ReimportUI, bShowOption, Obj->GetPathName(), bOperationCanceled, bForceImportType, FBXIT_SkeletalMesh );
	}

	if( !bOperationCanceled && ensure(ImportData) )
	{
		const FString Filename = FReimportManager::ResolveImportFilename(ImportData->SourceFilePath, SkeletalMesh);
		UE_LOG(LogEditorFactories, Log, TEXT("Performing atomic reimport of [%s]"), *Filename);

		// Ensure that the file provided by the path exists
		if (IFileManager::Get().FileSize(*Filename) == INDEX_NONE)
		{
			UE_LOG(LogEditorFactories, Warning, TEXT("-- cannot reimport: source file cannot be found.") );
			return EReimportResult::Failed;
		}
		CurrentFilename = Filename;

		if ( FFbxImporter->ImportFromFile( *Filename, FPaths::GetExtension( Filename ) ) )
		{
			if ( FFbxImporter->ReimportSkeletalMesh(SkeletalMesh, ImportData) )
			{
				UE_LOG(LogEditorFactories, Log, TEXT("-- imported successfully") );

				// Try to find the outer package so we can dirty it up
				if (SkeletalMesh->GetOuter())
				{
					SkeletalMesh->GetOuter()->MarkPackageDirty();
				}
				else
				{
					SkeletalMesh->MarkPackageDirty();
				}

				bSuccess = true;
			}
			else
			{
				UE_LOG(LogEditorFactories, Warning, TEXT("-- import failed") );
			}
		}
		else
		{
			UE_LOG(LogEditorFactories, Warning, TEXT("-- import failed") );
		}
		FFbxImporter->ReleaseScene(); 

		CleanUp();

		return bSuccess ? EReimportResult::Succeeded : EReimportResult::Failed;
	}
	else
	{
		return EReimportResult::Cancelled;
	}
}

/*-----------------------------------------------------------------------------
UReimportFbxAnimSequenceFactory
-----------------------------------------------------------------------------*/ 
USkeleton* ChooseSkeleton() 
{
	TSharedRef<SWindow> WidgetWindow = SNew(SWindow)
		.Title(LOCTEXT("ChooseSkeletonWindowTitle", "Choose Skeleton"))
		.ClientSize(FVector2D(500, 600));

	TSharedRef<SSkeletonSelectorWindow> SkeletonSelectorWindow = SNew(SSkeletonSelectorWindow) .WidgetWindow(WidgetWindow);

	WidgetWindow->SetContent(SkeletonSelectorWindow);

	GEditor->EditorAddModalWindow(WidgetWindow);
	return SkeletonSelectorWindow->GetSelectedSkeleton();
}

UReimportFbxAnimSequenceFactory::UReimportFbxAnimSequenceFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = UAnimSequence::StaticClass();
	Formats.Empty(1);
	Formats.Add(TEXT("fbx;FBX animation"));

	bCreateNew = false;
	bText = false;
}

bool UReimportFbxAnimSequenceFactory::CanReimport( UObject* Obj, TArray<FString>& OutFilenames )
{	
	UAnimSequence* AnimSequence = Cast<UAnimSequence>(Obj);
	if(AnimSequence)
	{
		if ( AnimSequence->AssetImportData )
		{
			OutFilenames.Add(FReimportManager::ResolveImportFilename(AnimSequence->AssetImportData->SourceFilePath, AnimSequence));
		}
		else
		{
			OutFilenames.Add(TEXT(""));
		}
		return true;
	}
	return false;
}

void UReimportFbxAnimSequenceFactory::SetReimportPaths( UObject* Obj, const TArray<FString>& NewReimportPaths )
{	
	UAnimSequence* AnimSequence = Cast<UAnimSequence>(Obj);
	if(AnimSequence && ensure(NewReimportPaths.Num() == 1))
	{
		UFbxAnimSequenceImportData* ImportData = UFbxAnimSequenceImportData::GetImportDataForAnimSequence(AnimSequence, ImportUI->AnimSequenceImportData);

		ImportData->SourceFilePath = FReimportManager::SanitizeImportFilename(NewReimportPaths[0], AnimSequence);
	}
}

EReimportResult::Type UReimportFbxAnimSequenceFactory::Reimport( UObject* Obj )
{
	// Only handle valid skeletal meshes that aren't destructible meshes
	if( !Obj || !Obj->IsA( UAnimSequence::StaticClass() ) )
	{
		return EReimportResult::Failed;
	}

	UAnimSequence* AnimSequence = Cast<UAnimSequence>( Obj );
	UFbxAnimSequenceImportData* ImportData = UFbxAnimSequenceImportData::GetImportDataForAnimSequence(AnimSequence, ImportUI->AnimSequenceImportData);
	if ( !ensure(ImportData) )
	{
		return EReimportResult::Failed;
	}

	const FString Filename = FReimportManager::ResolveImportFilename(ImportData->SourceFilePath, AnimSequence);
	const FString FileExtension = FPaths::GetExtension(Filename);
	const bool bIsNotFBXFile = ( FileExtension.Len() > 0  && FCString::Stricmp( *FileExtension, TEXT("FBX") ) != 0 );

	// Only handle FBX files
	if ( bIsNotFBXFile )
	{
		return EReimportResult::Failed;
	}
	// If there is no file path provided, can't reimport from source
// 	if ( !Filename.Len() )
// 	{
// 		// Since this is a new system most skeletal meshes don't have paths, so logging has been commented out
// 		//UE_LOG(LogEditorFactories, Warning, TEXT("-- cannot reimport: skeletal mesh resource does not have path stored."));
// 		return false;
// 	}

	UE_LOG(LogEditorFactories, Log, TEXT("Performing atomic reimport of [%s]"), *Filename);

	// Ensure that the file provided by the path exists
	if (IFileManager::Get().FileSize(*Filename) == INDEX_NONE)
	{
		UE_LOG(LogEditorFactories, Warning, TEXT("-- cannot reimport: source file cannot be found.") );
		return EReimportResult::Failed;
	}

	UnFbx::FFbxImporter* Importer = UnFbx::FFbxImporter::GetInstance();

	CurrentFilename = Filename;

	USkeleton * Skeleton = AnimSequence->GetSkeleton();
	if (!Skeleton)
	{
		// if it does not exist, ask for one
		Skeleton = ChooseSkeleton();
		if (!Skeleton)
		{
			// If skeleton wasn't found or the user canceled out of the dialog, we cannot proceed, but this reimport factory 
			// has still technically "handled" the reimport, so return true instead of false
			UE_LOG(LogEditorFactories, Warning, TEXT("-- import failed") );
			return EReimportResult::Succeeded;
		}
	}

	if ( UEditorEngine::ReimportFbxAnimation(Skeleton, AnimSequence, ImportData, *Filename) )
	{
		UE_LOG(LogEditorFactories, Log, TEXT("-- imported successfully") );

		// Try to find the outer package so we can dirty it up
		if (AnimSequence->GetOuter())
		{
			AnimSequence->GetOuter()->MarkPackageDirty();
		}
		else
		{
			AnimSequence->MarkPackageDirty();
		}
	}
	else
	{
		UE_LOG(LogEditorFactories, Warning, TEXT("-- import failed") );
	}

	Importer->ReleaseScene(); 

	return EReimportResult::Succeeded;
}

/*------------------------------------------------------------------------------
	FBlueprintParentFilter implementation.
------------------------------------------------------------------------------*/

class FBlueprintParentFilter : public IClassViewerFilter
{
public:
	/** Classes to not allow any children of into the Class Viewer/Picker. */
	TSet< const UClass* > DisallowedChildrenOfClasses;

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs ) OVERRIDE
	{
		return InFilterFuncs->IfInChildOfClassesSet(DisallowedChildrenOfClasses, InClass) != EFilterReturn::Passed;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) OVERRIDE
	{
		return InFilterFuncs->IfInChildOfClassesSet(DisallowedChildrenOfClasses, InUnloadedClassData) != EFilterReturn::Passed;
	}
};

/*------------------------------------------------------------------------------
	UBlueprintGeneratedClassFactory implementation.
------------------------------------------------------------------------------*/
UBlueprintGeneratedClassFactory::UBlueprintGeneratedClassFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	// Look in the config file to determine what the default base class is, if any
	FString ClassPath;
	GConfig->GetString(TEXT("/Script/Engine.Engine"), TEXT("DefaultBlueprintBaseClassName"), /*out*/ ClassPath, GEngineIni);
	UClass* DefaultParentClass = !ClassPath.IsEmpty() 
		? LoadClass<UObject>(NULL, *ClassPath, NULL, LOAD_None, NULL) 
		: NULL;
	
	if( !DefaultParentClass || !FKismetEditorUtilities::CanCreateBlueprintOfClass(DefaultParentClass) )
	{
		DefaultParentClass = AActor::StaticClass();
	}

	bCreateNew = false;
	bEditAfterNew = true;
	SupportedClass = UBlueprintGeneratedClass::StaticClass();
	ParentClass = DefaultParentClass;
}

bool UBlueprintGeneratedClassFactory::ConfigureProperties()
{
	// Null the parent class to ensure one is selected
	ParentClass = NULL;

	// Fill in options
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.DisplayMode = EClassViewerDisplayMode::TreeView;
	Options.bShowObjectRootClass = true;

	// Only want blueprint actor base classes.
	Options.bIsBlueprintBaseOnly = true;

	// This will allow unloaded blueprints to be shown.
	Options.bShowUnloadedBlueprints = true;

	// Prevent creating blueprints of classes that require special setup (they'll be allowed in the corresponding factories / via other means)
	TSharedPtr<FBlueprintParentFilter> Filter = MakeShareable(new FBlueprintParentFilter);
	Options.ClassFilter = Filter;
	if (!IsMacroFactory())
	{
		Filter->DisallowedChildrenOfClasses.Add(ALevelScriptActor::StaticClass());
		Filter->DisallowedChildrenOfClasses.Add(UAnimInstance::StaticClass());
	}

	// Filter out interfaces in all cases; they can never contain code, so it doesn't make sense to use them as a macro basis
	Filter->DisallowedChildrenOfClasses.Add(UInterface::StaticClass());

	const FText TitleText = LOCTEXT("CreateBlueprintOptions", "Pick Parent Class");
	UClass* ChosenClass = NULL;
	const bool bPressedOk = SClassPickerDialog::PickClass(TitleText, Options, ChosenClass, UBlueprint::StaticClass());

	if ( bPressedOk )
	{
		ParentClass = ChosenClass;

		FEditorDelegates::OnFinishPickingBlueprintClass.Broadcast(ParentClass);
	}

	return bPressedOk;
};

UObject* UBlueprintGeneratedClassFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	// Make sure we are trying to factory a blueprint, then create and init one
	check(Class->IsChildOf(UBlueprintGeneratedClass::StaticClass()));

	if ((ParentClass == NULL) || !FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass))
	{
		FMessageDialog::Open( EAppMsgType::Ok, FText::FromString(FString::Printf(TEXT("Cannot create a blueprint based on the class '%s'."),
			(ParentClass != NULL) ? *(ParentClass->GetName()) : TEXT("(null)"))));
		return NULL;
	}
	else
	{
		return FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
	}
}

/*------------------------------------------------------------------------------
	UBlueprintFactory implementation.
------------------------------------------------------------------------------*/
UBlueprintFactory::UBlueprintFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	// Look in the config file to determine what the default base class is, if any
	FString ClassPath;
	GConfig->GetString(TEXT("/Script/Engine.Engine"), TEXT("DefaultBlueprintBaseClassName"), /*out*/ ClassPath, GEngineIni);
	UClass* DefaultParentClass = !ClassPath.IsEmpty() 
		? LoadClass<UObject>(NULL, *ClassPath, NULL, LOAD_None, NULL) 
		: NULL;
	
	if( !DefaultParentClass || !FKismetEditorUtilities::CanCreateBlueprintOfClass(DefaultParentClass) )
	{
		DefaultParentClass = AActor::StaticClass();
	}

	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UBlueprint::StaticClass();
	ParentClass = DefaultParentClass;
}

bool UBlueprintFactory::ConfigureProperties()
{
	// Null the parent class to ensure one is selected
	ParentClass = NULL;

	// Fill in options
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.DisplayMode = EClassViewerDisplayMode::TreeView;
	Options.bShowObjectRootClass = true;

	// Only want blueprint actor base classes.
	Options.bIsBlueprintBaseOnly = true;

	// This will allow unloaded blueprints to be shown.
	Options.bShowUnloadedBlueprints = true;

	// Enable Class Dynamic Loading
	Options.bEnableClassDynamicLoading = false;

	// Prevent creating blueprints of classes that require special setup (they'll be allowed in the corresponding factories / via other means)
	TSharedPtr<FBlueprintParentFilter> Filter = MakeShareable(new FBlueprintParentFilter);
	Options.ClassFilter = Filter;
	if (!IsMacroFactory())
	{
		Filter->DisallowedChildrenOfClasses.Add(ALevelScriptActor::StaticClass());
		Filter->DisallowedChildrenOfClasses.Add(UAnimInstance::StaticClass());
	}

	// Filter out interfaces in all cases; they can never contain code, so it doesn't make sense to use them as a macro basis
	Filter->DisallowedChildrenOfClasses.Add(UInterface::StaticClass());

	const FText TitleText = LOCTEXT("CreateBlueprintOptions", "Pick Parent Class");
	UClass* ChosenClass = NULL;
	const bool bPressedOk = SClassPickerDialog::PickClass(TitleText, Options, ChosenClass, UBlueprint::StaticClass());

	if ( bPressedOk )
	{
		ParentClass = ChosenClass;

		FEditorDelegates::OnFinishPickingBlueprintClass.Broadcast(ParentClass);
	}

	return bPressedOk;
};

UObject* UBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	// Make sure we are trying to factory a blueprint, then create and init one
	check(Class->IsChildOf(UBlueprint::StaticClass()));

	if ((ParentClass == NULL) || !FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass))
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("ClassName"), (ParentClass != NULL) ? FText::FromString( ParentClass->GetName() ) : LOCTEXT("Null", "(null)") );
		FMessageDialog::Open( EAppMsgType::Ok, FText::Format( LOCTEXT("CannotCreateBlueprintFromClass", "Cannot create a blueprint based on the class '{0}'."), Args ) );
		return NULL;
	}
	else
	{
		return FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), CallingContext);
	}
}

UObject* UBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, NAME_None);
}


/*------------------------------------------------------------------------------
	UBlueprintMacroFactory implementation.
------------------------------------------------------------------------------*/
UBlueprintMacroFactory::UBlueprintMacroFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UBlueprint::StaticClass();
	ParentClass = AActor::StaticClass();
}

FText UBlueprintMacroFactory::GetDisplayName() const
{
	return LOCTEXT("BlueprintMacroLibraryFactoryDescription", "Blueprint Macro Library");
}

FName UBlueprintMacroFactory::GetNewAssetThumbnailOverride() const
{
	return TEXT("ClassThumbnail.BlueprintMacroLibrary");
}

uint32 UBlueprintMacroFactory::GetMenuCategories() const
{
	// Force this factory into the misc category, since it does not belong in the top menu
	return EAssetTypeCategories::Misc;
}

UObject* UBlueprintMacroFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	// Make sure we are trying to factory a blueprint, then create and init one
	check(Class->IsChildOf(UBlueprint::StaticClass()));

	if ((ParentClass == NULL) || !FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass))
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("ClassName"), (ParentClass != NULL) ? FText::FromString( ParentClass->GetName() ) : LOCTEXT("Null", "(null)") );
		FMessageDialog::Open( EAppMsgType::Ok, FText::Format( LOCTEXT("CannotCreateBlueprintFromClass", "Cannot create a blueprint based on the class '{0}'."), Args ) );
		return NULL;
	}
	else
	{
		return FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BPTYPE_MacroLibrary, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), CallingContext);
	}
}

/*------------------------------------------------------------------------------
BlueprintFunctionLibraryFactory implementation.
------------------------------------------------------------------------------*/
UBlueprintFunctionLibraryFactory::UBlueprintFunctionLibraryFactory(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	struct FCanCreateNewHelper
	{
		bool bCanCreateNew;
		FCanCreateNewHelper() : bCanCreateNew(false)
		{
			GConfig->GetBool(TEXT("CustomBlueprintFunctionLibrary"), TEXT("bCanCreateNew"), bCanCreateNew, GEditorIni);
		}
	};
	static const FCanCreateNewHelper Helper;
	bCreateNew = Helper.bCanCreateNew;
	bEditAfterNew = true;
	SupportedClass = UBlueprint::StaticClass();
	ParentClass = UBlueprintFunctionLibrary::StaticClass();
}

FText UBlueprintFunctionLibraryFactory::GetDisplayName() const
{
	return LOCTEXT("BlueprintFunctionLibraryFactoryDescription", "Blueprint Function Library");
}

FName UBlueprintFunctionLibraryFactory::GetNewAssetThumbnailOverride() const
{
	return TEXT("ClassThumbnail.BlueprintFunctionLibrary");
}

uint32 UBlueprintFunctionLibraryFactory::GetMenuCategories() const
{
	// Force this factory into the misc category, since it does not belong in the top menu
	return EAssetTypeCategories::Misc;
}

UObject* UBlueprintFunctionLibraryFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	// Make sure we are trying to factory a blueprint, then create and init one
	check(Class->IsChildOf(UBlueprint::StaticClass()));

	if (ParentClass != UBlueprintFunctionLibrary::StaticClass())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ClassName"), (ParentClass != NULL) ? FText::FromString(ParentClass->GetName()) : LOCTEXT("Null", "(null)"));
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("CannotCreateBlueprintFromClass", "Cannot create a blueprint based on the class '{0}'."), Args));
		return NULL;
	}
	else
	{
		return FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BPTYPE_FunctionLibrary, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), CallingContext);
	}
}

bool UBlueprintFunctionLibraryFactory::ConfigureProperties()
{
	return true;
}

/*------------------------------------------------------------------------------
	UBlueprintInterfaceFactory implementation.
------------------------------------------------------------------------------*/
UBlueprintInterfaceFactory::UBlueprintInterfaceFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UBlueprint::StaticClass();
}

FText UBlueprintInterfaceFactory::GetDisplayName() const
{
	return LOCTEXT("BlueprintInterfaceFactoryDescription", "Blueprint Interface");
}

FName UBlueprintInterfaceFactory::GetNewAssetThumbnailOverride() const
{
	return TEXT("ClassThumbnail.BlueprintInterface");
}

uint32 UBlueprintInterfaceFactory::GetMenuCategories() const
{
	// Force this factory into the misc category, since it does not belong in the top menu
	return EAssetTypeCategories::Misc;
}

UObject* UBlueprintInterfaceFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	// Make sure we are trying to factory a blueprint, then create and init one
	check(Class->IsChildOf(UBlueprint::StaticClass()));

	// Force the parent class to be UInterface as per original code
	UClass* ParentClass = UInterface::StaticClass();

	if ((ParentClass == NULL) || !FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass))
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("ClassName"), (ParentClass != NULL) ? FText::FromString( ParentClass->GetName() ) : LOCTEXT("Null", "(null)") );
		FMessageDialog::Open( EAppMsgType::Ok, FText::Format( LOCTEXT("CannotCreateBlueprintFromClass", "Cannot create a blueprint based on the class '{0}'."), Args ) );
		return NULL;
	}
	else
	{
		return FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BPTYPE_Interface, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), CallingContext);
	}
}

UObject* UBlueprintInterfaceFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, NAME_None);
}

/*------------------------------------------------------------------------------
	UCurveFactory implementation.
------------------------------------------------------------------------------*/

UCurveFactory::UCurveFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UCurveBase::StaticClass();

	CurveClass = UCurveFloat::StaticClass();
}

class FCurveDataAssetParentFilter : public IClassViewerFilter
{
public:
	/** All children of these classes will be included unless filtered out by another setting. */
	TSet< const UClass* > AllowedChildrenOfClasses;

	/** Disallowed class flags. */
	uint32 DisallowedClassFlags;

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs ) OVERRIDE
	{
		return !InClass->HasAnyClassFlags(DisallowedClassFlags)
				&& InFilterFuncs->IfInChildOfClassesSet( AllowedChildrenOfClasses, InClass) != EFilterReturn::Failed;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) OVERRIDE
	{
		return !InUnloadedClassData->HasAnyClassFlags(DisallowedClassFlags)
			&& InFilterFuncs->IfInChildOfClassesSet( AllowedChildrenOfClasses, InUnloadedClassData) != EFilterReturn::Failed;
	}
};

bool UCurveFactory::ConfigureProperties()
{
	// Null the CurveClass so we can get a clean class
	CurveClass = NULL;

	// Load the classviewer module to display a class picker
	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

	// Fill in options
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;

	TSharedPtr<FCurveDataAssetParentFilter> Filter = MakeShareable(new FCurveDataAssetParentFilter);
	Options.ClassFilter = Filter;

	Filter->DisallowedClassFlags = CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists;
	Filter->AllowedChildrenOfClasses.Add(UCurveBase::StaticClass());

	const FText TitleText = LOCTEXT("CreateCurveOptions", "Pick Curve Class");
	UClass* ChosenClass = NULL;
	const bool bPressedOk = SClassPickerDialog::PickClass(TitleText, Options, ChosenClass, UCurveBase::StaticClass());

	if ( bPressedOk )
	{
		CurveClass = ChosenClass;
	}

	return bPressedOk;
}

UObject* UCurveFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UCurveBase* NewCurve = NULL;
	if(CurveClass != NULL)
	{
		NewCurve = CastChecked<UCurveBase>(StaticConstructObject(CurveClass,InParent,Name,Flags));		
	}

	return NewCurve;
}

/*------------------------------------------------------------------------------
	UCurveImportFactory implementation.
------------------------------------------------------------------------------*/

UCurveImportFactory::UCurveImportFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	bCreateNew = false;
	SupportedClass = UCurveBase::StaticClass();

	bEditorImport = true;
	bText = true;

	Formats.Add(TEXT("as;Audio amplitude curve"));
}

// @note jf: for importing a curve from a text format.  this is experimental code for a prototype feature and not fully fleshed out
UObject* UCurveImportFactory::FactoryCreateText( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn )
{
	FEditorDelegates::OnAssetPreImport.Broadcast(this, InClass, InParent, InName, Type);

	if(	FCString::Stricmp( Type, TEXT( "AS" ) ) == 0 )
	{
		FString Str;
		TCHAR const* BufRead = Buffer;

		// first line is faFile="", we can ignore
		if ( !FParse::Line(&BufRead, Str) ) return NULL;
		FParse::Next(&BufRead);

		// 2nd line is fps=X
		float KeyFrameHz = 0.f;
		if ( !FParse::Value(BufRead, TEXT("fps="), KeyFrameHz) ) return NULL;
		if ( !FParse::Line(&BufRead, Str) ) return NULL;
		FParse::Next(&BufRead);

		// next line is scale=X, we can ignore?
		if ( !FParse::Line(&BufRead, Str) ) return NULL;
		FParse::Next(&BufRead);
		// next line is smoothing=X, we can ignore?
		if ( !FParse::Line(&BufRead, Str) ) return NULL;
		FParse::Next(&BufRead);
		// next line is dBValues=X, we can ignore?
		if ( !FParse::Line(&BufRead, Str) ) return NULL;
		FParse::Next(&BufRead);
		// next line is stereo=X, we can ignore?
		if ( !FParse::Line(&BufRead, Str) ) return NULL;
		FParse::Next(&BufRead);

		// next line is amplitude=[, then list of CSV floats
		if ( !FParse::Value(BufRead, TEXT("amplitude=["), Str) ) return NULL;
		BufRead += FCString::Strlen(TEXT("amplitude=["));

		TArray<float> FloatKeys;

		while (1)
		{
			if (!FParse::AlnumToken(BufRead, Str)) break;

			float Key = (float) FCString::Atoi(*Str);
			FloatKeys.Add(Key);

			if (*BufRead == ',')
			{
				BufRead++;
				FParse::Next(&BufRead);
			}
			else
			{
				break;
			}
		}

		// make the curve object and set up the keys
		if (FloatKeys.Num() > 0)
		{
			UCurveFloat* NewCurve = CastChecked<UCurveFloat>(StaticConstructObject(UCurveFloat::StaticClass(),InParent,InName,Flags));

			if (NewCurve)
			{
				for (int32 KeyIdx=0; KeyIdx<FloatKeys.Num(); ++KeyIdx)
				{
					float const KeyTime = KeyIdx / KeyFrameHz;
					float const KeyValue = FloatKeys[KeyIdx];
					FKeyHandle const KeyHandle = NewCurve->FloatCurve.AddKey(KeyTime, KeyValue);
					NewCurve->FloatCurve.SetKeyInterpMode(KeyHandle, RCIM_Cubic);
				}
			}

			FEditorDelegates::OnAssetPostImport.Broadcast(this, NewCurve);

			return NewCurve;
		}
	}

	FEditorDelegates::OnAssetPostImport.Broadcast( this, NULL );
	return NULL;
}


/*------------------------------------------------------------------------------
	UArchetypeLibraryFactory implementation.
------------------------------------------------------------------------------*/
UObjectLibraryFactory::UObjectLibraryFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UObjectLibrary::StaticClass();
}

UObject* UObjectLibraryFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return CastChecked<UObjectLibrary>(StaticConstructObject(Class,InParent,Name,Flags));
}

/*------------------------------------------------------------------------------
	UDataAssetFactory implementation.
------------------------------------------------------------------------------*/

UDataAssetFactory::UDataAssetFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UDataAsset::StaticClass();
}

bool UDataAssetFactory::ConfigureProperties()
{
	// NULL the DataAssetClass so we can check for selection
	DataAssetClass = NULL;

	// Load the classviewer module to display a class picker
	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

	// Fill in options
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;

	TSharedPtr<FCurveDataAssetParentFilter> Filter = MakeShareable(new FCurveDataAssetParentFilter);
	Options.ClassFilter = Filter;

	Filter->DisallowedClassFlags = CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists;
	Filter->AllowedChildrenOfClasses.Add(UDataAsset::StaticClass());

	const FText TitleText = LOCTEXT("CreateDataAssetOptions", "Pick Data Asset Class");
	UClass* ChosenClass = NULL;
	const bool bPressedOk = SClassPickerDialog::PickClass(TitleText, Options, ChosenClass, UDataAsset::StaticClass());

	if ( bPressedOk )
	{
		DataAssetClass = ChosenClass;
	}

	return bPressedOk;
};

UObject* UDataAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	if(DataAssetClass != NULL)
	{
		return CastChecked<UDataAsset>(StaticConstructObject(DataAssetClass,InParent,Name,Flags));
	}
	else
	{
		return NULL;
	}
}

#include "EditorPhysXSupport.h"
#if WITH_APEX_CLOTHING
#include "ApexClothingUtils.h"
#endif // #if WITH_APEX_CLOTHING
/*------------------------------------------------------------------------------
	UDestructibleMeshFactory implementation.
------------------------------------------------------------------------------*/
UDestructibleMeshFactory::UDestructibleMeshFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	bEditorImport = true;
	SupportedClass = UDestructibleMesh::StaticClass();
	bCreateNew = false;
	Formats.Add(TEXT("apx;APEX XML Asset"));
	Formats.Add(TEXT("apb;APEX Binary Asset"));
}

FText UDestructibleMeshFactory::GetDisplayName() const
{
	return LOCTEXT("APEXDestructibleFactoryDescription", "APEX Destructible Asset");
}

#if WITH_APEX

UObject* UDestructibleMeshFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	EObjectFlags		Flags,
	UObject*			Context,
	const TCHAR*		FileType,
	const uint8*&		Buffer,
	const uint8*			BufferEnd,
	FFeedbackContext*	Warn
)
{
	FEditorDelegates::OnAssetPreImport.Broadcast(this, Class, InParent, Name, FileType);

	// The return value
	UDestructibleMesh* DestructibleMesh = NULL;

	// Create an Apex NxDestructibleAsset from the binary blob
	NxDestructibleAsset* ApexDestructibleAsset = CreateApexDestructibleAssetFromBuffer(Buffer, (int32)(BufferEnd-Buffer));
	if( ApexDestructibleAsset != NULL )
	{
		// Succesfully created the NxDestructibleAsset, now create a UDestructibleMesh
		DestructibleMesh = ImportDestructibleMeshFromApexDestructibleAsset(InParent, *ApexDestructibleAsset, Name, Flags, NULL);
		if( DestructibleMesh != NULL )
		{
			FEditorDelegates::OnAssetPostImport.Broadcast(this, DestructibleMesh);

			// Success
			DestructibleMesh->PostEditChange();
		}
	}
#if WITH_APEX_CLOTHING
	else
	{
		// verify whether this is an Apex Clothing asset or not 
		NxClothingAsset* ApexClothingAsset = ApexClothingUtils::CreateApexClothingAssetFromBuffer(Buffer, (int32)(BufferEnd-Buffer));
		
		if(ApexClothingAsset)
		{
			FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("ApexClothingWrongImport", "The file you tried to import is an APEX clothing asset file. You need to use Persona to import this asset and associate it with a skeletal mesh.\n\n 1. Import a skeletal mesh from an FBX file, or choose an existing skeletal asset and open it up in Persona.\n 2. Choose \"Add APEX clothing file\" and choose this APEX clothing asset file." ));

			// This asset is used only for showing a message how to import an Apex Clothing asset properly
			GPhysCommandHandler->DeferredRelease(ApexClothingAsset);
		}
	}
#endif // #if WITH_APEX_CLOTHING

	return DestructibleMesh;
}

#endif // WITH_APEX

/*-----------------------------------------------------------------------------
	UReimportDestructibleMeshFactory implementation.
-----------------------------------------------------------------------------*/
UReimportDestructibleMeshFactory::UReimportDestructibleMeshFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = UDestructibleMesh::StaticClass();
	bCreateNew = false;
	bText = false;
	Formats.Add(TEXT("apx;APEX XML Asset"));
	Formats.Add(TEXT("apb;APEX Binary Asset"));

}

FText UReimportDestructibleMeshFactory::GetDisplayName() const
{
	return LOCTEXT("APEXReimportDestructibleAssetFactoryDescription", "APEX Reimport Destructible Asset");
}

#if WITH_APEX

bool UReimportDestructibleMeshFactory::CanReimport( UObject* Obj, TArray<FString>& OutFilenames )
{	
	UDestructibleMesh* DestructibleMesh = Cast<UDestructibleMesh>(Obj);
	if(DestructibleMesh)
	{
		if ( DestructibleMesh->AssetImportData )
		{
			OutFilenames.Add(FReimportManager::ResolveImportFilename(DestructibleMesh->AssetImportData->SourceFilePath, DestructibleMesh));
		}
		else
		{
			OutFilenames.Add(TEXT(""));
		}
		return true;
	}
	return false;
}

void UReimportDestructibleMeshFactory::SetReimportPaths( UObject* Obj, const TArray<FString>& NewReimportPaths )
{	
	UDestructibleMesh* DestructibleMesh = Cast<UDestructibleMesh>(Obj);
	if(DestructibleMesh && ensure(NewReimportPaths.Num() == 1))
	{
		if ( DestructibleMesh->AssetImportData == NULL )
		{
			// @todo AssetImportData make an apex destructible import data class
			DestructibleMesh->AssetImportData = ConstructObject<UAssetImportData>(UAssetImportData::StaticClass(), DestructibleMesh);
		}

		DestructibleMesh->AssetImportData->SourceFilePath = FReimportManager::SanitizeImportFilename(NewReimportPaths[0], DestructibleMesh);
	}
}

EReimportResult::Type UReimportDestructibleMeshFactory::Reimport( UObject* Obj )
{
	// Only handle valid skeletal meshes
	if( !Obj || !Obj->IsA( UDestructibleMesh::StaticClass() ) )
	{
		return EReimportResult::Failed;
	}

	UDestructibleMesh* DestructibleMesh = Cast<UDestructibleMesh>( Obj );

	if ( DestructibleMesh->AssetImportData == NULL )
	{
		// @todo AssetImportData make an apex destructible import data class
		DestructibleMesh->AssetImportData = ConstructObject<UAssetImportData>(UAssetImportData::StaticClass(), DestructibleMesh);
	}

	const FString Filename = FReimportManager::ResolveImportFilename(DestructibleMesh->AssetImportData->SourceFilePath, DestructibleMesh);

	// If there is no file path provided, can't reimport from source
	if ( !Filename.Len() )
	{
		// Since this is a new system most skeletal meshes don't have paths, so logging has been commented out
		//UE_LOG(LogEditorFactories, Warning, TEXT("-- cannot reimport: skeletal mesh resource does not have path stored."));
		return EReimportResult::Failed;
	}

	UE_LOG(LogEditorFactories, Log, TEXT("Performing atomic reimport of [%s]"), *Filename);

	// Ensure that the file provided by the path exists
	if (IFileManager::Get().FileSize(*Filename) == INDEX_NONE)
	{
		UE_LOG(LogEditorFactories, Warning, TEXT("-- cannot reimport: source file cannot be found.") );
		return EReimportResult::Failed;
	}

	CurrentFilename = Filename;

	// Create an Apex NxDestructibleAsset from the binary blob
	NxDestructibleAsset* ApexDestructibleAsset = CreateApexDestructibleAssetFromFile(Filename);
	if( ApexDestructibleAsset != NULL )
	{
		// Succesfully created the NxDestructibleAsset, now create a UDestructibleMesh
		UDestructibleMesh* ReimportedDestructibleMesh = ImportDestructibleMeshFromApexDestructibleAsset(DestructibleMesh->GetOuter(), *ApexDestructibleAsset, DestructibleMesh->GetFName(), DestructibleMesh->GetFlags(), NULL,
																										EImportOptions::PreserveSettings);
		if( ReimportedDestructibleMesh != NULL )
		{
			check( ReimportedDestructibleMesh == DestructibleMesh );

			UE_LOG(LogEditorFactories, Log, TEXT("-- imported successfully") );

			// Try to find the outer package so we can dirty it up
			if (DestructibleMesh->GetOuter())
			{
				DestructibleMesh->GetOuter()->MarkPackageDirty();
			}
			else
			{
				DestructibleMesh->MarkPackageDirty();
			}
		}
		else
		{
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "ImportFailed_Destructible", "Reimport Failed" ));
			UE_LOG(LogEditorFactories, Warning, TEXT("-- import failed") );
		}
	}
	else
	{
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "ImportFailed_Destructible", "Reimport Failed" ));
		UE_LOG(LogEditorFactories, Warning, TEXT("-- import failed") );
	}

	return EReimportResult::Succeeded;
}

#endif // #if WITH_APEX

/*------------------------------------------------------------------------------
	UBlendSpaceFactoryNew.
------------------------------------------------------------------------------*/
UBlendSpaceFactoryNew::UBlendSpaceFactoryNew(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = UBlendSpace::StaticClass();
	bCreateNew = true;
		}

bool UBlendSpaceFactoryNew::ConfigureProperties()
	{
	// Null the parent class so we can check for selection later
	TargetSkeleton = NULL;

		// Load the content browser module to display an asset picker
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

		FAssetPickerConfig AssetPickerConfig;

	/** The asset picker will only show skeletal meshes */
		AssetPickerConfig.Filter.ClassNames.Add(USkeleton::StaticClass()->GetFName());
		AssetPickerConfig.Filter.bRecursiveClasses = true;

		/** The delegate that fires when an asset was selected */
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateUObject(this, &UBlendSpaceFactoryNew::OnTargetSkeletonSelected);

		/** The default view mode should be a list view */
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;

		/** The default scale for thumbnails. [0-1] range */
		AssetPickerConfig.ThumbnailScale = 0.25f;

	PickerWindow = SNew(SWindow)
	.Title(LOCTEXT("CreateBlendSpaceOptions", "Pick Skeleton"))
	.ClientSize(FVector2D(500, 600))
	.SupportsMinimize(false) .SupportsMaximize(false)
	[
		SNew(SBorder)
		.BorderImage( FEditorStyle::GetBrush("Menu.Background") )
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		]
		];


	GEditor->EditorAddModalWindow(PickerWindow.ToSharedRef());
	PickerWindow.Reset();

	return TargetSkeleton != NULL;
}

UObject* UBlendSpaceFactoryNew::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn)
{
	if (TargetSkeleton)
	{
		UBlendSpace * BlendSpace = Cast<UBlendSpace>(StaticConstructObject(Class,InParent,Name,Flags));

		BlendSpace->SetSkeleton(TargetSkeleton);

		return BlendSpace;
	}

	return NULL;
}

void UBlendSpaceFactoryNew::OnTargetSkeletonSelected(const FAssetData& SelectedAsset)
{
	TargetSkeleton = Cast<USkeleton>(SelectedAsset.GetAsset());
	PickerWindow->RequestDestroyWindow();
}

/*------------------------------------------------------------------------------
	UBlendSpaceFactory1D.
------------------------------------------------------------------------------*/
UBlendSpaceFactory1D::UBlendSpaceFactory1D(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = UBlendSpace1D::StaticClass();
	bCreateNew = true;
}

bool UBlendSpaceFactory1D::ConfigureProperties()
{
	// Null the parent class so we can check for selection later
	TargetSkeleton = NULL;

		// Load the content browser module to display an asset picker
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	FAssetPickerConfig AssetPickerConfig;

	/** The asset picker will only show skeletal meshes */
	AssetPickerConfig.Filter.ClassNames.Add(USkeleton::StaticClass()->GetFName());
	AssetPickerConfig.Filter.bRecursiveClasses = true;

	/** The delegate that fires when an asset was selected */
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateUObject(this, &UBlendSpaceFactory1D::OnTargetSkeletonSelected);

	/** The default view mode should be a list view */
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;

	/** The default scale for thumbnails. [0-1] range */
	AssetPickerConfig.ThumbnailScale = 0.25f;

	PickerWindow = SNew(SWindow)
	.Title(LOCTEXT("CreateBlendSpaceOptions", "Pick Skeleton"))
	.ClientSize(FVector2D(500, 600))
	.SupportsMinimize(false) .SupportsMaximize(false)
	[
		SNew(SBorder)
		.BorderImage( FEditorStyle::GetBrush("Menu.Background") )
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		]
	];


	GEditor->EditorAddModalWindow(PickerWindow.ToSharedRef());
	PickerWindow.Reset();

	return TargetSkeleton != NULL;
}

UObject* UBlendSpaceFactory1D::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn)
{
	if (TargetSkeleton)
	{
		UBlendSpace1D * BlendSpace = Cast<UBlendSpace1D>(StaticConstructObject(Class,InParent,Name,Flags));

		BlendSpace->SetSkeleton(TargetSkeleton);

		return BlendSpace;
	}

	return NULL;
}

void UBlendSpaceFactory1D::OnTargetSkeletonSelected(const FAssetData& SelectedAsset)
{
	TargetSkeleton = Cast<USkeleton>(SelectedAsset.GetAsset());
	PickerWindow->RequestDestroyWindow();
}

/*------------------------------------------------------------------------------
	UAimOffsetBlendSpaceFactoryNew.
------------------------------------------------------------------------------*/
UAimOffsetBlendSpaceFactoryNew::UAimOffsetBlendSpaceFactoryNew(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SupportedClass = UAimOffsetBlendSpace::StaticClass();
	bCreateNew = true;
}

UObject* UAimOffsetBlendSpaceFactoryNew::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn)
{
	if (TargetSkeleton)
	{
		UAimOffsetBlendSpace * BlendSpace = Cast<UAimOffsetBlendSpace>(StaticConstructObject(Class,InParent,Name,Flags));

		BlendSpace->SetSkeleton(TargetSkeleton);

		return BlendSpace;
	}

	return NULL;
}

/*------------------------------------------------------------------------------
	UAimOffsetBlendSpaceFactory1D.
------------------------------------------------------------------------------*/
UAimOffsetBlendSpaceFactory1D::UAimOffsetBlendSpaceFactory1D(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SupportedClass = UAimOffsetBlendSpace1D::StaticClass();
	bCreateNew = true;
}

UObject* UAimOffsetBlendSpaceFactory1D::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn)
{
	if (TargetSkeleton)
	{
		UAimOffsetBlendSpace1D * BlendSpace = Cast<UAimOffsetBlendSpace1D>(StaticConstructObject(Class,InParent,Name,Flags));

		BlendSpace->SetSkeleton(TargetSkeleton);

		return BlendSpace;
	}

	return NULL;
}

/*------------------------------------------------------------------------------
	UEnumFactory implementation.
------------------------------------------------------------------------------*/
UEnumFactory::UEnumFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SupportedClass = UUserDefinedEnum::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UEnumFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	ensure(UUserDefinedEnum::StaticClass() == Class);

	if(!FEnumEditorUtils::IsNameAvailebleForUserDefinedEnum(Name))
	{
		const FText Message = FText::Format( LOCTEXT("EnumWithNameAlreadyExists", "Enum '{0}' already exists. The name must be unique."), FText::FromName( Name ) );
		if(Warn)
		{
			Warn->Log( Message );
		}
		FMessageDialog::Open( EAppMsgType::Ok, Message);
		return NULL;
	}

	return FEnumEditorUtils::CreateUserDefinedEnum(InParent, Name, Flags);
}

/*------------------------------------------------------------------------------
	UStructureFactory implementation.
------------------------------------------------------------------------------*/
UStructureFactory::UStructureFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SupportedClass = UUserDefinedStruct::StaticClass();
	bCreateNew = FStructureEditorUtils::UserDefinedStructEnabled();
	bEditAfterNew = true;
}

UObject* UStructureFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	ensure(UUserDefinedStruct::StaticClass() == Class);
	return FStructureEditorUtils::CreateUserDefinedStruct(InParent, Name, Flags);
}

/*-----------------------------------------------------------------------------
	UForceFeedbackEffectFactory implementation.
-----------------------------------------------------------------------------*/
UForceFeedbackEffectFactory::UForceFeedbackEffectFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = UForceFeedbackEffect::StaticClass();
	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* UForceFeedbackEffectFactory::FactoryCreateNew( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn )
{
	return ConstructObject<UForceFeedbackEffect>( UForceFeedbackEffect::StaticClass(), InParent, InName, Flags );
}

/*-----------------------------------------------------------------------------
	UTouchInterfaceFactory implementation.
-----------------------------------------------------------------------------*/
UTouchInterfaceFactory::UTouchInterfaceFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = UTouchInterface::StaticClass();

	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* UTouchInterfaceFactory::FactoryCreateNew( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn )
{
	return ConstructObject<UTouchInterface>( UTouchInterface::StaticClass(), InParent, InName, Flags );
}

/*------------------------------------------------------------------------------
	UCameraAnimFactory implementation.
------------------------------------------------------------------------------*/

UCameraAnimFactory::UCameraAnimFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SupportedClass = UCameraAnim::StaticClass();
	bCreateNew = true;
}

FText UCameraAnimFactory::GetDisplayName() const
{
	return LOCTEXT("CameraAnimFactoryDescription", "Camera Anim");
}

FName UCameraAnimFactory::GetNewAssetThumbnailOverride() const
{
	return TEXT("ClassThumbnail.CameraAnim");
}

uint32 UCameraAnimFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Misc;
}

UObject* UCameraAnimFactory::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn)
{
	UCameraAnim* NewCamAnim = CastChecked<UCameraAnim>(StaticConstructObject(Class,InParent,Name,Flags));
	NewCamAnim->CameraInterpGroup = ConstructObject<UInterpGroupCamera>(UInterpGroupCamera::StaticClass(), NewCamAnim);
	NewCamAnim->CameraInterpGroup->GroupName = Name;
	return NewCamAnim;
}

#undef LOCTEXT_NAMESPACE

