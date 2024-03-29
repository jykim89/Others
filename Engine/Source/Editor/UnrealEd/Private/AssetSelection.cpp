// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "AssetSelection.h"
#include "ScopedTransaction.h"

#include "LevelUtils.h"
#include "EditorLevelUtils.h"
#include "MainFrame.h"

#include "ComponentAssetBroker.h"

#include "DragAndDrop/AssetDragDropOp.h"

#include "AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "SnappingUtils.h"
#include "ActorEditorUtils.h"
#include "LevelEditorViewport.h"

extern void OnPlaceStaticMeshActor( AActor* MeshActor, bool bUseSurfaceOrientation );

namespace AssetSelectionUtils
{
	bool IsClassPlaceable(const UClass* Class)
	{
		const bool bIsAddable =
			Class
			&& !(Class->HasAnyClassFlags(CLASS_NotPlaceable | CLASS_Deprecated | CLASS_Abstract))
			&& Class->IsChildOf( AActor::StaticClass() );
		return bIsAddable;
	}

	void GetSelectedAssets( TArray<FAssetData>& OutSelectedAssets )
	{
		// Add the selection from the content browser module
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().GetSelectedAssets(OutSelectedAssets);
	}

	FSelectedActorInfo BuildSelectedActorInfo( const TArray<AActor*>& SelectedActors)
	{
		FSelectedActorInfo ActorInfo;
		if( SelectedActors.Num() > 0 )
		{
			// Get the class type of the first actor.
			AActor* FirstActor = SelectedActors[0];

			if( FirstActor && !FirstActor->HasAnyFlags( RF_ClassDefaultObject ) )
			{
				UClass* FirstClass = FirstActor->GetClass();
				UObject* FirstArchetype = FirstActor->GetArchetype();

				ActorInfo.bAllSelectedAreBrushes = Cast< ABrush >( FirstActor ) != NULL;
				ActorInfo.SelectionClass = FirstClass;

				// Compare all actor types with the baseline.
				for ( int32 ActorIndex = 0; ActorIndex < SelectedActors.Num(); ++ActorIndex )
				{
					AActor* CurrentActor = SelectedActors[ ActorIndex ];

					if( CurrentActor->HasAnyFlags( RF_ClassDefaultObject ) )
					{
						continue;
					}

					ABrush* Brush = Cast< ABrush >( CurrentActor );
					if( !Brush)
					{
						ActorInfo.bAllSelectedAreBrushes = false;
					}
					else
					{
						if( !ActorInfo.bHaveBuilderBrush )
						{
							ActorInfo.bHaveBuilderBrush = FActorEditorUtils::IsABuilderBrush(Brush);
						}
						ActorInfo.bHaveBrush |= true;
					}

					UClass* CurrentClass = CurrentActor->GetClass();
					if( FirstClass != CurrentClass )
					{
						ActorInfo.bAllSelectedActorsOfSameType = false;
						ActorInfo.SelectionClass = NULL;
						FirstClass = NULL;
					}
					else
					{
						ActorInfo.SelectionClass = CurrentActor->GetClass();
					}

					++ActorInfo.NumSelected;

					if( ActorInfo.bAllSelectedActorsBelongToCurrentLevel )
					{
						if( !CurrentActor->GetOuter()->IsA(ULevel::StaticClass()) || !CurrentActor->GetLevel()->IsCurrentLevel() )
						{
							ActorInfo.bAllSelectedActorsBelongToCurrentLevel = false;
						}
					}

					if( ActorInfo.bAllSelectedActorsBelongToSameWorld )
					{
						if ( !ActorInfo.SharedWorld )
						{
							ActorInfo.SharedWorld = CurrentActor->GetWorld();
							check(ActorInfo.SharedWorld);
						}
						else
						{
							if( ActorInfo.SharedWorld != CurrentActor->GetWorld() )
							{
								ActorInfo.bAllSelectedActorsBelongToCurrentLevel = false;
								ActorInfo.SharedWorld = NULL;
							}
						}
					}

					// To prevent move to other level for Landscape if its components are distributed in streaming levels
					if (CurrentActor->IsA(ALandscape::StaticClass()))
					{
						ALandscape* Landscape = CastChecked<ALandscape>(CurrentActor);
						if (!Landscape || !Landscape->HasAllComponent())
						{
							if( !ActorInfo.bAllSelectedActorsBelongToCurrentLevel )
							{
								ActorInfo.bAllSelectedActorsBelongToCurrentLevel = true;
							}
						}
					}

					if ( ActorInfo.bSelectedActorsBelongToSameLevel )
					{
						ULevel* ActorLevel = CurrentActor->GetOuter()->IsA(ULevel::StaticClass()) ? CurrentActor->GetLevel() : NULL;
						if ( !ActorInfo.SharedLevel )
						{
							// This is the first selected actor we've encountered.
							ActorInfo.SharedLevel = ActorLevel;
						}
						else
						{
							// Does this actor's level match the others?
							if ( ActorInfo.SharedLevel != ActorLevel )
							{
								ActorInfo.bSelectedActorsBelongToSameLevel = false;
								ActorInfo.SharedLevel = NULL;
							}
						}
					}


					AGroupActor* FoundGroup = Cast<AGroupActor>(CurrentActor);
					if(!FoundGroup)
					{
						FoundGroup = AGroupActor::GetParentForActor(CurrentActor);
					}
					if( FoundGroup )
					{
						if( !ActorInfo.bHaveSelectedSubGroup )
						{
							ActorInfo.bHaveSelectedSubGroup  = AGroupActor::GetParentForActor(FoundGroup) != NULL;
						}
						if( !ActorInfo.bHaveSelectedLockedGroup )
						{
							ActorInfo.bHaveSelectedLockedGroup = FoundGroup->IsLocked();
						}
						if( !ActorInfo.bHaveSelectedUnlockedGroup )
						{
							AGroupActor* FoundRoot = AGroupActor::GetRootForActor(CurrentActor);
							ActorInfo.bHaveSelectedUnlockedGroup = !FoundGroup->IsLocked() || ( FoundRoot && !FoundRoot->IsLocked() );
						}
					}
					else
					{
						++ActorInfo.NumSelectedUngroupedActors;
					}

					USceneComponent* RootComp = CurrentActor->GetRootComponent();
					if(RootComp != NULL && RootComp->AttachParent != NULL)
					{
						ActorInfo.bHaveAttachedActor = true;
					}

					TArray<UStaticMeshComponent*> StaticMeshComponents;
					CurrentActor->GetComponents(StaticMeshComponents);

					for( int32 CompIndex = 0; CompIndex < StaticMeshComponents.Num(); ++CompIndex )
					{
						UStaticMeshComponent* SMComp = StaticMeshComponents[ CompIndex ];
						if( SMComp->IsRegistered() )
						{
							ActorInfo.bHaveStaticMeshComponent = true;
						}

					}

					if( CurrentActor->IsA( ALight::StaticClass() ) )
					{
						ActorInfo.bHaveLight = true;
					}

					if( CurrentActor->IsA( AStaticMeshActor::StaticClass() ) ) 
					{
						ActorInfo.bHaveStaticMesh = true;
						AStaticMeshActor* StaticMeshActor = CastChecked<AStaticMeshActor>( CurrentActor );
						if ( StaticMeshActor->StaticMeshComponent )
						{
							UStaticMesh* StaticMesh = StaticMeshActor->StaticMeshComponent->StaticMesh;

							ActorInfo.bAllSelectedStaticMeshesHaveCollisionModels &= ( (StaticMesh && StaticMesh->BodySetup) ? true : false );
						}
					}

					if( CurrentActor->IsA( ASkeletalMeshActor::StaticClass() ) )
					{
						ActorInfo.bHaveSkeletalMesh = true;
					}

					if( CurrentActor->IsA( APawn::StaticClass() ) )
					{
						ActorInfo.bHavePawn = true;
					}

					if( CurrentActor->IsA( AEmitter::StaticClass() ) )
					{
						ActorInfo.bHaveEmitter = true;
					}

					if ( CurrentActor->IsA( AMatineeActor::StaticClass() ) )
					{
						ActorInfo.bHaveMatinee = true;
					}

					if ( CurrentActor->IsTemporarilyHiddenInEditor() )
					{
						ActorInfo.bHaveHidden = true;
					}

					if ( CurrentActor->IsA( ALandscapeProxy::StaticClass() ) )
					{
						ActorInfo.bHaveLandscape = true;
					}

					// Find our counterpart actor
					AActor* EditorWorldActor = EditorUtilities::GetEditorWorldCounterpartActor( CurrentActor );
					if( EditorWorldActor != NULL )
					{
						// Just count the total number of actors with counterparts
						++ActorInfo.NumSimulationChanges;
					}
				}

				if( ActorInfo.SelectionClass != NULL )
				{
					ActorInfo.SelectionStr = ActorInfo.SelectionClass->GetName();
				}
				else
				{
					ActorInfo.SelectionStr = TEXT("Actor");
				}


			}
		}

		return ActorInfo;
	}

	FSelectedActorInfo GetSelectedActorInfo()
	{
		TArray<AActor*> SelectedActors;
		GEditor->GetSelectedActors()->GetSelectedObjects<AActor>( SelectedActors );
		return BuildSelectedActorInfo( SelectedActors );	
	}

	int32 GetNumSelectedSurfaces( UWorld* InWorld )
	{
		int32 NumSurfs = 0;
		UWorld* World = InWorld;
		if( !World )
		{
			World = GWorld;	// Fallback to GWorld
		}
		if( World )
		{
			// Count the number of selected surfaces
			for( int32 Surface=0; Surface<World->GetModel()->Surfs.Num(); ++Surface )
			{
				FBspSurf *Poly = &World->GetModel()->Surfs[Surface];

				if( Poly->PolyFlags & PF_Selected )
				{
					++NumSurfs;
				}
			}
		}

		return NumSurfs;
	}

	bool IsBuilderBrushSelected()
	{
		bool bHasBuilderBrushSelected = false;

		for( FSelectionIterator SelectionIter = GEditor->GetSelectedActorIterator(); SelectionIter; ++SelectionIter )
		{
			AActor* Actor = Cast< AActor >(*SelectionIter);
			if( Actor && FActorEditorUtils::IsABuilderBrush( Actor ) )
			{
				bHasBuilderBrushSelected = true;
				break;
			}
		}

		return bHasBuilderBrushSelected;
	}
}

/**
* Creates an actor using the specified factory.  
*
* Does nothing if ActorClass is NULL.
*/
static AActor* PrivateAddActor( UObject* Asset, UActorFactory* Factory, const FVector* ActorLocation=NULL, bool bUseSurfaceOrientation=false, bool SelectActor = true, EObjectFlags ObjectFlags = RF_Transactional, const FName Name = NAME_None )
{
	AActor* Actor = NULL;
	if ( Factory )
	{
		AActor* NewActorTemplate = Factory->GetDefaultActor( Asset );
		if ( !NewActorTemplate )
		{
			return NULL;
		}

		// Position the actor relative to the mouse?
		FVector Location = FVector::ZeroVector;
		FVector SnapNormal = FVector::ZeroVector;
		if ( ActorLocation )
		{
			Location = *ActorLocation;
		}
		else
		{
			FSnappingUtils::SnapPointToGrid( GEditor->ClickLocation, FVector(0, 0, 0) );

			Location = GEditor->ClickLocation;
			if( !GCurrentLevelEditingViewportClient || !FSnappingUtils::SnapLocationToNearestVertex( Location, GCurrentLevelEditingViewportClient->GetDropPreviewLocation(), GCurrentLevelEditingViewportClient, SnapNormal ) )
			{
				FVector Collision = NewActorTemplate->GetPlacementExtent();

				Location += GEditor->ClickPlane * (FVector::BoxPushOut(GEditor->ClickPlane, Collision) + 0.1f);
			}

			// Do not fade snapping indicators over time if the viewport is not realtime
			bool bClearImmediately = !GCurrentLevelEditingViewportClient || !GCurrentLevelEditingViewportClient->IsRealtime();
			FSnappingUtils::ClearSnappingHelpers( bClearImmediately );

			FSnappingUtils::SnapPointToGrid( Location, FVector(0, 0, 0) );
		}		

		const FRotator* Rotation = NULL;
		if( bUseSurfaceOrientation )
		{
			FRotator SurfaceOrientation;
			if( SnapNormal != FVector::ZeroVector )
			{
				// Orient the new actor with the snapped surface normal?
				SurfaceOrientation = SnapNormal.Rotation();
			}
			else
			{
				SurfaceOrientation =  GEditor->ClickPlane.Rotation();
			}

			Rotation = &SurfaceOrientation;
		}

		ULevel* DesiredLevel = GWorld->GetCurrentLevel();

		// Don't spawn the actor if the current level is locked.
		if ( FLevelUtils::IsLevelLocked(DesiredLevel) )
		{
			FNotificationInfo Info( NSLOCTEXT("UnrealEd", "Error_OperationDisallowedOnLockedLevel", "The requested operation could not be completed because the level is locked.") );
			Info.ExpireDuration = 3.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
			return NULL;
		}

		{
			FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "CreateActor", "Create Actor") );
			if ( !(ObjectFlags & RF_Transactional) )
			{
				Transaction.Cancel();
			}

			// Create the actor.
			Actor = Factory->CreateActor( Asset, DesiredLevel, Location, Rotation, ObjectFlags, Name ); 
			if(Actor)
			{
				// Apply any static mesh tool settings if we placed a static mesh. 
				OnPlaceStaticMeshActor( Actor, bUseSurfaceOrientation );

				if ( SelectActor )
				{
					GEditor->SelectNone( false, true );
					GEditor->SelectActor( Actor, true, true );
				}

				Actor->InvalidateLightingCache();
				Actor->PostEditChange();
			}
		}

		GEditor->RedrawLevelEditingViewports();

		if ( Actor )
		{
			Actor->MarkPackageDirty();
			ULevel::LevelDirtiedEvent.Broadcast();
		}
	}

	return Actor;
}


namespace AssetUtil
{
	TArray<FAssetData> ExtractAssetDataFromDrag( const FDragDropEvent &DragDropEvent )
	{
		TArray<FAssetData> DroppedAssetData;

		TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
		if (!Operation.IsValid())
		{
			return DroppedAssetData;
		}

		if (Operation->IsOfType<FExternalDragOperation>())
		{
			TSharedPtr<FExternalDragOperation> DragDropOp = StaticCastSharedPtr<FExternalDragOperation>(Operation);
			if ( DragDropOp->HasText() )
			{
				TArray<FString> DroppedAssetStrings;
				const TCHAR AssetDelimiter[] = { AssetMarshalDefs::AssetDelimiter, TEXT('\0') };
				DragDropOp->GetText().ParseIntoArray(&DroppedAssetStrings, AssetDelimiter, true);

				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
				IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

				for (int Index = 0; Index < DroppedAssetStrings.Num(); Index++)
				{
					FAssetData AssetData = AssetRegistry.GetAssetByObjectPath( *DroppedAssetStrings[ Index ] );
					if ( AssetData.IsValid() )
					{
						DroppedAssetData.Add( AssetData );
					}
				}
			}
		}
		else if (Operation->IsOfType<FAssetDragDropOp>())
		{
			TSharedPtr<FAssetDragDropOp> DragDropOp = StaticCastSharedPtr<FAssetDragDropOp>( Operation );
			DroppedAssetData.Append( DragDropOp->AssetData );
		}

		return DroppedAssetData;
	}

	FReply CanHandleAssetDrag( const FDragDropEvent &DragDropEvent )
	{
		TArray<FAssetData> DroppedAssetData = ExtractAssetDataFromDrag(DragDropEvent);
		for ( auto AssetIt = DroppedAssetData.CreateConstIterator(); AssetIt; ++AssetIt )
		{
			const FAssetData& AssetData = *AssetIt;

			if ( AssetData.IsValid() && FComponentAssetBrokerage::GetPrimaryComponentForAsset( AssetData.GetClass() ) )
			{
				return FReply::Handled();
			}
		}

		return FReply::Unhandled();
	}
}



/* ==========================================================================================================
FActorFactoryAssetProxy
========================================================================================================== */

void FActorFactoryAssetProxy::GenerateActorFactoryMenuItems( const FAssetData& AssetData, TArray<FMenuItem>* OutMenuItems, bool ExcludeStandAloneFactories  )
{
	FText UnusedErrorMessage;
	const FAssetData NoAssetData;
	for ( int32 FactoryIdx = 0; FactoryIdx < GEditor->ActorFactories.Num(); FactoryIdx++ )
	{
		UActorFactory* Factory = GEditor->ActorFactories[FactoryIdx];

		const bool FactoryWorksWithoutAsset = Factory->CanCreateActorFrom( NoAssetData, UnusedErrorMessage );
		const bool FactoryWorksWithAsset = AssetData.IsValid() && Factory->CanCreateActorFrom( AssetData, UnusedErrorMessage );
		const bool FactoryWorks = FactoryWorksWithAsset || FactoryWorksWithoutAsset;

		if ( FactoryWorks )
		{
			FMenuItem MenuItem = FMenuItem( Factory, NoAssetData );
			if ( FactoryWorksWithAsset )
			{
				MenuItem = FMenuItem( Factory, AssetData );
			}

			if ( FactoryWorksWithAsset || ( !ExcludeStandAloneFactories && FactoryWorksWithoutAsset ) )
			{
				OutMenuItems->Add( MenuItem );
			}
		}
	}
}

/**
* Find the appropriate actor factory for an asset by type.
*
* @param	AssetData			contains information about an asset that to get a factory for
* @param	bRequireValidObject	indicates whether a valid asset object is required.  specify false to allow the asset
*								class's CDO to be used in place of the asset if no asset is part of the drag-n-drop
*
* @return	the factory that is responsible for creating actors for the specified asset type.
*/
UActorFactory* FActorFactoryAssetProxy::GetFactoryForAsset( const FAssetData& AssetData, bool bRequireValidObject/*=false*/ )
{
	UObject* Asset = NULL;
	
	if ( AssetData.IsAssetLoaded() )
	{
		Asset = AssetData.GetAsset();
	}
	else if ( !bRequireValidObject )
	{
		Asset = AssetData.GetClass()->GetDefaultObject();
	}

	return FActorFactoryAssetProxy::GetFactoryForAssetObject( Asset );
}

/**
* Find the appropriate actor factory for an asset.
*
* @param	AssetObj	The asset that to find the appropriate actor factory for
*
* @return	The factory that is responsible for creating actors for the specified asset
*/
UActorFactory* FActorFactoryAssetProxy::GetFactoryForAssetObject( UObject* AssetObj )
{
	UActorFactory* Result = NULL;

	// Attempt to find a factory that is capable of creating the asset
	const TArray< UActorFactory *>& ActorFactories = GEditor->ActorFactories;
	FText UnusedErrorMessage;
	FAssetData AssetData( AssetObj );
	for ( int32 FactoryIdx = 0; Result == NULL && FactoryIdx < ActorFactories.Num(); ++FactoryIdx )
	{
		UActorFactory* ActorFactory = ActorFactories[FactoryIdx];
		// Check if the actor can be created using this factory, making sure to check for an asset to be assigned from the selector
		if ( ActorFactory->CanCreateActorFrom( AssetData, UnusedErrorMessage ) )
		{
			Result = ActorFactory;
		}
	}

	return Result;
}

AActor* FActorFactoryAssetProxy::AddActorForAsset( UObject* AssetObj, const FVector* ActorLocation, bool bUseSurfaceOrientation, bool SelectActor, EObjectFlags ObjectFlags, UActorFactory* FactoryToUse /*= NULL*/, const FName Name )
{
	AActor* Result = NULL;

	const FAssetData AssetData( AssetObj );
	FText UnusedErrorMessage;
	if ( AssetObj != NULL )
	{
		// If a specific factory has been provided, verify its validity and then use it to create the actor
		if ( FactoryToUse )
		{
			if ( FactoryToUse->CanCreateActorFrom( AssetData, UnusedErrorMessage ) )
			{
				Result = PrivateAddActor( AssetObj, FactoryToUse, ActorLocation, bUseSurfaceOrientation, SelectActor, ObjectFlags, Name );
			}
		}
		// If no specific factory has been provided, find the highest priority one that is valid for the asset and use
		// it to create the actor
		else
		{
			const TArray<UActorFactory*>& ActorFactories = GEditor->ActorFactories;
			for ( int32 FactoryIdx = 0; FactoryIdx < ActorFactories.Num(); FactoryIdx++ )
			{
				UActorFactory* ActorFactory = ActorFactories[FactoryIdx];

				// Check if the actor can be created using this factory, making sure to check for an asset to be assigned from the selector
				if ( ActorFactory->CanCreateActorFrom( AssetData, UnusedErrorMessage ) )
				{
					Result = PrivateAddActor(AssetObj, ActorFactory, ActorLocation, bUseSurfaceOrientation, SelectActor, ObjectFlags, Name);
					if ( Result != NULL )
					{
						break;
					}
				}
			}
		}
	}


	return Result;
}

AActor* FActorFactoryAssetProxy::AddActorFromSelection( UClass* ActorClass, const FVector* ActorLocation, bool bUseSurfaceOrientation, bool SelectActor, EObjectFlags ObjectFlags, UActorFactory* ActorFactory, const FName Name )
{
	check( ActorClass != NULL );

	if( !ActorFactory )
	{
		// Look for an actor factory capable of creating actors of the actors type.
		ActorFactory = GEditor->FindActorFactoryForActorClass( ActorClass );
	}

	AActor* Result = NULL;
	FText ErrorMessage;

	if ( ActorFactory )
	{
		UObject* TargetObject = GEditor->GetSelectedObjects()->GetTop<UObject>();

		if( ActorFactory->CanCreateActorFrom( FAssetData(TargetObject), ErrorMessage ) )
		{
			// Attempt to add the actor
			Result = PrivateAddActor( TargetObject, ActorFactory, ActorLocation, bUseSurfaceOrientation, SelectActor, ObjectFlags );
		}
	}

	return Result;
}

/**
* Determines if the provided actor is capable of having a material applied to it.
*
* @param	TargetActor	Actor to check for the validity of material application
*
* @return	true if the actor is valid for material application; false otherwise
*/
bool FActorFactoryAssetProxy::IsActorValidForMaterialApplication( AActor* TargetActor )
{
	bool bIsValid = false;

	// Check if the actor has a mesh or fog volume density. If so, it can likely have
	// a material applied to it. Otherwise, it cannot.
	if ( TargetActor )
	{
		TArray<UMeshComponent*> MeshComponents;
		TargetActor->GetComponents(MeshComponents);

		bIsValid = (MeshComponents.Num() > 0);
	}

	return bIsValid;
}
/**
* Attempts to apply the material to the specified actor.
*
* @param	TargetActor		the actor to apply the material to
* @param	MaterialToApply	the material to apply to the actor
* @param    OptionalMaterialSlot the material slot to apply to.
*
* @return	true if the material was successfully applied to the actor
*/
bool FActorFactoryAssetProxy::ApplyMaterialToActor( AActor* TargetActor, UMaterialInterface* MaterialToApply, int32 OptionalMaterialSlot )
{
	bool bResult = false;

	if ( TargetActor != NULL && MaterialToApply != NULL )
	{
		ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(TargetActor);
		if (Landscape != NULL)
		{
			UProperty* MaterialProperty = FindField<UProperty>(ALandscapeProxy::StaticClass(), "LandscapeMaterial");
			Landscape->PreEditChange(MaterialProperty);
			Landscape->LandscapeMaterial = MaterialToApply;
			FPropertyChangedEvent PropertyChangedEvent(MaterialProperty);
			Landscape->PostEditChangeProperty(PropertyChangedEvent);
			bResult = true;
		}
		else
		{
			TArray<UActorComponent*> EditableComponents;
			FActorEditorUtils::GetEditableComponents( TargetActor, EditableComponents );

			// Some actors could potentially have multiple mesh components, so we need to store all of the potentially valid ones
			// (or else perform special cases with IsA checks on the target actor)
			TArray<UActorComponent*> FoundMeshComponents;

			// Find which mesh the user clicked on first.
			TArray<USceneComponent*> SceneComponents;
			TargetActor->GetComponents(SceneComponents);

			for ( int32 ComponentIdx=0; ComponentIdx < SceneComponents.Num(); ComponentIdx++ )
			{
				USceneComponent* SceneComp = SceneComponents[ComponentIdx];
				
				// Only apply the material to editable components.  Components which are not exposed are not intended to be changed.
				if( EditableComponents.Contains( SceneComp ) )
				{
					UMeshComponent* MeshComponent = Cast<UMeshComponent>(SceneComp);

					if((MeshComponent && MeshComponent->IsRegistered()) ||
						SceneComp->IsA<UDecalComponent>())
					{
						// Intentionally do not break the loop here, as there could be potentially multiple mesh components
						FoundMeshComponents.AddUnique( SceneComp );
					}
				}
			}

			if ( FoundMeshComponents.Num() > 0 )
			{
				// Check each component that was found
				for ( TArray<UActorComponent*>::TConstIterator MeshCompIter( FoundMeshComponents ); MeshCompIter; ++MeshCompIter )
				{
					UActorComponent* ActorComp = *MeshCompIter;

					UMeshComponent* FoundMeshComponent = Cast<UMeshComponent>(ActorComp);
					UDecalComponent* DecalComponent = Cast<UDecalComponent>(ActorComp);

					if(FoundMeshComponent)
					{
						// OK, we need to figure out how many material slots this mesh component/static mesh has.
						// Start with the actor's material count, then drill into the static/skeletal mesh to make sure 
						// we have the right total.
						int32 MaterialCount = FMath::Max(FoundMeshComponent->Materials.Num(), FoundMeshComponent->GetNumMaterials());

						// Any materials to overwrite?
						if( MaterialCount > 0 )
						{
							const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "DropTarget_UndoSetActorMaterial", "Assign Material (Drag and Drop)") );
							FoundMeshComponent->Modify();

							// If the material slot is -1 then apply the material to every slot.
							if ( OptionalMaterialSlot == -1 )
							{
								for ( int32 CurMaterialIndex = 0; CurMaterialIndex < MaterialCount; ++CurMaterialIndex )
								{
									FoundMeshComponent->SetMaterial(CurMaterialIndex, MaterialToApply);
								}
							}
							else
							{
								check(OptionalMaterialSlot < MaterialCount);
								FoundMeshComponent->SetMaterial(OptionalMaterialSlot, MaterialToApply);
							}

							TargetActor->MarkComponentsRenderStateDirty();
							bResult = true;
						}
					}
					else if(DecalComponent)
					{
						const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "DropTarget_UndoSetActorMaterial", "Assign Material (Drag and Drop)") );
						DecalComponent->Modify();
						// set material #0
						DecalComponent->SetMaterial(0, MaterialToApply);
						TargetActor->MarkComponentsRenderStateDirty();
						bResult = true;
					}
				}
			}
		}
	}


	return bResult;
}


// EOF




