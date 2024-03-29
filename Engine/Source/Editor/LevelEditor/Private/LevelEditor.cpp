// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "LevelEditor.h"
#include "SLevelEditor.h"
#include "ModuleManager.h"
#include "LevelEditorActions.h"
#include "LevelEditorModesActions.h"
#include "AssetSelection.h"
#include "LevelEditorContextMenu.h"
#include "SLevelViewport.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "Developer/MessageLog/Public/MessageLogModule.h"
#include "EditorViewportCommands.h"
#include "LevelViewportActions.h"
#include "GlobalEditorCommonCommands.h"
#include "IUserFeedbackModule.h"
#include "SlateReflector.h"

// @todo Editor: remove this circular dependency
#include "Editor/MainFrame/Public/Interfaces/IMainFrameModule.h"

#define LOCTEXT_NAMESPACE "LevelEditor"

IMPLEMENT_MODULE( FLevelEditorModule, LevelEditor );

const FName LevelEditorApp = FName(TEXT("LevelEditorApp"));
const FName MainFrame("MainFrame");

FLevelEditorModule::FLevelEditorModule()
	: ToggleImmersiveConsoleCommand(
	TEXT( "LevelEditor.ToggleImmersive" ),
	TEXT( "Toggle 'Immersive Mode' for the active level editing viewport" ),
	FConsoleCommandDelegate::CreateRaw( this, &FLevelEditorModule::ToggleImmersiveOnActiveLevelViewport ) )
	{
	}

TSharedRef<SDockTab> SpawnLevelEditor( const FSpawnTabArgs& InArgs )
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( TEXT("LevelEditor") );
	TSharedRef<SDockTab> LevelEditorTab = SNew(SDockTab) .TabRole(ETabRole::MajorTab) .ContentPadding( FMargin(0,2,0,0) );
	LevelEditorModule.SetLevelEditorInstanceTab(LevelEditorTab);
	TSharedPtr< SWindow > OwnerWindow = InArgs.GetOwnerWindow();
	
	if ( !OwnerWindow.IsValid() )
	{
		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>( MainFrame );
		OwnerWindow = MainFrameModule.GetParentWindow();
	}

	if ( OwnerWindow.IsValid() )
	{
		TSharedPtr<SLevelEditor> LevelEditorTmp;
		LevelEditorTab->SetContent( SAssignNew(LevelEditorTmp, SLevelEditor ) );
		LevelEditorModule.SetLevelEditorInstance(LevelEditorTmp);
		LevelEditorTmp->Initialize( LevelEditorTab, OwnerWindow.ToSharedRef() );

		GEditorModeTools().DeactivateAllModes();
		GEditorModeTools().ActivateMode(FBuiltinEditorModes::EM_Placement);
	}

	IUserFeedbackModule& UserFeedback = FModuleManager::LoadModuleChecked<IUserFeedbackModule>(TEXT("UserFeedback"));
	TSharedRef<SWidget> UserFeedbackWidget = UserFeedback.CreateFeedbackWidget(NSLOCTEXT("UserFeedback", "LevelEditing", "Level Editing"));

	TSharedPtr< SWidget > RightContent;
	{
		FString OptionalBranchPrefix;
		GConfig->GetString(TEXT("LevelEditor"), TEXT("ProjectNameWatermarkPrefix"), /*out*/ OptionalBranchPrefix, GEditorUserSettingsIni);

		FFormatNamedArguments Args;
		Args.Add( TEXT("Branch"), FText::FromString(OptionalBranchPrefix) );
		Args.Add( TEXT("GameName"), FText::FromString(FString(FApp::GetGameName())) );

		FText RightContentText;

		const EBuildConfigurations::Type BuildConfig = FApp::GetBuildConfiguration();
		if (BuildConfig != EBuildConfigurations::Shipping && BuildConfig != EBuildConfigurations::Development && BuildConfig != EBuildConfigurations::Unknown)
		{
			Args.Add( TEXT("Config"), EBuildConfigurations::ToText(BuildConfig) );
			RightContentText = FText::Format(NSLOCTEXT("UnrealEditor", "TitleBarRightContentAndConfig", "{Branch}{GameName} [{Config}]"), Args);
		}
		else
		{
			RightContentText = FText::Format(NSLOCTEXT("UnrealEditor", "TitleBarRightContent", "{Branch}{GameName}"), Args);
		}

		RightContent =
				SNew( SHorizontalBox )

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.Visibility( EVisibility::HitTestInvisible )
					[
						SNew( STextBlock )
						.Text( RightContentText )
						.Font( FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 14 ) )
						.ColorAndOpacity( FLinearColor( 1.0f, 1.0f, 1.0f, 0.3f ) )
					]
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(16.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					UserFeedbackWidget
				]
		;
	}
	LevelEditorTab->SetRightContent( RightContent.ToSharedRef() );
	
	return LevelEditorTab;
}

/**
 * Called right after the module's DLL has been loaded and the module object has been created
 */
void FLevelEditorModule::StartupModule()
{
	// Our command context bindings depend on having the mainframe loaded
	FModuleManager::LoadModuleChecked<IMainFrameModule>(MainFrame);

	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	ModeBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	NotificationBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	// Note this must come before any tab spawning because that can create the SLevelEditor and attempt to map commands
	FLevelEditorCommands::Register();
	FLevelEditorModesCommands::Register();
	FEditorViewportCommands::Register();
	FLevelViewportCommands::Register();

	// Bind level editor commands shared across an instance
	BindGlobalLevelEditorCommands();

	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();

	FGlobalTabmanager::Get()->RegisterTabSpawner("LevelEditor", FOnSpawnTab::CreateStatic( &SpawnLevelEditor ) )
		.SetDisplayName( NSLOCTEXT("LevelEditor", "LevelEditorTab", "Level Editor") );

	FModuleManager::LoadModuleChecked<ISlateReflectorModule>("SlateReflector").RegisterTabSpawner(MenuStructure.GetDeveloperToolsCategory());

	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	MessageLogModule.RegisterLogListing("BuildAndSubmitErrors", LOCTEXT("BuildAndSubmitErrors", "Build and Submit Errors"));

	// Figure out if we recompile the level editor.
	FString SourcePath = FPaths::Combine(*FPaths::EngineDir(), TEXT("Source/Editor/LevelEditor/Private"));
	bCanBeRecompiled = IFileManager::Get().DirectoryExists(*SourcePath);
}

/**
 * Called before the module is unloaded, right before the module object is destroyed.
 */
void FLevelEditorModule::ShutdownModule()
{
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	MessageLogModule.UnregisterLogListing("BuildAndSubmitErrors");

	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();

	// Stop PIE/SIE before unloading the level editor module
	// Otherwise, when the module is reloaded, it's likely to be in a bad state
	if (GUnrealEd && GUnrealEd->PlayWorld)
	{
		GUnrealEd->EndPlayMap();
	}

	// If the level editor tab is currently open, close it
	{
		TSharedPtr<SDockTab> LevelEditorTab = LevelEditorInstanceTabPtr.Pin();
		if (LevelEditorTab.IsValid())
		{
			LevelEditorTab->RequestCloseTab();
		}
		LevelEditorInstanceTabPtr.Reset();
	}

	// Clear out some globals that may be referencing this module
	SetLevelEditorTabManager(nullptr);
	WorkspaceMenu::GetModule().ResetLevelEditorCategory();

	if ( FSlateApplication::IsInitialized() )
	{
		FGlobalTabmanager::Get()->UnregisterTabSpawner("LevelEditor");
		FModuleManager::LoadModuleChecked<ISlateReflectorModule>("SlateReflector").UnregisterTabSpawner();
	}	

	FLevelEditorCommands::Unregister();
	FLevelEditorModesCommands::Unregister();
	FEditorViewportCommands::Unregister();
	FLevelViewportCommands::Unregister();
}

void FLevelEditorModule::PreUnloadCallback()
{
	// Disable the "tab closed" delegate that closes the editor if the level editor tab is closed
	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>( MainFrame );
	MainFrameModule.DisableTabClosedDelegate();
}

void FLevelEditorModule::PostLoadCallback()
{
	// Re-open the level editor tab and re-enable the "tab closed" delegate
	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>( MainFrame );
	TSharedRef<SDockTab> LevelEditorTab = FGlobalTabmanager::Get()->InvokeTab(FTabId("LevelEditor"));
	MainFrameModule.SetMainTab(LevelEditorTab);
	MainFrameModule.EnableTabClosedDelegate();
}

/**
 * Spawns a new property viewer
 * @todo This only works with the first level editor. Fix it.
 */
void FLevelEditorModule::SummonSelectionDetails()
{
	TSharedPtr<SLevelEditor> LevelEditorInstance = LevelEditorInstancePtr.Pin();
	LevelEditorInstance->SyncDetailsToSelection();
}

void FLevelEditorModule::SummonBuildAndSubmit()
{
	TSharedPtr<SLevelEditor> LevelEditorInstance = LevelEditorInstancePtr.Pin();
	LevelEditorInstance->InvokeTab("LevelEditorBuildAndSubmit");
}

void FLevelEditorModule::SummonLevelBrowser()
{
	TSharedPtr<SLevelEditor> LevelEditorInstance = LevelEditorInstancePtr.Pin();
	LevelEditorInstance->InvokeTab("LevelEditorLevelBrowser");
}

// @todo remove when world-centric mode is added
void FLevelEditorModule::AttachSequencer(TSharedPtr<SWidget> Sequencer)
{
	if( FParse::Param( FCommandLine::Get(), TEXT( "Sequencer" ) ) )
	{
		TSharedPtr<SLevelEditor> LevelEditorInstance = LevelEditorInstancePtr.Pin();
		LevelEditorInstance->InvokeTab("Sequencer");
		LevelEditorInstance->SequencerTab->SetContent(Sequencer.ToSharedRef());
	}
}

TSharedPtr<ILevelViewport> FLevelEditorModule::GetFirstActiveViewport()
{
	TSharedPtr<SLevelEditor> LevelEditorInstance = LevelEditorInstancePtr.Pin();
	return (LevelEditorInstance.IsValid()) ? LevelEditorInstance->GetActiveViewport() : nullptr;
}

void FLevelEditorModule::FocusPIEViewport()
{
	TSharedPtr<SLevelEditor> LevelEditorInstance = LevelEditorInstancePtr.Pin();
	if( LevelEditorInstance.IsValid() && LevelEditorTabManager.IsValid() && LevelEditorInstance->HasActivePlayInEditorViewport() )
		{
		FGlobalTabmanager::Get()->DrawAttentionToTabManager( LevelEditorTabManager.ToSharedRef() );
	}
}

void FLevelEditorModule::FocusViewport()
{
	TSharedPtr<ILevelViewport> ActiveLevelViewport = GetFirstActiveViewport();
	if( ActiveLevelViewport.IsValid() )
	{
		TSharedRef< const SWidget > ViewportAsWidget = ActiveLevelViewport->AsWidget();
		FWidgetPath FocusWidgetPath;

		if( FSlateApplication::Get().GeneratePathToWidgetUnchecked( ViewportAsWidget, FocusWidgetPath ) )
		{
			FSlateApplication::Get().SetKeyboardFocus( FocusWidgetPath, EKeyboardFocusCause::SetDirectly );
		}
	}
}

void FLevelEditorModule::BroadcastActorSelectionChanged( const TArray<UObject*>& NewSelection )
{
	ActorSelectionChangedEvent.Broadcast( NewSelection );
}

void FLevelEditorModule::BroadcastRedrawViewports( bool bInvalidateHitProxies )
{
	RedrawLevelEditingViewportsEvent.Broadcast( bInvalidateHitProxies );
}

void FLevelEditorModule::BroadcastTakeHighResScreenShots( )
{
	TakeHighResScreenShotsEvent.Broadcast();
}

void FLevelEditorModule::BroadcastMapChanged( UWorld* World, EMapChangeType::Type MapChangeType )
{
	MapChangedEvent.Broadcast( World, MapChangeType );
}

const FLevelEditorCommands& FLevelEditorModule::GetLevelEditorCommands() const
{
	return FLevelEditorCommands::Get();
}

const FLevelEditorModesCommands& FLevelEditorModule::GetLevelEditorModesCommands() const
{
	return FLevelEditorModesCommands::Get();
}

const FLevelViewportCommands& FLevelEditorModule::GetLevelViewportCommands() const
{
	return FLevelViewportCommands::Get();
}

TWeakPtr<class SLevelEditor> FLevelEditorModule::GetLevelEditorInstance() const
{
	return LevelEditorInstancePtr;
}

TWeakPtr<class SDockTab> FLevelEditorModule::GetLevelEditorInstanceTab() const
{
	return LevelEditorInstanceTabPtr;
}

TSharedPtr<FTabManager> FLevelEditorModule::GetLevelEditorTabManager() const
{
	return LevelEditorTabManager;
}

void FLevelEditorModule::SetLevelEditorInstance( TWeakPtr<SLevelEditor> LevelEditor )
{
	LevelEditorInstancePtr = LevelEditor;
}

void FLevelEditorModule::SetLevelEditorInstanceTab( TWeakPtr<SDockTab> LevelEditorTab )
{
	LevelEditorInstanceTabPtr = LevelEditorTab;
}

void FLevelEditorModule::SetLevelEditorTabManager( const TSharedPtr<SDockTab>& OwnerTab )
{
	if (LevelEditorTabManager.IsValid())
	{
		LevelEditorTabManager->UnregisterAllTabSpawners();
		LevelEditorTabManager.Reset();
	}

	if (OwnerTab.IsValid())
	{
		LevelEditorTabManager = FGlobalTabmanager::Get()->NewTabManager(OwnerTab.ToSharedRef());
		LevelEditorTabManager->SetOnPersistLayout( FTabManager::FOnPersistLayout::CreateStatic( FLayoutSaveRestore::SaveTheLayout ) );
	}
}

void FLevelEditorModule::StartImmersivePlayInEditorSession()
{
	TSharedPtr<ILevelViewport> ActiveLevelViewport = GetFirstActiveViewport();

	if( ActiveLevelViewport.IsValid() )
	{
		// Make sure we can find a patch to the viewport.  This will fail in cases where the viewport widget
		// is in a backgrounded tab, etc.  We can't currently support starting PIE in a backgrounded tab
		// due to how PIE manages focus and requires event forwarding from the application.
		TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow( ActiveLevelViewport->AsWidget() );
		if(Window.IsValid() )
		{
			// When in immersive play in editor, toggle game view on the active viewport
			if( !ActiveLevelViewport->IsInGameView() )
			{
				ActiveLevelViewport->ToggleGameView();
			}

			// Start level viewport initially in immersive mode
			{
				const bool bWantImmersive = true;
				const bool bAllowAnimation = false;
				ActiveLevelViewport->MakeImmersive( bWantImmersive, bAllowAnimation );
				FVector2D WindowSize = Window->GetSizeInScreen();
				// Set the initial size of the viewport to be the size of the window. This must be done because Slate has not ticked yet so the viewport will have no initial size
				ActiveLevelViewport->GetActiveViewport()->SetInitialSize( FIntPoint( FMath::TruncToInt( WindowSize.X ), FMath::TruncToInt( WindowSize.Y ) ) );
			}

			// Launch PIE
			{
				const FVector* StartLocation = NULL;
				const FRotator* StartRotation = NULL;

				// We never want to play from the camera's location at startup, because the camera could have
				// been abandoned in a strange location in the map
				if( 0 )	// @todo immersive
				{
					// If this is a perspective viewport, then we'll Play From Here
					const FLevelEditorViewportClient& LevelViewportClient = ActiveLevelViewport->GetLevelViewportClient();
					if( LevelViewportClient.IsPerspective() )
					{
						// Start PIE from the camera's location and orientation!
						StartLocation = &LevelViewportClient.GetViewLocation();
						StartRotation = &LevelViewportClient.GetViewRotation();
					}
				}

				// Queue up the PIE session
				const bool bSimulateInEditor = false;
				const bool bUseMobilePreview = false;
				GUnrealEd->RequestPlaySession( true, ActiveLevelViewport, bSimulateInEditor, StartLocation, StartRotation, -1, bUseMobilePreview );
				// Kick off the queued PIE session immediately.  This is so that at startup, we don't need to
				// wait for the next engine tick.  We want to see PIE gameplay when the editor first appears!
				GUnrealEd->StartQueuedPlayMapRequest();

				// Special case for immersive pie startup, When in immersive pie at startup we use the player start but we want to move the camera where the player
				// was at when pie ended.
				GEditor->bHasPlayWorldPlacement = true;
			}
		}
	}
}

void FLevelEditorModule::ToggleImmersiveOnActiveLevelViewport()
{
	TSharedPtr< ILevelViewport > ActiveLevelViewport = GetFirstActiveViewport();
	if( ActiveLevelViewport.IsValid() )
	{
		// Toggle immersive mode (with animation!)
		const bool bAllowAnimation = true;
		ActiveLevelViewport->MakeImmersive( !ActiveLevelViewport->IsImmersive(), bAllowAnimation );
	}
}

/** @return Returns the first Level Editor that we currently know about */
TSharedPtr<ILevelEditor> FLevelEditorModule::GetFirstLevelEditor()
{
	return LevelEditorInstancePtr.Pin();
}

TSharedPtr<SDockTab> FLevelEditorModule::GetLevelEditorTab() const
{
	return LevelEditorInstanceTabPtr.Pin().ToSharedRef();
}

void FLevelEditorModule::BindGlobalLevelEditorCommands()
{
	check( !GlobalLevelEditorActions.IsValid() );

	GlobalLevelEditorActions = MakeShareable( new FUICommandList );

	const FLevelEditorCommands& Commands = FLevelEditorCommands::Get();
	FUICommandList& ActionList = *GlobalLevelEditorActions;

	// Make a default can execute action that disables input when in debug mode
	FCanExecuteAction DefaultExecuteAction = FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::DefaultCanExecuteAction );

	ActionList.MapAction( Commands.BrowseDocumentation, FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::BrowseDocumentation ) );
	ActionList.MapAction( Commands.BrowseAPIReference, FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::BrowseAPIReference ) );
	ActionList.MapAction( Commands.BrowseViewportControls, FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::BrowseViewportControls ) );
	ActionList.MapAction( Commands.NewLevel, FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::NewLevel ), FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::NewLevel_CanExecute ) );
	ActionList.MapAction( Commands.OpenLevel, FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OpenLevel ), FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OpenLevel_CanExecute ) );
	ActionList.MapAction( Commands.Save, FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::Save ) );
	ActionList.MapAction( Commands.SaveAs, FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SaveAs ), DefaultExecuteAction );
	ActionList.MapAction( Commands.SaveAllLevels, FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SaveAllLevels ) );
	ActionList.MapAction( Commands.ToggleFavorite, FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ToggleFavorite ), FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ToggleFavorite_CanExecute ), FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::ToggleFavorite_IsChecked ) );

	for( int32 CurRecentIndex = 0; CurRecentIndex < FLevelEditorCommands::MaxRecentFiles; ++CurRecentIndex )
	{
		ActionList.MapAction( Commands.OpenRecentFileCommands[ CurRecentIndex ], FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OpenRecentFile, CurRecentIndex ), DefaultExecuteAction );
	}

	for( int32 CurFavoriteIndex = 0; CurFavoriteIndex < FLevelEditorCommands::MaxFavoriteFiles; ++CurFavoriteIndex )
	{
		ActionList.MapAction( Commands.OpenFavoriteFileCommands[ CurFavoriteIndex ], FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OpenFavoriteFile, CurFavoriteIndex ), DefaultExecuteAction );
	}

	for( int32 CurFavoriteIndex = 0; CurFavoriteIndex < FLevelEditorCommands::MaxFavoriteFiles; ++CurFavoriteIndex )
	{
		ActionList.MapAction( Commands.RemoveFavoriteCommands[ CurFavoriteIndex ], FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::RemoveFavorite, CurFavoriteIndex ), DefaultExecuteAction );
	}

	ActionList.MapAction( Commands.Import,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::Import_Clicked ) );

	ActionList.MapAction( Commands.ExportAll,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExportAll_Clicked ) );

	ActionList.MapAction( Commands.ExportSelected,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExportSelected_Clicked ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExportSelected_CanExecute ) );

	ActionList.MapAction( Commands.Build,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::Build_Execute ) );


	if (CanBeRecompiled())
	{
		ActionList.MapAction( Commands.RecompileLevelEditor,
			FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::RecompileLevelEditor_Clicked ),
			FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::Recompile_CanExecute )
			);

		ActionList.MapAction( Commands.ReloadLevelEditor,
			FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ReloadLevelEditor_Clicked ),
			FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::Reload_CanExecute )
			);
	}

	ActionList.MapAction( Commands.RecompileGameCode,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::RecompileGameCode_Clicked ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::Recompile_CanExecute )
		);

	ActionList.MapAction( 
		FGlobalEditorCommonCommands::Get().FindInContentBrowser, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::FindInContentBrowser_Clicked )
		);
						
	ActionList.MapAction( 
		Commands.SnapCameraToActor, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("CAMERA SNAP") ) )
		);

	ActionList.MapAction( 
		Commands.GoToCodeForActor, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::GoToCodeForActor_Clicked )
		);

	ActionList.MapAction( 
		FGenericCommands::Get().Duplicate, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("DUPLICATE") ) ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::Duplicate_CanExecute )
		);

	ActionList.MapAction( 
		FGenericCommands::Get().Delete, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("DELETE") ) ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::Delete_CanExecute )
		);

	ActionList.MapAction( 
		FGenericCommands::Get().Rename, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::Rename_Execute ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::Rename_CanExecute )
		);

	ActionList.MapAction( 
		FGenericCommands::Get().Cut, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("EDIT CUT") ) ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::Cut_CanExecute )
		);

	ActionList.MapAction( 
		FGenericCommands::Get().Copy, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("EDIT COPY") ) ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::Copy_CanExecute )
		);

	ActionList.MapAction( 
		FGenericCommands::Get().Paste, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("EDIT PASTE") ) ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::Paste_CanExecute )
		);

	ActionList.MapAction( 
		Commands.PasteHere, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("EDIT PASTE TO=HERE") ) ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::PasteHere_CanExecute )
		);

	bool bAlign = false;
	bool bPerActor = false;
	ActionList.MapAction(
		Commands.SnapOriginToGrid,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::MoveActorToGrid_Clicked, bAlign, bPerActor),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
		);

	bPerActor = true;
	ActionList.MapAction(
		Commands.SnapOriginToGridPerActor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::MoveActorToGrid_Clicked, bAlign, bPerActor),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
		);
	
	bAlign = true;
	bPerActor = false;
	ActionList.MapAction(
		Commands.AlignOriginToGrid,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::MoveActorToGrid_Clicked, bAlign, bPerActor),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
		);

	bAlign = false;
	ActionList.MapAction(
		Commands.SnapOriginToActor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::MoveActorToActor_Clicked, bAlign),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorsSelected_CanExecute)
		);
	
	bAlign = true;
	ActionList.MapAction(
		Commands.AlignOriginToActor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::MoveActorToActor_Clicked, bAlign),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorsSelected_CanExecute)
		);

	bAlign = false;
	bool bUseLineTrace = false;
	bool bUseBounds = false;
	bool bUsePivot = false;
	ActionList.MapAction(
		Commands.SnapToFloor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapActorToFloor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
		);

	bAlign = true;
	bUseLineTrace = false;
	bUseBounds = false;
	bUsePivot = false;
	ActionList.MapAction(
		Commands.AlignToFloor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapActorToFloor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
		);

	bAlign = false;
	bUseLineTrace = true;
	bUseBounds = false;
	bUsePivot = true;
	ActionList.MapAction(
		Commands.SnapPivotToFloor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapActorToFloor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
		);

	bAlign = true;
	bUseLineTrace = true;
	bUseBounds = false;
	bUsePivot = true;
	ActionList.MapAction(
		Commands.AlignPivotToFloor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapActorToFloor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
		);

	bAlign = false;
	bUseLineTrace = true;
	bUseBounds = true;
	bUsePivot = false;
	ActionList.MapAction(
		Commands.SnapBottomCenterBoundsToFloor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapActorToFloor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
		);

	bAlign = true;
	bUseLineTrace = true;
	bUseBounds = true;
	bUsePivot = false;
	ActionList.MapAction(
		Commands.AlignBottomCenterBoundsToFloor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapActorToFloor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
		);

	bAlign = false;
	bUseLineTrace = false;
	bUseBounds = false;
	bUsePivot = false;
	ActionList.MapAction(
		Commands.SnapToActor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapActorToActor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorsSelected_CanExecute)
		);

	bAlign = true;
	bUseLineTrace = false;
	bUseBounds = false;
	bUsePivot = false;
	ActionList.MapAction(
		Commands.AlignToActor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapActorToActor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorsSelected_CanExecute)
		);

	bAlign = false;
	bUseLineTrace = true;
	bUseBounds = false;
	bUsePivot = true;
	ActionList.MapAction(
		Commands.SnapPivotToActor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapActorToActor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorsSelected_CanExecute)
		);

	bAlign = true;
	bUseLineTrace = true;
	bUseBounds = false;
	bUsePivot = true;
	ActionList.MapAction(
		Commands.AlignPivotToActor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapActorToActor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorsSelected_CanExecute)
		);

	bAlign = false;
	bUseLineTrace = true;
	bUseBounds = true;
	bUsePivot = false;
	ActionList.MapAction(
		Commands.SnapBottomCenterBoundsToActor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapActorToActor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorsSelected_CanExecute)
		);

	bAlign = true;
	bUseLineTrace = true;
	bUseBounds = true;
	bUsePivot = false;
	ActionList.MapAction(
		Commands.AlignBottomCenterBoundsToActor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapActorToActor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorsSelected_CanExecute)
		);

	ActionList.MapAction(
		Commands.DeltaTransformToActors, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::DeltaTransform ), 
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ActorSelected_CanExecute ) );

	ActionList.MapAction(
		Commands.MirrorActorX,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR MIRROR X=-1") ) ),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
		);

	ActionList.MapAction(
		Commands.MirrorActorY,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR MIRROR Y=-1") ) ),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
		);

	ActionList.MapAction(
		Commands.MirrorActorZ,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR MIRROR Z=-1") ) ),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
		);

	ActionList.MapAction(
		Commands.DetachFromParent,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::DetachActor_Clicked )
		);

	ActionList.MapAction(
		Commands.AttachSelectedActors,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::AttachSelectedActors )
		);

	ActionList.MapAction(
		Commands.AttachActorIteractive,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::AttachActorIteractive )
		);

	ActionList.MapAction(
		Commands.CreateNewOutlinerFolder,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::CreateNewOutlinerFolder_Clicked )
		);

	ActionList.MapAction(
		Commands.LockActorMovement,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::LockActorMovement_Clicked ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::LockActorMovement_IsChecked )
		);

	ActionList.MapAction(
		Commands.RegroupActors,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::RegroupActor_Clicked )
		);

	ActionList.MapAction(
		Commands.UngroupActors,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::UngroupActor_Clicked )
		);

	ActionList.MapAction(
		Commands.LockGroup,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::LockGroup_Clicked )
		);

	ActionList.MapAction(
		Commands.UnlockGroup,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::UnlockGroup_Clicked )
		);

	ActionList.MapAction(
		Commands.AddActorsToGroup,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::AddActorsToGroup_Clicked )
		);

	ActionList.MapAction(
		Commands.RemoveActorsFromGroup,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::RemoveActorsFromGroup_Clicked )
		);
	
	ActionList.MapAction(
		Commands.MergeActors,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::MergeActors_Clicked ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::CanExecuteMergeActors )
		);

	ActionList.MapAction(
		Commands.MergeActorsByMaterials,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::MergeActorsByMaterials_Clicked ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::CanExecuteMergeActorsByMaterials )
		);
	
	ActionList.MapAction(
		Commands.ShowAll,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR UNHIDE ALL") ) ) 
		);

	ActionList.MapAction(
		Commands.ShowSelectedOnly,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnShowOnlySelectedActors )
		);

	ActionList.MapAction(
		Commands.ShowSelected,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR UNHIDE SELECTED") ) )
		);

	ActionList.MapAction(
		Commands.HideSelected,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR HIDE SELECTED") ) )
		);

	ActionList.MapAction(
		Commands.ShowAllStartup,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR UNHIDE ALL STARTUP") ) )
		);

	ActionList.MapAction(
		Commands.ShowSelectedStartup,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR UNHIDE SELECTED STARTUP") ) )
		);

	ActionList.MapAction(
		Commands.HideSelectedStartup,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR HIDE SELECTED STARTUP") ) )
		);

	ActionList.MapAction(
		Commands.CycleNavigationDataDrawn,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("CYCLENAVDRAWN") ) )
		);

	ActionList.MapAction(
		FGenericCommands::Get().SelectAll,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR SELECT ALL") ) )
		);

	ActionList.MapAction(
		Commands.SelectNone,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("SELECT NONE") ) )
		);

	ActionList.MapAction(
		Commands.InvertSelection,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR SELECT INVERT") ) )
		);

	ActionList.MapAction(
		Commands.SelectAllActorsOfSameClass,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnSelectAllActorsOfClass, (bool)false )
		);

	ActionList.MapAction(
		Commands.SelectAllActorsOfSameClassWithArchetype,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnSelectAllActorsOfClass, (bool)true )
		);

	ActionList.MapAction(
		Commands.SelectRelevantLights,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR SELECT RELEVANTLIGHTS") ) )
		);

	ActionList.MapAction(
		Commands.SelectStaticMeshesOfSameClass,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR SELECT MATCHINGSTATICMESH") ) )
		);

	ActionList.MapAction(
		Commands.SelectStaticMeshesAllClasses,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR SELECT MATCHINGSTATICMESH ALLCLASSES") ) )
		);

	ActionList.MapAction(
		Commands.SelectSkeletalMeshesOfSameClass,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR SELECT MATCHINGSKELETALMESH") ) )
		);

	ActionList.MapAction(
		Commands.SelectSkeletalMeshesAllClasses,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR SELECT MATCHINGSKELETALMESH ALLCLASSES") ) )
		);

	ActionList.MapAction(
		Commands.SelectAllWithSameMaterial,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR SELECT MATCHINGMATERIAL") ) )
		);

	ActionList.MapAction(
		Commands.SelectAllActorsControlledByMatinee,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnSelectAllActorsControlledByMatinee )
		);

	ActionList.MapAction(
		Commands.SelectMatchingEmitter,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR SELECT MATCHINGEMITTER") ) )
		);

	ActionList.MapAction(
		Commands.SelectAllLights,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnSelectAllLights )
		);

	ActionList.MapAction(
		Commands.SelectStationaryLightsExceedingOverlap,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnSelectStationaryLightsExceedingOverlap )
		);

	ActionList.MapAction(
		Commands.SelectAllAddditiveBrushes,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("MAP SELECT ADDS") ) )
		);

	ActionList.MapAction(
		Commands.SelectAllSubtractiveBrushes,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("MAP SELECT SUBTRACTS") ) )
		);

	ActionList.MapAction(
		Commands.SelectAllSemiSolidBrushes,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("MAP SELECT SEMISOLIDS") ) )
		);

	ActionList.MapAction(
		Commands.SelectAllNonSolidBrushes,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("MAP SELECT NONSOLIDS") ) )
		);

	ActionList.MapAction(
		Commands.SelectAllSurfaces,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT ALL") ) )
		);

	ActionList.MapAction( 
		Commands.SurfSelectAllMatchingBrush,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT MATCHING BRUSH") ) )
		);

	ActionList.MapAction( 
		Commands.SurfSelectAllMatchingTexture,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT MATCHING TEXTURE") ) )
		);

	ActionList.MapAction( 
		Commands.SurfSelectAllAdjacents,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT ADJACENT ALL") ) )
		);

	ActionList.MapAction( 
		Commands.SurfSelectAllAdjacentCoplanars,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT ADJACENT COPLANARS") ) )
		);

	ActionList.MapAction( 
		Commands.SurfSelectAllAdjacentWalls,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT ADJACENT WALLS") ) )
		);

	ActionList.MapAction( 
		Commands.SurfSelectAllAdjacentFloors,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT ADJACENT FLOORS") ) )
		);

	ActionList.MapAction( 
		Commands.SurfSelectAllAdjacentSlants,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT ADJACENT SLANTS") ) )
		);

	ActionList.MapAction( 
		Commands.SurfSelectReverse,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT REVERSE") ) )
		);

	ActionList.MapAction( 
		Commands.SurfSelectMemorize,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT MEMORY SET") ) )
		);

	ActionList.MapAction( 
		Commands.SurfSelectRecall,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT MEMORY RECALL") ) )
		);

	ActionList.MapAction( 
		Commands.SurfSelectOr,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT MEMORY INTERSECTION") ) )
		);

	ActionList.MapAction( 
		Commands.SurfSelectAnd,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT MEMORY UNION") ) )
		);

	ActionList.MapAction( 
		Commands.SurfSelectXor,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT MEMORY XOR") ) )
		);

	ActionList.MapAction( 
		Commands.SurfUnalign,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnSurfaceAlignment, TEXALIGN_Default )
		);

	ActionList.MapAction( 
		Commands.SurfAlignPlanarAuto,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnSurfaceAlignment, TEXALIGN_PlanarAuto )
		);

	ActionList.MapAction( 
		Commands.SurfAlignPlanarWall,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnSurfaceAlignment, TEXALIGN_PlanarWall )
		);

	ActionList.MapAction( 
		Commands.SurfAlignPlanarFloor,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnSurfaceAlignment, TEXALIGN_PlanarFloor )
		);

	ActionList.MapAction( 
		Commands.SurfAlignBox,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnSurfaceAlignment, TEXALIGN_Box )
		);

	ActionList.MapAction(
		Commands.SurfAlignFit,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnSurfaceAlignment, TEXALIGN_Fit )
		);


	ActionList.MapAction(
		Commands.ApplyMaterialToSurface,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnApplyMaterialToSurface )
		);

	ActionList.MapAction(
		Commands.SavePivotToPrePivot,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR BAKEPREPIVOT") ) )
		);

	ActionList.MapAction(
		Commands.ResetPivot,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR RESET PIVOT") ) )
		);

	ActionList.MapAction(
		Commands.ResetPrePivot,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR UNBAKEPREPIVOT") ) )
		);

	ActionList.MapAction(
		Commands.MovePivotHere,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("PIVOT HERE") ) )
		);

	ActionList.MapAction(
		Commands.MovePivotHereSnapped,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("PIVOT SNAPPED") ) )
		);

	ActionList.MapAction(
		Commands.MovePivotToCenter,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("PIVOT CENTERSELECTION") ) )
		);

	ActionList.MapAction(
		Commands.ConvertToAdditive,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString::Printf(TEXT("MAP SETBRUSH BRUSHTYPE=%d"), (int32)Brush_Add) )
		);

	ActionList.MapAction(
		Commands.ConvertToSubtractive,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString::Printf(TEXT("MAP SETBRUSH BRUSHTYPE=%d"), (int32)Brush_Subtract) )
		);

	ActionList.MapAction(
		Commands.OrderFirst,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("MAP SENDTO FIRST") ) )
		);

	ActionList.MapAction(
		Commands.OrderLast,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("MAP SENDTO LAST") ) )
		);

	ActionList.MapAction(
		Commands.MakeSolid,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString::Printf( TEXT("MAP SETBRUSH CLEARFLAGS=%d SETFLAGS=%d"), PF_Semisolid + PF_NotSolid, 0 ) )
		);

	ActionList.MapAction(
		Commands.MakeSemiSolid,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString::Printf( TEXT("MAP SETBRUSH CLEARFLAGS=%d SETFLAGS=%d"), (int32)(PF_Semisolid + PF_NotSolid), (int32)PF_Semisolid ) )
		);

	ActionList.MapAction(
		Commands.MakeNonSolid,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString::Printf( TEXT("MAP SETBRUSH CLEARFLAGS=%d SETFLAGS=%d"), (int32)(PF_Semisolid + PF_NotSolid), (int32)PF_NotSolid ) )
		);

	ActionList.MapAction(
		Commands.MergePolys,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("BRUSH MERGEPOLYS") ) )
		);

	ActionList.MapAction(
		Commands.SeparatePolys,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("BRUSH SEPARATEPOLYS") ) )
		);

	ActionList.MapAction(
		Commands.CreateBoundingBoxVolume,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR CREATE_BV_BOUNDINGBOX SnapToGrid=1") ) )
		);


	ActionList.MapAction(
		Commands.CreateHeavyConvexVolume,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR CREATE_BV_CONVEXVOLUME NORMALTOLERANCE=0.01 SnapToGrid=1") ) )
		);

	ActionList.MapAction(
		Commands.CreateNormalConvexVolume,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR CREATE_BV_CONVEXVOLUME NORMALTOLERANCE=0.15 SnapToGrid=1") ) )
		);

	ActionList.MapAction(
		Commands.CreateLightConvexVolume,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR CREATE_BV_CONVEXVOLUME NORMALTOLERANCE=.5 SnapToGrid=1") ) )
		);

	ActionList.MapAction(
		Commands.CreateRoughConvexVolume,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR CREATE_BV_CONVEXVOLUME NORMALTOLERANCE=0.75 SnapToGrid=1") ) )
		);

	ActionList.MapAction(
		Commands.SaveBrushAsCollision,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnSaveBrushAsCollision )
		);


	ActionList.MapAction(
		Commands.KeepSimulationChanges,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnKeepSimulationChanges ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::CanExecuteKeepSimulationChanges )
		);


	ActionList.MapAction( 
		Commands.MakeActorLevelCurrent,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnMakeSelectedActorLevelCurrent )
		);

	ActionList.MapAction(
		Commands.MoveSelectedToCurrentLevel,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnMoveSelectedToCurrentLevel )
		);

	ActionList.MapAction(
		Commands.FindLevelsInLevelBrowser,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnFindLevelsInLevelBrowser )
		);

	ActionList.MapAction(
		Commands.AddLevelsToSelection,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnSelectLevelInLevelBrowser )
		);

	ActionList.MapAction(
		Commands.RemoveLevelsFromSelection,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnDeselectLevelInLevelBrowser )
		);

	ActionList.MapAction(
		Commands.FindActorInLevelScript,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnFindActorInLevelScript )
		);

	ActionList.MapAction( Commands.BuildAndSubmitToSourceControl,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::BuildAndSubmitToSourceControl_Execute ) );

	ActionList.MapAction( Commands.BuildLightingOnly,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::BuildLightingOnly_Execute ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::BuildLighting_CanExecute ) );

	ActionList.MapAction( Commands.BuildReflectionCapturesOnly,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::BuildReflectionCapturesOnly_Execute ) );

	ActionList.MapAction( Commands.BuildLightingOnly_VisibilityOnly,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::BuildLightingOnly_VisibilityOnly_Execute ) );

	ActionList.MapAction( Commands.LightingBuildOptions_UseErrorColoring,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::LightingBuildOptions_UseErrorColoring_Toggled ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::LightingBuildOptions_UseErrorColoring_IsChecked ) );

	ActionList.MapAction( Commands.LightingBuildOptions_ShowLightingStats,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::LightingBuildOptions_ShowLightingStats_Toggled ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::LightingBuildOptions_ShowLightingStats_IsChecked ) );

	ActionList.MapAction( Commands.BuildGeometryOnly,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::BuildGeometryOnly_Execute ) );

	ActionList.MapAction( Commands.BuildGeometryOnly_OnlyCurrentLevel,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::BuildGeometryOnly_OnlyCurrentLevel_Execute ) );

	ActionList.MapAction( Commands.BuildPathsOnly,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::BuildPathsOnly_Execute ) );

	ActionList.MapAction( 
		Commands.LightingQuality_Production, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetLightingQuality, (ELightingBuildQuality)Quality_Production ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsLightingQualityChecked, (ELightingBuildQuality)Quality_Production ) );
	ActionList.MapAction( 
		Commands.LightingQuality_High, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetLightingQuality, (ELightingBuildQuality)Quality_High ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsLightingQualityChecked, (ELightingBuildQuality)Quality_High ) );
	ActionList.MapAction( 
		Commands.LightingQuality_Medium, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetLightingQuality, (ELightingBuildQuality)Quality_Medium ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsLightingQualityChecked, (ELightingBuildQuality)Quality_Medium ) );
	ActionList.MapAction( 
		Commands.LightingQuality_Preview, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetLightingQuality, (ELightingBuildQuality)Quality_Preview ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsLightingQualityChecked, (ELightingBuildQuality)Quality_Preview) );

	ActionList.MapAction( 
		Commands.LightingTools_ShowBounds, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetLightingToolShowBounds ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsLightingToolShowBoundsChecked ) );
	ActionList.MapAction( 
		Commands.LightingTools_ShowTraces, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetLightingToolShowTraces ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsLightingToolShowTracesChecked ) );
	ActionList.MapAction( 
		Commands.LightingTools_ShowDirectOnly, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetLightingToolShowDirectOnly ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsLightingToolShowDirectOnlyChecked ) );
	ActionList.MapAction( 
		Commands.LightingTools_ShowIndirectOnly, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetLightingToolShowIndirectOnly ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsLightingToolShowIndirectOnlyChecked ) );
	ActionList.MapAction( 
		Commands.LightingTools_ShowIndirectSamples, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetLightingToolShowIndirectSamples ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsLightingToolShowIndirectSamplesChecked ) );

	ActionList.MapAction( 
		Commands.LightingDensity_RenderGrayscale, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetLightingDensityRenderGrayscale ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsLightingDensityRenderGrayscaleChecked ) );

	ActionList.MapAction( 
		Commands.LightingResolution_CurrentLevel, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetLightingResolutionLevel, (FLightmapResRatioAdjustSettings::AdjustLevels)FLightmapResRatioAdjustSettings::Current ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsLightingResolutionLevelChecked, (FLightmapResRatioAdjustSettings::AdjustLevels)FLightmapResRatioAdjustSettings::Current ) );
	ActionList.MapAction( 
		Commands.LightingResolution_SelectedLevels, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetLightingResolutionLevel, (FLightmapResRatioAdjustSettings::AdjustLevels)FLightmapResRatioAdjustSettings::Selected ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsLightingResolutionLevelChecked, (FLightmapResRatioAdjustSettings::AdjustLevels)FLightmapResRatioAdjustSettings::Selected ) );
	ActionList.MapAction( 
		Commands.LightingResolution_AllLoadedLevels, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetLightingResolutionLevel, (FLightmapResRatioAdjustSettings::AdjustLevels)FLightmapResRatioAdjustSettings::AllLoaded ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsLightingResolutionLevelChecked, (FLightmapResRatioAdjustSettings::AdjustLevels)FLightmapResRatioAdjustSettings::AllLoaded ) );
	ActionList.MapAction( 
		Commands.LightingResolution_SelectedObjectsOnly, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetLightingResolutionSelectedObjectsOnly ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsLightingResolutionSelectedObjectsOnlyChecked ) );

	ActionList.MapAction( 
		Commands.LightingStaticMeshInfo, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ShowLightingStaticMeshInfo ) );

	ActionList.MapAction( 
		Commands.SceneStats,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ShowSceneStats ) );

	ActionList.MapAction( 
		Commands.TextureStats,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ShowTextureStats ) );

	ActionList.MapAction( 
		Commands.MapCheck,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::MapCheck_Execute ) );

	ActionList.MapAction(
		Commands.ShowTransformWidget,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnToggleTransformWidgetVisibility ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnGetTransformWidgetVisibility )
		);

	ActionList.MapAction(
		Commands.AllowTranslucentSelection,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnAllowTranslucentSelection ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsAllowTranslucentSelectionEnabled ) 
		);


	ActionList.MapAction(
		Commands.AllowGroupSelection,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnAllowGroupSelection ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsAllowGroupSelectionEnabled ) 
		);

	ActionList.MapAction(
		Commands.StrictBoxSelect,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnToggleStrictBoxSelect ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsStrictBoxSelectEnabled ) 
		);

	ActionList.MapAction(
		Commands.DrawBrushMarkerPolys,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnDrawBrushMarkerPolys ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsDrawBrushMarkerPolysEnabled ) 
		);

	ActionList.MapAction(
		Commands.OnlyLoadVisibleInPIE,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnToggleOnlyLoadVisibleInPIE ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsOnlyLoadVisibleInPIEEnabled ) 
		);

	ActionList.MapAction(
		Commands.ToggleSocketSnapping,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnToggleSocketSnapping ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsSocketSnappingEnabled ) 
		);

	ActionList.MapAction(
		Commands.ToggleParticleSystemLOD,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnToggleParticleSystemLOD ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsParticleSystemLODEnabled ) 
		);

	ActionList.MapAction(
		Commands.ToggleFreezeParticleSimulation,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnToggleFreezeParticleSimulation ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsParticleSimulationFrozen )
		);

	ActionList.MapAction(
		Commands.ToggleParticleSystemHelpers,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnToggleParticleSystemHelpers ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsParticleSystemHelpersEnabled ) 
		);

	ActionList.MapAction(
		Commands.ToggleLODViewLocking,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnToggleLODViewLocking ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsLODViewLockingEnabled ) 
		);

	ActionList.MapAction(
		Commands.LevelStreamingVolumePrevis,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnToggleLevelStreamingVolumePrevis ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsLevelStreamingVolumePrevisEnabled ) 
		);

	ActionList.MapAction(
		Commands.EnableActorSnap,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnEnableActorSnap ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsActorSnapEnabled ) 
		);

	ActionList.MapAction(
		Commands.EnableVertexSnap,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnEnableVertexSnap ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsVertexSnapEnabled ) 
		);

	ActionList.MapAction(
		Commands.ShowSelectedDetails,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("EDCALLBACK SELECTEDPROPS" ) ) ) 
		);

	//if (FParse::Param( FCommandLine::Get(), TEXT( "editortoolbox" ) ))
	//{
	//	ActionList.MapAction(
	//		Commands.BspMode,
	//		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("MODE BSP") ) ),
	//		FCanExecuteAction(),
	//		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsEditorModeActive, FBuiltinEditorModes::EM_Bsp ) 
	//		);

	//	ActionList.MapAction(
	//		Commands.MeshPaintMode,
	//		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("MODE MESHPAINT") ) ),
	//		FCanExecuteAction(),
	//		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsEditorModeActive, FBuiltinEditorModes::EM_MeshPaint ) 
	//		);

	//	ActionList.MapAction(
	//		Commands.LandscapeMode,
	//		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("MODE LANDSCAPE") ) ),
	//		FCanExecuteAction(),
	//		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsEditorModeActive, FBuiltinEditorModes::EM_Landscape ) 
	//		);

	//	ActionList.MapAction(
	//		Commands.FoliageMode,
	//		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("MODE FOLIAGE") ) ),
	//		FCanExecuteAction(),
	//		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsEditorModeActive, FBuiltinEditorModes::EM_Foliage ) 
	//		);
	//}

	ActionList.MapAction(
		Commands.RecompileShaders,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("RECOMPILESHADERS CHANGED" ) ) )
		);

	ActionList.MapAction(
		Commands.ProfileGPU,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("PROFILEGPU") ) )
		);

	ActionList.MapAction(
		Commands.ResetAllParticleSystems,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("PARTICLE RESET ALL") ) )
		);

	ActionList.MapAction(
		Commands.ResetSelectedParticleSystem,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("PARTICLE RESET SELECTED") ) )
		);

	ActionList.MapAction(
		FEditorViewportCommands::Get().LocationGridSnap,
		FExecuteAction::CreateStatic(FLevelEditorActionCallbacks::LocationGridSnap_Clicked),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(FLevelEditorActionCallbacks::LocationGridSnap_IsChecked)
		);
	ActionList.MapAction(
		FEditorViewportCommands::Get().RotationGridSnap,
		FExecuteAction::CreateStatic(FLevelEditorActionCallbacks::RotationGridSnap_Clicked),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(FLevelEditorActionCallbacks::RotationGridSnap_IsChecked)
		);
	ActionList.MapAction(
		FEditorViewportCommands::Get().ScaleGridSnap,
		FExecuteAction::CreateStatic(FLevelEditorActionCallbacks::ScaleGridSnap_Clicked),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(FLevelEditorActionCallbacks::ScaleGridSnap_IsChecked)
		);
	ActionList.MapAction(
		Commands.ToggleHideViewportUI,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnToggleHideViewportUI ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsViewportUIHidden ) 
		);
	ActionList.MapAction(
		Commands.AddMatinee,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnAddMatinee )
		);

	ActionList.MapAction( 
		Commands.MaterialQualityLevel_Low, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetMaterialQualityLevel, (EMaterialQualityLevel::Type)EMaterialQualityLevel::Low ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsMaterialQualityLevelChecked, (EMaterialQualityLevel::Type)EMaterialQualityLevel::Low ) );
	ActionList.MapAction( 
		Commands.MaterialQualityLevel_High, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetMaterialQualityLevel, (EMaterialQualityLevel::Type)EMaterialQualityLevel::High ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsMaterialQualityLevelChecked, (EMaterialQualityLevel::Type)EMaterialQualityLevel::High ) );


	for (int32 i = 0; i < ERHIFeatureLevel::Num; ++i)
	{
		ActionList.MapAction(
			Commands.FeatureLevelPreview[i],
			FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SetFeatureLevelPreview, (ERHIFeatureLevel::Type)i),
			FCanExecuteAction(),
			FIsActionChecked::CreateStatic(&FLevelEditorActionCallbacks::IsFeatureLevelPreviewChecked, (ERHIFeatureLevel::Type)i));
	}
}
	
#undef LOCTEXT_NAMESPACE
