// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "UnrealEd.h"
#include "ScopedTransaction.h"
#include "Factories.h"
#include "LevelUtils.h"
#include "BusyCursor.h"
#include "BSPOps.h"
#include "EditorLevelUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Layers/Layers.h"
#include "Editor/GeometryMode/Public/GeometryEdMode.h"
#include "Editor/GeometryMode/Public/EditorGeometry.h"
#include "ActorEditorUtils.h"

#define LOCTEXT_NAMESPACE "UnrealEd.EditorActor"

DEFINE_LOG_CATEGORY_STATIC(LogEditorActor, Log, All);

PRAGMA_DISABLE_OPTIMIZATION /* Not performance-critical */

	 

static int32 RecomputePoly( ABrush* InOwner, FPoly* Poly )
{
	// force recalculation of normal, and texture U and V coordinates in FPoly::Finalize()
	Poly->Normal = FVector::ZeroVector;

	return Poly->Finalize( InOwner, 0 );
}

/*-----------------------------------------------------------------------------
   Actor adding/deleting functions.
-----------------------------------------------------------------------------*/

class FSelectedActorExportObjectInnerContext : public FExportObjectInnerContext
{
public:
	FSelectedActorExportObjectInnerContext()
		//call the empty version of the base class
		: FExportObjectInnerContext(false)
	{
		// For each object . . .
		for (UObject* InnerObj : TObjectRange<UObject>(RF_ClassDefaultObject | RF_PendingKill))
		{
			UObject* OuterObj = InnerObj->GetOuter();

			//assume this is not part of a selected actor
			bool bIsChildOfSelectedActor = false;

			UObject* TestParent = OuterObj;
			while (TestParent)
			{
				AActor* TestParentAsActor = Cast<AActor>(TestParent);
				if (TestParentAsActor && TestParentAsActor->IsSelected())
				{
					bIsChildOfSelectedActor = true;
					break;
				}
				TestParent = TestParent->GetOuter();
			}

			if (bIsChildOfSelectedActor)
			{
				InnerList* Inners = ObjectToInnerMap.Find(OuterObj);
				if (Inners)
				{
					// Add object to existing inner list.
					Inners->Add( InnerObj );
				}
				else
				{
					// Create a new inner list for the outer object.
					InnerList& InnersForOuterObject = ObjectToInnerMap.Add(OuterObj, InnerList());
					InnersForOuterObject.Add(InnerObj);
				}
			}
		}
	}
};

UUnrealEdEngine::UUnrealEdEngine(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}


void UUnrealEdEngine::edactCopySelected( UWorld* InWorld, FString* DestinationData )
{
	// Before copying, deselect:
	//		- Actors belonging to prefabs unless all actors in the prefab are selected.
	//		- Builder brushes.
	TArray<AActor*> ActorsToDeselect;

	bool bSomeSelectedActorsNotInCurrentLevel = false;
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		// Deselect any selected builder brushes.
		ABrush* Brush = Cast< ABrush >( Actor );
		const bool bActorIsBuilderBrush = (Brush && FActorEditorUtils::IsABuilderBrush(Brush));
		if( bActorIsBuilderBrush )
		{
			ActorsToDeselect.Add(Actor);
		}

		// If any selected actors are not in the current level, warn the user that some actors will not be copied.
		if ( !bSomeSelectedActorsNotInCurrentLevel && !Actor->GetLevel()->IsCurrentLevel() )
		{
			bSomeSelectedActorsNotInCurrentLevel = true;
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "CopySelectedActorsInNonCurrentLevel", "Some selected actors are not in the current level and will not be copied.") );
		}
	}

	const FScopedBusyCursor BusyCursor;
	for( int32 ActorIndex = 0 ; ActorIndex < ActorsToDeselect.Num() ; ++ActorIndex )
	{
		AActor* Actor = ActorsToDeselect[ActorIndex];
		GetSelectedActors()->Deselect( Actor );
	}

	// Export the actors.
	FStringOutputDevice Ar;
	const FSelectedActorExportObjectInnerContext Context;
	UExporter::ExportToOutputDevice( &Context, InWorld, NULL, Ar, TEXT("copy"), 0, PPF_DeepCompareInstances | PPF_ExportsNotFullyQualified);
	FPlatformMisc::ClipboardCopy( *Ar );
	if( DestinationData )
	{
		*DestinationData = MoveTemp(Ar);
	}
}



/**
 * Creates offsets for locations based on the editor grid size and active viewport.
 */
static FVector CreateLocationOffset(bool bDuplicate, bool bOffsetLocations)
{
	const float Offset = static_cast<float>( bOffsetLocations ? GEditor->GetGridSize() : 0 );
	FVector LocationOffset(Offset,Offset,Offset);
	if ( bDuplicate && GCurrentLevelEditingViewportClient )
	{
		switch( GCurrentLevelEditingViewportClient->ViewportType )
		{
		case LVT_OrthoXZ:
			LocationOffset = FVector(Offset,0.f,Offset);
			break;
		case LVT_OrthoYZ:
			LocationOffset = FVector(0.f,Offset,Offset);
			break;
		default:
			LocationOffset = FVector(Offset,Offset,0.f);
			break;
		}
	}
	return LocationOffset;
}


bool UUnrealEdEngine::WarnIfDestinationLevelIsHidden( UWorld* InWorld )
{
	bool result = false;
	//prepare the warning dialog
	FSuppressableWarningDialog::FSetupInfo Info( LOCTEXT( "Warning_PasteWarningBody","You are trying to paste to a hidden level.\nSupressing this will default to Do Not Paste" ), LOCTEXT( "Warning_PasteWarningHeader","Pasting To Hidden Level" ), "PasteHiddenWarning" );
	Info.ConfirmText = LOCTEXT( "Warning_PasteContinue","Unhide Level and paste" );
	Info.CancelText = LOCTEXT( "Warning_PasteCancel","Do not paste" );
	FSuppressableWarningDialog PasteHiddenWarning( Info );

	//check streaming levels first	
	for (int32 i = 0; i < InWorld->StreamingLevels.Num(); i++)
	{
		ULevelStreaming* StreamedLevel = InWorld->StreamingLevels[i];
		//this is the active level - check if it is visible
		if ( StreamedLevel != NULL && StreamedLevel->bShouldBeVisibleInEditor == false )
		{
			ULevel* Level = StreamedLevel->GetLoadedLevel();
			if( Level != NULL && Level->IsCurrentLevel() )
			{
				//the streamed level is not visible - check what the user wants to do
				FSuppressableWarningDialog::EResult DialogResult = PasteHiddenWarning.ShowModal();
				if( ( DialogResult == FSuppressableWarningDialog::Cancel )  || ( DialogResult == FSuppressableWarningDialog::Suppressed ) )
				{
					result = true;
				}
				else
				{
					EditorLevelUtils::SetLevelVisibility( Level, true, true );					
				}
			}
		}
	}

	//now check the active level (this handles the persistent level also)
	if( result == false )
	{
		if( FLevelUtils::IsLevelVisible( InWorld->GetCurrentLevel() ) == false )
		{	
			//the level is not visible - check what the user wants to do
			FSuppressableWarningDialog::EResult DialogResult = PasteHiddenWarning.ShowModal();
			if( ( DialogResult == FSuppressableWarningDialog::Cancel )  || ( DialogResult == FSuppressableWarningDialog::Suppressed ) )
			{
				result = true;
			}
			else 
			{
				EditorLevelUtils::SetLevelVisibility( InWorld->GetCurrentLevel(), true, true );
			}

		}
	}
	return result;
}

void UUnrealEdEngine::edactPasteSelected(UWorld* InWorld, bool bDuplicate, bool bOffsetLocations, bool bWarnIfHidden, FString* SourceData)
{
	//check and warn if the user is trying to paste to a hidden level. This will return if he wishes to abort the process
	if( bWarnIfHidden && WarnIfDestinationLevelIsHidden( InWorld ) == true )
	{
		return;
	}
	

	const FScopedBusyCursor BusyCursor;

	// Create a location offset.
	const FVector LocationOffset = CreateLocationOffset( bDuplicate, bOffsetLocations );

	// Transact the current selection set.
	USelection* SelectedActors = GetSelectedActors();
	SelectedActors->Modify();

	// Get pasted text.
	FString PasteString;
	if( SourceData )
	{
		PasteString = *SourceData;
	}
	else
	{
		FPlatformMisc::ClipboardPaste(PasteString);
	}
	const TCHAR* Paste = *PasteString;

	// Import the actors.
	ULevelFactory* Factory = new ULevelFactory(FPostConstructInitializeProperties());
	Factory->FactoryCreateText( ULevel::StaticClass(), InWorld->GetCurrentLevel(), InWorld->GetCurrentLevel()->GetFName(), RF_Transactional, NULL, bDuplicate ? TEXT("move") : TEXT("paste"), Paste, Paste+FCString::Strlen(Paste), GWarn );

	// Fire ULevel::LevelDirtiedEvent when falling out of scope.
	FScopedLevelDirtied			LevelDirtyCallback;

	// Update the actors' locations and update the global list of visible layers.
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		// We only want to offset the location if this actor is the root of a selected attachment hierarchy
		// Offsetting children of an attachment hierarchy would cause them to drift away from the node they're attached to
		// as the offset would effectively get applied twice
		const AActor* const ParentActor = Actor->GetAttachParentActor();
		const FVector& ActorLocationOffset = (ParentActor && ParentActor->IsSelected()) ? FVector::ZeroVector : LocationOffset;

		// Offset the actor's location.
		Actor->TeleportTo(Actor->GetActorLocation() + ActorLocationOffset, Actor->GetActorRotation(), false, true);

		// Re-label duplicated actors so that labels become unique
		GEditor->SetActorLabelUnique(Actor, Actor->GetActorLabel());
		
		GEditor->Layers->InitializeNewActorLayers( Actor );

			// Ensure any layers this actor belongs to are visible
		GEditor->Layers->SetLayersVisibility( Actor->Layers, true );

		Actor->CheckDefaultSubobjects();
		Actor->InvalidateLightingCache();
		// Call PostEditMove to update components, etc.
		Actor->PostEditMove( true );
		Actor->PostDuplicate(false);
		Actor->CheckDefaultSubobjects();

		// Request saves/refreshes.
		Actor->MarkPackageDirty();
		LevelDirtyCallback.Request();
	}
	// Note the selection change.  This will also redraw level viewports and update the pivot.
	NoteSelectionChange();
}

namespace DuplicateSelectedActors {
/**
 * A collection of actors to duplicate and prefabs to instance that all belong to the same level.
 */
class FDuplicateJob
{
public:
	/** A list of actors to duplicate. */
	TArray<AActor*>	Actors;

	/** The source level that all actors in the Actors array come from. */
	ULevel*			SrcLevel;

	/**
	 * Duplicate the job's actors to the specified destination level.  The new actors
	 * are appended to the specified output lists of actors.
	 *
	 * @param	OutNewActors			[out] Newly created actors are appended to this list.
	 * @param	DestLevel				The level to duplicate the actors in this job to.
	 * @param	bOffsetLocations		Passed to edactPasteSelected; true if new actor locations should be offset.
	 */
	void DuplicateActorsToLevel(TArray<AActor*>& OutNewActors, ULevel* DestLevel, bool bOffsetLocations)
	{
		// Check neither level is locked
		if ( FLevelUtils::IsLevelLocked(SrcLevel) || FLevelUtils::IsLevelLocked(DestLevel) )
		{
			UE_LOG(LogEditorActor, Warning, TEXT("DuplicateActorsToLevel: The requested operation could not be completed because the level is locked."));
			return;
		}

		// Cache the current source level
		ULevel* OldLevel = SrcLevel->OwningWorld->GetCurrentLevel();
		// Set the selection set to be precisely the actors belonging to this job.
		SrcLevel->OwningWorld->SetCurrentLevel( SrcLevel );
		GEditor->SelectNone( false, true );
		for ( int32 ActorIndex = 0 ; ActorIndex < Actors.Num() ; ++ActorIndex )
		{
			AActor* Actor = Actors[ ActorIndex ];
			GEditor->SelectActor( Actor, true, false, true );
		}

		FString ScratchData;
		// Copy actors from src level.
		GEditor->edactCopySelected( SrcLevel->OwningWorld, &ScratchData );
		// Restore source level
		SrcLevel->OwningWorld->SetCurrentLevel( OldLevel );

		// Cache the current dest level
		OldLevel = DestLevel->OwningWorld->GetCurrentLevel();
		// Paste to the dest level.
		DestLevel->OwningWorld->SetCurrentLevel( DestLevel );
		GEditor->edactPasteSelected( DestLevel->OwningWorld, true, bOffsetLocations, true, &ScratchData );

		// The selection set will be the newly created actors; copy them over to the output array.
		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );
			OutNewActors.Add( Actor );
		}
		// Restore dest level
		DestLevel->OwningWorld->SetCurrentLevel( OldLevel );
	}
};
}


void UUnrealEdEngine::edactDuplicateSelected( ULevel* InLevel, bool bOffsetLocations )
{
	using namespace DuplicateSelectedActors;

	const FScopedBusyCursor BusyCursor;
	GetSelectedActors()->Modify();

	// Create per-level job lists.
	typedef TMap<ULevel*, FDuplicateJob*>	DuplicateJobMap;
	DuplicateJobMap							DuplicateJobs;

	// Build set of selected actors before duplication
	TArray<AActor*> PreDuplicateSelection;

	// Add selected actors to the per-level job lists.
	bool bHaveActorLocation = false;
	FVector AnyActorLocation = FVector::ZeroVector;
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if( !bHaveActorLocation )
		{
			bHaveActorLocation = true;
			AnyActorLocation = Actor->GetActorLocation();
		}

		PreDuplicateSelection.Add(Actor);

		ULevel* OldLevel = Actor->GetLevel();
		FDuplicateJob** Job = DuplicateJobs.Find( OldLevel );
		if ( Job )
		{
			(*Job)->Actors.Add( Actor );
		}
		else
		{
			// Allocate a new job for the level.
			FDuplicateJob* NewJob = new FDuplicateJob;
			NewJob->SrcLevel = OldLevel;
			NewJob->Actors.Add( Actor );
			DuplicateJobs.Add( OldLevel, NewJob );
		}
	}

	UWorld* World = InLevel->OwningWorld;
	ULevel* DesiredLevel = InLevel;

	USelection* SelectedActors = GetSelectedActors();
	SelectedActors->BeginBatchSelectOperation();
	SelectedActors->Modify();

	// For each level, select the actors in that level and copy-paste into the destination level.
	TArray<AActor*>	NewActors;
	for ( DuplicateJobMap::TIterator It( DuplicateJobs ) ; It ; ++It )
	{
		FDuplicateJob* Job = It.Value();
		check( Job );
		Job->DuplicateActorsToLevel( NewActors, InLevel, bOffsetLocations );
	}

	// Select any newly created actors and prefabs.
	SelectNone( false, true );
	for ( int32 ActorIndex = 0 ; ActorIndex < NewActors.Num() ; ++ActorIndex )
	{
		AActor* Actor = NewActors[ ActorIndex ];
		SelectActor( Actor, true, false );
	}
	SelectedActors->EndBatchSelectOperation();
	NoteSelectionChange();

	// Finally, cleanup.
	for ( DuplicateJobMap::TIterator It( DuplicateJobs ) ; It ; ++It )
	{
		FDuplicateJob* Job = It.Value();
		delete Job;
	}
	
	// Build set of selected actors after duplication
	TArray<AActor*> PostDuplicateSelection;
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		// We generate new seeds when we duplicate
		Actor->SeedAllRandomStreams();

		PostDuplicateSelection.Add(Actor);
	}
	
	TArray<FEdMode*> ActiveModes;
	GEditorModeTools().GetActiveModes( ActiveModes );

	for( int32 ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		// Tell the tools about the duplication
		ActiveModes[ModeIndex]->ActorsDuplicatedNotify(PreDuplicateSelection, PostDuplicateSelection, bOffsetLocations);
	}
}

bool UUnrealEdEngine::CanDeleteSelectedActors( const UWorld* InWorld, const bool bStopAtFirst, const bool bLogUndeletable, TArray<AActor*>* OutDeletableActors ) const
{
	// Iterate over all levels and create a list of world infos.
	TArray<AWorldSettings*> WorldSettingss;
	for ( int32 LevelIndex = 0 ; LevelIndex < InWorld->GetNumLevels() ; ++LevelIndex )
	{
		ULevel* Level = InWorld->GetLevel( LevelIndex );
		WorldSettingss.Add( Level->GetWorldSettings() );
	}

	// Iterate over selected actors and assemble a list of actors to delete.
	bool bContainsDeletable = false;
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor		= static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		// Only delete transactional actors that aren't a level's builder brush or worldsettings.
		bool bDeletable	= false;
		if ( Actor->HasAllFlags( RF_Transactional ) )
		{
			ABrush* Brush = Cast< ABrush >( Actor );
			const bool bIsDefaultBrush = Brush && FActorEditorUtils::IsABuilderBrush(Brush);
			if ( !bIsDefaultBrush )
			{
				const bool bIsWorldSettings =
					Actor->IsA( AWorldSettings::StaticClass() ) && WorldSettingss.Contains( static_cast<AWorldSettings*>(Actor) );
				if ( !bIsWorldSettings )
				{
					bContainsDeletable = true;
					bDeletable = true;
				}
			}
		}

		// Can this actor be deleted
		if ( bDeletable )
		{
			if ( OutDeletableActors )
			{
				OutDeletableActors->Add( Actor );
			}
			if ( bStopAtFirst )
			{
				break;	// Did we only want to know if ANY of the actors were deletable
			}
		}
		else if ( bLogUndeletable )
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("Name"), FText::FromString( Actor->GetFullName() ));

			const FText LogText = FText::Format( LOCTEXT( "CannotDeleteSpecialActor", "Cannot delete special actor {Name}" ), Arguments );
			UE_LOG(LogEditorActor, Log, TEXT("%s"), *LogText.ToString() );
		}
	}
	return bContainsDeletable;
}

bool UUnrealEdEngine::edactDeleteSelected( UWorld* InWorld, bool bVerifyDeletionCanHappen)
{
	if ( bVerifyDeletionCanHappen )
	{
		// Provide the option to abort the delete
		if ( ShouldAbortActorDeletion() )
		{
			return false;
		}
	}

	const double StartSeconds = FPlatformTime::Seconds();

	GetSelectedActors()->Modify();

	// Fire ULevel::LevelDirtiedEvent when falling out of scope.
	FScopedLevelDirtied			LevelDirtyCallback;

	// Get a list of all the deletable actors in the selection
	TArray<AActor*> ActorsToDelete;
	CanDeleteSelectedActors( InWorld, false, true, &ActorsToDelete );

	// Maintain a list of levels that have already been Modify()'d so that each level
	// is modify'd only once.
	TArray<ULevel*> LevelsAlreadyModified;
	// A list of levels that will need their Bsp updated after the deletion is complete
	TArray<ULevel*> LevelsToRebuild;

	bool	bBrushWasDeleted = false;
	bool	bRequestedDeleteAllByLevel = false;
	bool	bRequestedDeleteAllByActor = false;
	EAppMsgType::Type MessageType = ActorsToDelete.Num() > 1 ? EAppMsgType::YesNoYesAllNoAll : EAppMsgType::YesNo;
	int32		DeleteCount = 0;

	USelection* SelectedActors = GetSelectedActors();

	for ( int32 ActorIndex = 0 ; ActorIndex < ActorsToDelete.Num() ; ++ActorIndex )
	{
		AActor* Actor = ActorsToDelete[ ActorIndex ];

		//If actor is referenced by script, ask user if they really want to delete
		ULevelScriptBlueprint* LSB = Actor->GetLevel()->GetLevelScriptBlueprint(true);
		TArray<AActor*> ReferencingActors;
		TArray<UClass*> ClassTypesToIgnore;
		ClassTypesToIgnore.Add( ALevelScriptActor::StaticClass() );
		FBlueprintEditorUtils::FindActorsThatReferenceActor( Actor, ClassTypesToIgnore, ReferencingActors );

		bool bReferencedByLevelScript = (NULL != LSB && FBlueprintEditorUtils::FindNumReferencesToActorFromLevelScript(LSB, Actor) > 0);
		bool bReferencedByActor = ( ReferencingActors.Num() > 0 );

		if ( bReferencedByLevelScript || bReferencedByActor )
		{
			if (( bReferencedByLevelScript && !bRequestedDeleteAllByLevel ) ||
				( bReferencedByActor && !bRequestedDeleteAllByActor ))
			{
				FText ConfirmDelete;
				if ( bReferencedByLevelScript && bReferencedByActor )
				{
					ConfirmDelete = FText::Format(LOCTEXT( "ConfirmDeleteActorReferenceByScriptAndActor",
														   "Actor {0} is referenced by the level blueprint and another Actor, do you really want to delete it?"),
														   FText::FromString( Actor->GetName() ) );
				}
				else if ( bReferencedByLevelScript )
				{
					ConfirmDelete = FText::Format(LOCTEXT( "ConfirmDeleteActorReferencedByScript",
														   "Actor {0} is referenced by the level blueprint, do you really want to delete it?"),
														   FText::FromString( Actor->GetName() ) );
				}
				else
				{
					ConfirmDelete = FText::Format(LOCTEXT( "ConfirmDeleteActorReferencedByActor",
														   "Actor {0} is referenced by another Actor, do you really want to delete it?"),
														   FText::FromString( Actor->GetName() ) );
				}

				int32 Result = FMessageDialog::Open( MessageType, ConfirmDelete );
				if ( Result == EAppReturnType::YesAll )
				{
					bRequestedDeleteAllByLevel = bReferencedByLevelScript;
					bRequestedDeleteAllByActor = bReferencedByActor;
				}
				else if ( Result == EAppReturnType::NoAll )
				{
					break;
				}
				else if( Result == EAppReturnType::No || Result == EAppReturnType::Cancel )
				{ 
					continue;
				}
			}

			if ( bReferencedByLevelScript )
			{
				FBlueprintEditorUtils::ModifyActorReferencedGraphNodes(LSB, Actor);
			}
			if ( bReferencedByActor )
			{
				for ( int32 ReferencingActorIndex = 0; ReferencingActorIndex < ReferencingActors.Num(); ReferencingActorIndex++ )
				{
					ReferencingActors[ReferencingActorIndex]->Modify();
				}
			}
		}
	
		ABrush* Brush = Cast< ABrush >( Actor );
		if (Brush && !FActorEditorUtils::IsABuilderBrush(Brush)) // Track whether or not a brush actor was deleted.
		{
			bBrushWasDeleted = true;
			ULevel* BrushLevel = Actor->GetLevel();
			if (BrushLevel)
			{
				LevelsToRebuild.AddUnique(BrushLevel);
			}
		}
		// If the actor about to be deleted is in a group, be sure to remove it from the group
		AGroupActor* ActorParentGroup = AGroupActor::GetParentForActor(Actor);
		if(ActorParentGroup)
		{
			ActorParentGroup->Remove(*Actor);
		}

		// Mark the actor's level as dirty.
		Actor->MarkPackageDirty();
		LevelDirtyCallback.Request();

		// Deselect the Actor.
		SelectedActors->Deselect(Actor);

		// Modify the level.  Each level is modified only once.
		// @todo DB: Shouldn't this be calling UWorld::ModifyLevel?
		ULevel* Level = Actor->GetLevel();
		if ( LevelsAlreadyModified.Find( Level ) == INDEX_NONE )
		{
			LevelsAlreadyModified.Add( Level );
			Level->Modify();
		}

		// See if there is any foliage that also needs to be removed
		AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(Level);
		if( IFA )
		{
			TArray<UActorComponent*> Components;
			Actor->GetComponents(Components);

			for(int32 ComponentIndex = 0;ComponentIndex < Components.Num();ComponentIndex++)
			{
				IFA->DeleteInstancesForComponent( Components[ComponentIndex] );
			}
		}

		UE_LOG(LogEditorActor, Log,  TEXT("Deleted Actor: %s"), *Actor->GetClass()->GetName() );

		// Destroy actor and clear references.
		GEditor->Layers->DisassociateActorFromLayers( Actor );
		bool WasDestroyed = Actor->GetWorld()->EditorDestroyActor( Actor, false );
		checkf( WasDestroyed,TEXT( "Failed to destroy Actor %s (%s)"), *Actor->GetClass()->GetName(), *Actor->GetActorLabel() );

		DeleteCount++;
	}

	// Remove all references to destroyed actors once at the end, instead of once for each Actor destroyed..
	CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

	NoteSelectionChange();
	// If any brush actors were deleted, update the Bsp in the appropriate levels
	if (bBrushWasDeleted)
	{
		FlushRenderingCommands();

		for (int32 LevelIdx = 0; LevelIdx < LevelsToRebuild.Num(); ++LevelIdx)
		{
			GEditor->RebuildLevel(*(LevelsToRebuild[LevelIdx]));
		}

		RedrawLevelEditingViewports();
		ULevel::LevelDirtiedEvent.Broadcast();
	}

	UE_LOG(LogEditorActor, Log,  TEXT("Deleted %d Actors (%3.3f secs)"), DeleteCount, FPlatformTime::Seconds() - StartSeconds );

	return true;
}



bool UUnrealEdEngine::ShouldAbortActorDeletion() const
{
	bool bResult = false;

	// Can't delete actors if Matinee is open.
	const FText ErrorMsg = NSLOCTEXT("UnrealEd", "Error_WrongModeForActorDeletion", "Cannot delete actor while Matinee is open" );
	if ( !GEditorModeTools().EnsureNotInMode( FBuiltinEditorModes::EM_InterpEdit, ErrorMsg, true ) )
	{
		bResult = true;
	}

	if ( !bResult )
	{
		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			ULevel* ActorLevel = Actor->GetLevel();
			if ( FLevelUtils::IsLevelLocked(ActorLevel) )
			{
				UE_LOG(LogEditorActor, Warning, TEXT("Cannot perform action on actor %s because the actor's level is locked"), *Actor->GetName());
				bResult = true;
				break;
			}
		}
	}

	return bResult;
}


void UUnrealEdEngine::edactReplaceSelectedBrush( UWorld* InWorld )
{
	// Make a list of brush actors to replace.
	ABrush* DefaultBrush = InWorld->GetBrush();

	TArray<ABrush*> BrushesToReplace;
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );
		ABrush* Brush = Cast< ABrush >( Actor );
		if ( Brush && Actor->HasAnyFlags(RF_Transactional) && Actor != DefaultBrush )
		{
			BrushesToReplace.Add( Brush );
		}
	}

	// Fire ULevel::LevelDirtiedEvent when falling out of scope.
	FScopedLevelDirtied			LevelDirtyCallback;

	USelection* SelectedActors = GetSelectedActors();
	SelectedActors->BeginBatchSelectOperation();
	SelectedActors->Modify();

	// Replace brushes.
	for ( int32 BrushIndex = 0 ; BrushIndex < BrushesToReplace.Num() ; ++BrushIndex )
	{
		ABrush* SrcBrush = BrushesToReplace[BrushIndex];
		ABrush* NewBrush = FBSPOps::csgAddOperation( DefaultBrush, SrcBrush->PolyFlags, (EBrushType)SrcBrush->BrushType );
		if( NewBrush )
		{
			SrcBrush->MarkPackageDirty();
			NewBrush->MarkPackageDirty();

			LevelDirtyCallback.Request();

			NewBrush->Modify();

				NewBrush->Layers.Append( SrcBrush->Layers );

			NewBrush->CopyPosRotScaleFrom( SrcBrush );
			NewBrush->PostEditMove( true );
			SelectActor( SrcBrush, false, false );
			SelectActor( NewBrush, true, false );

			GEditor->Layers->DisassociateActorFromLayers( SrcBrush );
			InWorld->EditorDestroyActor( SrcBrush, true );
		}
	}

	SelectedActors->EndBatchSelectOperation();
	NoteSelectionChange();
}


AActor* UUnrealEdEngine::ReplaceActor( AActor* CurrentActor, UClass* NewActorClass, UObject* Archetype, bool bNoteSelectionChange )
{
	FVector SpawnLoc = CurrentActor->GetActorLocation();
	FRotator SpawnRot = CurrentActor->GetActorRotation();
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Template = Cast<AActor>(Archetype);
	SpawnInfo.bNoCollisionFail = true;
	AActor* NewActor = CurrentActor->GetWorld()->SpawnActor( NewActorClass, &SpawnLoc, &SpawnRot, SpawnInfo );
	if( NewActor )
	{
		NewActor->Modify();
		GEditor->Layers->InitializeNewActorLayers( NewActor );

		const bool bCurrentActorSelected = GetSelectedActors()->IsSelected( CurrentActor );
		if ( bCurrentActorSelected )
		{
			// The source actor was selected, so deselect the old actor and select the new one.
			GetSelectedActors()->Modify();
			SelectActor( NewActor, bCurrentActorSelected, false );
			SelectActor( CurrentActor, false, false );
		}

		{
			GEditor->Layers->DisassociateActorFromLayers( NewActor );
			NewActor->Layers.Empty();

			GEditor->Layers->AddActorToLayers( NewActor, CurrentActor->Layers );

			NewActor->SetActorLabel( CurrentActor->GetActorLabel() );
			NewActor->Tags = CurrentActor->Tags;

			NewActor->EditorReplacedActor( CurrentActor );
		}

		GEditor->Layers->DisassociateActorFromLayers( CurrentActor );
		CurrentActor->GetWorld()->EditorDestroyActor( CurrentActor, true );

		// Note selection change if necessary and requested.
		if ( bCurrentActorSelected && bNoteSelectionChange )
		{
			NoteSelectionChange();
		}

		//whenever selection changes, recompute whether the selection contains a locked actor
		bCheckForLockActors = true;

		//whenever selection changes, recompute whether the selection contains a world info actor
		bCheckForWorldSettingsActors = true;
	}

	return NewActor;
}


void UUnrealEdEngine::edactReplaceSelectedNonBrushWithClass(UClass* Class)
{
	// Make a list of actors to replace.
	TArray<AActor*> ActorsToReplace;
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );
		ABrush* Brush = Cast< ABrush >( Actor );
		if ( !Brush && Actor->HasAnyFlags(RF_Transactional) )
		{
			ActorsToReplace.Add( Actor );
		}
	}

	// Fire ULevel::LevelDirtiedEvent when falling out of scope.
	FScopedLevelDirtied			LevelDirtyCallback;

	// Replace actors.
	for ( int32 i = 0 ; i < ActorsToReplace.Num() ; ++i )
	{
		AActor* SrcActor = ActorsToReplace[i];
		AActor* NewActor = ReplaceActor( SrcActor, Class, NULL, false );
		if ( NewActor )
		{
			NewActor->MarkPackageDirty();
			LevelDirtyCallback.Request();
		}
	}

	NoteSelectionChange();
}


void UUnrealEdEngine::edactReplaceClassWithClass(UWorld* InWorld, UClass* SrcClass, UClass* DstClass)
{
	// Make a list of actors to replace.
	TArray<AActor*> ActorsToReplace;
	for( FActorIterator It(InWorld); It; ++It )
	{
		AActor* Actor = *It;
		if ( Actor->IsA( SrcClass ) && Actor->HasAnyFlags(RF_Transactional) )
		{
			ActorsToReplace.Add( Actor );
		}
	}

	// Fires ULevel::LevelDirtiedEvent when falling out of scope.
	FScopedLevelDirtied			LevelDirtyCallback;

	// Replace actors.
	for ( int32 i = 0 ; i < ActorsToReplace.Num() ; ++i )
	{
		AActor* SrcActor = ActorsToReplace[i];
		AActor* NewActor = ReplaceActor( SrcActor, DstClass, NULL, false );
		if ( NewActor )
		{
			NewActor->MarkPackageDirty();
			LevelDirtyCallback.Request();
		}
	}

	NoteSelectionChange();
}

void UUnrealEdEngine::edactHideSelected( UWorld* InWorld )
{
	// Assemble a list of actors to hide.
	TArray<AActor*> ActorsToHide;
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );
		
		// Don't consider already hidden actors or the builder brush
		if ( !FActorEditorUtils::IsABuilderBrush(Actor) && !Actor->IsHiddenEd() )
		{
			ActorsToHide.Add( Actor );
		}
	}
	
	// Hide the actors that were selected and deselect them in the process
	if ( ActorsToHide.Num() > 0 )
	{
		USelection* SelectedActors = GetSelectedActors();
		SelectedActors->Modify();

		for( int32 ActorIndex = 0 ; ActorIndex < ActorsToHide.Num() ; ++ActorIndex )
		{
			AActor* Actor = ActorsToHide[ ActorIndex ];
			
			// Save the actor to the transaction buffer to support undo/redo, but do
			// not call Modify, as we do not want to dirty the actor's package and
			// we're only editing temporary, transient values
			SaveToTransactionBuffer(Actor, false);
			Actor->SetIsTemporarilyHiddenInEditor( true );
			SelectedActors->Deselect( Actor );
		}

		NoteSelectionChange();
	}

	// Iterate through all of the BSP models and hide any that were selected (deselecting them in the process)
	if ( InWorld )
	{
		for ( TArray<ULevel*>::TConstIterator LevelIterator = InWorld->GetLevels().CreateConstIterator(); LevelIterator; ++LevelIterator )
		{
			UModel& CurLevelModel = *( ( *LevelIterator )->Model );
			for ( TArray<FBspSurf>::TIterator SurfaceIterator( CurLevelModel.Surfs ); SurfaceIterator; ++SurfaceIterator )
			{
				FBspSurf& CurSurface = *SurfaceIterator;
				if ( ( CurSurface.PolyFlags & PF_Selected ) && !CurSurface.IsHiddenEd() )
				{
					CurLevelModel.ModifySurf( SurfaceIterator.GetIndex(), false );
					
					// Deselect the surface and mark it as hidden to the editor
					CurSurface.PolyFlags &= ~PF_Selected;
					CurSurface.bHiddenEdTemporary = true;
				}
			}
		}
	}
	RedrawLevelEditingViewports();
}


void UUnrealEdEngine::edactHideUnselected( UWorld* InWorld )
{
	// Iterate through all of the actors and hide the ones which are not selected and are not already hidden
	for( FActorIterator It(InWorld); It; ++It )
	{
		AActor* Actor = *It;
		if( !FActorEditorUtils::IsABuilderBrush(Actor) && !Actor->IsSelected() && !Actor->IsHiddenEd() )
		{
			// Save the actor to the transaction buffer to support undo/redo, but do
			// not call Modify, as we do not want to dirty the actor's package and
			// we're only editing temporary, transient values
			SaveToTransactionBuffer(Actor, false);
			Actor->SetIsTemporarilyHiddenInEditor( true );
		}
	}

	// Iterate through all of the BSP models and hide the ones which are not selected and are not already hidden
	if ( InWorld )
	{
		for ( TArray<ULevel*>::TConstIterator LevelIterator = InWorld->GetLevels().CreateConstIterator(); LevelIterator; ++LevelIterator )		
		{
			UModel& CurLevelModel = *( ( *LevelIterator )->Model );
			for ( TArray<FBspSurf>::TIterator SurfaceIterator( CurLevelModel.Surfs ); SurfaceIterator; ++SurfaceIterator )
			{
				FBspSurf& CurSurface = *SurfaceIterator;

				// Only modify surfaces that aren't selected and aren't already hidden
				if ( !( CurSurface.PolyFlags & PF_Selected ) && !CurSurface.IsHiddenEd() )
				{
					CurLevelModel.ModifySurf( SurfaceIterator.GetIndex(), false );
					CurSurface.bHiddenEdTemporary = true;
				}
			}
		}
	}
	RedrawLevelEditingViewports();
}


void UUnrealEdEngine::edactUnHideAll( UWorld* InWorld )
{
	// Iterate through all of the actors and unhide them
	for( FActorIterator It(InWorld); It; ++It )
	{
		AActor* Actor = *It;
		if( !FActorEditorUtils::IsABuilderBrush(Actor) && Actor->IsTemporarilyHiddenInEditor() )
		{
			// Save the actor to the transaction buffer to support undo/redo, but do
			// not call Modify, as we do not want to dirty the actor's package and
			// we're only editing temporary, transient values
			SaveToTransactionBuffer(Actor, false);
			Actor->SetIsTemporarilyHiddenInEditor( false );
		}
	}

	// Iterate through all of the BSP models and unhide them if they are already hidden
	if ( InWorld )
	{
		for ( TArray<ULevel*>::TConstIterator LevelIterator = InWorld->GetLevels().CreateConstIterator(); LevelIterator; ++LevelIterator )
		{
			UModel& CurLevelModel = *( ( *LevelIterator )->Model );
			for ( TArray<FBspSurf>::TIterator SurfaceIterator( CurLevelModel.Surfs ); SurfaceIterator; ++SurfaceIterator )
			{
				FBspSurf& CurSurface = *SurfaceIterator;
				if ( CurSurface.bHiddenEdTemporary )
				{
					CurLevelModel.ModifySurf( SurfaceIterator.GetIndex(), false );
					CurSurface.bHiddenEdTemporary = false;
				}
			}
		}
	}
	RedrawLevelEditingViewports();
}


void UUnrealEdEngine::edactHideSelectedStartup( UWorld* InWorld )
{
	// Fires ULevel::LevelDirtiedEvent when falling out of scope.
	FScopedLevelDirtied LevelDirtyCallback;

	// Iterate through all of the selected actors
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		// Set the actor to hide at editor startup, if it's not already set that way
		if ( !FActorEditorUtils::IsABuilderBrush(Actor) && !Actor->IsHiddenEd() && !Actor->IsHiddenEdAtStartup() )
		{
			Actor->Modify();
			Actor->bHiddenEd = true;
			LevelDirtyCallback.Request();
		}
	}

	if ( InWorld )
	{
		// Iterate through all of the selected BSP surfaces
		for ( TArray<ULevel*>::TConstIterator LevelIterator = InWorld->GetLevels().CreateConstIterator(); LevelIterator; ++LevelIterator )
		{
			UModel& CurLevelModel = *( ( *LevelIterator )->Model );
			for ( TArray<FBspSurf>::TIterator SurfaceIterator( CurLevelModel.Surfs ); SurfaceIterator; ++SurfaceIterator )
			{
				FBspSurf& CurSurface = *SurfaceIterator;
				
				// Set the BSP surface to hide at editor startup, if it's not already set that way
				if ( ( CurSurface.PolyFlags & PF_Selected ) && !CurSurface.IsHiddenEdAtStartup() && !CurSurface.IsHiddenEd() )
				{
					CurLevelModel.Modify();
					CurLevelModel.ModifySurf( SurfaceIterator.GetIndex(), false );
					CurSurface.PolyFlags |= PF_HiddenEd;
					LevelDirtyCallback.Request();
				}
			}
		}
	}
	RedrawLevelEditingViewports();
}


void UUnrealEdEngine::edactUnHideAllStartup( UWorld* InWorld )
{
	// Fires ULevel::LevelDirtiedEvent when falling out of scope.
	FScopedLevelDirtied LevelDirtyCallback;

	// Iterate over all actors
	for ( FActorIterator It(InWorld); It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		// If the actor is set to be hidden at editor startup, change it so that it will be shown at startup
		if ( !FActorEditorUtils::IsABuilderBrush(Actor) && Actor->IsHiddenEdAtStartup() )
		{
			Actor->Modify();
			Actor->bHiddenEd = false;
			LevelDirtyCallback.Request();
		}
	}

	if ( InWorld )
	{
		// Iterate over all BSP surfaces
		for ( TArray<ULevel*>::TConstIterator LevelIterator = InWorld->GetLevels().CreateConstIterator(); LevelIterator; ++LevelIterator )
		{
			UModel& CurLevelModel = *( ( *LevelIterator )->Model );
			for ( TArray<FBspSurf>::TIterator SurfaceIterator( CurLevelModel.Surfs ); SurfaceIterator; ++SurfaceIterator )
			{
				FBspSurf& CurSurface = *SurfaceIterator;

				// If the BSP surface is set to be hidden at editor startup, change it so that it will be shown at startup
				if ( CurSurface.IsHiddenEdAtStartup() )
				{
					CurLevelModel.Modify();
					CurLevelModel.ModifySurf( SurfaceIterator.GetIndex(), false );
					CurSurface.PolyFlags &= ~PF_HiddenEd;
					LevelDirtyCallback.Request();
				}
			}
		}
	}
	RedrawLevelEditingViewports();
}


void UUnrealEdEngine::edactUnHideSelectedStartup( UWorld* InWorld )
{
	// Fires ULevel::LevelDirtiedEvent when falling out of scope.
	FScopedLevelDirtied LevelDirtyCallback;

	// Iterate over all selected actors
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		// Mark the selected actor as showing at editor startup if it was currently set to be hidden
		if ( !FActorEditorUtils::IsABuilderBrush(Actor) && Actor->IsHiddenEdAtStartup() )
		{
			Actor->Modify();
			Actor->bHiddenEd = false;
			LevelDirtyCallback.Request();
		}
	}

	if ( InWorld )
	{
		// Iterate over all selected BSP surfaces
		for ( TArray<ULevel*>::TConstIterator LevelIterator = InWorld->GetLevels().CreateConstIterator(); LevelIterator; ++LevelIterator )
		{
			UModel& CurLevelModel = *( ( *LevelIterator )->Model );
			for ( TArray<FBspSurf>::TIterator SurfaceIterator( CurLevelModel.Surfs ); SurfaceIterator; ++SurfaceIterator )
			{
				FBspSurf& CurSurface = *SurfaceIterator;

				// Mark the selected BSP surface as showing at editor startup if it was currently set to be hidden
				if ( ( CurSurface.PolyFlags & PF_Selected ) && CurSurface.IsHiddenEdAtStartup() )
				{
					CurLevelModel.Modify();
					CurLevelModel.ModifySurf( SurfaceIterator.GetIndex(), false );
					CurSurface.PolyFlags &= ~PF_HiddenEd;
					LevelDirtyCallback.Request();
				}
			}
		}
	}
	RedrawLevelEditingViewports();
}


void UUnrealEdEngine::edactUnhideSelected( UWorld* InWorld )
{
	// Assemble a list of actors to hide.
	TArray<AActor*> ActorsToShow;
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );
		
		// Don't consider already visible actors or the builder brush
		if ( !FActorEditorUtils::IsABuilderBrush(Actor) && Actor->IsHiddenEd() )
		{
			ActorsToShow.Add( Actor );
		}
	}
	
	// Show the actors that were selected
	if ( ActorsToShow.Num() > 0 )
	{
		USelection* SelectedActors = GetSelectedActors();
		SelectedActors->Modify();

		for( int32 ActorIndex = 0 ; ActorIndex < ActorsToShow.Num() ; ++ActorIndex )
		{
			AActor* Actor = ActorsToShow[ ActorIndex ];
			
			// Save the actor to the transaction buffer to support undo/redo, but do
			// not call Modify, as we do not want to dirty the actor's package and
			// we're only editing temporary, transient values
			SaveToTransactionBuffer(Actor, false);
			Actor->SetIsTemporarilyHiddenInEditor( false );
		}
	}

	// Iterate through all of the BSP models and show any that were selected
	if ( InWorld )
	{
		for ( TArray<ULevel*>::TConstIterator LevelIterator = InWorld->GetLevels().CreateConstIterator(); LevelIterator; ++LevelIterator )
		{
			UModel& CurLevelModel = *( ( *LevelIterator )->Model );
			for ( TArray<FBspSurf>::TIterator SurfaceIterator( CurLevelModel.Surfs ); SurfaceIterator; ++SurfaceIterator )
			{
				FBspSurf& CurSurface = *SurfaceIterator;
				if ( ( CurSurface.PolyFlags & PF_Selected ) && !CurSurface.IsHiddenEd() )
				{
					CurLevelModel.ModifySurf( SurfaceIterator.GetIndex(), false );
					CurSurface.bHiddenEdTemporary = false;
				}
			}
		}
	}
	RedrawLevelEditingViewports();
}

void UUnrealEdEngine::CreateBSPVisibilityMap(UWorld* InWorld, TMap<AActor*, TArray<int32>>& OutBSPMap, bool& bOutAllVisible  )
{
	// Start out true, we do not know otherwise.
	bOutAllVisible = true;

	// Iterate through all of the BSP models and any that are visible to the list.
	if ( InWorld )
	{
		for ( TArray<ULevel*>::TConstIterator LevelIterator = InWorld->GetLevels().CreateConstIterator(); LevelIterator; ++LevelIterator )
		{
			UModel& CurLevelModel = *( ( *LevelIterator )->Model );
			for ( TArray<FBspSurf>::TIterator SurfaceIterator( CurLevelModel.Surfs ); SurfaceIterator; ++SurfaceIterator )
			{
				FBspSurf& CurSurface = *SurfaceIterator;

				// If the surface is visible, we will want to add it to the map.
				if ( CurSurface.bHiddenEdTemporary == false )
				{
					// First check if we have already added our surface's brush actor to the map.
					TArray<int32>* BrushPolyList = OutBSPMap.Find(CurSurface.Actor);
					if(BrushPolyList)
					{
						// We found the brush actor on the list, so add our polygon ID to the list.
						BrushPolyList->Add(CurSurface.iBrushPoly);
					}
					else
					{
						// The brush actor has not been added to the map, add it.
						OutBSPMap.Add(CurSurface.Actor, TArray<int32>());

						// Grab the list out and add our brush poly to it.
						BrushPolyList = OutBSPMap.Find(CurSurface.Actor);
						BrushPolyList->Add(CurSurface.iBrushPoly);

					}
				}
				else
				{
					// We found one that is not visible, so they are not ALL visible. We will continue to map out geometry to come up with a complete Visibility map.
					bOutAllVisible = false;
				}
			}
		}
	}
}

void UUnrealEdEngine::MakeBSPMapVisible(const TMap<AActor*, TArray<int32>>& InBSPMap, UWorld* InWorld )
{
	// Iterate through all of the BSP models and show any that were selected
	if ( InWorld )
	{
		for ( TArray<ULevel*>::TConstIterator LevelIterator = InWorld->GetLevels().CreateConstIterator(); LevelIterator; ++LevelIterator )
		{
			UModel& CurLevelModel = *( ( *LevelIterator )->Model );
			for ( TArray<FBspSurf>::TIterator SurfaceIterator( CurLevelModel.Surfs ); SurfaceIterator; ++SurfaceIterator )
			{
				FBspSurf& CurSurface = *SurfaceIterator;

				// Check if we can find the surface's actor in the map.
				const TArray<int32>* BrushPolyList = InBSPMap.Find(CurSurface.Actor);
				if(BrushPolyList)
				{
					// We have the list of brush polygons that are visible, check if the current one is on the list.
					if(BrushPolyList->FindByKey(CurSurface.iBrushPoly))
					{
						// Make the surface visible.
						CurSurface.bHiddenEdTemporary = false;
					}
					else
					{
						// The brush poly was not in the map, so it should be hidden.
						CurSurface.bHiddenEdTemporary = true;
					}
				}
				else
				{
					// There was no brush poly list, that means no polygon on this brush was visible, make this surface hidden.
					CurSurface.bHiddenEdTemporary = true;
				}
			}
		}
	}

}

AActor* UUnrealEdEngine::GetDesiredAttachmentState(TArray<AActor*>& OutNewChildren)
{
	// Get the selection set (first one will be the new base)
	AActor* NewBase = NULL;
	OutNewChildren.Empty();
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* SelectedActor = Cast<AActor>(*It);
		if(SelectedActor)
		{
			OutNewChildren.AddUnique(SelectedActor);
		}
	}

	// Last element of the array becomes new base
	if(OutNewChildren.Num() > 0)
	{
		NewBase = OutNewChildren.Pop();
	}

	return NewBase;
}

void UUnrealEdEngine::AttachSelectedActors()
{
	const FScopedTransaction Transaction( NSLOCTEXT("Editor", "UndoAction_PerformAttachment", "Attach actors") );

	// Get what we want attachment to be
	TArray<AActor*> NewChildren;
	AActor* NewBase = GetDesiredAttachmentState(NewChildren);
	if(NewBase && NewBase->GetRootComponent() && (NewChildren.Num() > 0))
	{
	
		// Do the actual base change
		for(int32 ChildIdx=0; ChildIdx<NewChildren.Num(); ChildIdx++)
		{
			AActor* Child = NewChildren[ChildIdx];
			if( Child )
			{
				ParentActors( NewBase, Child, NAME_None );
			}
		}

		RedrawLevelEditingViewports();
	}
}

void UUnrealEdEngine::edactSelectAll( UWorld* InWorld )
{
	// If there are a lot of actors to process, pop up a warning "are you sure?" box
	int32 NumActors = InWorld->GetActorCount();
	bool bShowProgress = false;
	if( NumActors >= EditorActorSelectionDefs::MaxActorsToSelectBeforeWarning )
	{
		bShowProgress = true;

		const FText ConfirmText = FText::Format( NSLOCTEXT("UnrealEd", "Warning_ManyActorsForSelect", "There are {0} actors in the world. Are you sure you want to select them all?" ), FText::AsNumber(NumActors) );

		FSuppressableWarningDialog::FSetupInfo Info( ConfirmText, NSLOCTEXT("UnrealEd", "Warning_ManyActors", "Warning: Many Actors" ), "Warning_ManyActors" );
		Info.ConfirmText = NSLOCTEXT("ModalDialogs", "SelectAllConfirm", "Select All");
		Info.CancelText = NSLOCTEXT("ModalDialogs", "SelectAllCancel", "Cancel");

		FSuppressableWarningDialog ManyActorsWarning( Info );
		if( ManyActorsWarning.ShowModal() == FSuppressableWarningDialog::Cancel )
		{
			return;
		}
	}

	if( bShowProgress )
	{
		GWarn->BeginSlowTask( LOCTEXT("BeginSelectAllActorsTaskStatusMessage", "Selecting All Actors"), true);
	}

	// Add all selected actors' layer name to the LayerArray.
	USelection* SelectedActors = GetSelectedActors();

	SelectedActors->BeginBatchSelectOperation();

	SelectedActors->Modify();

	for( FActorIterator It(InWorld); It; ++It )
	{
		AActor* Actor = *It;
		if( !Actor->IsSelected() && !Actor->IsHiddenEd() )
		{
			SelectActor( Actor, 1, 0 );
		}
	}

	// Iterate through all of the BSP models and select them if they are not hidden
	if ( InWorld )
	{
		for ( TArray<ULevel*>::TConstIterator LevelIterator = InWorld->GetLevels().CreateConstIterator(); LevelIterator; ++LevelIterator )
		{
			UModel& CurLevelModel = *( ( *LevelIterator )->Model );
			for ( TArray<FBspSurf>::TIterator SurfaceIterator( CurLevelModel.Surfs ); SurfaceIterator; ++SurfaceIterator )
			{
				FBspSurf& CurSurface = *SurfaceIterator;
				if ( !CurSurface.IsHiddenEd() )
				{
					CurLevelModel.ModifySurf( SurfaceIterator.GetIndex(), false );
					CurSurface.PolyFlags |= PF_Selected;
				}
			}
		}
	}

	SelectedActors->EndBatchSelectOperation();

	NoteSelectionChange();

	if( bShowProgress )
	{
		GWarn->EndSlowTask( );
	}
}


void UUnrealEdEngine::edactSelectInvert( UWorld* InWorld )
{
	// If there are a lot of actors to process, pop up a warning "are you sure?" box
	int32 NumActors = InWorld->GetActorCount();
	bool bShowProgress = false;
	if( NumActors >= EditorActorSelectionDefs::MaxActorsToSelectBeforeWarning )
	{
		bShowProgress = true;
		const FText ConfirmText = FText::Format( NSLOCTEXT("UnrealEd", "Warning_ManyActorsForInvertSelect", "There are {0} actors in the world. Are you sure you want to invert selection on them all?" ), FText::AsNumber(NumActors) );

		FSuppressableWarningDialog::FSetupInfo Info ( ConfirmText, NSLOCTEXT("UnrealEd", "Warning_ManyActors", "Warning: Many Actors" ), "Warning_ManyActors" );
		Info.ConfirmText = NSLOCTEXT("ModalDialogs", "InvertSelectionConfirm", "Invert Selection");
		Info.CancelText = NSLOCTEXT("ModalDialogs", "InvertSelectionCancel", "Cancel");

		FSuppressableWarningDialog ManyActorsWarning( Info );
		if( ManyActorsWarning.ShowModal() == FSuppressableWarningDialog::Cancel )
		{
			return;
		}
	}

	if( bShowProgress )
	{
		GWarn->BeginSlowTask( LOCTEXT("BeginInvertingActorSelectionTaskMessage", "Inverting Selected Actors"), true);
	}

	USelection* SelectedActors = GetSelectedActors();
	SelectedActors->BeginBatchSelectOperation();

	SelectedActors->Modify();

	// Iterate through all of the actors and select them if they are not currently selected (and not hidden)
	// or deselect them if they are currently selected

	// Turn off Grouping during this process to avoid double toggling of selected actors via group selection
	const bool bGroupingActiveSaved = bGroupingActive;
	bGroupingActive = false;
	for( FActorIterator It(InWorld); It; ++It )
	{
		AActor* Actor = *It;
		if( !FActorEditorUtils::IsABuilderBrush(Actor) && !Actor->IsHiddenEd() )
		{
			SelectActor( Actor, !Actor->IsSelected(), false );
		}
	}
	// Restore bGroupingActive to its original value
	bGroupingActive = bGroupingActiveSaved;

	// Iterate through all of the BSP models and select them if they are not currently selected (and not hidden)
	// or deselect them if they are currently selected
	if ( InWorld )
	{
		for ( TArray<ULevel*>::TConstIterator LevelIterator = InWorld->GetLevels().CreateConstIterator(); LevelIterator; ++LevelIterator )
		{
			UModel& CurLevelModel = *( ( *LevelIterator )->Model );
			for ( TArray<FBspSurf>::TIterator SurfaceIterator( CurLevelModel.Surfs ); SurfaceIterator; ++SurfaceIterator )
			{
				FBspSurf& CurSurface = *SurfaceIterator;
				if ( !CurSurface.IsHiddenEd() )
				{
					CurLevelModel.ModifySurf( SurfaceIterator.GetIndex(), false );
					CurSurface.PolyFlags ^= PF_Selected;
				}
			}
		}
	}

	SelectedActors->EndBatchSelectOperation();

	NoteSelectionChange();

	if( bShowProgress )
	{
		GWarn->EndSlowTask( );
	}
}


void UUnrealEdEngine::edactSelectOfClass( UWorld* InWorld, UClass* Class )
{
	USelection* SelectedActors = GetSelectedActors();
	SelectedActors->BeginBatchSelectOperation();

	SelectedActors->Modify();

	for( FActorIterator It(InWorld); It; ++It )
	{
		AActor* Actor = *It;
		if( Actor->GetClass()==Class && !Actor->IsSelected() && !Actor->IsHiddenEd() )
		{
			// Selection by class not permitted for actors belonging to prefabs.
			// Selection by class not permitted for builder brushes.
			if ( !FActorEditorUtils::IsABuilderBrush(Actor) )
			{
				SelectActor( Actor, 1, 0 );
			}
		}
	}

	SelectedActors->EndBatchSelectOperation();
	NoteSelectionChange();
}


void UUnrealEdEngine::edactSelectOfClassAndArchetype( UWorld* InWorld, const UClass* InClass, const UObject* InArchetype )
{
	USelection* SelectedActors = GetSelectedActors();
	SelectedActors->BeginBatchSelectOperation();

	SelectedActors->Modify();

	// Select all actors with of the provided class and archetype, assuming they aren't already selected, 
	// aren't hidden in the editor, aren't a member of a prefab, and aren't builder brushes
	for( FActorIterator ActorIter(InWorld); ActorIter; ++ActorIter )
	{
		AActor* CurActor = *ActorIter;
		if ( CurActor->GetClass() == InClass && CurActor->GetArchetype() == InArchetype && !CurActor->IsSelected() 
			&& !CurActor->IsHiddenEd() && !FActorEditorUtils::IsABuilderBrush(CurActor) )
		{
			SelectActor( CurActor, true, false );
		}
	}

	SelectedActors->EndBatchSelectOperation();
	NoteSelectionChange();
}


void UUnrealEdEngine::edactSelectSubclassOf( UWorld* InWorld, UClass* Class )
{
	USelection* SelectedActors = GetSelectedActors();
	SelectedActors->BeginBatchSelectOperation();

	SelectedActors->Modify();

	for( FActorIterator It(InWorld); It; ++It )
	{
		AActor* Actor = *It;
		if( !Actor->IsSelected() && !Actor->IsHiddenEd() && Actor->GetClass()->IsChildOf(Class) )
		{
			// Selection by class not permitted for actors belonging to prefabs.
			// Selection by class not permitted for builder brushes.
			if ( !FActorEditorUtils::IsABuilderBrush(Actor) )
			{
				SelectActor( Actor, 1, 0 );
			}
		}
	}

	SelectedActors->EndBatchSelectOperation();
	NoteSelectionChange();
}


void UUnrealEdEngine::edactSelectDeleted( UWorld* InWorld )
{
	USelection* SelectedActors = GetSelectedActors();
	SelectedActors->BeginBatchSelectOperation();

	SelectedActors->Modify();

	bool bSelectionChanged = false;
	for( FActorIterator It(InWorld); It; ++It )
	{
		AActor* Actor = *It;
		if( !Actor->IsSelected() && !Actor->IsHiddenEd() )
		{
			if( Actor->IsPendingKill() )
			{
				bSelectionChanged = true;
				SelectActor( Actor, 1, 0 );
			}
		}
	}

	SelectedActors->EndBatchSelectOperation();

	if ( bSelectionChanged )
	{
		NoteSelectionChange();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Select matching static meshes.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {

/**
 * Information about an actor and its static mesh.
 */
class FStaticMeshActor
{
public:
	/** Non-NULL if the actor is a static mesh. */
	AStaticMeshActor* StaticMeshActor;
	/** Non-NULL if the actor has a static mesh. */
	UStaticMesh* StaticMesh;

	FStaticMeshActor()
		: StaticMeshActor(NULL)
		, StaticMesh(NULL)
	{}

	bool IsStaticMeshActor() const
	{
		return StaticMeshActor != NULL;
	}

	bool HasStaticMesh() const
	{
		return StaticMesh != NULL;
	}

	/**
	 * Extracts the static mesh information from the specified actor.
	 */
	static bool GetStaticMeshInfoFromActor(AActor* Actor, FStaticMeshActor& OutStaticMeshActor)
	{
		OutStaticMeshActor.StaticMeshActor = Cast<AStaticMeshActor>( Actor );

		if( OutStaticMeshActor.IsStaticMeshActor() )
		{
			if ( OutStaticMeshActor.StaticMeshActor->StaticMeshComponent )
			{
				OutStaticMeshActor.StaticMesh = OutStaticMeshActor.StaticMeshActor->StaticMeshComponent->StaticMesh;
			}
		}
		return OutStaticMeshActor.HasStaticMesh();
	}
};

} // namespace


void UUnrealEdEngine::edactSelectMatchingStaticMesh( bool bAllClasses )
{
	TArray<FStaticMeshActor> StaticMeshActors;

	TArray<UWorld*> WorldList;
	// Make a list of selected actors with static meshes.
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		FStaticMeshActor ActorInfo;
		if ( FStaticMeshActor::GetStaticMeshInfoFromActor( Actor, ActorInfo ) )
		{
			if ( ActorInfo.IsStaticMeshActor() )
			{
				StaticMeshActors.Add( ActorInfo );
				WorldList.AddUnique( Actor->GetWorld() );	
			}				
		}
	}
	if( WorldList.Num() == 0 )
	{
		UE_LOG(LogEditorActor, Log, TEXT("No worlds found in edactSelectMatchingStaticMesh") );
		return;
	}
	// Make sure we have only 1 valid world 
	check(WorldList.Num() == 1);
	USelection* SelectedActors = GetSelectedActors();
	SelectedActors->BeginBatchSelectOperation();
	SelectedActors->Modify();

	// Loop through all non-hidden actors in visible levels, selecting those that have one of the
	// static meshes in the list.
	for( FActorIterator It(WorldList[0]); It; ++It )
	{
		AActor* Actor = *It;
		if ( !Actor->IsHiddenEd() )
		{
			FStaticMeshActor ActorInfo;
			if ( FStaticMeshActor::GetStaticMeshInfoFromActor( Actor, ActorInfo ) )
			{
				bool bSelectActor = false;
				if ( !bSelectActor && (bAllClasses || ActorInfo.IsStaticMeshActor()) )
				{
					for ( int32 i = 0 ; i < StaticMeshActors.Num() ; ++i )
					{
						if ( StaticMeshActors[i].StaticMesh == ActorInfo.StaticMesh )
						{
							bSelectActor = true;
							break;
						}
					}
				}

				if ( bSelectActor )
				{
					SelectActor( Actor, true, false );
				}
			}
		}
	}

	SelectedActors->EndBatchSelectOperation();
	NoteSelectionChange();
}


void UUnrealEdEngine::edactSelectMatchingSkeletalMesh(bool bAllClasses)
{
	TArray<USkeletalMesh*> SelectedMeshes;
	bool bSelectSkelMeshActors = false;
	bool bSelectPawns = false;

	TArray<UWorld*> WorldList;
	// Make a list of skeletal meshes of selected actors, and note what classes we have selected.
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		// Look for SkelMeshActor
		ASkeletalMeshActor* SkelMeshActor = Cast<ASkeletalMeshActor>(Actor);
		if(SkelMeshActor && SkelMeshActor->SkeletalMeshComponent)
		{
			bSelectSkelMeshActors = true;
			SelectedMeshes.AddUnique(SkelMeshActor->SkeletalMeshComponent->SkeletalMesh);
			WorldList.AddUnique(Actor->GetWorld());			
		}

		// Look for Pawn
		APawn* Pawn = Cast<APawn>(Actor);
		if (Pawn)
		{
			USkeletalMeshComponent* PawnSkeletalMesh = Pawn->FindComponentByClass<USkeletalMeshComponent>();
			if (PawnSkeletalMesh)
			{
				bSelectPawns = true;
				SelectedMeshes.AddUnique(PawnSkeletalMesh->SkeletalMesh);
				WorldList.AddUnique(Actor->GetWorld());
			}
		}
	}
	if( WorldList.Num() == 0 )
	{
		UE_LOG(LogEditorActor, Log, TEXT("No worlds found in edactSelectMatchingSkeletalMesh") );
		return;
	}
	// Make sure we have only 1 valid world 
	check( WorldList.Num() == 1 );
	// If desired, select all class types
	if(bAllClasses)
	{
		bSelectSkelMeshActors = true;
		bSelectPawns = true;
	}

	USelection* SelectedActors = GetSelectedActors();
	SelectedActors->BeginBatchSelectOperation();
	SelectedActors->Modify();

	// Loop through all non-hidden actors in visible levels, selecting those that have one of the skeletal meshes in the list.
	for( FActorIterator It(WorldList[0]); It; ++It )
	{
		AActor* Actor = *It;
		if ( !Actor->IsHiddenEd() )
		{
			bool bSelectActor = false;

			if(bSelectSkelMeshActors)
			{
				ASkeletalMeshActor* SkelMeshActor = Cast<ASkeletalMeshActor>(Actor);
				if( SkelMeshActor && 
					SkelMeshActor->SkeletalMeshComponent && 
					SelectedMeshes.Contains(SkelMeshActor->SkeletalMeshComponent->SkeletalMesh) )
				{
					bSelectActor = true;
				}
			}

			if(bSelectPawns)
			{
				APawn* Pawn = Cast<APawn>(Actor);
				if (Pawn)
				{
					USkeletalMeshComponent* PawnSkeletalMesh = Pawn->FindComponentByClass<USkeletalMeshComponent>();
					if (PawnSkeletalMesh && SelectedMeshes.Contains(PawnSkeletalMesh->SkeletalMesh) )
					{
						bSelectActor = true;
					}
				}
			}

			if ( bSelectActor )
			{
				SelectActor( Actor, true, false );
			}
		}
	}

	SelectedActors->EndBatchSelectOperation();
	NoteSelectionChange();
}


void UUnrealEdEngine::edactSelectMatchingMaterial()
{
	// Set for fast lookup of used materials.
	TSet<UMaterialInterface*> MaterialsInSelection;

	TArray<UWorld*> WorldList;
	// For each selected actor, find all the materials used by this actor.
	for ( FSelectionIterator ActorItr( GetSelectedActorIterator() ) ; ActorItr ; ++ActorItr )
	{
		AActor* CurrentActor = Cast<AActor>( *ActorItr );

		if( CurrentActor )
		{
			// Find the materials by iterating over every primitive component.
			TArray<UPrimitiveComponent*> PrimitiveComponents;
			CurrentActor->GetComponents(PrimitiveComponents);

			for (int32 ComponentIdx = 0; ComponentIdx < PrimitiveComponents.Num(); ComponentIdx++)
			{
				UPrimitiveComponent* CurrentComponent = PrimitiveComponents[ComponentIdx];
				TArray<UMaterialInterface*> UsedMaterials;
				CurrentComponent->GetUsedMaterials( UsedMaterials );
				MaterialsInSelection.Append( UsedMaterials );
				WorldList.AddUnique( CurrentActor->GetWorld() );
			}
		}
	}

	if( WorldList.Num() == 0 )
	{
		UE_LOG(LogEditorActor, Log, TEXT("No worlds found in edactSelectMatchingMaterial") );
		return;
	}
	// Make sure we have only 1 valid world 
	check( WorldList.Num() == 1 );

	USelection* SelectedActors = GetSelectedActors();
	SelectedActors->BeginBatchSelectOperation();
	SelectedActors->Modify();	

	// Now go over every actor and see if any of the actors are using any of the materials that 
	// we found above.
	for( FActorIterator ActorIt(WorldList[0]); ActorIt; ++ActorIt )
	{
		AActor* Actor = *ActorIt;

		// Do not bother checking hidden actors
		if( !Actor->IsHiddenEd() )
		{
			TArray<UPrimitiveComponent*> PrimitiveComponents;
			Actor->GetComponents(PrimitiveComponents);

			const int32 NumComponents = PrimitiveComponents.Num();
			for (int32 ComponentIndex = 0; ComponentIndex < NumComponents; ++ComponentIndex )
			{
				UPrimitiveComponent* CurrentComponent = PrimitiveComponents[ComponentIndex];

				TArray<UMaterialInterface*> UsedMaterials;
				CurrentComponent->GetUsedMaterials( UsedMaterials );
				const int32 NumMaterials = UsedMaterials.Num();
				// Iterate over every material we found so far and see if its in the list of materials used by selected actors.
				for( int32 MatIndex = 0; MatIndex < NumMaterials; ++MatIndex )
				{
					UMaterialInterface* Material = UsedMaterials[ MatIndex ];
					// Is this material used by currently selected actors?
					if( MaterialsInSelection.Find( Material ) )
					{
						SelectActor( Actor, true, false );
						// We dont need to continue searching as this actor has already been selected
						MatIndex = NumMaterials;
						ComponentIndex = NumComponents;
					}
				}
			}
		}
	}

	SelectedActors->EndBatchSelectOperation();
	NoteSelectionChange();
}


void UUnrealEdEngine::edactSelectMatchingEmitter()
{
	TArray<UParticleSystem*> SelectedParticleSystemTemplates;

	TArray<UWorld*> WorldList;
	// Check all of the currently selected actors to find the relevant particle system templates to use to match
	for ( FSelectionIterator SelectedIterator( GetSelectedActorIterator() ) ; SelectedIterator ; ++SelectedIterator )
	{
		AActor* Actor = static_cast<AActor*>( *SelectedIterator );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		AEmitter* Emitter = Cast<AEmitter>( Actor );
		
		if ( Emitter && Emitter->ParticleSystemComponent && Emitter->ParticleSystemComponent->Template )
		{
			SelectedParticleSystemTemplates.AddUnique( Emitter->ParticleSystemComponent->Template );
			WorldList.AddUnique( Actor->GetWorld() );
		}
	}

	if( WorldList.Num() == 0 )
	{
		UE_LOG(LogEditorActor, Log, TEXT("No worlds found in edactSelectMatchingEmitter") );
		return;
	}
	// Make sure we have only 1 valid world 
	check( WorldList.Num() == 1 );

	USelection* SelectedActors = GetSelectedActors();
	SelectedActors->BeginBatchSelectOperation();
	SelectedActors->Modify();
	// Iterate over all of the non-hidden actors, selecting those who have a particle system template that matches one from the previously-found list
	for( FActorIterator ActorIterator(WorldList[0]); ActorIterator; ++ActorIterator )
	{
		AActor* Actor = *ActorIterator;
		if ( !Actor->IsHiddenEd() )
		{
			AEmitter* ActorAsEmitter = Cast<AEmitter>( Actor );
			if ( ActorAsEmitter && ActorAsEmitter->ParticleSystemComponent && SelectedParticleSystemTemplates.Contains( ActorAsEmitter->ParticleSystemComponent->Template ) )
			{
				SelectActor( Actor, true, false );
			}
		}
	}

	SelectedActors->EndBatchSelectOperation();
	NoteSelectionChange();
}



void UUnrealEdEngine::edactSelectRelevantLights( UWorld* InWorld )
{
	TArray<ALight*> RelevantLightList;
	// Make a list of selected actors with static meshes.
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if (Actor->GetLevel()->IsCurrentLevel() )
		{
			TArray<UPrimitiveComponent*> PrimitiveComponents;
			Actor->GetComponents(PrimitiveComponents);

			// Gather static lighting info from each of the actor's components.
			for(int32 ComponentIndex = 0;ComponentIndex < PrimitiveComponents.Num();ComponentIndex++)
			{
				UPrimitiveComponent* Primitive = PrimitiveComponents[ComponentIndex];
				if (Primitive->IsRegistered())
				{
					TArray<const ULightComponent*> RelevantLightComponents;
					InWorld->Scene->GetRelevantLights(Primitive, &RelevantLightComponents);

					for (int32 LightComponentIndex = 0; LightComponentIndex < RelevantLightComponents.Num(); LightComponentIndex++)
					{
						const ULightComponent* LightComponent = RelevantLightComponents[LightComponentIndex];
						ALight* LightOwner = Cast<ALight>(LightComponent->GetOwner());
						if (LightOwner)
						{
							RelevantLightList.AddUnique(LightOwner);
						}
					}
				}
			}
		}
	}

	USelection* SelectedActors = GetSelectedActors();
	SelectedActors->BeginBatchSelectOperation();
	SelectedActors->Modify();

	SelectNone( false, true );

	UE_LOG(LogEditorActor, Log, TEXT("Found %d relevant lights!"), RelevantLightList.Num());
	for (int32 LightIdx = 0; LightIdx < RelevantLightList.Num(); LightIdx++)
	{
		ALight* Light = RelevantLightList[LightIdx];
		if (Light)
		{
			SelectActor(Light, true, false);
			UE_LOG(LogEditorActor, Log, TEXT("\t%s"), *(Light->GetPathName()));
		}
	}

	SelectedActors->EndBatchSelectOperation();
	NoteSelectionChange();
}


void UUnrealEdEngine::edactAlignOrigin()
{
	// Fires ULevel::LevelDirtiedEvent when falling out of scope.
	FScopedLevelDirtied LevelDirtyCallback;

	// Apply transformations to all selected brushes.
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		ABrush* Brush = Cast< ABrush >( Actor );
		if ( Brush )
		{
			LevelDirtyCallback.Request();

			Brush->PreEditChange(NULL);
			Brush->Modify();

			//Snap the location of the brush to the grid
			FVector BrushLocation = Brush->GetActorLocation();
			BrushLocation.X = FMath::RoundToFloat( BrushLocation.X  / GetGridSize() ) * GetGridSize();
			BrushLocation.Y = FMath::RoundToFloat( BrushLocation.Y  / GetGridSize() ) * GetGridSize();
			BrushLocation.Z = FMath::RoundToFloat( BrushLocation.Z  / GetGridSize() ) * GetGridSize();
			Brush->SetActorLocation(BrushLocation, false);

			//Update EditorMode locations to match the new brush location
			FEditorModeTools& Tools = GEditorModeTools();
			Tools.SetPivotLocation( Brush->GetActorLocation(), true );

			Brush->Brush->BuildBound();
			Brush->PostEditChange();
		}
	}
}


void UUnrealEdEngine::edactAlignVertices()
{
	// Fires ULevel::LevelDirtiedEvent when falling out of scope.
	FScopedLevelDirtied LevelDirtyCallback;
	
	//Before aligning verts, align the origin with the grid
	edactAlignOrigin();

	// Apply transformations to all selected brushes.
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );
		ABrush* Brush = Cast< ABrush >( Actor );
		if ( Brush )
		{
			LevelDirtyCallback.Request();

			Brush->PreEditChange(NULL);
			Brush->Modify();
			FVector BrushLocation = Brush->GetActorLocation();

			// Snap each vertex in the brush to an integer grid.
			UPolys* Polys = Brush->Brush->Polys;
			for( int32 PolyIdx=0; PolyIdx<Polys->Element.Num(); PolyIdx++ )
			{
				FPoly* Poly = &Polys->Element[PolyIdx];
				for( int32 VertIdx=0; VertIdx<Poly->Vertices.Num(); VertIdx++ )
				{
					// Snap each vertex to the nearest grid.
					Poly->Vertices[VertIdx].X = FMath::RoundToFloat( ( Poly->Vertices[VertIdx].X + BrushLocation.X )  / GetGridSize() ) * GetGridSize() - BrushLocation.X;
					Poly->Vertices[VertIdx].Y = FMath::RoundToFloat( ( Poly->Vertices[VertIdx].Y + BrushLocation.Y )  / GetGridSize() ) * GetGridSize() - BrushLocation.Y;
					Poly->Vertices[VertIdx].Z = FMath::RoundToFloat( ( Poly->Vertices[VertIdx].Z + BrushLocation.Z )  / GetGridSize() ) * GetGridSize() - BrushLocation.Z;
				}

				// If the snapping resulted in an off plane polygon, triangulate it to compensate.
				if( !Poly->IsCoplanar() || !Poly->IsConvex() )
				{

					FPoly BadPoly = *Poly;
					// Remove the bad poly
					Polys->Element.RemoveAt( PolyIdx );

					// Triangulate the bad poly
					TArray<FPoly> Triangles;
					if ( BadPoly.Triangulate( Brush, Triangles ) > 0 )
					{
						// Add all new triangles to the brush
						for( int32 TriIdx = 0 ; TriIdx < Triangles.Num() ; ++TriIdx )
						{
							Polys->Element.Add( Triangles[TriIdx] );
						}
					}
					
					PolyIdx = -1;
				}
				else
				{
					if( RecomputePoly( Brush, &Polys->Element[PolyIdx] ) == -2 )
					{
						PolyIdx = -1;
					}

					// Determine if we are in geometry edit mode.
					if ( GEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_Geometry) )
					{
						// If we are in geometry mode, go through the list of geometry objects
						// and find our current brush and update its source data as it might have changed 
						// in RecomputePoly
						FEdModeGeometry* GeomMode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Geometry);
						FEdModeGeometry::TGeomObjectIterator GeomModeIt = GeomMode->GeomObjectItor();
						for( ; GeomModeIt; ++GeomModeIt )
						{
							FGeomObject* Object = *GeomModeIt;
							if( Object->GetActualBrush() == Brush )
							{
								// We found our current brush, update the geometry object's data
								Object->GetFromSource();
								break;
							}
						}
					}
				}
			}

			Brush->Brush->BuildBound();

			Brush->PostEditChange();
		}
	}
}

PRAGMA_ENABLE_OPTIMIZATION

#undef LOCTEXT_NAMESPACE

