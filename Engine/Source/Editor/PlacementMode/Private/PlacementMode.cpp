// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "PlacementModePrivatePCH.h"

#include "PlacementMode.h"
#include "PlacementModeToolkit.h"

#include "ScopedTransaction.h"
#include "AssetData.h"

#include "Editor/UnrealEd/Public/Toolkits/ToolkitManager.h"
#include "Editor/LevelEditor/Public/LevelEditor.h"
#include "Editor/LevelEditor/Public/ILevelViewport.h"

TSharedRef< FPlacementMode > FPlacementMode::Create()
{
	TSharedRef< FPlacementMode > PlacementMode = MakeShareable( new FPlacementMode() );
	PlacementMode->Initialize();
	return PlacementMode;
}

FPlacementMode::FPlacementMode()
	: PlacementsChanged( false )
	, CreatedPreviewActors( false )
	, AssetsToPlace()
	, ActiveTransactionIndex( INDEX_NONE )
{
	ID = TEXT("PLACEMENT");
	Name = NSLOCTEXT( "PlacementMode", "DisplayName", "Place" );
	IconBrush = FSlateIcon( FEditorStyle::GetStyleSetName(), "LevelEditor.PlacementMode", "LevelEditor.PlacementMode.Small" );
	bVisible = true;
	PriorityOrder = 0;
}

FPlacementMode::~FPlacementMode()
{
	GEditorModeTools().OnEditorModeChanged().RemoveAll( this );
}

void FPlacementMode::Initialize()
{
	//GConfig->GetFloat(TEXT("PlacementMode"), TEXT("AssetThumbnailScale"), AssetThumbnailScale, GEditorUserSettingsIni);
	//GConfig->GetBool( TEXT( "PlacementMode" ), TEXT( "ShowOtherDeveloperAssets" ), ShowOtherDeveloperAssets, GEditorUserSettingsIni );
	
	TArray< FString > RecentlyPlacedAsStrings;
	GConfig->GetArray(TEXT("PlacementMode"), TEXT("RecentlyPlaced"), RecentlyPlacedAsStrings, GEditorUserSettingsIni);

	//FString ActivePaletteName;
	//GConfig->GetString( TEXT( "PlacementMode" ), TEXT( "ActivePalette" ), ActivePaletteName, GEditorUserSettingsIni );

	for (int Index = 0; Index < RecentlyPlacedAsStrings.Num(); Index++)
	{
		RecentlyPlaced.Add( FActorPlacementInfo( RecentlyPlacedAsStrings[Index] ) );
	}

	GEditorModeTools().OnEditorModeChanged().AddSP( this, &FPlacementMode::EditorModeChanged );
}

bool FPlacementMode::UsesToolkits() const
{
	return true;
}

void FPlacementMode::EditorModeChanged( FEdMode* Mode, bool IsEntering )
{
	if ( IsEntering && FModuleManager::Get().IsModuleLoaded( "LevelEditor" ) )
	{
		TSharedPtr< ILevelEditor > ToolkitHost = FModuleManager::LoadModuleChecked< FLevelEditorModule >( "LevelEditor" ).GetFirstLevelEditor();

		if ( ToolkitHost.IsValid() )
		{
			TArray< FEdMode* > ActiveModes;
			GEditorModeTools().GetActiveModes( ActiveModes );

			if ( ActiveModes.Num() == 1 && ActiveModes[0]->GetID() == FBuiltinEditorModes::EM_Default )
			{
				FLevelEditorActionCallbacks::ExecuteExecCommand( FString( TEXT( "MODE " ) ) + ID.ToString() );
			}
		}
	}
}

void FPlacementMode::Enter()
{
	// Call parent implementation
	FEdMode::Enter();

	if ( !Toolkit.IsValid() )
	{
		TSharedPtr< ILevelEditor > ToolkitHost = FModuleManager::LoadModuleChecked< FLevelEditorModule >( "LevelEditor" ).GetFirstLevelEditor();
		Toolkit = MakeShareable( new FPlacementModeToolkit );
		Toolkit->Init( ToolkitHost );
	}
}

void FPlacementMode::Exit()
{
	if ( Toolkit.IsValid() )
	{
		FToolkitManager::Get().CloseToolkit( Toolkit.ToSharedRef() );
		Toolkit.Reset();
	}

	// Call parent implementation
	FEdMode::Exit();
}

void FPlacementMode::Tick(FLevelEditorViewportClient* ViewportClient,float DeltaTime)
{
	if ( IsCurrentlyPlacing() )
	{
		ViewportClient->SetRequiredCursorOverride( true, EMouseCursor::GrabHandClosed );

		bool HasValidFocusTarget = false;
		for (int Index = ValidFocusTargetsForPlacement.Num() - 1; !HasValidFocusTarget && Index >= 0 ; Index--)
		{
			TSharedPtr< SWidget > FocusTarget = ValidFocusTargetsForPlacement[Index].Pin();

			if ( FocusTarget.IsValid() )
			{
				if ( FocusTarget->HasKeyboardFocus() || FocusTarget->HasFocusedDescendants() )
				{
					HasValidFocusTarget = true;
				}
			}
			else
			{
				ValidFocusTargetsForPlacement.RemoveAt( Index );
			}
		}

		if(!HasValidFocusTarget)
		{
			FLevelEditorModule* LevelEditorModule = FModuleManager::Get().LoadModulePtr<FLevelEditorModule>("LevelEditor");
			if(LevelEditorModule != NULL)
			{
				TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule->GetFirstLevelEditor();
				const TArray< TSharedPtr<ILevelViewport> >& LevelViewports = LevelEditor->GetViewports();
				for(auto It(LevelViewports.CreateConstIterator()); !HasValidFocusTarget && It; It++)
				{
					const TSharedPtr<ILevelViewport>& Viewport = *It;
					const TSharedPtr<const SWidget> ViewportWidget = Viewport->AsWidget();
					HasValidFocusTarget = ViewportWidget->HasKeyboardFocus() || ViewportWidget->HasFocusedDescendants();
				}
			}
		}

		if ( !HasValidFocusTarget )
		{
			StopPlacing();
		}
	}
	else
	{
		ViewportClient->SetRequiredCursorOverride( false );
	}

	if ( CreatedPreviewActors && ViewportClient != NULL && PlacementsChanged )
	{
		ViewportClient->DestroyDropPreviewActors();
		PlacementsChanged = false;
		CreatedPreviewActors = false;
	}

	FEdMode::Tick( ViewportClient, DeltaTime );
}

bool FPlacementMode::MouseEnter( FLevelEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y )
{
	if ( IsCurrentlyPlacing() )
	{
		ViewportClient->SetRequiredCursorOverride( true, EMouseCursor::GrabHandClosed );
	}

	return FEdMode::MouseEnter( ViewportClient, Viewport, x, y );
}

bool FPlacementMode::MouseLeave( FLevelEditorViewportClient* ViewportClient, FViewport* Viewport )
{
	if ( !ViewportClient->IsTracking() )
	{
		ViewportClient->SetRequiredCursorOverride( false );
		ViewportClient->DestroyDropPreviewActors();
		CreatedPreviewActors = false;
	}

	return FEdMode::MouseLeave( ViewportClient, Viewport );
}

bool FPlacementMode::AllowPreviewActors( FLevelEditorViewportClient* ViewportClient ) const
{
	return IsCurrentlyPlacing() && ( ViewportClient->IsTracking() && AllowPreviewActorsWhileTracking ) || !ViewportClient->IsTracking();
}

bool FPlacementMode::MouseMove( FLevelEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y )
{
	UpdatePreviewActors( ViewportClient, Viewport, x, y );
	return FEdMode::MouseMove( ViewportClient, Viewport, x, y );
}

void FPlacementMode::UpdatePreviewActors( FLevelEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y )
{
	bool AllAssetsValid = false;
	bool AllAssetsCanBeDropped = false;

	if ( PlacementsChanged )
	{
		ViewportClient->DestroyDropPreviewActors();
		PlacementsChanged = false;
		CreatedPreviewActors = false;
	}

	const bool bAllowPreviewActors = AllowPreviewActors( ViewportClient );

	if ( bAllowPreviewActors && AssetsToPlace.Num() > 0 )
	{
		AllAssetsValid = true;
		TArray< UObject* > Assets;
		for (int Index = 0; Index < AssetsToPlace.Num(); Index++)
		{
			if ( AssetsToPlace[Index].IsValid() )
			{
				Assets.Add( AssetsToPlace[Index].Get() );
			}
			else
			{
				ViewportClient->DestroyDropPreviewActors();
				AllAssetsValid = false;
				CreatedPreviewActors = false;
				break;
			}
		}

		if ( AllAssetsValid )
		{
			AllAssetsCanBeDropped = true;
			// Determine if we can drop the assets
			for ( auto AssetIt = Assets.CreateConstIterator(); AssetIt; ++AssetIt )
			{
				UObject* Asset = *AssetIt;

				FDropQuery DropResult = ViewportClient->CanDropObjectsAtCoordinates( x, y, FAssetData( Asset ) );
				if ( !DropResult.bCanDrop )
				{
					// At least one of the assets can't be dropped.
					ViewportClient->DestroyDropPreviewActors();
					AllAssetsCanBeDropped = false;
					CreatedPreviewActors = false;
				}
			}

			if ( AllAssetsCanBeDropped )
			{
				// Update the currently dragged actor if it exists
				bool bDroppedObjectsVisible = true;
				if ( !ViewportClient->UpdateDropPreviewActors(x, y, Assets, bDroppedObjectsVisible, PlacementFactory.Get()) )
				{
					const bool bOnlyDropOnTarget = false;
					const bool bCreateDropPreview = true;
					const bool bSelectActors = false;
					TArray< AActor* > TemporaryActors;
					CreatedPreviewActors = ViewportClient->DropObjectsAtCoordinates(x, y, Assets, TemporaryActors, bOnlyDropOnTarget, bCreateDropPreview, bSelectActors, PlacementFactory.Get() );
				}
			}
			else
			{
				StopPlacing();
			}
		}
	}

	if ( !bAllowPreviewActors || !AllAssetsValid || !AllAssetsCanBeDropped )
	{
		ViewportClient->DestroyDropPreviewActors();
		CreatedPreviewActors = false;
	}
}

bool FPlacementMode::InputKey( FLevelEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent )
{
	bool Handled = false;
	const bool bIsCtrlDown = ( ( InKey == EKeys::LeftControl || InKey == EKeys::RightControl ) && InEvent != IE_Released ) || InViewport->KeyState( EKeys::LeftControl ) || InViewport->KeyState( EKeys::RightControl );

	if ( IsCurrentlyPlacing() )
	{
		if ( InEvent == EInputEvent::IE_Pressed && ( InKey == EKeys::Escape || InKey == EKeys::SpaceBar ) )
		{
			StopPlacing();
			Handled = true;
		}
		else if ( bIsCtrlDown )
		{
			AllowPreviewActorsWhileTracking = true;
		}
		else if ( !bIsCtrlDown )
		{
			AllowPreviewActorsWhileTracking = false;

			if ( PlacedActors.Num() > 0 )
			{
				SelectPlacedActors();

				ClearAssetsToPlace();
				BroadcastStoppedPlacing(true);
			}
		}
	}

	if ( Handled )
	{
		return true;
	}

	return FEdMode::InputKey( InViewportClient, InViewport, InKey, InEvent );
}

bool FPlacementMode::StartTracking(FLevelEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	PlacedActorsThisTrackingSession = false;

	if( IsCurrentlyPlacing() && ActiveTransactionIndex == INDEX_NONE )
	{
		InViewportClient->SetRequiredCursorOverride( true, EMouseCursor::GrabHandClosed );
		ActiveTransactionIndex = GEditor->BeginTransaction( NSLOCTEXT( "BuilderMode", "PlaceActor", "Placed Actor") );
		return true;
	}

	return FEdMode::StartTracking(InViewportClient, InViewport);
}

bool FPlacementMode::EndTracking(FLevelEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	if ( IsCurrentlyPlacing() )
	{
		InViewportClient->SetRequiredCursorOverride( true, EMouseCursor::GrabHandClosed );
	}

	if ( ActiveTransactionIndex != INDEX_NONE )
	{
		if ( !PlacedActorsThisTrackingSession )
		{
			GEditor->CancelTransaction(	ActiveTransactionIndex );
			ActiveTransactionIndex = INDEX_NONE;
		}
		else
		{
			GEditor->EndTransaction();
			ActiveTransactionIndex = INDEX_NONE;
		}
		return true;
	}

	return FEdMode::EndTracking(InViewportClient, InViewport);
}

bool FPlacementMode::HandleClick(FLevelEditorViewportClient* InViewportClient, HHitProxy *HitProxy, const FViewportClick &Click )
{
	bool Handled = false;

	if ( IsCurrentlyPlacing() )
	{
		if ( Click.GetKey() == EKeys::LeftMouseButton )
		{
			TArray< UObject* > Assets;
			for (int Index = 0; Index < AssetsToPlace.Num(); Index++)
			{
				if ( AssetsToPlace[Index].IsValid() )
				{
					Assets.Add( AssetsToPlace[Index].Get() );
				}
			}

			TArray<AActor*> OutNewActors;
			const bool bCreateDropPreview = false;
			const bool SelectActor = false;
			const FViewport* const Viewport = Click.GetViewportClient()->Viewport;

			bool AllAssetsCanBeDropped = true;
			// Determine if we can drop the assets
			for ( auto AssetIt = Assets.CreateConstIterator(); AssetIt; ++AssetIt )
			{
				UObject* Asset = *AssetIt;
				FDropQuery DropResult = InViewportClient->CanDropObjectsAtCoordinates( Viewport->GetMouseX(), Viewport->GetMouseY(), FAssetData( Asset ) );
				if ( !DropResult.bCanDrop )
				{
					// At least one of the assets can't be dropped.
					InViewportClient->DestroyDropPreviewActors();
					AllAssetsCanBeDropped = false;
					CreatedPreviewActors = false;
				}
			}

			if ( AllAssetsCanBeDropped )
			{
				if ( !Click.IsControlDown() )
				{
					ClearAssetsToPlace();
					BroadcastStoppedPlacing(true);
					InViewportClient->SetRequiredCursorOverride( true, EMouseCursor::GrabHand );
				}

				InViewportClient->DropObjectsAtCoordinates( Viewport->GetMouseX(), Viewport->GetMouseY(), Assets, OutNewActors, false, bCreateDropPreview, SelectActor, PlacementFactory.Get() );

				for (int Index = 0; Index < OutNewActors.Num(); Index++)
				{
					if ( OutNewActors[Index] != NULL )
					{
						PlacedActorsThisTrackingSession = true;
						PlacedActors.Add( OutNewActors[Index] );
					}
				}

				if ( !Click.IsControlDown() )
				{
					SelectPlacedActors();
					ClearAssetsToPlace();
				}

				Handled = true;
			}
		}
		else
		{
			InViewportClient->DestroyDropPreviewActors();
			CreatedPreviewActors = false;
			StopPlacing();
		}
	}

	if ( !Handled )
	{
		Handled = FEdMode::HandleClick(InViewportClient, HitProxy, Click);
	}

	return Handled;
}

bool FPlacementMode::InputDelta( FLevelEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale )
{
	if ( IsCurrentlyPlacing() )
	{
		const bool bIsCtrlDown = InViewport->KeyState( EKeys::LeftControl ) || InViewport->KeyState( EKeys::RightControl );

		if ( InViewport->KeyState(EKeys::MiddleMouseButton) )
		{
			StopPlacing();

			InViewportClient->DestroyDropPreviewActors();
			CreatedPreviewActors = false;
		}
		else if ( InViewport->KeyState(EKeys::RightMouseButton) )
		{
			if ( bIsCtrlDown )
			{
				StopPlacing();
			}

			InViewportClient->DestroyDropPreviewActors();
			CreatedPreviewActors = false;
		}
		else if ( InViewport->KeyState(EKeys::LeftMouseButton) )
		{
			if ( !bIsCtrlDown )
			{
				InViewportClient->DestroyDropPreviewActors();
				CreatedPreviewActors = false;
			}
			else
			{
				return true;
			}
		}
	}

	return FEdMode::InputDelta(InViewportClient, InViewport, InDrag, InRot, InScale);
}

bool FPlacementMode::ShouldDrawWidget() const 
{ 
	return IsCurrentlyPlacing() ? false : FEdMode::ShouldDrawWidget(); 
}

bool FPlacementMode::UsesPropertyWidgets() const
{
	return IsCurrentlyPlacing() ? false : FEdMode::ShouldDrawWidget(); 
}

void FPlacementMode::StartPlacing( const TArray< UObject* >& Assets, UActorFactory* Factory )
{
	const bool bNotifySelectNone = true;
	const bool bDeselectBSPSurfs = true;
	GEditor->SelectNone( bNotifySelectNone, bDeselectBSPSurfs );

	if ( Assets.Num() == 1 )
	{
		if ( Assets[0] != NULL )
		{
			AssetsToPlace.Add( Assets[0] );
			PlacementsChanged = true;

			if ( Factory == NULL )
			{
				UActorFactory* LastSetFactory = FindLastUsedFactoryForAssetType( Assets[0] );
				if ( LastSetFactory != NULL )
				{
					Factory = LastSetFactory;
				}
			}

			SetPlacingFactory( Factory );
		}
	}
	else
	{
		for (int Index = 0; Index < Assets.Num(); Index++)
		{
			if ( Assets[Index] != NULL ) {
				AssetsToPlace.Add( Assets[Index] );
				PlacementsChanged = true;
			}
		}

		if ( PlacementsChanged == true )
		{
			SetPlacingFactory( Factory );
		}
	}

	StartedPlacingEvent.Broadcast( Assets );
}

void FPlacementMode::StopPlacing()
{
	if ( IsCurrentlyPlacing() )
	{
		ClearAssetsToPlace();
		BroadcastStoppedPlacing(false);
		PlacementsChanged = true;
	}
}

void FPlacementMode::SetPlacingFactory( UActorFactory* Factory )
{
	PlacementFactory = Factory;
	PlacementsChanged = true;

	if ( AssetsToPlace.Num() == 1 && AssetsToPlace[0].IsValid() )
	{
		AssetTypeToFactory.Add( *AssetsToPlace[0]->GetClass()->GetPathName(), PlacementFactory );
	}
}

UActorFactory* FPlacementMode::FindLastUsedFactoryForAssetType( UObject* Asset ) const
{
	if ( Asset == NULL )
	{
		return NULL;
	}

	UActorFactory* LastUsedFactory = NULL;

	UClass* CurrentClass = Cast<UClass>( Asset ); 
	if ( CurrentClass == NULL )
	{
		CurrentClass = Asset->GetClass();
	}

	while ( LastUsedFactory == NULL && CurrentClass != NULL && CurrentClass != UClass::StaticClass() )
	{
		const TWeakObjectPtr< UActorFactory >* FoundFactory = AssetTypeToFactory.Find( *CurrentClass->GetPathName() );
		if ( FoundFactory != NULL && FoundFactory->IsValid() )
		{
			LastUsedFactory = FoundFactory->Get();
		}
		else
		{
			CurrentClass = CurrentClass->GetSuperClass();
		}
	}

	return LastUsedFactory;
}

void FPlacementMode::ClearAssetsToPlace()
{
	AssetsToPlace.Empty();
	PlacedActors.Empty();
	PlacementsChanged = true;
}

void FPlacementMode::BroadcastStoppedPlacing( bool WasSuccessfullyPlaced ) const
{
	StoppedPlacingEvent.Broadcast( WasSuccessfullyPlaced );
}

void FPlacementMode::SelectPlacedActors()
{
	const FScopedTransaction Transaction( NSLOCTEXT( "BuilderMode", "SelectActors", "Select Actors") );

	const bool bNotifySelectNone = false;
	const bool bDeselectBSPSurfs = true;
	GEditor->SelectNone( bNotifySelectNone, bDeselectBSPSurfs );

	GEditor->GetSelectedActors()->BeginBatchSelectOperation();

	const bool bSelect = true;
	const bool bNotifyForActor = false;
	const bool bSelectEvenIfHidden = false;
	for (int Index = 0; Index < PlacedActors.Num(); Index++)
	{
		if ( PlacedActors[Index].IsValid() )
		{
			GEditor->GetSelectedActors()->Modify();
			GEditor->SelectActor( PlacedActors[Index].Get(), bSelect, bNotifyForActor, bSelectEvenIfHidden );
		}
	}

	GEditor->GetSelectedActors()->EndBatchSelectOperation();
	GEditor->NoteSelectionChange();
}

void FPlacementMode::AddToRecentlyPlaced( const TArray< UObject* >& PlacedObjects, UActorFactory* FactoryUsed )
{
	FString FactoryPath;
	if ( FactoryUsed != NULL )
	{
		FactoryPath = FactoryUsed->GetPathName();
	}

	TArray< UObject* > FilteredPlacedObjects;
	for ( UObject* PlacedObject : PlacedObjects )
	{
		// Don't include null placed objects that just have factories.
		if ( PlacedObject == NULL )
		{
			continue;
		}

		// Don't add brush builders to the recently placed.
		if ( PlacedObject->IsA(UBrushBuilder::StaticClass()) )
		{
			continue;
		}

		FilteredPlacedObjects.Add(PlacedObject);
	}

	// Don't change the recently placed if nothing passed the filter.
	if ( FilteredPlacedObjects.Num() == 0 )
	{
		return;
	}

	bool Changed = false;
	for ( int Index = 0; Index < FilteredPlacedObjects.Num(); Index++ )
	{
		Changed |= RecentlyPlaced.Remove(FActorPlacementInfo(FilteredPlacedObjects[Index]->GetPathName(), FactoryPath)) > 0;
	}

	for ( int Index = 0; Index < FilteredPlacedObjects.Num(); Index++ )
	{
		if ( FilteredPlacedObjects[Index] != NULL )
		{
			RecentlyPlaced.Insert(FActorPlacementInfo(FilteredPlacedObjects[Index]->GetPathName(), FactoryPath), 0);
			Changed = true;
		}
	}

	for (int Index = RecentlyPlaced.Num() - 1; Index >= 20; Index--)
	{
		RecentlyPlaced.RemoveAt( Index );
		Changed = true;
	}

	if ( Changed )
	{
		TArray< FString > RecentlyPlacedAsStrings;
		for (int Index = 0; Index < RecentlyPlaced.Num(); Index++)
		{
			RecentlyPlacedAsStrings.Add( RecentlyPlaced[Index].ToString() );
		}

		GConfig->SetArray(TEXT("PlacementMode"), TEXT("RecentlyPlaced"), RecentlyPlacedAsStrings, GEditorUserSettingsIni);
		RecentlyPlacedChanged.Broadcast( RecentlyPlaced );
	}
}

const TArray< TWeakObjectPtr<UObject> >& FPlacementMode::GetCurrentlyPlacingObjects() const
{
	return AssetsToPlace;
}
