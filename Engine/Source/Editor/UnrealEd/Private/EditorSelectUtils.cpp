// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "UnrealEd.h"
#include "ScopedTransaction.h"
#include "LevelUtils.h"
#include "Editor/StatsViewer/Public/StatsViewerModule.h"
#include "SnappingUtils.h"
#include "MessageLog.h"

#define LOCTEXT_NAMESPACE "EditorSelectUtils"

DEFINE_LOG_CATEGORY_STATIC(LogEditorSelectUtils, Log, All);

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/


// Click flags.
enum EViewportClick
{
	CF_MOVE_ACTOR	= 1,	// Set if the actors have been moved since first click
	CF_MOVE_TEXTURE = 2,	// Set if textures have been adjusted since first click
	CF_MOVE_ALL     = (CF_MOVE_ACTOR | CF_MOVE_TEXTURE),
};

/*-----------------------------------------------------------------------------
   Change transacting.
-----------------------------------------------------------------------------*/


void UUnrealEdEngine::NoteActorMovement()
{
	if( !GUndo && !(GEditor->ClickFlags & CF_MOVE_ACTOR) )
	{
		GEditor->ClickFlags |= CF_MOVE_ACTOR;

		const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "ActorMovement", "Actor Movement") );
		GEditorModeTools().Snapping=0;
		
		AActor* SelectedActor = NULL;
		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			SelectedActor = Actor;
			break;
		}

		if( SelectedActor == NULL )
		{
			USelection* SelectedActors = GetSelectedActors();
			SelectedActors->Modify();
			SelectActor( GWorld->GetBrush(), true, true );
		}

		// Look for an actor that requires snapping.
		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			GEditorModeTools().Snapping = 1;
			break;
		}

		TSet<AGroupActor*> GroupActors;

		// Modify selected actors.
		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			Actor->Modify();

			if (GEditor->bGroupingActive)
			{
				// if this actor is in a group, add the GroupActor into a list to be modified shortly
				AGroupActor* ActorLockedRootGroup = AGroupActor::GetRootForActor(Actor, true);
				if (ActorLockedRootGroup != nullptr)
				{
					GroupActors.Add(ActorLockedRootGroup);
				}
			}

			ABrush* Brush = Cast< ABrush >( Actor );
			if ( Brush )
			{
				if( Brush->Brush )
				{
					Brush->Brush->Polys->Element.ModifyAllItems();
				}
			}
		}

		// Modify unique group actors
		for (auto* GroupActor : GroupActors)
		{
			GroupActor->Modify();
		}
	}
}

void UUnrealEdEngine::FinishAllSnaps()
{
	if(!IsRunningCommandlet())
	{
		if( ClickFlags & CF_MOVE_ACTOR )
		{
			ClickFlags &= ~CF_MOVE_ACTOR;

			for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
			{
				AActor* Actor = static_cast<AActor*>( *It );
				checkSlow( Actor->IsA(AActor::StaticClass()) );

				Actor->Modify();
				Actor->InvalidateLightingCache();
				Actor->PostEditMove( true );
			}
		}
	}
}


void UUnrealEdEngine::Cleanse( bool ClearSelection, bool Redraw, const FText& Reason )
{
	if (GIsRunning)
	{
		FMessageLog("MapCheck").NewPage(LOCTEXT("MapCheck", "Map Check"));

		FMessageLog("LightingResults").NewPage(LOCTEXT("LightingBuildNewLogPage", "Lighting Build"));

		FStatsViewerModule& StatsViewerModule = FModuleManager::Get().LoadModuleChecked<FStatsViewerModule>(TEXT("StatsViewer"));
		StatsViewerModule.Clear();
	}

	Super::Cleanse( ClearSelection, Redraw, Reason );
}


FVector UUnrealEdEngine::GetPivotLocation()
{
	return GEditorModeTools().PivotLocation;
}


void UUnrealEdEngine::SetPivot( FVector NewPivot, bool bSnapPivotToGrid, bool bIgnoreAxis, bool bAssignPivot/*=false*/ )
{
	FEditorModeTools& EditorModeTools = GEditorModeTools();

	if( !bIgnoreAxis )
	{
		// Don't stomp on orthonormal axis.
		if( NewPivot.X==0 ) NewPivot.X=EditorModeTools.PivotLocation.X;
		if( NewPivot.Y==0 ) NewPivot.Y=EditorModeTools.PivotLocation.Y;
		if( NewPivot.Z==0 ) NewPivot.Z=EditorModeTools.PivotLocation.Z;
	}

	// Set the pivot.
	//EditorModeTools.CachedLocation	= NewPivot;	// Don't set the cached location, this is our pre-move point
	EditorModeTools.PivotLocation		= NewPivot;
	EditorModeTools.SnappedLocation		= NewPivot;
	EditorModeTools.GridBase			= FVector::ZeroVector;

	if( bSnapPivotToGrid )
	{
		FRotator DummyRotator(0,0,0);
		FSnappingUtils::SnapToBSPVertex( EditorModeTools.SnappedLocation, EditorModeTools.GridBase, DummyRotator );
		EditorModeTools.PivotLocation = EditorModeTools.SnappedLocation;
	}

	// Check all actors.
	int32 Count=0, SnapCount=0;

	//default to using the x axis for the translate rotate widget
	EditorModeTools.TranslateRotateXAxisAngle = 0.0f;
	FVector TranslateRotateWidgetWorldXAxis;

	AActor* LastSelectedActor = NULL;
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if (Count==0)
		{
			TranslateRotateWidgetWorldXAxis = Actor->ActorToWorld().TransformVector(FVector(1.0f, 0.0f, 0.0f));
			//get the xy plane project of this vector
			TranslateRotateWidgetWorldXAxis.Z = 0.0f;
			if (!TranslateRotateWidgetWorldXAxis.Normalize())
			{
				TranslateRotateWidgetWorldXAxis = FVector(1.0f, 0.0f, 0.0f);
			}
		}

		LastSelectedActor = Actor;
		++Count;
		++SnapCount;
	}
	
	if( bAssignPivot && LastSelectedActor && GEditor->bGroupingActive ) 
	{
		// set group pivot for the root-most group
		AGroupActor* ActorGroupRoot = AGroupActor::GetRootForActor(LastSelectedActor, true, true);
		if(ActorGroupRoot)
		{
			ActorGroupRoot->SetActorLocation( EditorModeTools.PivotLocation, false );
		}
	}

	//if there are multiple actors selected, just use the x-axis for the "translate/rotate" widget
	if (Count == 1)
	{
		EditorModeTools.TranslateRotateXAxisAngle = TranslateRotateWidgetWorldXAxis.Rotation().Yaw;
	}

	// Update showing.
	EditorModeTools.PivotShown = SnapCount>0 || Count>1;
}


void UUnrealEdEngine::ResetPivot()
{
	GEditorModeTools().PivotShown	= 0;
	GEditorModeTools().Snapping		= 0;
	GEditorModeTools().SnappedActor	= 0;
}

/*-----------------------------------------------------------------------------
	Selection.
-----------------------------------------------------------------------------*/

void UUnrealEdEngine::SetActorSelectionFlags (AActor* InActor)
{
	TArray<UActorComponent*> Components;
	InActor->GetComponents(Components);

	//for every component in the actor
	for(int32 ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++)
	{
		UActorComponent* Component = Components[ComponentIndex];
		if (Component->IsRegistered())
		{
			// If we have a 'child actor' component, want to update its visible selection state
			UChildActorComponent* ChildActorComponent = Cast<UChildActorComponent>(Component);
			if(ChildActorComponent != NULL && ChildActorComponent->ChildActor != NULL)
			{
				SetActorSelectionFlags(ChildActorComponent->ChildActor);
			}

			UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(Component);
			if(PrimComponent != NULL && PrimComponent->IsRegistered())
			{
				PrimComponent->PushSelectionToProxy();
			}

			UDecalComponent* DecalComponent = Cast<UDecalComponent>(Component);
			if(DecalComponent != NULL)// && DecalComponent->IsRegistered())
			{
				DecalComponent->PushSelectionToProxy();
			}
		}
	}
}


void UUnrealEdEngine::UpdatePivotLocationForSelection( bool bOnChange )
{
	// Pick a new common pivot, or not.
	int32 ActorCount=0;
	AActor* SingleActor=NULL;

	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if ( Actor->GetWorld() == GWorld )
		{
			SingleActor = Actor;
			bool IsTemplate = Actor->IsTemplate();
			bool LevelLocked = !FLevelUtils::IsLevelLocked(Actor->GetLevel());
			check( IsTemplate || LevelLocked );
			ActorCount++;
		}
	}
	
	if( ActorCount > 0 ) 
	{		
		// For geometry mode use current pivot location as it's set to selected face, not actor
		FEditorModeTools& Tools = GEditorModeTools();
		if( Tools.IsModeActive(FBuiltinEditorModes::EM_Geometry) == false || bOnChange == true )
		{
			// Set pivot point to the actor's location
			FVector PivotPoint = SingleActor->GetActorLocation();

			// If grouping is active, see if this actor is part of a locked group and use that pivot instead
			if(GEditor->bGroupingActive)
			{
				AGroupActor* ActorGroupRoot = AGroupActor::GetRootForActor(SingleActor, true, true);
				if(ActorGroupRoot)
				{
					PivotPoint = ActorGroupRoot->GetActorLocation();
				}
			}
			SetPivot( PivotPoint, false, true );
		}
	}
	else
	{
		ResetPivot();
	}
}



void UUnrealEdEngine::NoteSelectionChange()
{
	// The selection changed, so make sure the pivot (widget) is located in the right place
	UpdatePivotLocationForSelection( true );

	TArray<FEdMode*> ActiveModes;
	GEditorModeTools().GetActiveModes( ActiveModes );
	for( int32 ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		ActiveModes[ModeIndex]->ActorSelectionChangeNotify();
	}

	USelection* Section = GetSelectedActors();
	USelection::SelectionChangedEvent.Broadcast(Section);

	//whenever selection changes, recompute whether the selection contains a locked actor
	bCheckForLockActors = true;

	//whenever selection changes, recompute whether the selection contains a world info actor
	bCheckForWorldSettingsActors = true;

	UpdateFloatingPropertyWindows();

	RedrawLevelEditingViewports();
}


void UUnrealEdEngine::SelectGroup(AGroupActor* InGroupActor, bool bForceSelection/*=false*/, bool bInSelected/*=true*/, bool bNotify/*=true*/)
{
	USelection* SelectedActors = GetSelectedActors();
	SelectedActors->BeginBatchSelectOperation();
	SelectedActors->Modify();

	static bool bIteratingGroups = false;

	if( !bIteratingGroups )
	{
		bIteratingGroups = true;
		// Select all actors within the group (if locked or forced)
		if( bForceSelection || InGroupActor->IsLocked() )
		{	
			TArray<AActor*> GroupActors;
			InGroupActor->GetGroupActors(GroupActors);
			for( int32 ActorIndex=0; ActorIndex < GroupActors.Num(); ++ActorIndex )
			{                  
				SelectActor(GroupActors[ActorIndex], bInSelected, false );
			}
			bForceSelection = true;

			// Recursively select any subgroups
			TArray<AGroupActor*> SubGroups;
			InGroupActor->GetSubGroups(SubGroups);
			for( int32 GroupIndex=0; GroupIndex < SubGroups.Num(); ++GroupIndex )
			{
				SelectGroup(SubGroups[GroupIndex], bForceSelection, bInSelected, false);
			}
		}

		SelectedActors->EndBatchSelectOperation(bNotify);
		if (bNotify)
		{
			NoteSelectionChange();
		}

		//whenever selection changes, recompute whether the selection contains a locked actor
		bCheckForLockActors = true;

		//whenever selection changes, recompute whether the selection contains a world info actor
		bCheckForWorldSettingsActors = true;

		bIteratingGroups = false;
	}
}


bool UUnrealEdEngine::CanSelectActor(AActor* Actor, bool bInSelected, bool bSelectEvenIfHidden, bool bWarnIfLevelLocked ) const
{
	// If selections are globally locked, leave.
	if( !Actor || GEdSelectionLock || !Actor->IsEditable() )
	{
		return false;
	}

	// Only abort from hidden actors if we are selecting. You can deselect hidden actors without a problem.
	if ( bInSelected )
	{
		// If the actor is NULL or hidden, leave.
		if ( !bSelectEvenIfHidden && ( Actor->IsHiddenEd() || !FLevelUtils::IsLevelVisible( Actor->GetLevel() ) ) )
		{
			return false;
		}

		// Ensure that neither the level nor the actor is being destroyed or is unreachable
		EObjectFlags InvalidSelectableFlags = RF_PendingKill|RF_BeginDestroyed|RF_Unreachable;
		if( Actor->GetLevel()->HasAnyFlags(InvalidSelectableFlags) )
		{
			UE_LOG(LogEditorSelectUtils, Warning, TEXT("SelectActor: %s (%s)"), TEXT("The requested operation could not be completed because the level has invalid flags."),*Actor->GetActorLabel());
			return false;
		}
		if( Actor->HasAnyFlags(InvalidSelectableFlags) )
		{
			UE_LOG(LogEditorSelectUtils, Warning, TEXT("SelectActor: %s (%s)"), TEXT("The requested operation could not be completed because the actor has invalid flags."),*Actor->GetActorLabel());
			return false;
		}

		if ( !Actor->IsTemplate() && FLevelUtils::IsLevelLocked(Actor->GetLevel()) )
		{
			if( bWarnIfLevelLocked )
			{
				UE_LOG(LogEditorSelectUtils, Warning, TEXT("SelectActor: %s (%s)"), TEXT("The requested operation could not be completed because the level is locked."),*Actor->GetActorLabel());
			}
			return false;
		}
	}

	// If grouping operations are not currently allowed, don't select groups.
	AGroupActor* SelectedGroupActor = Cast<AGroupActor>(Actor);
	if( SelectedGroupActor && !GEditor->bGroupingActive )
	{
		return false;
	}

	bool bSelectionAllowed = false;

	TArray<FEdMode*> ActiveModes;
	GEditorModeTools().GetActiveModes( ActiveModes );
	for( int32 ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		bSelectionAllowed |= ActiveModes[ModeIndex]->IsSelectionAllowed( Actor, bInSelected );
	}

	return bSelectionAllowed;
}

void UUnrealEdEngine::SelectActor(AActor* Actor, bool bInSelected, bool bNotify, bool bSelectEvenIfHidden)
{
	const bool bWarnIfLevelLocked = true;
	if( !CanSelectActor( Actor, bInSelected, bSelectEvenIfHidden, bWarnIfLevelLocked ) )
	{
		return;
	}

	bool bSelectionHandled = false;

	TArray<FEdMode*> ActiveModes;
	GEditorModeTools().GetActiveModes( ActiveModes );
	for( int32 ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		bSelectionHandled |= ActiveModes[ModeIndex]->Select( Actor, bInSelected );
	}

	// Select the actor and update its internals.
	if( !bSelectionHandled )
	{
		if(bInSelected)
		{
			// If trying to select an Actor spawned by a ChildACtorComponent, instead select Actor that spawned us
			if(Actor->ParentComponentActor.IsValid())
			{
				Actor = Actor->ParentComponentActor.Get();
			}
		}

			if(GEditor->bGroupingActive)
			{
			// if this actor is a group, do a group select/deselect
				AGroupActor* SelectedGroupActor = Cast<AGroupActor>(Actor);
				if( SelectedGroupActor )
				{
				SelectGroup(SelectedGroupActor, true, bInSelected, bNotify);
				}
				else
				{
				// Select/Deselect this actor's entire group, starting from the top locked group.
				// If none is found, just use the actor.
					AGroupActor* ActorLockedRootGroup = AGroupActor::GetRootForActor(Actor, true);
					if( ActorLockedRootGroup )
					{
					SelectGroup(ActorLockedRootGroup, false, bInSelected, bNotify);
					}
				}
			}

		// Don't do any work if the actor's selection state is already the selected state.
		const bool bActorSelected = Actor->IsSelected();
		if ( (bActorSelected && !bInSelected) || (!bActorSelected && bInSelected) )
		{
			if(bInSelected)
			{
				UE_LOG(LogEditorSelectUtils, Verbose,  TEXT("Selected Actor: %s"), *Actor->GetClass()->GetName());
			}
			else
			{
				UE_LOG(LogEditorSelectUtils, Verbose,  TEXT("Deselected Actor: %s"), *Actor->GetClass()->GetName() );
			}

			GetSelectedActors()->Select( Actor, bInSelected );
			
			//A fast path to mark selection rather than reconnecting ALL components for ALL actors that have changed state
			SetActorSelectionFlags (Actor);

			if( bNotify )
			{
				NoteSelectionChange();
			}

			//whenever selection changes, recompute whether the selection contains a locked actor
			bCheckForLockActors = true;

			//whenever selection changes, recompute whether the selection contains a world info actor
			bCheckForWorldSettingsActors = true;
		}
		else
		{
			if( bNotify )
			{
				//reset the property windows.  In case something has changed since previous selection
				UpdateFloatingPropertyWindows();
			}
		}
	}
}

void UUnrealEdEngine::SelectBSPSurf(UModel* InModel, int32 iSurf, bool bSelected, bool bNoteSelectionChange)
{
	if( GEdSelectionLock )
	{
		return;
	}

	FBspSurf& Surf = InModel->Surfs[ iSurf ];
	InModel->ModifySurf( iSurf, false );

	if( bSelected )
	{
		Surf.PolyFlags |= PF_Selected;
	}
	else
	{
		Surf.PolyFlags &= ~PF_Selected;
	}

	if( bNoteSelectionChange )
	{
		NoteSelectionChange();
	}

	//whenever selection changes, recompute whether the selection contains a locked actor
	bCheckForLockActors = true;

	//whenever selection changes, recompute whether the selection contains a world info actor
	bCheckForWorldSettingsActors = true;
}

/**
 * Deselects all BSP surfaces in the specified level.
 *
 * @param	Level		The level for which to deselect all levels.
 * @return				The number of surfaces that were deselected
 */
static uint32 DeselectAllSurfacesForLevel(ULevel* Level)
{
	uint32 NumSurfacesDeselected = 0;
	if ( Level )
	{
		UModel* Model = Level->Model;
		for( int32 SurfaceIndex = 0 ; SurfaceIndex < Model->Surfs.Num() ; ++SurfaceIndex )
		{
			FBspSurf& Surf = Model->Surfs[SurfaceIndex];
			if( Surf.PolyFlags & PF_Selected )
			{
				Model->ModifySurf( SurfaceIndex, false );
				Surf.PolyFlags &= ~PF_Selected;
				++NumSurfacesDeselected;
			}
		}
	}
	return NumSurfacesDeselected;
}


void UUnrealEdEngine::SelectNone(bool bNoteSelectionChange, bool bDeselectBSPSurfs, bool WarnAboutManyActors)
{
	if( GEdSelectionLock )
	{
		return;
	}

	bool bShowProgress = false;

	// If there are a lot of actors to process, pop up a warning "are you sure?" box
	if( WarnAboutManyActors )
	{
		int32 NumSelectedActors = GEditor->GetSelectedActorCount();
		if( NumSelectedActors >= EditorActorSelectionDefs::MaxActorsToSelectBeforeWarning )
		{
			bShowProgress = true;

			const FText ConfirmText = FText::Format( NSLOCTEXT("UnrealEd", "Warning_ManyActorsForDeselect", "There are {0} selected actors. Are you sure you want to deselect them all?" ), FText::AsNumber(NumSelectedActors) );

			FSuppressableWarningDialog::FSetupInfo Info( ConfirmText, NSLOCTEXT( "UnrealEd", "Warning_ManyActors", "Warning: Many Actors" ), "Warning_ManyActors" );
			Info.ConfirmText = NSLOCTEXT("ModalDialogs", "ManyActorsForDeselectConfirm", "Continue Deselection");
			Info.CancelText = NSLOCTEXT("ModalDialogs", "ManyActorsForDeselectCancel", "Keep Current Selection");

			FSuppressableWarningDialog ManyActorsWarning( Info );
			if( ManyActorsWarning.ShowModal() == FSuppressableWarningDialog::Cancel )
			{
				return;
			}
		}
	}

	if( bShowProgress )
	{
		GWarn->BeginSlowTask( LOCTEXT("BeginDeselectingActorsTaskMessage", "Deselecting Actors"), true );
	}

	// Make a list of selected actors . . .
	TArray<AActor*> ActorsToDeselect;
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		ActorsToDeselect.Add( Actor );
	}

	USelection* SelectedActors = GetSelectedActors();
	SelectedActors->BeginBatchSelectOperation();
	SelectedActors->Modify();

	// . . . and deselect them.
	for ( int32 ActorIndex = 0 ; ActorIndex < ActorsToDeselect.Num() ; ++ActorIndex )
	{
		AActor* Actor = ActorsToDeselect[ ActorIndex ];
		SelectActor( Actor, false, false );
	}

	uint32 NumDeselectSurfaces = 0;
	UWorld* World = GWorld;
	if( bDeselectBSPSurfs && World )
	{
		// Unselect all surfaces in all levels.
		NumDeselectSurfaces += DeselectAllSurfacesForLevel( World->PersistentLevel );
		for( int32 LevelIndex = 0 ; LevelIndex < World->StreamingLevels.Num() ; ++LevelIndex )
		{
			ULevelStreaming* StreamingLevel = World->StreamingLevels[LevelIndex];
			if( StreamingLevel )
			{
				ULevel* Level = StreamingLevel->GetLoadedLevel();
				if ( Level != NULL )
				{
					NumDeselectSurfaces += DeselectAllSurfacesForLevel( Level );
				}
			}
		}
	}

	SelectedActors->EndBatchSelectOperation(bNoteSelectionChange);

	//prevents clicking on background multiple times spamming selection changes
	if (ActorsToDeselect.Num() || NumDeselectSurfaces)
	{
		GetSelectedActors()->DeselectAll();

		if( bNoteSelectionChange )
		{
			NoteSelectionChange();
		}

		//whenever selection changes, recompute whether the selection contains a locked actor
		bCheckForLockActors = true;

		//whenever selection changes, recompute whether the selection contains a world info actor
		bCheckForWorldSettingsActors = true;
	}

	if( bShowProgress )
	{
		GWarn->EndSlowTask();
	}
}

#undef LOCTEXT_NAMESPACE