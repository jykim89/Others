// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "LevelEditor.h"
#include "LevelViewportActions.h"
#include "EditorShowFlags.h"
#include "StatsData.h"

#define LOCTEXT_NAMESPACE "LevelViewportActions"

FLevelViewportCommands::FOnNewStatCommandAdded FLevelViewportCommands::NewStatCommandDelegate;

FLevelViewportCommands::~FLevelViewportCommands()
{
	UEngine::NewStatDelegate.RemoveAll(this);
#if STATS
	FStatGroupGameThreadNotifier::Get().NewStatGroupDelegate.Unbind();
#endif
}

/** UI_COMMAND takes long for the compile to optimize */
PRAGMA_DISABLE_OPTIMIZATION
void FLevelViewportCommands::RegisterCommands()
{
	
	UI_COMMAND( ToggleMaximize, "Maximize Viewport", "Toggles the Maximize state of the current viewport", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ToggleGameView, "Game View", "Toggles game view.  Game view shows the scene as it appears in game", EUserInterfaceActionType::ToggleButton, FInputGesture( EKeys::G ) );
	UI_COMMAND( ToggleImmersive, "Immersive Mode", "Switches this viewport between immersive mode and regular mode", EUserInterfaceActionType::ToggleButton, FInputGesture( EKeys::F11 ) );
	
	UI_COMMAND( CreateCamera, "Create Camera Here", "Creates a new camera actor at the current location of this viewport's camera", EUserInterfaceActionType::Button, FInputGesture() );
	UI_COMMAND( HighResScreenshot, "High Resolution Screenshot...", "Opens the control panel for high resolution screenshots", EUserInterfaceActionType::Button, FInputGesture() );
	
	UI_COMMAND( UseDefaultShowFlags, "Use Defaults", "Resets all show flags to default", EUserInterfaceActionType::Button, FInputGesture() );

	UI_COMMAND( ActorUnlock, "Unlock from Actor", "Unlock the viewport's position and orientation from the locked actor.", EUserInterfaceActionType::Button, FInputGesture() );
	UI_COMMAND( ActorLockSelected, "Lock Selected Actor", "Lock the viewport's position and orientation to the selected actor.", EUserInterfaceActionType::Button, FInputGesture() );
	UI_COMMAND( ActorUnlockSelected, "Unlock Selected Actor", "Unlock the viewport's position and orientation from the selected actor.", EUserInterfaceActionType::Check, FInputGesture() );

	UI_COMMAND( ViewportConfig_OnePane, "Layout One Pane", "Changes the viewport arrangement to one pane", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ViewportConfig_TwoPanesH, "Layout Two Panes (horizontal)", "Changes the viewport arrangement to two panes, side-by-side", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ViewportConfig_TwoPanesV, "Layout Two Panes (vertical)", "Changes the viewport arrangement to two panes, one above the other", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ViewportConfig_ThreePanesLeft, "Layout Three Panes (one left, two right)", "Changes the viewport arrangement to three panes, one on the left, two on the right", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ViewportConfig_ThreePanesRight, "Layout Three Panes (one right, two left)", "Changes the viewport arrangement to three panes, one on the right, two on the left", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ViewportConfig_ThreePanesTop, "Layout Three Panes (one top, two bottom)", "Changes the viewport arrangement to three panes, one on the top, two on the bottom", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ViewportConfig_ThreePanesBottom, "Layout Three Panes (one bottom, two top)", "Changes the viewport arrangement to three panes, one on the bottom, two on the top", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ViewportConfig_FourPanesLeft, "Layout Four Panes (one left, three right)", "Changes the viewport arrangement to four panes, one on the left, three on the right", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ViewportConfig_FourPanesRight, "Layout Four Panes (one right, three left)", "Changes the viewport arrangement to four panes, one on the right, three on the left", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ViewportConfig_FourPanesTop, "Layout Four Panes (one top, three bottom)", "Changes the viewport arrangement to four panes, one on the top, three on the bottom", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ViewportConfig_FourPanesBottom, "Layout Four Panes (one bottom, three top)", "Changes the viewport arrangement to four panes, one on the bottom, three on the top", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ViewportConfig_FourPanes2x2, "Layout Four Panes (2x2)", "Changes the viewport arrangement to four panes, in a 2x2 grid", EUserInterfaceActionType::ToggleButton, FInputGesture() );

	UI_COMMAND( ApplyMaterialToActor, "Apply Material", "Attempts to apply a dropped material to this object", EUserInterfaceActionType::Button, FInputGesture() );

	UI_COMMAND( AllowMatineePreview, "Allow Matinee Preview", "If enabled, allows matinee to be previewed in this viewport", EUserInterfaceActionType::ToggleButton, FInputGesture() );

	UI_COMMAND( FindInLevelScriptBlueprint, "Find In Level Script", "Finds references of a selected actor in the level script blueprint", EUserInterfaceActionType::Button, FInputGesture(EModifierKey::Control, EKeys::K) );
	UI_COMMAND( AdvancedSettings, "Advanced Settings...", "Opens the advanced viewport settings", EUserInterfaceActionType::Button, FInputGesture());

	// Generate a command for each buffer visualization mode
	{
		struct FMaterialIterator
		{
			const TSharedRef<class FBindingContext> Parent;
			FLevelViewportCommands::TBufferVisualizationModeCommandMap& CommandMap;

			FMaterialIterator(const TSharedRef<class FBindingContext> InParent, FLevelViewportCommands::TBufferVisualizationModeCommandMap& InCommandMap)
				: Parent(InParent)
				, CommandMap(InCommandMap)
			{
			}

			void ProcessValue(const FString& InMaterialName, const UMaterial* InMaterial, const FText& InDisplayName)
			{
				FName ViewportCommandName = *(FString(TEXT("BufferVisualizationMenu")) + InMaterialName);

				FBufferVisualizationRecord& Record = CommandMap.Add(ViewportCommandName, FBufferVisualizationRecord());
				Record.Name = *InMaterialName;
				const FText MaterialNameText = FText::FromString( InMaterialName );
				Record.Command = FUICommandInfoDecl( Parent, ViewportCommandName, MaterialNameText, MaterialNameText )
					.UserInterfaceType( EUserInterfaceActionType::RadioButton )
					.DefaultGesture( FInputGesture() );
			}
		};


		BufferVisualizationModeCommands.Empty();
		FName ViewportCommandName = *(FString(TEXT("BufferVisualizationOverview")));
		FBufferVisualizationRecord& OverviewRecord = BufferVisualizationModeCommands.Add(ViewportCommandName, FBufferVisualizationRecord());
		OverviewRecord.Name = NAME_None;
		OverviewRecord.Command = FUICommandInfoDecl( this->AsShared(), ViewportCommandName, LOCTEXT("BufferVisualization", "Overview"), LOCTEXT("BufferVisualization", "Overview") )
			.UserInterfaceType( EUserInterfaceActionType::RadioButton )
			.DefaultGesture( FInputGesture() );

		FMaterialIterator It(this->AsShared(), BufferVisualizationModeCommands);
		GetBufferVisualizationData().IterateOverAvailableMaterials(It);
	}

	const TArray<FShowFlagData>& ShowFlagData = GetShowFlagMenuItems();

	// Generate a command for each show flag
	for( int32 ShowFlag = 0; ShowFlag < ShowFlagData.Num(); ++ShowFlag )
	{
		const FShowFlagData& SFData = ShowFlagData[ShowFlag];

		FFormatNamedArguments Args;
		Args.Add( TEXT("ShowFlagName"), SFData.DisplayName );
		FText LocalizedName;
		switch( SFData.Group )
		{
		case SFG_Visualize:
			LocalizedName = FText::Format( LOCTEXT("VisualizeFlagLabel", "Visualize {ShowFlagName}"), Args );
			break;
		default:
			LocalizedName = FText::Format( LOCTEXT("ShowFlagLabel", "Show {ShowFlagName}"), Args );
			break;
		}

		//@todo Slate: The show flags system does not support descriptions currently
		const FText ShowFlagDesc;

		TSharedPtr<FUICommandInfo> ShowFlagCommand 
			= FUICommandInfoDecl( this->AsShared(), SFData.ShowFlagName, LocalizedName, ShowFlagDesc )
			.UserInterfaceType( EUserInterfaceActionType::ToggleButton )
			.DefaultGesture( SFData.InputGesture )
			.Icon(SFData.Group == EShowFlagGroup::SFG_Normal ?
						FSlateIcon(FEditorStyle::GetStyleSetName(), FEditorStyle::Join( GetContextName(), TCHAR_TO_ANSI( *FString::Printf( TEXT(".%s"), *SFData.ShowFlagName.ToString() ) ) ) ) :
						FSlateIcon());

		ShowFlagCommands.Add( FLevelViewportCommands::FShowMenuCommand( ShowFlagCommand, SFData.DisplayName ) );
	}

	// Generate a command for each volume class
	{
		UI_COMMAND( ShowAllVolumes, "Show All Volumes", "Shows all volumes", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( HideAllVolumes, "Hide All Volumes", "Hides all volumes", EUserInterfaceActionType::Button, FInputGesture() );

		TArray< UClass* > VolumeClasses;
		GUnrealEd->GetSortedVolumeClasses( &VolumeClasses );

		for( int32 VolumeClassIndex = 0; VolumeClassIndex < VolumeClasses.Num(); ++VolumeClassIndex )
		{
			//@todo Slate: The show flags system does not support descriptions currently
			const FText VolumeDesc;
			const FName VolumeName = VolumeClasses[VolumeClassIndex]->GetFName();

			FText DisplayName;
			FEngineShowFlags::FindShowFlagDisplayName( VolumeName.ToString(), DisplayName );

			FFormatNamedArguments Args;
			Args.Add( TEXT("ShowFlagName"), DisplayName );
			const FText LocalizedName = FText::Format( LOCTEXT("ShowFlagLabel_Visualize", "Visualize {ShowFlagName}"), Args );

			TSharedPtr<FUICommandInfo> ShowVolumeCommand 
				= FUICommandInfoDecl( this->AsShared(), VolumeName, LocalizedName, VolumeDesc )
				.UserInterfaceType( EUserInterfaceActionType::ToggleButton );

			ShowVolumeCommands.Add( FLevelViewportCommands::FShowMenuCommand( ShowVolumeCommand, DisplayName ) );
		}
	}

	// Generate a command for show/hide all layers
	{
		UI_COMMAND( ShowAllLayers, "Show All Layers", "Shows all layers", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( HideAllLayers, "Hide All Layers", "Hides all layers", EUserInterfaceActionType::Button, FInputGesture() );
	}

	// Generate a command for each sprite category
	{
		UI_COMMAND( ShowAllSprites, "Show All Sprites", "Shows all sprites", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( HideAllSprites, "Hide All Sprites", "Hides all sprites", EUserInterfaceActionType::Button, FInputGesture() );

		// get all the known layers
		// Get a fresh list as GUnrealEd->SortedSpriteInfo may not yet be built.
		TArray<FSpriteCategoryInfo> SortedSpriteInfo;
		GUnrealEd->MakeSortedSpriteInfo(SortedSpriteInfo);

		FString SpritePrefix = TEXT("ShowSprite_");
		for( int32 InfoIndex = 0; InfoIndex < SortedSpriteInfo.Num(); ++InfoIndex )
		{
			const FSpriteCategoryInfo& SpriteInfo = SortedSpriteInfo[InfoIndex];

			const FName CommandName = FName( *(SpritePrefix + SpriteInfo.Category.ToString()) );

			FFormatNamedArguments Args;
			Args.Add( TEXT("SpriteName"), SpriteInfo.DisplayName );
			const FText LocalizedName = FText::Format( NSLOCTEXT("UICommands", "SpriteShowFlagName", "Show {SpriteName} Sprites"), Args );

			TSharedPtr<FUICommandInfo> ShowSpriteCommand 
				= FUICommandInfoDecl( this->AsShared(), CommandName, LocalizedName, SpriteInfo.Description )
				.UserInterfaceType( EUserInterfaceActionType::ToggleButton );

			ShowSpriteCommands.Add( FLevelViewportCommands::FShowMenuCommand( ShowSpriteCommand, SpriteInfo.DisplayName ) );
		}
	}

	// Generate a command for each Stat category
	{
		UI_COMMAND(HideAllStats, "Hide All Stats", "Hides all Stats", EUserInterfaceActionType::Button, FInputGesture());

		// Bind a listener here for any additional stat commands that get registered later.
		UEngine::NewStatDelegate.AddRaw(this, &FLevelViewportCommands::HandleNewStat);
#if STATS
		FStatGroupGameThreadNotifier::Get().NewStatGroupDelegate.BindRaw(this, &FLevelViewportCommands::HandleNewStatGroup);
#endif
	}

	// Map the bookmark index to default key.
	// If the max bookmark number ever increases the new bookmarks will not have default keys
	TArray< FKey > NumberKeyNames;
	NumberKeyNames.Add( EKeys::Zero );
	NumberKeyNames.Add( EKeys::One );
	NumberKeyNames.Add( EKeys::Two );
	NumberKeyNames.Add( EKeys::Three );
	NumberKeyNames.Add( EKeys::Four );
	NumberKeyNames.Add( EKeys::Five );
	NumberKeyNames.Add( EKeys::Six );
	NumberKeyNames.Add( EKeys::Seven );
	NumberKeyNames.Add( EKeys::Eight );
	NumberKeyNames.Add( EKeys::Nine );

	for( int32 BookmarkIndex = 0; BookmarkIndex < AWorldSettings::MAX_BOOKMARK_NUMBER; ++BookmarkIndex )
	{
		TSharedRef< FUICommandInfo > JumpToBookmark =
			FUICommandInfoDecl(
			this->AsShared(), //Command class
			FName( *FString::Printf( TEXT( "JumpToBookmark%i" ), BookmarkIndex ) ), //CommandName
			FText::Format( NSLOCTEXT("LevelEditorCommands", "JumpToBookmark", "Jump to Bookmark {0}"), FText::AsNumber( BookmarkIndex ) ), //Localized label
			FText::Format( NSLOCTEXT("LevelEditorCommands", "JumpToBookmark_ToolTip", "Moves the viewport to the location and orientation stored at bookmark {0}"), FText::AsNumber( BookmarkIndex ) ) )//Localized tooltip
			.UserInterfaceType( EUserInterfaceActionType::Button ) //interface type
			.DefaultGesture( FInputGesture( NumberKeyNames.IsValidIndex( BookmarkIndex ) ? NumberKeyNames[BookmarkIndex] : EKeys::Invalid ) ); //default gesture

		JumpToBookmarkCommands.Add( JumpToBookmark );

		TSharedRef< FUICommandInfo > SetBookmark =
			FUICommandInfoDecl(
			this->AsShared(), //Command class
			FName( *FString::Printf( TEXT( "SetBookmark%i" ), BookmarkIndex ) ), //CommandName
			FText::Format( NSLOCTEXT("LevelEditorCommands", "SetBookmark", "Set Bookmark {0}"), FText::AsNumber( BookmarkIndex ) ), //Localized label
			FText::Format( NSLOCTEXT("LevelEditorCommands", "SetBookmark_ToolTip", "Stores the viewports location and orientation in bookmark {0}"), FText::AsNumber( BookmarkIndex ) ) )//Localized tooltip
			.UserInterfaceType( EUserInterfaceActionType::Button ) //interface type
			.DefaultGesture( FInputGesture( EModifierKey::Control, NumberKeyNames.IsValidIndex( BookmarkIndex ) ? NumberKeyNames[BookmarkIndex] : EKeys::Invalid ) ); //default gesture

		SetBookmarkCommands.Add( SetBookmark );

		TSharedRef< FUICommandInfo > ClearBookMark =
			FUICommandInfoDecl(
			this->AsShared(), //Command class
			FName( *FString::Printf( TEXT( "ClearBookmark%i" ), BookmarkIndex ) ), //CommandName
			FText::Format( NSLOCTEXT("LevelEditorCommands", "ClearBookmark", "Clear Bookmark {0}"), FText::AsNumber( BookmarkIndex ) ), //Localized label
			FText::Format( NSLOCTEXT("LevelEditorCommands", "ClearBookmark_ToolTip", "Clears the viewports location and orientation in bookmark {0}"), FText::AsNumber( BookmarkIndex ) ) )//Localized tooltip
			.UserInterfaceType( EUserInterfaceActionType::Button ) //interface type
			.DefaultGesture( FInputGesture() ); //default gesture 

		ClearBookmarkCommands.Add( ClearBookMark );
	}
	UI_COMMAND( ClearAllBookMarks, "Clear All Bookmarks", "Clears all the bookmarks", EUserInterfaceActionType::Button, FInputGesture() );

	UI_COMMAND( EnablePreviewMesh, "Hold To Enable Preview Mesh", "When held down a preview mesh appears under the cursor", EUserInterfaceActionType::Button, FInputGesture(EKeys::Backslash) );
	UI_COMMAND( CyclePreviewMesh, "Cycles Preview Mesh", "Cycles available preview meshes", EUserInterfaceActionType::Button, FInputGesture( EModifierKey::Shift, EKeys::Backslash ) );

}

void FLevelViewportCommands::HandleNewStatGroup(const TArray<FStatNameAndInfo>& NameAndInfos)
{
	for (int32 InfoIdx = 0; InfoIdx < NameAndInfos.Num(); InfoIdx++)
	{
		const FStatNameAndInfo& NameAndInfo = NameAndInfos[InfoIdx];
		const FName GroupName = NameAndInfo.GetGroupName();
		const FName GroupCategory = NameAndInfo.GetGroupCategory();
		const FText GroupDescription = FText::FromString(NameAndInfo.GetDescription());	// @todo localize description?
		HandleNewStat(GroupName, GroupCategory, GroupDescription);
	}
}

void FLevelViewportCommands::HandleNewStat(const FName& InStatName, const FName& InStatCategory, const FText& InStatDescription)
{
	FString CommandName = InStatName.ToString();
	if (CommandName.RemoveFromStart(TEXT("STATGROUP_")) || CommandName.RemoveFromStart(TEXT("STAT_")))
	{
		// Trim the front to get our category name
		FString GroupCategory = InStatCategory.ToString();
		if (!GroupCategory.RemoveFromStart(TEXT("STATCAT_")))
		{
			GroupCategory.Empty();
		}

		// If we already have an entry (which can happen if a category has changed [when loading older saved stat data]) or we don't have a valid category then skip adding
		if (!FInputBindingManager::Get().FindCommandInContext(this->GetContextName(), InStatName).IsValid() && !GroupCategory.IsEmpty())
		{
			// Find or Add the category
			TArray< FShowMenuCommand >* ShowStatCommands = ShowStatCatCommands.Find(GroupCategory);
			if (!ShowStatCommands)
			{
				// New category means we'll need to resort
				ShowStatCatCommands.Add(GroupCategory);
				ShowStatCatCommands.KeySort(TLess<FString>());
				ShowStatCommands = ShowStatCatCommands.Find(GroupCategory);
			}

			const int32 NewIndex = FindStatIndex(ShowStatCommands, CommandName);
			if (NewIndex != INDEX_NONE)
			{
				const FText DisplayName = FText::FromString(CommandName);

				FText DescriptionName = InStatDescription;
				FFormatNamedArguments Args;
				Args.Add(TEXT("StatName"), DisplayName);
				if (DescriptionName.IsEmpty())
				{
					DescriptionName = FText::Format(NSLOCTEXT("UICommands", "StatShowCommandName", "Show {StatName} Stat"), Args);
				}

				TSharedPtr<FUICommandInfo> StatCommand
					= FUICommandInfoDecl(this->AsShared(), InStatName, FText::GetEmpty(), DescriptionName)
					.UserInterfaceType(EUserInterfaceActionType::ToggleButton);

				FLevelViewportCommands::FShowMenuCommand ShowStatCommand(StatCommand, DisplayName);
				ShowStatCommands->Insert(ShowStatCommand, NewIndex);
				NewStatCommandDelegate.Broadcast(ShowStatCommand.ShowMenuItem, ShowStatCommand.LabelOverride.ToString());
			}
		}
	}
}

int32 FLevelViewportCommands::FindStatIndex(const TArray< FShowMenuCommand >* ShowStatCommands, const FString& InCommandName) const
{
	check(ShowStatCommands);
	for (int32 StatIndex = 0; StatIndex < ShowStatCommands->Num(); ++StatIndex)
	{
		const FString CommandName = (*ShowStatCommands)[StatIndex].LabelOverride.ToString();
		const int32 Compare = InCommandName.Compare(CommandName);
		if (Compare == 0)
		{
			return INDEX_NONE;
		}
		else if (Compare < 0)
		{
			return StatIndex;
		}
	}
	return ShowStatCommands->Num();
}

PRAGMA_ENABLE_OPTIMIZATION

#undef LOCTEXT_NAMESPACE