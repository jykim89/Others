// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "LevelEditor.h"
#include "SLevelViewportToolBar.h"
#include "Editor/UnrealEd/Public/STransformViewportToolbar.h"
#include "SLevelViewport.h"
#include "EditorShowFlags.h"
#include "LevelViewportActions.h"
#include "LevelEditorActions.h"
#include "Layers/ILayers.h"
#include "Editor/SceneOutliner/Public/SceneOutlinerModule.h"
#include "DelegateFilter.h"
#include "Editor/SceneOutliner/Public/ISceneOutlinerColumn.h"
#include "DeviceProfileServices.h"
#include "EditorViewportCommands.h"
#include "SLevelEditor.h"
#include "Editor/UnrealEd/Public/SEditorViewportToolBarMenu.h"
#include "Editor/UnrealEd/Public/SEditorViewportToolBarButton.h"
#include "Editor/UnrealEd/Public/SEditorViewportViewMenu.h"
#include "StatsData.h"

#define LOCTEXT_NAMESPACE "LevelViewportToolBar"

/** Override the view menu, just so we can specify the level viewport as active when the button is clicked */
class SLevelEditorViewportViewMenu : public SEditorViewportViewMenu
{
public:
	virtual TSharedRef<SWidget> GenerateViewMenuContent() const OVERRIDE
	{
		SLevelViewport* LevelViewport = static_cast<SLevelViewport*>(Viewport.Pin().Get());
		LevelViewport->OnFloatingButtonClicked();
		
		return SEditorViewportViewMenu::GenerateViewMenuContent();
	}
};


void SLevelViewportToolBar::Construct( const FArguments& InArgs )
{
	Viewport = InArgs._Viewport;
	TSharedRef<SLevelViewport> ViewportRef = Viewport.Pin().ToSharedRef();

	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( "LevelEditor");

	const FMargin ToolbarSlotPadding( 2.0f, 2.0f );
	const FMargin ToolbarButtonPadding( 2.0f, 0.0f );

	ChildSlot
	[
		SNew( SBorder )
		.BorderImage( FEditorStyle::GetBrush("NoBorder") )
		// Color and opacity is changed based on whether or not the mouse cursor is hovering over the toolbar area
		.ColorAndOpacity( this, &SViewportToolBar::OnGetColorAndOpacity )
		.ForegroundColor( FEditorStyle::GetSlateColor("DefaultForeground") )
		[
			SNew( SVerticalBox )
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew( SHorizontalBox )
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( ToolbarSlotPadding )
				[
					SNew( SEditorViewportToolbarMenu )
					.ParentToolBar( SharedThis( this ) )
					.Cursor( EMouseCursor::Default )
					.Image( "EditorViewportToolBar.MenuDropdown" )
					.OnGetMenuContent( this, &SLevelViewportToolBar::GenerateOptionsMenu )
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( ToolbarSlotPadding )
				[
					SNew( SEditorViewportToolbarMenu )
					.ParentToolBar( SharedThis( this ) )
					.Cursor( EMouseCursor::Default )
					.Label( this, &SLevelViewportToolBar::GetCameraMenuLabel )
					.LabelIcon( this, &SLevelViewportToolBar::GetCameraMenuLabelIcon )
					.OnGetMenuContent( this, &SLevelViewportToolBar::GenerateCameraMenu ) 
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( ToolbarSlotPadding )
				[
					SNew( SLevelEditorViewportViewMenu, ViewportRef, SharedThis(this) )
					.Cursor( EMouseCursor::Default )
					.MenuExtenders( GetViewMenuExtender() )
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( ToolbarSlotPadding )
				[
					SNew( SEditorViewportToolbarMenu )
					.Label( LOCTEXT("ShowMenuTitle", "Show") )
					.Cursor( EMouseCursor::Default )
					.ParentToolBar( SharedThis( this ) )
					.OnGetMenuContent( this, &SLevelViewportToolBar::GenerateShowMenu ) 
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( ToolbarSlotPadding )
				[
					SNew( SEditorViewportToolbarMenu )
					.ParentToolBar( SharedThis( this ) )
					.Cursor( EMouseCursor::Default )
					.Label( this, &SLevelViewportToolBar::GetDevicePreviewMenuLabel )
					.LabelIcon( this, &SLevelViewportToolBar::GetDevicePreviewMenuLabelIcon )
					.OnGetMenuContent( this, &SLevelViewportToolBar::GenerateDevicePreviewMenu )
					//@todo rendering: mobile preview in view port is not functional yet - remove this once it is.
					.Visibility(EVisibility::Collapsed)
				]
				+ SHorizontalBox::Slot()
				.Padding( ToolbarSlotPadding )
				.HAlign( HAlign_Right )
				[
					SNew(STransformViewportToolBar)
					.Viewport(ViewportRef)
					.CommandList(LevelEditorModule.GetGlobalLevelEditorActions())
					.Extenders(LevelEditorModule.GetToolBarExtensibilityManager()->GetAllExtenders())
					.Visibility(ViewportRef, &SLevelViewport::GetTransformToolbarVisibility)
				]
				+ SHorizontalBox::Slot()
				.HAlign( HAlign_Right )
				.AutoWidth()
				.Padding( ToolbarButtonPadding )
				[
					//The Maximize/Minimize button is only displayed when not in Immersive mode.
					SNew( SEditorViewportToolBarButton )
					.Cursor( EMouseCursor::Default )
					.ButtonType( EUserInterfaceActionType::ToggleButton )
					.IsChecked( ViewportRef, &SLevelViewport::IsMaximized )
					.OnClicked( ViewportRef, &SLevelViewport::OnToggleMaximize )
					.Visibility( ViewportRef, &SLevelViewport::GetMaximizeToggleVisibility )
					.Image( "LevelViewportToolBar.Maximize" )
					.ToolTipText( LOCTEXT("Maximize_ToolTip", "Maximizes or restores this viewport") )
				]
				+ SHorizontalBox::Slot()
				.HAlign( HAlign_Right )
				.AutoWidth()
				.Padding( ToolbarButtonPadding )
				[
					//The Restore from Immersive' button is only displayed when the editor is in Immersive mode.
					SNew( SEditorViewportToolBarButton )
					.Cursor( EMouseCursor::Default )
					.ButtonType( EUserInterfaceActionType::Button )
					.OnClicked( ViewportRef, &SLevelViewport::OnToggleMaximize )
					.Visibility( ViewportRef, &SLevelViewport::GetCloseImmersiveButtonVisibility )
					.Image( "LevelViewportToolBar.RestoreFromImmersive.Normal" )
					.ToolTipText( LOCTEXT("RestoreFromImmersive_ToolTip", "Restore from Immersive") )
				]
			]
		]
	];

	SViewportToolBar::Construct(SViewportToolBar::FArguments());
}

FText SLevelViewportToolBar::GetCameraMenuLabel() const
{
	FText Label = LOCTEXT("CameraMenuTitle_Default", "Camera");
	TSharedPtr< SLevelViewport > PinnedViewport( Viewport.Pin() );
	if( PinnedViewport.IsValid() )
	{
		switch( PinnedViewport->GetLevelViewportClient().ViewportType )
		{
			case LVT_Perspective:
				Label = LOCTEXT("CameraMenuTitle_Perspective", "Perspective");
				break;

			case LVT_OrthoXY:
				Label = LOCTEXT("CameraMenuTitle_Top", "Top");
				break;

			case LVT_OrthoYZ:
				Label = LOCTEXT("CameraMenuTitle_Side", "Side");
				break;

			case LVT_OrthoXZ:
				Label = LOCTEXT("CameraMenuTitle_Front", "Front");
				break;
		}
	}

	return Label;
}

FText SLevelViewportToolBar::GetDevicePreviewMenuLabel() const
{
	FText Label = LOCTEXT("DevicePreviewMenuTitle_Default", "Preview");

	TSharedPtr< SLevelViewport > PinnedViewport( Viewport.Pin() );
	if( PinnedViewport.IsValid() )
	{
		if ( PinnedViewport->GetDeviceProfileString() != "Default" )
		{
			Label = FText::FromString( PinnedViewport->GetDeviceProfileString() );
		}
	}

	return Label;
}

const FSlateBrush* SLevelViewportToolBar::GetDevicePreviewMenuLabelIcon() const
{
	TSharedRef<SLevelViewport> ViewportRef = Viewport.Pin().ToSharedRef();
	FString DeviceProfileName = ViewportRef->GetDeviceProfileString();

	FName PlatformIcon = NAME_None;
	if( !DeviceProfileName.IsEmpty() && DeviceProfileName != "Default" )
	{
		static FName DeviceProfileServices( "DeviceProfileServices" );

		IDeviceProfileServicesModule& ScreenDeviceProfileUIServices = FModuleManager::LoadModuleChecked<IDeviceProfileServicesModule>(TEXT( "DeviceProfileServices"));
		IDeviceProfileServicesUIManagerPtr UIManager = ScreenDeviceProfileUIServices.GetProfileServicesManager();

		PlatformIcon = UIManager->GetDeviceIconName( DeviceProfileName );

		return FEditorStyle::GetOptionalBrush( PlatformIcon );
	}

	return NULL;
}

const FSlateBrush* SLevelViewportToolBar::GetCameraMenuLabelIcon() const
{
	FName Icon = NAME_None;
	TSharedPtr< SLevelViewport > PinnedViewport( Viewport.Pin() );
	if( PinnedViewport.IsValid() )
	{
		switch( PinnedViewport->GetLevelViewportClient().ViewportType )
		{
			case LVT_Perspective:
				Icon = FName( "EditorViewport.Perspective" );
				break;

			case LVT_OrthoXY:
				Icon = FName( "EditorViewport.Top" );
				break;

			case LVT_OrthoYZ:
				Icon = FName( "EditorViewport.Side" );
				break;

			case LVT_OrthoXZ:
				Icon = FName( "EditorViewport.Front" );
				break;
		}
	}

	return FEditorStyle::GetBrush( Icon );
}






bool SLevelViewportToolBar::IsCurrentLevelViewport() const
{
	TSharedPtr< SLevelViewport > PinnedViewport( Viewport.Pin() );
	if( PinnedViewport.IsValid() )
	{
		if( &( PinnedViewport->GetLevelViewportClient() ) == GCurrentLevelEditingViewportClient )
		{
			return true;
		}
	}
	return false;
}

bool SLevelViewportToolBar::IsPerspectiveViewport() const
{
	TSharedPtr< SLevelViewport > PinnedViewport( Viewport.Pin() );
	if( PinnedViewport.IsValid() )
	{
		if(  PinnedViewport->GetLevelViewportClient().IsPerspective() )
		{
			return true;
		}
	}
	return false;
}

/**
 * Called to generate the set bookmark submenu
 */
static void OnGenerateSetBookmarkMenu( FMenuBuilder& MenuBuilder )
{
	// Add a menu entry for each bookmark
	for( int32 BookmarkIndex = 0; BookmarkIndex < AWorldSettings::MAX_BOOKMARK_NUMBER; ++BookmarkIndex )
	{
		MenuBuilder.AddMenuEntry( FLevelViewportCommands::Get().SetBookmarkCommands[ BookmarkIndex ], NAME_None, FText::Format( LOCTEXT("SetBookmarkOverride", "Bookmark {0}"), FText::AsNumber( BookmarkIndex ) ) );
	}
}

/**
 * Called to generate the clear bookmark submenu
 */
static void OnGenerateClearBookmarkMenu( FMenuBuilder& MenuBuilder , TWeakPtr<class SLevelViewport> Viewport )
{
	// Add a menu entry for each bookmark
	FEditorModeTools& Tools = GEditorModeTools();

	// Get the viewport client to pass down to the CheckBookmark function
	FLevelEditorViewportClient& ViewportClient = Viewport.Pin()->GetLevelViewportClient();	
	for( int32 BookmarkIndex = 0; BookmarkIndex < AWorldSettings::MAX_BOOKMARK_NUMBER; ++BookmarkIndex )
	{
		if ( Tools.CheckBookmark( BookmarkIndex , &ViewportClient ) )
		{
			MenuBuilder.AddMenuEntry( FLevelViewportCommands::Get().ClearBookmarkCommands[ BookmarkIndex ], NAME_None, FText::Format( LOCTEXT("ClearBookmarkOverride", "Bookmark {0}"), FText::AsNumber( BookmarkIndex ) ) );
		}
	}
}

template<typename ActorClass>
static void OnGenerateActorLockingMenuSection( TWeakPtr<SLevelViewport> Viewport, FMenuBuilder& MenuBuilder )
{
	check(ActorClass::StaticClass()->IsChildOf(AActor::StaticClass()));

	// Creates a scene outliner to fill the menu. The outliner lists only actors of class T and its children

	FSceneOutlinerInitializationOptions InitOptions;
	InitOptions.Mode = ESceneOutlinerMode::ActorPicker;			
	InitOptions.bShowHeaderRow = false;
	InitOptions.CustomColumnFixedWidth = SLevelViewport::GetActorLockSceneOutlinerColumnWidth();
	InitOptions.CustomColumnFactory = FCreateSceneOutlinerColumnDelegate::CreateSP( Viewport.Pin().ToSharedRef(), &SLevelViewport::CreateActorLockSceneOutlinerColumn );

	// Predicate for selecting actors by class
	struct FActorClassPredicate
	{
		static bool IsLockableActor( const AActor* InActor )
		{			
			return InActor != NULL && InActor->IsA(ActorClass::StaticClass()) && !InActor->IsPendingKill();
		}
	};

	InitOptions.Filters->AddFilterPredicate( SceneOutliner::FActorFilterPredicate::CreateStatic(&FActorClassPredicate::IsLockableActor) );

	// Create an outliner with the options from above. It sits in a box with a max height limit to stop it getting too tall when lots of actors are added.
	FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>("SceneOutliner");
	TSharedRef< SWidget > MiniSceneOutliner =
		SNew( SVerticalBox )
		+SVerticalBox::Slot()
		.MaxHeight(400.0f)
		[
			SceneOutlinerModule.CreateSceneOutliner(
				InitOptions,
				FOnActorPicked::CreateSP( Viewport.Pin().ToSharedRef(), &SLevelViewport::OnActorLockToggleFromMenu ) )
		];

	// Add the outliner to the menu
	MenuBuilder.AddWidget(MiniSceneOutliner, FText(), true);
}

static void OnGenerateCameraActorLockingMenu( FMenuBuilder& MenuBuilder, TWeakPtr<SLevelViewport> Viewport )
{
	MenuBuilder.BeginSection("LevelViewportCameraActors", LOCTEXT("ActorLockingMenu_CameraActorsHeader", "Camera Actors"));
	{
		OnGenerateActorLockingMenuSection<ACameraActor>(Viewport, MenuBuilder);
	}
	MenuBuilder.EndSection();
}

static void OnGenerateLightActorLockingMenu( FMenuBuilder& MenuBuilder, TWeakPtr<SLevelViewport> Viewport )
{
	MenuBuilder.BeginSection("LevelViewportLightActors", LOCTEXT("ActorLockingMenu_LightActorsHeader", "Light Actors"));
	{
		OnGenerateActorLockingMenuSection<ALight>(Viewport, MenuBuilder);
	}
	MenuBuilder.EndSection();
}

static void OnGenerateActorLockingMenu( FMenuBuilder& MenuBuilder, TWeakPtr<SLevelViewport> Viewport )
{
	const FLevelViewportCommands& Actions = FLevelViewportCommands::Get();
	FLevelEditorViewportClient& ViewportClient = Viewport.Pin()->GetLevelViewportClient();

	bool IsLocked = false;
	if (ViewportClient.GetActiveActorLock().IsValid())
	{
		// Viewport is locked - show the unlock item
		AActor* Actor = ViewportClient.GetActiveActorLock().Get();

		if (!Actor->IsPendingKill())
		{
			MenuBuilder.BeginSection("LevelViewportLocked", LOCTEXT("LockingMenuLocked", "Locked"));
			{
				MenuBuilder.AddMenuEntry(
					Actions.ActorUnlock,
					NAME_None,
					FText::Format( LOCTEXT("UnlockMenuItem", "Unlock from {0}"), FText::FromString( Actor->GetActorLabel() ) ) );
			}
			MenuBuilder.EndSection();

			IsLocked = true;
		}
	}

	if (!IsLocked)
	{
		MenuBuilder.BeginSection("LevelViewportNotLocked", LOCTEXT("LockingMenuNotLocked", "Not Locked"));
		MenuBuilder.EndSection();
	}

	// If a single actor is selected, show an item to lock the viewport to it
	USelection* ActorSelection = GEditor->GetSelectedActors();
	if (1 == ActorSelection->Num() && ActorSelection->GetSelectedObject(0) != NULL)
	{
		MenuBuilder.BeginSection("LevelViewportSelectedActor", LOCTEXT("LockingMenuSelectionHeader", "Selected Actor"));
		{

		    AActor* Actor = CastChecked<AActor>(ActorSelection->GetSelectedObject(0));
    
		    if (Viewport.Pin()->IsSelectedActorLocked())
		    {
			    MenuBuilder.AddMenuEntry(
				    Actions.ActorUnlockSelected,
				    NAME_None,
				    FText::FromString( Actor->GetActorLabel() ) );
			}
			else
			{
				MenuBuilder.AddMenuEntry(
					Actions.ActorLockSelected,
					NAME_None,
					FText::FromString( Actor->GetActorLabel() ) );
			}
		}
		MenuBuilder.EndSection();
	}

	MenuBuilder.BeginSection("LevelViewportCamerasLights");
	{
		// Add sub-menu listing camera actors that can be locked
		MenuBuilder.AddSubMenu( 
			LOCTEXT("ActorLockingCamerasSubMenu", "Cameras"), 
			FText::GetEmpty(), 
			FNewMenuDelegate::CreateStatic( &OnGenerateCameraActorLockingMenu, Viewport )
			);

		// Add sub-menu listing camera actors that can be locked
		MenuBuilder.AddSubMenu( 
			LOCTEXT("ActorLockingLightsSubMenu", "Lights"), 
			FText::GetEmpty(), 
			FNewMenuDelegate::CreateStatic( &OnGenerateLightActorLockingMenu, Viewport )
			);
	}
	MenuBuilder.EndSection();
}

/**
 * Called to generate the bookmark submenu
 */
static void OnGenerateBookmarkMenu( FMenuBuilder& MenuBuilder , TWeakPtr<class SLevelViewport> Viewport )
{
	FEditorModeTools& Tools = GEditorModeTools();

	// true if a bookmark was found. 
	bool bFoundBookmark = false;

	// Get the viewport client to pass down to the CheckBookmark function
	FLevelEditorViewportClient& ViewportClient = Viewport.Pin()->GetLevelViewportClient();

	MenuBuilder.BeginSection("LevelViewportActiveBoookmarks", LOCTEXT("JumpToBookmarkHeader", "Active Bookmarks") );

	for( int32 BookmarkIndex = 0; BookmarkIndex < AWorldSettings::MAX_BOOKMARK_NUMBER; ++BookmarkIndex )
	{
		// Only add bookmarks to the menu if the bookmark is valid
		if ( Tools.CheckBookmark( BookmarkIndex , &ViewportClient ) )
		{
			bFoundBookmark = true;
			MenuBuilder.AddMenuEntry( FLevelViewportCommands::Get().JumpToBookmarkCommands[ BookmarkIndex ] );
		}
	}

	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("LevelViewportBookmarkSubmenus");
	{
		MenuBuilder.AddSubMenu( 
			LOCTEXT("SetBookmarkSubMenu", "Set Bookmark"),
			LOCTEXT("SetBookmarkSubMenu_ToolTip", "Set viewport bookmarks"),
			FNewMenuDelegate::CreateStatic( &OnGenerateSetBookmarkMenu )
			);

		if( bFoundBookmark )
		{
			MenuBuilder.AddSubMenu( 
				LOCTEXT("ClearBookmarkSubMenu", "Clear Bookmark"),
				LOCTEXT("ClearBookmarkSubMenu_ToolTip", "Clear viewport bookmarks"),
				FNewMenuDelegate::CreateStatic( &OnGenerateClearBookmarkMenu , Viewport )
				);

			const FLevelViewportCommands& Actions = FLevelViewportCommands::Get();
			MenuBuilder.AddMenuEntry( Actions.ClearAllBookMarks );
		}
	}
	MenuBuilder.EndSection();
}

TSharedRef<SWidget> SLevelViewportToolBar::GenerateOptionsMenu() const
{
	Viewport.Pin()->OnFloatingButtonClicked();

	const FLevelViewportCommands& LevelViewportActions = FLevelViewportCommands::Get();

	// Get all menu extenders for this context menu from the level editor module
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( TEXT("LevelEditor") );
	TArray<FLevelEditorModule::FLevelEditorMenuExtender> MenuExtenderDelegates = LevelEditorModule.GetAllLevelViewportOptionsMenuExtenders();
	
	TArray<TSharedPtr<FExtender>> Extenders;
	for (int32 i = 0; i < MenuExtenderDelegates.Num(); ++i)
	{
		if (MenuExtenderDelegates[i].IsBound())
		{
			Extenders.Add(MenuExtenderDelegates[i].Execute(Viewport.Pin()->GetCommandList().ToSharedRef()));
		}
	}
	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	const bool bIsPerspective = Viewport.Pin()->GetLevelViewportClient().IsPerspective();
	const bool bIsLocked = Viewport.Pin()->GetLevelViewportClient().IsAnyActorLocked();
	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder OptionsMenuBuilder( bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList(), MenuExtender );
	{
		OptionsMenuBuilder.BeginSection("LevelViewportViewportOptions", LOCTEXT("OptionsMenuHeader", "Viewport Options") );
		{
			OptionsMenuBuilder.AddMenuEntry( FEditorViewportCommands::Get().ToggleRealTime );
			OptionsMenuBuilder.AddMenuEntry( FEditorViewportCommands::Get().ToggleStats );
			OptionsMenuBuilder.AddMenuEntry( FEditorViewportCommands::Get().ToggleFPS );

			if( bIsPerspective )
			{
				OptionsMenuBuilder.AddWidget( GenerateFOVMenu(), LOCTEXT("FOVAngle", "Field of View") );
				OptionsMenuBuilder.AddWidget( GenerateFarViewPlaneMenu(), LOCTEXT("FarViewPlane", "Far View Plane") );
			}
		}
		OptionsMenuBuilder.EndSection();

		OptionsMenuBuilder.BeginSection("LevelViewportViewportOptions2");
		{
			if( bIsPerspective )
			{
				// Allow matinee preview only applies to perspective
				OptionsMenuBuilder.AddMenuEntry( LevelViewportActions.AllowMatineePreview );
			}

			OptionsMenuBuilder.AddMenuEntry( LevelViewportActions.ToggleGameView );
			OptionsMenuBuilder.AddMenuEntry( LevelViewportActions.ToggleImmersive );
		}
		OptionsMenuBuilder.EndSection();

		if( (bIsPerspective || bIsLocked) && !Viewport.Pin()->GetLevelViewportClient().IsLockedToMatinee())
		{
			OptionsMenuBuilder.BeginSection("LevelViewportActorLocking");
			{
				OptionsMenuBuilder.AddSubMenu( 
					LOCTEXT("ActorLockingSubMenu", "Lock Viewport to Actor"), 
					LOCTEXT("ActorLockingSubMenu_ToolTip", "Lock Viewport position and orientation to Cameras, Lights or other scene actors"), 
					FNewMenuDelegate::CreateStatic( &OnGenerateActorLockingMenu, Viewport )
					);
			}
			OptionsMenuBuilder.EndSection();
		}

		if( bIsPerspective )
		{
			// Bookmarks only work in perspective viewports so only show the menu option if this toolbar is in one

			OptionsMenuBuilder.BeginSection("LevelViewportBookmarks");
			{
				OptionsMenuBuilder.AddSubMenu( 
					LOCTEXT("BookmarkSubMenu", "Bookmarks"), 
					LOCTEXT("BookmarkSubMenu_ToolTip", "Viewport location bookmarking"), 
					FNewMenuDelegate::CreateStatic( &OnGenerateBookmarkMenu , Viewport )
					);
			}
			OptionsMenuBuilder.EndSection();

			OptionsMenuBuilder.AddMenuEntry( LevelViewportActions.CreateCamera );
		}

		OptionsMenuBuilder.AddMenuEntry( LevelViewportActions.HighResScreenshot );

		OptionsMenuBuilder.BeginSection("LevelViewportLayouts");
		{
			OptionsMenuBuilder.AddSubMenu(
				LOCTEXT("ConfigsSubMenu", "Layouts"),
				FText::GetEmpty(),
				FNewMenuDelegate::CreateSP(this, &SLevelViewportToolBar::GenerateViewportConfigsMenu));
		}
		OptionsMenuBuilder.EndSection();

		OptionsMenuBuilder.BeginSection("LevelViewportSettings");
		{
			OptionsMenuBuilder.AddMenuEntry( LevelViewportActions.AdvancedSettings );
		}
		OptionsMenuBuilder.EndSection();
	}

	return OptionsMenuBuilder.MakeWidget();
}


TSharedRef<SWidget> SLevelViewportToolBar::GenerateDevicePreviewMenu() const
{
	IDeviceProfileServicesModule& ScreenDeviceProfileUIServices = FModuleManager::LoadModuleChecked<IDeviceProfileServicesModule>(TEXT( "DeviceProfileServices"));
	IDeviceProfileServicesUIManagerPtr UIManager = ScreenDeviceProfileUIServices.GetProfileServicesManager();

	// Create the menu
	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder DeviceMenuBuilder( bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList() );

	DeviceMenuBuilder.BeginSection( "DevicePreview", LOCTEXT("DevicePreviewMenuTitle", "Device Preview") );

	TSharedRef<SLevelViewport> ViewportRef = Viewport.Pin().ToSharedRef();

	// Default menu - clear all settings
	{
		FUIAction Action( FExecuteAction::CreateSP( this, &SLevelViewportToolBar::SetLevelProfile, FString( "Default" ) ),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP( ViewportRef, &SLevelViewport::IsDeviceProfileStringSet, FString( "Default" ) ) );
		DeviceMenuBuilder.AddMenuEntry( LOCTEXT("DevicePreviewMenuClear", "Off"), FText::GetEmpty(), FSlateIcon(), Action, NAME_None, EUserInterfaceActionType::Button );
		}

	DeviceMenuBuilder.EndSection();

	// Recent Device Profiles	
	DeviceMenuBuilder.BeginSection( "Recent", LOCTEXT("RecentMenuHeading", "Recent") );

	const FString INISection = "SelectedProfile";
	const FString INIKeyBase = "ProfileItem";
	const int32 MaxItems = 4; // Move this into a config file
	FString CurItem;
	for( int32 ItemIdx = 0 ; ItemIdx < MaxItems; ++ItemIdx )
	{
		// Build the menu from the contents of the game ini
		//@todo This should probably be using GConfig->GetText [10/21/2013 justin.sargent]
		if ( GConfig->GetString( *INISection, *FString::Printf( TEXT("%s%d"), *INIKeyBase, ItemIdx ), CurItem, GEditorUserSettingsIni ) )
		{
			const FName PlatformIcon = UIManager->GetDeviceIconName( CurItem );

			FUIAction Action( FExecuteAction::CreateSP( this, &SLevelViewportToolBar::SetLevelProfile, CurItem ),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP( ViewportRef, &SLevelViewport::IsDeviceProfileStringSet, CurItem ) );
			DeviceMenuBuilder.AddMenuEntry( FText::FromString( CurItem ), FText(), FSlateIcon(FEditorStyle::GetStyleSetName(), PlatformIcon), Action, NAME_None, EUserInterfaceActionType::Button );
		}
	}

	DeviceMenuBuilder.EndSection();

	// Device List
	DeviceMenuBuilder.BeginSection( "Devices", LOCTEXT("DevicesMenuHeading", "Devices") );

	const TArray<TSharedPtr<FString> > PlatformList = UIManager->GetPlatformList();
	for ( int32 Index = 0; Index < PlatformList.Num(); Index++ )
	{
		TArray< UDeviceProfile* > DeviceProfiles;
		UIManager->GetProfilesByType( DeviceProfiles, *PlatformList[Index] );
		if ( DeviceProfiles.Num() > 0 )
		{
			const FString PlatformNameStr = DeviceProfiles[0]->DeviceType;
			const FName PlatformIcon =  UIManager->GetPlatformIconName( PlatformNameStr );
			DeviceMenuBuilder.AddSubMenu(
				FText::FromString( PlatformNameStr ),
				FText::GetEmpty(),
				FNewMenuDelegate::CreateRaw( this, &SLevelViewportToolBar::MakeDevicePreviewSubMenu, DeviceProfiles ),
				false,
				FSlateIcon(FEditorStyle::GetStyleSetName(), PlatformIcon)
				);
		}
	}
	DeviceMenuBuilder.EndSection();

	return DeviceMenuBuilder.MakeWidget();
}

void SLevelViewportToolBar::MakeDevicePreviewSubMenu(FMenuBuilder& MenuBuilder, TArray< class UDeviceProfile* > InProfiles )
{
	TSharedRef<SLevelViewport> ViewportRef = Viewport.Pin().ToSharedRef();

	for ( int32 Index = 0; Index < InProfiles.Num(); Index++ )
	{
		FUIAction Action( FExecuteAction::CreateSP( this, &SLevelViewportToolBar::SetLevelProfile, InProfiles[ Index ]->GetName() ),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP( ViewportRef, &SLevelViewport::IsDeviceProfileStringSet, InProfiles[ Index ]->GetName() ) );

		MenuBuilder.AddMenuEntry( FText::FromString( InProfiles[ Index ]->GetName() ), FText(), FSlateIcon(), Action, NAME_None, EUserInterfaceActionType::RadioButton );
	}
}

void SLevelViewportToolBar::SetLevelProfile( FString DeviceProfileName )
{
	TSharedRef<SLevelViewport> ViewportRef = Viewport.Pin().ToSharedRef();
	ViewportRef->SetDeviceProfileString( DeviceProfileName );

	IDeviceProfileServicesModule& ScreenDeviceProfileUIServices = FModuleManager::LoadModuleChecked<IDeviceProfileServicesModule>(TEXT( "DeviceProfileServices"));
	IDeviceProfileServicesUIManagerPtr UIManager = ScreenDeviceProfileUIServices.GetProfileServicesManager();
	UIManager->SetProfile( DeviceProfileName );
}


TSharedRef<SWidget> SLevelViewportToolBar::GenerateCameraMenu() const
{
	Viewport.Pin()->OnFloatingButtonClicked();

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder CameraMenuBuilder( bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList() );

	// Camera types
	CameraMenuBuilder.AddMenuEntry( FEditorViewportCommands::Get().Perspective );

	CameraMenuBuilder.BeginSection("LevelViewportCameraType_Ortho", LOCTEXT("CameraTypeHeader_Ortho", "Othographic") );
		CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Top);
		CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Side);
		CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Front);
	CameraMenuBuilder.EndSection();

	return CameraMenuBuilder.MakeWidget();
}

void SLevelViewportToolBar::GenerateViewportConfigsMenu(FMenuBuilder& MenuBuilder) const
{
	check (Viewport.IsValid());
	TSharedPtr<FUICommandList> CommandList = Viewport.Pin()->GetCommandList();

	MenuBuilder.BeginSection("LevelViewportOnePaneConfigs", LOCTEXT("OnePaneConfigHeader", "One Pane"));
	{
		FToolBarBuilder OnePaneButton(CommandList, FMultiBoxCustomization::None);
		OnePaneButton.SetLabelVisibility(EVisibility::Collapsed);
		OnePaneButton.SetStyle(&FEditorStyle::Get(), "ViewportLayoutToolbar");

		OnePaneButton.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_OnePane);

		MenuBuilder.AddWidget(
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				OnePaneButton.MakeWidget()
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNullWidget::NullWidget
			],
			FText::GetEmpty(), true
			);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("LevelViewportTwoPaneConfigs", LOCTEXT("TwoPaneConfigHeader", "Two Panes") );
	{
		FToolBarBuilder TwoPaneButtons(CommandList, FMultiBoxCustomization::None);
		TwoPaneButtons.SetLabelVisibility(EVisibility::Collapsed);
		TwoPaneButtons.SetStyle(&FEditorStyle::Get(), "ViewportLayoutToolbar");

		TwoPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_TwoPanesH, NAME_None, FText());
		TwoPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_TwoPanesV, NAME_None, FText());

		MenuBuilder.AddWidget(
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				TwoPaneButtons.MakeWidget()
			]
			+SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNullWidget::NullWidget
			],
			FText::GetEmpty(), true
			);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("LevelViewportThreePaneConfigs", LOCTEXT("ThreePaneConfigHeader", "Three Panes") );
	{
		FToolBarBuilder ThreePaneButtons(CommandList, FMultiBoxCustomization::None);
		ThreePaneButtons.SetLabelVisibility(EVisibility::Collapsed);
		ThreePaneButtons.SetStyle(&FEditorStyle::Get(), "ViewportLayoutToolbar");

		ThreePaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_ThreePanesLeft, NAME_None, FText());
		ThreePaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_ThreePanesRight, NAME_None, FText());
		ThreePaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_ThreePanesTop, NAME_None, FText());
		ThreePaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_ThreePanesBottom, NAME_None, FText());

		MenuBuilder.AddWidget(
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				ThreePaneButtons.MakeWidget()
			]
			+SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNullWidget::NullWidget
			],
			FText::GetEmpty(), true
			);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("LevelViewportFourPaneConfigs", LOCTEXT("FourPaneConfigHeader", "Four Panes") );
	{
		FToolBarBuilder FourPaneButtons(CommandList, FMultiBoxCustomization::None);
		FourPaneButtons.SetLabelVisibility(EVisibility::Collapsed);
		FourPaneButtons.SetStyle(&FEditorStyle::Get(), "ViewportLayoutToolbar");

		FourPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_FourPanes2x2, NAME_None, FText());
		FourPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_FourPanesLeft, NAME_None, FText());
		FourPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_FourPanesRight, NAME_None, FText());
		FourPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_FourPanesTop, NAME_None, FText());
		FourPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_FourPanesBottom, NAME_None, FText());	

		MenuBuilder.AddWidget(
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				FourPaneButtons.MakeWidget()
			]
			+SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNullWidget::NullWidget
			],
			FText::GetEmpty(), true
			);
	}
	MenuBuilder.EndSection();
}



TSharedRef<SWidget> SLevelViewportToolBar::GenerateShowMenu() const
{
	Viewport.Pin()->OnFloatingButtonClicked();

	const FLevelViewportCommands& Actions = FLevelViewportCommands::Get();

	const TArray<FShowFlagData>& ShowFlagData = GetShowFlagMenuItems();

	TArray< FLevelViewportCommands::FShowMenuCommand > ShowMenu[SFG_Max];

	// Get each show flag command and put them in their corresponding groups
	for( int32 ShowFlag = 0; ShowFlag < ShowFlagData.Num(); ++ShowFlag )
	{
		const FShowFlagData& SFData = ShowFlagData[ShowFlag];
		
		ShowMenu[SFData.Group].Add( Actions.ShowFlagCommands[ ShowFlag ] );
	}

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ShowMenuBuilder( bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList() );
	{
		ShowMenuBuilder.AddMenuEntry( Actions.UseDefaultShowFlags );
		
		if( ShowMenu[SFG_Normal].Num() > 0 )
		{
			// Generate entries for the standard show flags
			ShowMenuBuilder.BeginSection("LevelViewportShowFlagsCommon", LOCTEXT("CommonShowFlagHeader", "Common") );
			{
				for( int32 EntryIndex = 0; EntryIndex < ShowMenu[SFG_Normal].Num(); ++EntryIndex )
				{
					ShowMenuBuilder.AddMenuEntry( ShowMenu[SFG_Normal][ EntryIndex ].ShowMenuItem, NAME_None, ShowMenu[SFG_Normal][ EntryIndex ].LabelOverride );
				}
			}
			ShowMenuBuilder.EndSection();
		}

		struct FLocal
		{
			static void FillShowMenu( class FMenuBuilder& MenuBuilder, TArray< FLevelViewportCommands::FShowMenuCommand > MenuCommands, int32 EntryOffset )
			{
				// Generate entries for the standard show flags
				// Assumption: the first 'n' entries types like 'Show All' and 'Hide All' buttons, so insert a separator after them
				for (int32 EntryIndex = 0; EntryIndex < MenuCommands.Num(); ++EntryIndex)
				{
					MenuBuilder.AddMenuEntry(MenuCommands[EntryIndex].ShowMenuItem, NAME_None, MenuCommands[EntryIndex].LabelOverride);
					if (EntryIndex == EntryOffset-1)
					{
						MenuBuilder.AddMenuSeparator();
					}
				}
			}

			static void FillShowStatsSubMenus(class FMenuBuilder& MenuBuilder, TArray< FLevelViewportCommands::FShowMenuCommand > MenuCommands, TMap< FString, TArray< FLevelViewportCommands::FShowMenuCommand > > StatCatCommands)
			{
				FLocal::FillShowMenu(MenuBuilder, MenuCommands, 1);

				// Separate out stats into two list, those with and without submenus
				TArray< FLevelViewportCommands::FShowMenuCommand > SingleStatCommands;
				TMap< FString, TArray< FLevelViewportCommands::FShowMenuCommand > > SubbedStatCommands;
				for (auto StatCatIt = StatCatCommands.CreateConstIterator(); StatCatIt; ++StatCatIt)
				{
					const TArray< FLevelViewportCommands::FShowMenuCommand >& ShowStatCommands = StatCatIt.Value();
					const FString& CategoryName = StatCatIt.Key();

					// If no category is specified, or there's only one category, don't use submenus
					FString NoCategory = FStatConstants::NAME_NoCategory.ToString();
					NoCategory.RemoveFromStart(TEXT("STATCAT_"));
					if (CategoryName == NoCategory || StatCatCommands.Num() == 1)
					{
						for (int32 StatIndex = 0; StatIndex < ShowStatCommands.Num(); ++StatIndex)
						{
							const FLevelViewportCommands::FShowMenuCommand& StatCommand = ShowStatCommands[StatIndex];
							SingleStatCommands.Add(StatCommand);
						}
					}
					else
					{
						SubbedStatCommands.Add(CategoryName, ShowStatCommands);
					}
				}

				// First add all the stats that don't have a sub menu
				for (auto StatCatIt = SingleStatCommands.CreateConstIterator(); StatCatIt; ++StatCatIt)
				{
					const FLevelViewportCommands::FShowMenuCommand& StatCommand = *StatCatIt;
					MenuBuilder.AddMenuEntry(StatCommand.ShowMenuItem, NAME_None, StatCommand.LabelOverride);
				}

				// Now add all the stats that have sub menus
				for (auto StatCatIt = SubbedStatCommands.CreateConstIterator(); StatCatIt; ++StatCatIt)
				{
					const TArray< FLevelViewportCommands::FShowMenuCommand >& StatCommands = StatCatIt.Value();
					const FText CategoryName = FText::FromString(StatCatIt.Key());

					FFormatNamedArguments Args;
					Args.Add(TEXT("StatCat"), CategoryName);
					const FText CategoryDescription = FText::Format(NSLOCTEXT("UICommands", "StatShowCatName", "Show {StatCat} stats"), Args);

					MenuBuilder.AddSubMenu(CategoryName, CategoryDescription,
						FNewMenuDelegate::CreateStatic(&FLocal::FillShowMenu, StatCommands, 0));
				}
			}
		};

		// Generate entries for the different show flags groups
		ShowMenuBuilder.BeginSection("LevelViewportShowFlags");
		{
			ShowMenuBuilder.AddSubMenu( LOCTEXT("PostProcessShowFlagsMenu", "Post Processing"), LOCTEXT("PostProcessShowFlagsMenu_ToolTip", "Post process show flags"),
				FNewMenuDelegate::CreateStatic( &FLocal::FillShowMenu, ShowMenu[SFG_PostProcess], 0 ) );

			ShowMenuBuilder.AddSubMenu( LOCTEXT("LightingComponentsShowFlagsMenu", "Lighting Components"), LOCTEXT("LightingComponentsShowFlagsMenu_ToolTip", "Lighting Components show flags"),
				FNewMenuDelegate::CreateStatic( &FLocal::FillShowMenu, ShowMenu[SFG_LightingComponents], 0 ) );

			ShowMenuBuilder.AddSubMenu( LOCTEXT("LightingFeaturesShowFlagsMenu", "Lighting Features"), LOCTEXT("LightingFeaturesShowFlagsMenu_ToolTip", "Lighting Features show flags"),
				FNewMenuDelegate::CreateStatic( &FLocal::FillShowMenu, ShowMenu[SFG_LightingFeatures], 0 ) );

			ShowMenuBuilder.AddSubMenu( LOCTEXT("DeveloperShowFlagsMenu", "Developer"), LOCTEXT("DeveloperShowFlagsMenu_ToolTip", "Developer show flags"),
				FNewMenuDelegate::CreateStatic( &FLocal::FillShowMenu, ShowMenu[SFG_Developer], 0 ) );

			ShowMenuBuilder.AddSubMenu( LOCTEXT("VisualizeShowFlagsMenu", "Visualize"), LOCTEXT("VisualizeShowFlagsMenu_ToolTip", "Visualize show flags"),
				FNewMenuDelegate::CreateStatic( &FLocal::FillShowMenu, ShowMenu[SFG_Visualize], 0 ) );

			ShowMenuBuilder.AddSubMenu( LOCTEXT("AdvancedShowFlagsMenu", "Advanced"), LOCTEXT("AdvancedShowFlagsMenu_ToolTip", "Advanced show flags"),
				FNewMenuDelegate::CreateStatic( &FLocal::FillShowMenu, ShowMenu[SFG_Advanced], 0 ) );
		}
		ShowMenuBuilder.EndSection();


		FText ShowAllLabel = LOCTEXT("ShowAllLabel", "Show All");
		FText HideAllLabel = LOCTEXT("HideAllLabel", "Hide All");

		// Show Volumes sub-menu
		{
			TArray< FLevelViewportCommands::FShowMenuCommand > ShowVolumesMenu;

			// 'Show All' and 'Hide All' buttons
			ShowVolumesMenu.Add( FLevelViewportCommands::FShowMenuCommand( Actions.ShowAllVolumes, ShowAllLabel ) );
			ShowVolumesMenu.Add( FLevelViewportCommands::FShowMenuCommand( Actions.HideAllVolumes, HideAllLabel ) );

			// Get each show flag command and put them in their corresponding groups
			ShowVolumesMenu += Actions.ShowVolumeCommands;

			ShowMenuBuilder.AddSubMenu( LOCTEXT("ShowVolumesMenu", "Volumes"), LOCTEXT("ShowVolumesMenu_ToolTip", "Show volumes flags"),
				FNewMenuDelegate::CreateStatic( &FLocal::FillShowMenu, ShowVolumesMenu, 2 ) );
		}

		// Show Layers sub-menu is dynamically generated when the user enters 'show' menu
		{
			ShowMenuBuilder.AddSubMenu( LOCTEXT("ShowLayersMenu", "Layers"), LOCTEXT("ShowLayersMenu_ToolTip", "Show layers flags"),
				FNewMenuDelegate::CreateStatic( &SLevelViewportToolBar::FillShowLayersMenu, Viewport ) );
		}

		// Show Sprites sub-menu
		{
			TArray< FLevelViewportCommands::FShowMenuCommand > ShowSpritesMenu;

			// 'Show All' and 'Hide All' buttons
			ShowSpritesMenu.Add( FLevelViewportCommands::FShowMenuCommand( Actions.ShowAllSprites, ShowAllLabel ) );
			ShowSpritesMenu.Add( FLevelViewportCommands::FShowMenuCommand( Actions.HideAllSprites, HideAllLabel ) );

			// Get each show flag command and put them in their corresponding groups
			ShowSpritesMenu += Actions.ShowSpriteCommands;

			ShowMenuBuilder.AddSubMenu( LOCTEXT("ShowSpritesMenu", "Sprites"), LOCTEXT("ShowSpritesMenu_ToolTip", "Show sprites flags"),
				FNewMenuDelegate::CreateStatic( &FLocal::FillShowMenu, ShowSpritesMenu, 2 ) );
		}

		// Show Stats sub-menu
		{
			TArray< FLevelViewportCommands::FShowMenuCommand > HideStatsMenu;

			// 'Hide All' button
			HideStatsMenu.Add(FLevelViewportCommands::FShowMenuCommand(Actions.HideAllStats, HideAllLabel));

			ShowMenuBuilder.AddSubMenu(LOCTEXT("ShowStatsMenu", "Stat"), LOCTEXT("ShowStatsMenu_ToolTip", "Show Stat commands"),
				FNewMenuDelegate::CreateStatic(&FLocal::FillShowStatsSubMenus, HideStatsMenu, Actions.ShowStatCatCommands));
		}
	}

	return ShowMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SLevelViewportToolBar::GenerateFOVMenu() const
{
	const float FOVMin = 5.f;
	const float FOVMax = 170.f;

	return
		SNew( SBox )
		.HAlign( HAlign_Right )
		[
			SNew( SBox )
			.Padding( FMargin(4.0f, 0.0f, 0.0f, 0.0f) )
			.WidthOverride( 100.0f )
			[
				SNew(SSpinBox<float>)
				.Font( FEditorStyle::GetFontStyle( TEXT( "MenuItem.Font" ) ) )
				.MinValue(FOVMin)
				.MaxValue(FOVMax)
				.Value( this, &SLevelViewportToolBar::OnGetFOVValue )
				.OnValueChanged( this, &SLevelViewportToolBar::OnFOVValueChanged )
			]
		];
}

float SLevelViewportToolBar::OnGetFOVValue( ) const
{
	return Viewport.Pin()->GetLevelViewportClient().ViewFOV;
}

void SLevelViewportToolBar::OnFOVValueChanged( float NewValue )
{
	bool bUpdateStoredFOV = true;
	FLevelEditorViewportClient& ViewportClient = Viewport.Pin()->GetLevelViewportClient();
	if (ViewportClient.GetActiveActorLock().IsValid())
	{
		ACameraActor* CameraActor = Cast< ACameraActor >( ViewportClient.GetActiveActorLock().Get() );
		if( CameraActor != NULL )
		{
			CameraActor->CameraComponent->FieldOfView = NewValue;
			bUpdateStoredFOV = false;
		}
	}

	if ( bUpdateStoredFOV )
	{
		ViewportClient.FOVAngle = NewValue;
	}

	ViewportClient.ViewFOV = NewValue;
	ViewportClient.Invalidate();
}

TSharedRef<SWidget> SLevelViewportToolBar::GenerateFarViewPlaneMenu() const
{
	return
		SNew(SBox)
		.HAlign(HAlign_Right)
		[
			SNew(SBox)
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			.WidthOverride(100.0f)
			[
				SNew(SSpinBox<float>)
				.ToolTipText(LOCTEXT("FarViewPlaneTooltip", "Distance to use as the far view plane, or zero to enable an infinite far view plane"))
				.MinValue(0.0f)
				.MaxValue(100000.0f)
				.Font(FEditorStyle::GetFontStyle(TEXT("MenuItem.Font")))
				.Value(this, &SLevelViewportToolBar::OnGetFarViewPlaneValue)
				.OnValueChanged(this, &SLevelViewportToolBar::OnFarViewPlaneValueChanged)
			]
		];
}

float SLevelViewportToolBar::OnGetFarViewPlaneValue() const
{
	return Viewport.Pin()->GetLevelViewportClient().GetFarClipPlaneOverride();
}

void SLevelViewportToolBar::OnFarViewPlaneValueChanged( float NewValue )
{
	Viewport.Pin()->GetLevelViewportClient().OverrideFarClipPlane(NewValue);
}

void SLevelViewportToolBar::FillShowLayersMenu( FMenuBuilder& MenuBuilder, TWeakPtr<class SLevelViewport> Viewport )
{	
	MenuBuilder.BeginSection("LevelViewportLayers");
	{
		MenuBuilder.AddMenuEntry( FLevelViewportCommands::Get().ShowAllLayers, NAME_None, LOCTEXT("ShowAllLabel", "Show All"));
		MenuBuilder.AddMenuEntry( FLevelViewportCommands::Get().HideAllLayers, NAME_None, LOCTEXT("HideAllLabel", "Hide All") );
	}
	MenuBuilder.EndSection();

	if( Viewport.IsValid() )
	{
		TSharedRef<SLevelViewport> ViewportRef = Viewport.Pin().ToSharedRef();
		MenuBuilder.BeginSection("LevelViewportLayers2");
		{
			// Get all the layers and create an entry for each of them
			TArray<FName> AllLayerNames;

				GEditor->Layers->AddAllLayerNamesTo(AllLayerNames);

			for( int32 LayerIndex = 0; LayerIndex < AllLayerNames.Num(); ++LayerIndex )
			{
				const FName LayerName = AllLayerNames[LayerIndex];
				//const FString LayerNameString = LayerName;

				FUIAction Action( FExecuteAction::CreateSP( ViewportRef, &SLevelViewport::ToggleShowLayer, LayerName ),
						FCanExecuteAction(),
						FIsActionChecked::CreateSP( ViewportRef, &SLevelViewport::IsLayerVisible, LayerName ) );

				MenuBuilder.AddMenuEntry( FText::FromName( LayerName ), FText::GetEmpty(), FSlateIcon(), Action, NAME_None, EUserInterfaceActionType::ToggleButton );
			}
		}
		MenuBuilder.EndSection();
	}
}


TWeakObjectPtr<UWorld> SLevelViewportToolBar::GetWorld() const
{
	if (Viewport.IsValid())
	{
		return Viewport.Pin()->GetWorld();
	}
	return NULL;
}

TSharedPtr<FExtender> SLevelViewportToolBar::GetViewMenuExtender()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<FExtender> LevelEditorExtenders = LevelEditorModule.GetMenuExtensibilityManager()->GetAllExtenders();

	TSharedRef<FExtender> Extender(new FExtender());
	Extender->AddMenuExtension(
		TEXT("ViewMode"),
		EExtensionHook::After,
		Viewport.Pin()->GetCommandList(),
		FMenuExtensionDelegate::CreateSP(this, &SLevelViewportToolBar::CreateViewMenuExtensions));

	TArray<TSharedPtr<FExtender>> Extenders;
	Extenders.Reserve(2);
	Extenders.Add(LevelEditorExtenders);
	Extenders.Add(Extender);

	return FExtender::Combine(Extenders);
}

static void BuildBufferVisualizationMenu( FMenuBuilder& Menu )
{
	Menu.BeginSection("LevelViewportBufferVisualizationMode", LOCTEXT( "BufferVisualizationHeader", "Buffer Visualization Mode" ) );
	{
		struct FMaterialIterator
		{
			FMenuBuilder& Menu;
			const FLevelViewportCommands& Actions;
			int32 CurrentMaterial;

			FMaterialIterator(FMenuBuilder& InMenu, const FLevelViewportCommands& InActions)
			: Menu (InMenu)
			, Actions (InActions)
			, CurrentMaterial (0)
			{
			}

			void ProcessValue(const FString& InMaterialName, const UMaterial* InMaterial, const FText& InDisplayNameText )
			{
				FName ViewportCommandName = *(FString(TEXT("BufferVisualizationMenu")) + InMaterialName);
				const FLevelViewportCommands::FBufferVisualizationRecord* Record = Actions.BufferVisualizationModeCommands.Find(ViewportCommandName);
				if ( ensureMsgf(Record != NULL, TEXT("BufferVisualizationMenu doesn't contain entry [%s]"), *ViewportCommandName.ToString()) )
				{
					Menu.AddMenuEntry( Record->Command, NAME_None, InDisplayNameText );
				}
			}
		};

		const FLevelViewportCommands& Actions = FLevelViewportCommands::Get();
		
		Menu.AddMenuEntry(Actions.BufferVisualizationModeCommands.Find(FName(TEXT("BufferVisualizationOverview")))->Command, NAME_None, LOCTEXT("BufferVisualization", "Overview"));
		Menu.AddMenuSeparator();

		FMaterialIterator It(Menu, Actions);
		GetBufferVisualizationData().IterateOverAvailableMaterials(It);
	}
	Menu.EndSection();
}	

void SLevelViewportToolBar::CreateViewMenuExtensions(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("LevelViewportDeferredRendering", LOCTEXT("DeferredRenderingHeader", "Deferred Rendering") );
	MenuBuilder.EndSection();

	MenuBuilder.AddSubMenu( LOCTEXT("VisualizeBufferViewModeDisplayName", "Buffer Visualization"), LOCTEXT("BufferVisualizationMenu_ToolTip", "Select a mode for buffer visualization"), FNewMenuDelegate::CreateStatic( &BuildBufferVisualizationMenu ), /*Default*/false, FSlateIcon(FEditorStyle::GetStyleSetName(), "EditorViewport.VisualizeBufferMode" ));

	MenuBuilder.BeginSection("LevelViewportCollision", LOCTEXT("CollisionViewModeHeader", "Collision") );
	{
		MenuBuilder.AddMenuEntry( FEditorViewportCommands::Get().CollisionPawn, NAME_None, LOCTEXT("CollisionPawnViewModeDisplayName", "Player Collision") );
		MenuBuilder.AddMenuEntry( FEditorViewportCommands::Get().CollisionVisibility, NAME_None, LOCTEXT("CollisionVisibilityViewModeDisplayName", "Visibility Collision") );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("LevelViewportLandscape", LOCTEXT("LandscapeHeader", "Landscape") );
	{
		struct Local
		{
			static void BuildLandscapeLODMenu(FMenuBuilder& Menu, SLevelViewportToolBar* Toolbar)
			{
				Menu.BeginSection("LevelViewportLandScapeLOD", LOCTEXT("LandscapeLODHeader", "Landscape LOD"));
				{
					static const FText FormatString = LOCTEXT("LandscapeLODFixed", "Fixed at {0}");
					Menu.AddMenuEntry(LOCTEXT("LandscapeLODAuto", "Auto"), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(Toolbar, &SLevelViewportToolBar::OnLandscapeLODChanged, -1), FCanExecuteAction(), FIsActionChecked::CreateSP(Toolbar, &SLevelViewportToolBar::IsLandscapeLODSettingChecked, -1)), NAME_None, EUserInterfaceActionType::RadioButton);
					Menu.AddMenuEntry(FText::Format(FormatString, FText::AsNumber(0)), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(Toolbar, &SLevelViewportToolBar::OnLandscapeLODChanged, 0), FCanExecuteAction(), FIsActionChecked::CreateSP(Toolbar, &SLevelViewportToolBar::IsLandscapeLODSettingChecked, 0)), NAME_None, EUserInterfaceActionType::RadioButton);
					Menu.AddMenuEntry(FText::Format(FormatString, FText::AsNumber(1)), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(Toolbar, &SLevelViewportToolBar::OnLandscapeLODChanged, 1), FCanExecuteAction(), FIsActionChecked::CreateSP(Toolbar, &SLevelViewportToolBar::IsLandscapeLODSettingChecked, 1)), NAME_None, EUserInterfaceActionType::RadioButton);
					Menu.AddMenuEntry(FText::Format(FormatString, FText::AsNumber(2)), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(Toolbar, &SLevelViewportToolBar::OnLandscapeLODChanged, 2), FCanExecuteAction(), FIsActionChecked::CreateSP(Toolbar, &SLevelViewportToolBar::IsLandscapeLODSettingChecked, 2)), NAME_None, EUserInterfaceActionType::RadioButton);
					Menu.AddMenuEntry(FText::Format(FormatString, FText::AsNumber(3)), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(Toolbar, &SLevelViewportToolBar::OnLandscapeLODChanged, 3), FCanExecuteAction(), FIsActionChecked::CreateSP(Toolbar, &SLevelViewportToolBar::IsLandscapeLODSettingChecked, 3)), NAME_None, EUserInterfaceActionType::RadioButton);
					Menu.AddMenuEntry(FText::Format(FormatString, FText::AsNumber(4)), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(Toolbar, &SLevelViewportToolBar::OnLandscapeLODChanged, 4), FCanExecuteAction(), FIsActionChecked::CreateSP(Toolbar, &SLevelViewportToolBar::IsLandscapeLODSettingChecked, 4)), NAME_None, EUserInterfaceActionType::RadioButton);
					Menu.AddMenuEntry(FText::Format(FormatString, FText::AsNumber(5)), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(Toolbar, &SLevelViewportToolBar::OnLandscapeLODChanged, 5), FCanExecuteAction(), FIsActionChecked::CreateSP(Toolbar, &SLevelViewportToolBar::IsLandscapeLODSettingChecked, 5)), NAME_None, EUserInterfaceActionType::RadioButton);
					Menu.AddMenuEntry(FText::Format(FormatString, FText::AsNumber(6)), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(Toolbar, &SLevelViewportToolBar::OnLandscapeLODChanged, 6), FCanExecuteAction(), FIsActionChecked::CreateSP(Toolbar, &SLevelViewportToolBar::IsLandscapeLODSettingChecked, 6)), NAME_None, EUserInterfaceActionType::RadioButton);
					Menu.AddMenuEntry(FText::Format(FormatString, FText::AsNumber(7)), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(Toolbar, &SLevelViewportToolBar::OnLandscapeLODChanged, 7), FCanExecuteAction(), FIsActionChecked::CreateSP(Toolbar, &SLevelViewportToolBar::IsLandscapeLODSettingChecked, 7)), NAME_None, EUserInterfaceActionType::RadioButton);
				}
				Menu.EndSection();
			}
		};

		MenuBuilder.AddSubMenu(LOCTEXT("LandscapeLODDisplayName", "LOD"), LOCTEXT("LandscapeLODMenu_ToolTip", "Override Landscape LOD in this viewport"), FNewMenuDelegate::CreateStatic(&Local::BuildLandscapeLODMenu, this), /*Default*/false, FSlateIcon());
	}
	MenuBuilder.EndSection();
}


bool SLevelViewportToolBar::IsLandscapeLODSettingChecked(int32 Value) const
{
	return Viewport.Pin()->GetLevelViewportClient().LandscapeLODOverride == Value;
}

void SLevelViewportToolBar::OnLandscapeLODChanged(int32 NewValue)
{
	FLevelEditorViewportClient& ViewportClient = Viewport.Pin()->GetLevelViewportClient();
	ViewportClient.LandscapeLODOverride = NewValue;
	ViewportClient.Invalidate();
}


#undef LOCTEXT_NAMESPACE
