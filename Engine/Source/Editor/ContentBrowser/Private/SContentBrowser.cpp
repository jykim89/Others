// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "ContentBrowserPCH.h"
#include "AssetContextMenu.h"
#include "NewAssetContextMenu.h"
#include "PathContextMenu.h"
#include "ContentBrowserModule.h"
#include "STutorialWrapper.h"
#include "ContentBrowserCommands.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

void AssetDataToString( AssetFilterType Asset, OUT TArray< FString >& Array )
{
	Array.Add( Asset.GetExportTextName() );
}

const FString SContentBrowser::SettingsIniSection = TEXT("ContentBrowser");

SContentBrowser::~SContentBrowser()
{
	// Remove the listener for when view settings are changed
	UContentBrowserSettings::OnSettingChanged().RemoveAll( this );
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SContentBrowser::Construct( const FArguments& InArgs, const FName& InInstanceName )
{
	TextFilter = MakeShareable( new TTextFilter< AssetFilterType >( TTextFilter< AssetFilterType >::FItemToStringArray::CreateStatic( &AssetDataToString ) ) );

	if ( InArgs._ContainingTab.IsValid() )
	{
		// For content browsers that are placed in tabs, save settings when the tab is closing.
		ContainingTab = InArgs._ContainingTab;
		InArgs._ContainingTab->SetOnPersistVisualState( SDockTab::FOnPersistVisualState::CreateSP( this, &SContentBrowser::OnContainingTabSavingVisualState ) );
		InArgs._ContainingTab->SetOnTabClosed( SDockTab::FOnTabClosedCallback::CreateSP( this, &SContentBrowser::OnContainingTabClosed ) );
		InArgs._ContainingTab->SetOnTabActivated( SDockTab::FOnTabActivatedCallback::CreateSP( this, &SContentBrowser::OnContainingTabActivated ) );
	}
	
	bIsLocked = InArgs._InitiallyLocked;

	// Listen for when view settings are changed
	UContentBrowserSettings::OnSettingChanged().AddSP(this, &SContentBrowser::HandleSettingChanged);

	HistoryManager.SetOnApplyHistoryData(FOnApplyHistoryData::CreateSP(this, &SContentBrowser::OnApplyHistoryData));
	HistoryManager.SetOnUpdateHistoryData(FOnUpdateHistoryData::CreateSP(this, &SContentBrowser::OnUpdateHistoryData));

	PathContextMenu = MakeShareable(new FPathContextMenu( AsShared() ));
	PathContextMenu->SetOnNewAssetRequested( FNewAssetContextMenu::FOnNewAssetRequested::CreateSP(this, &SContentBrowser::NewAssetRequested) );

	TSharedPtr<AssetFilterCollectionType> FrontendFilters = MakeShareable(new AssetFilterCollectionType());
	TSharedPtr<AssetFilterCollectionType> ExtraFilters = MakeShareable(new AssetFilterCollectionType());
	ExtraFilters->Add( TextFilter );

	FContentBrowserCommands::Register();

	BindCommands();

	ChildSlot
	[
		SNew(SVerticalBox)

		// Path and history
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding( 0, 0, 0, 0 )
		[
			SNew( SWrapBox )
			.UseAllottedWidth( true )
			.InnerSlotPadding( FVector2D( 5, 2 ) )

			+ SWrapBox::Slot()
			.FillLineWhenWidthLessThan( 600 )
			.FillEmptySpace( true )
			[
				SNew( SHorizontalBox )

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew( SBorder )
					.Padding( FMargin( 3 ) )
					.BorderImage( FEditorStyle::GetBrush( "ToolPanel.GroupBorder" ) )
					[
						SNew( SHorizontalBox )

						// New
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign( VAlign_Center )
						.HAlign( HAlign_Left )
						.Padding(0,0,4,0)
						[
							SNew( STutorialWrapper, TEXT("ContentBrowserNewAsset") )
							[
								SNew( SComboButton )
								.ComboButtonStyle( FEditorStyle::Get(), "ContentBrowser.NewAsset.Style" )
								.ForegroundColor(FLinearColor::White)
								.ContentPadding(0)
								.OnGetMenuContent( this, &SContentBrowser::MakeCreateAssetContextMenu )
								.ToolTipText( this, &SContentBrowser::GetNewAssetToolTipText )
								.IsEnabled( this, &SContentBrowser::IsAssetPathSelected )
								.ButtonContent()
								[
									SNew( SHorizontalBox )

									// New Icon
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign( VAlign_Center )
									[
										SNew( SImage )
										.Image( FEditorStyle::GetBrush( "ContentBrowser.NewAsset" ) )
									]

									// New Text
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									.Padding(0,0,2,0)
									[
										SNew( STextBlock )
										.TextStyle( FEditorStyle::Get(), "ContentBrowser.TopBar.Font" )
										.Text( LOCTEXT( "NewButton", "New" ) )
									]
								]
							]
						]

						// Import
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign( VAlign_Center )
						.HAlign( HAlign_Left )
						.Padding(0,0,10,0)
						[
							SNew( STutorialWrapper, TEXT("ContentBrowserImportAsset") )
							[
								SNew( SButton )
								.ButtonStyle( FEditorStyle::Get(), "ToggleButton" )
								.ToolTipText( this, &SContentBrowser::GetImportTooltipText )
								.IsEnabled( this, &SContentBrowser::IsAssetPathSelected )
								.OnClicked( this, &SContentBrowser::HandleImportClicked )
								.ContentPadding( 0 )
								[
									SNew( SHorizontalBox )

									// Import Icon
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign( VAlign_Center )
									[
										SNew( SImage )
										.Image( FEditorStyle::GetBrush( "ContentBrowser.ImportPackage" ) )
									]

									// Import Text
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									.Padding(0,0,2,0)
									[
										SNew( STextBlock )
										.TextStyle( FEditorStyle::Get(), "ContentBrowser.TopBar.Font" )
										.Text( LOCTEXT( "Import", "Import" ) )
									]
								]
							]
						]

						// Save
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						[
							SNew( STutorialWrapper, TEXT("ContentBrowserSaveDirtyPackages") )
							[
								SNew( SButton )
								.ButtonStyle( FEditorStyle::Get(), "ToggleButton" )
								.ToolTipText( LOCTEXT( "SaveDirtyPackagesTooltip", "Save all modified assets." ) )
								.ContentPadding( 0 )
								.OnClicked( this, &SContentBrowser::OnSaveClicked )
								[
									SNew( SImage )
									.Image( FEditorStyle::GetBrush( "ContentBrowser.SaveDirtyPackages" ) )
								]
							]
						]
					]
				]
			]

			+ SWrapBox::Slot()
			.FillEmptySpace( true )
			[
				SNew(SBorder)
				.Padding(FMargin(3))
				.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
				[
					SNew(SHorizontalBox)

					// History Back Button
					+SHorizontalBox::Slot()
					.AutoWidth()
					//.Padding(0, 1)
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SNew( STutorialWrapper, TEXT("ContentBrowserHistoryBack") )
							[
								SNew(SButton)
								.VAlign(EVerticalAlignment::VAlign_Center)
								.ButtonStyle( FEditorStyle::Get(), "ToggleButton" )
								.ForegroundColor( FEditorStyle::GetSlateColor("DefaultForeground") )
								.ToolTipText( this, &SContentBrowser::GetHistoryBackTooltip )
								.ContentPadding( FMargin(1, 0) )
								.OnClicked(this, &SContentBrowser::BackClicked)
								.IsEnabled(this, &SContentBrowser::IsBackEnabled)
								[
									SNew(SImage)
									.Image(FEditorStyle::GetBrush("ContentBrowser.HistoryBack"))
								]
							]
						]
					]

					// History Forward Button
					+ SHorizontalBox::Slot()
					.AutoWidth()
					//.Padding(0, 1)
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SNew(STutorialWrapper, "ContentBrowserHistoryForward")
							[
								SNew(SButton)
								.VAlign(EVerticalAlignment::VAlign_Center)
								.ButtonStyle( FEditorStyle::Get(), "ToggleButton" )
								.ForegroundColor( FEditorStyle::GetSlateColor("DefaultForeground") )
								.ToolTipText( this, &SContentBrowser::GetHistoryForwardTooltip )
								.ContentPadding( FMargin(1, 0) )
								.OnClicked(this, &SContentBrowser::ForwardClicked)
								.IsEnabled(this, &SContentBrowser::IsForwardEnabled)
								[
									SNew(SImage)
									.Image(FEditorStyle::GetBrush("ContentBrowser.HistoryForward"))
								]
							]
						]
					]

					// Separator
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(3, 0)
					[
						SNew(SSeparator)
						.Orientation(Orient_Vertical)
					]

					// Path picker
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign( VAlign_Fill )
					[
						SNew( STutorialWrapper, TEXT("ContentBrowserPathPicker") )
						[
							SAssignNew( PathPickerButton, SComboButton )
							.ComboButtonStyle( FEditorStyle::Get(), "ToolbarComboButton" )
							.ForegroundColor(FLinearColor::White)
							.ToolTipText( LOCTEXT( "PathPickerTooltip", "Choose a path" ) )
							.OnGetMenuContent( this, &SContentBrowser::GetPathPickerContent )
							.HasDownArrow( false )
							.ButtonContent()
							[
								SNew( SImage )
								.Image( FEditorStyle::GetBrush( "ContentBrowser.Sources" ) )
							]
						]
					]

					// Path
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.FillWidth(1.0f)
					.Padding( FMargin(0) )
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.FillWidth(1.0f)
						[
							SNew( STutorialWrapper, TEXT("ContentBrowserPath") )
							[
								SAssignNew(PathBreadcrumbTrail, SBreadcrumbTrail<FString>)
								//.ToolTipText( LOCTEXT("PathTooltip", "Content Path") )
								.ButtonContentPadding(FMargin(3.0f, 3.0f))
								.DelimiterImage(FEditorStyle::GetBrush("ContentBrowser.PathDelimiter"))
								.TextStyle(FEditorStyle::Get(), "ContentBrowser.PathText")
								.ShowLeadingDelimiter( false )
								.InvertTextColorOnHover( false )
								.OnCrumbClicked(this, &SContentBrowser::OnPathClicked)
								.GetCrumbMenuContent(this, &SContentBrowser::OnGetCrumbDelimiterContent)
							]
						]
					]

					// Lock button
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SNew( STutorialWrapper, TEXT("ContentBrowserLock") )
							[
								SNew(SButton)
								.VAlign(EVerticalAlignment::VAlign_Center)
								.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
								.ToolTipText( LOCTEXT("LockToggleTooltip", "Toggle lock. If locked, this browser will ignore Find in Content Browser requests.") )
								.ContentPadding( FMargin(1, 0) )
								.OnClicked(this, &SContentBrowser::ToggleLockClicked)
								[
									SNew(SImage)
									.Image( this, &SContentBrowser::GetToggleLockImage)
								]
							]
						]
					]
				]
			]
		]

		// Assets/tree
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			// The tree/assets splitter
			SAssignNew(PathAssetSplitterPtr, SSplitter)

			// Sources View
			+ SSplitter::Slot()
			.Value(0.3f)
			[
				SNew(SVerticalBox)
				.Visibility( this, &SContentBrowser::GetSourcesViewVisibility )

				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SAssignNew(PathCollectionSplitterPtr, SSplitter)
					.Style( FEditorStyle::Get(), "ContentBrowser.Splitter" )
					.Orientation( Orient_Vertical )

					// Path View
					+ SSplitter::Slot()
					.Value(0.9f)
					[
						SNew(SBorder)
						.Padding(FMargin(3))
						.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
						[
							SNew( STutorialWrapper, TEXT("ContentBrowserSources") )
							[
								SAssignNew( PathViewPtr, SPathView )
								.OnPathSelected( this, &SContentBrowser::PathSelected )
								.OnGetFolderContextMenu( this, &SContentBrowser::GetFolderContextMenu )
								.OnGetPathContextMenuExtender( this, &SContentBrowser::GetPathContextMenuExtender )
								.FocusSearchBoxWhenOpened( false )
								.ShowTreeTitle( false )
								.ShowSeparator( false )
								.SearchContent()
								[
									SNew( STutorialWrapper, TEXT("ContentBrowserSourcesToggle") )
									[
										SNew( SVerticalBox )

										+ SVerticalBox::Slot()
										.FillHeight( 1.0f )
										.Padding(0,0,2,0)
										[
											SNew( SButton )
											.VAlign( EVerticalAlignment::VAlign_Center )
											.ButtonStyle( FEditorStyle::Get(), "ToggleButton" )
											.ToolTipText( LOCTEXT( "SourcesTreeToggleTooltip", "Show or hide the sources panel" ) )
											.ContentPadding( FMargin( 1, 0 ) )
											.ForegroundColor( FEditorStyle::GetSlateColor( "DefaultForeground" ) )
											.OnClicked( this, &SContentBrowser::SourcesViewExpandClicked )
											[
												SNew( SImage )
												.Image( this, &SContentBrowser::GetSourcesToggleImage )
											]
										]
									]
								]
							]
						]
					]

					// Collection View
					+ SSplitter::Slot()
					.Value(0.1f)
					[
						SNew(SBorder)
						.Padding(FMargin(3))
						.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
						[
							SNew( STutorialWrapper, TEXT("ContentBrowserCollections") )
							[
								SAssignNew(CollectionViewPtr, SCollectionView)
								.OnCollectionSelected(this, &SContentBrowser::CollectionSelected)
							]
						]
					]
				]
			]

			// Asset View
			+ SSplitter::Slot()
			.Value(0.7f)
			[
				SNew(SBorder)
				.Padding(FMargin(3))
				.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
				[
					SNew(SVerticalBox)

					// Search and commands
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						// Expand/collapse sources button
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding( 0, 0, 4, 0 )
						[
							SNew( STutorialWrapper, TEXT("ContentBrowserSourcesToggle") )
							[
								SNew( SVerticalBox )

								+ SVerticalBox::Slot()
								.FillHeight( 1.0f )
								[
									SNew( SButton )
									.VAlign( EVerticalAlignment::VAlign_Center )
									.ButtonStyle( FEditorStyle::Get(), "ToggleButton" )
									.ToolTipText( LOCTEXT( "SourcesTreeToggleTooltip", "Show or hide the sources panel" ) )
									.ContentPadding( FMargin( 1, 0 ) )
									.ForegroundColor( FEditorStyle::GetSlateColor( "DefaultForeground" ) )
									.OnClicked( this, &SContentBrowser::SourcesViewExpandClicked )
									.Visibility( this, &SContentBrowser::GetPathExpanderVisibility )
									[
										SNew( SImage )
										.Image( this, &SContentBrowser::GetSourcesToggleImage )
									]
								]
							]
						]

						// Filter
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew( STutorialWrapper, TEXT("ContentBrowserFiltersCombo") )
							[
								SNew( SComboButton )
								.ComboButtonStyle( FEditorStyle::Get(), "ContentBrowser.Filters.Style" )
								.ForegroundColor(FLinearColor::White)
								.ContentPadding(0)
								.ToolTipText( LOCTEXT( "AddFilterToolTip", "Add an asset filter." ) )
								.OnGetMenuContent( this, &SContentBrowser::MakeAddFilterMenu )
								.HasDownArrow( true )
								.ContentPadding( FMargin( 1, 0 ) )
								.ButtonContent()
								[
									SNew( STextBlock )
									.TextStyle( FEditorStyle::Get(), "ContentBrowser.Filters.Text" )
									.Text( LOCTEXT( "Filters", "Filters" ) )
								]
							]
						]

						// Search
						+SHorizontalBox::Slot()
						.Padding(4, 0, 0, 0)
						.VAlign(VAlign_Center)
						.FillWidth(1.0f)
						[
							SNew( STutorialWrapper, TEXT("ContentBrowserSearchAssets") )
							[
								SAssignNew(SearchBoxPtr, SAssetSearchBox)
								.HintText( this, &SContentBrowser::GetSearchAssetsHintText )
								.OnTextChanged( this, &SContentBrowser::OnSearchBoxChanged )
								.OnTextCommitted( this, &SContentBrowser::OnSearchBoxCommitted )
								.PossibleSuggestions( this, &SContentBrowser::GetAssetSearchSuggestions )
								.DelayChangeNotificationsWhileTyping( true )
							]
						]
					]

					// Filters
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew( STutorialWrapper, TEXT("ContentBrowserFilters") )
						[
							SAssignNew(FilterListPtr, SFilterList)
							.OnFilterChanged(this, &SContentBrowser::OnFilterChanged)
							.OnGetContextMenu(this, &SContentBrowser::GetFilterContextMenu)
							.FrontendFilters(FrontendFilters)
						]
					]

					// Assets
					+ SVerticalBox::Slot()
					.FillHeight( 1.0f )
					.Padding( 0 )
					[
						SNew( STutorialWrapper, TEXT("ContentBrowserAssets") )
						[
							SAssignNew(AssetViewPtr, SAssetView)
							.ThumbnailScale( 0.0f )
							.OnPathSelected(this, &SContentBrowser::FolderEntered)
							.OnAssetSelected(this, &SContentBrowser::OnAssetSelectionChanged)
							.OnAssetsActivated(this, &SContentBrowser::OnAssetsActivated)
							.OnGetAssetContextMenu(this, &SContentBrowser::OnGetAssetContextMenu)
							.OnGetFolderContextMenu(this, &SContentBrowser::GetFolderContextMenu)
							.OnGetPathContextMenuExtender(this, &SContentBrowser::GetPathContextMenuExtender)
							.OnFindInAssetTreeRequested(this, &SContentBrowser::OnFindInAssetTreeRequested)
							.OnAssetRenameCommitted(this, &SContentBrowser::OnAssetRenameCommitted)
							.AreRealTimeThumbnailsAllowed(this, &SContentBrowser::IsHovered)
							.FrontendFilters(FrontendFilters)
							.DynamicFilters(ExtraFilters)
							.HighlightedText(this, &SContentBrowser::GetHighlightedText)
							.AllowThumbnailEditMode(true)
							.AllowThumbnailHintLabel(false)
							.CanShowFolders(true)
							.CanShowOnlyAssetsInSelectedFolders(true)
							.CanShowRealTimeThumbnails(true)
							.CanShowDevelopersFolder(true)
						]
					]
				]
			]
		]
	];

	AssetContextMenu = MakeShareable(new FAssetContextMenu(AssetViewPtr));
	AssetContextMenu->BindCommands(Commands);
	AssetContextMenu->SetOnFindInAssetTreeRequested( FOnFindInAssetTreeRequested::CreateSP(this, &SContentBrowser::OnFindInAssetTreeRequested) );
	AssetContextMenu->SetOnRenameRequested( FAssetContextMenu::FOnRenameRequested::CreateSP(this, &SContentBrowser::OnRenameRequested) );
	AssetContextMenu->SetOnRenameFolderRequested( FAssetContextMenu::FOnRenameFolderRequested::CreateSP(this, &SContentBrowser::OnRenameFolderRequested) );
	AssetContextMenu->SetOnDuplicateRequested( FAssetContextMenu::FOnDuplicateRequested::CreateSP(this, &SContentBrowser::OnDuplicateRequested) );
	AssetContextMenu->SetOnAssetViewRefreshRequested( FAssetContextMenu::FOnAssetViewRefreshRequested::CreateSP( this, &SContentBrowser::OnAssetViewRefreshRequested) );

	// Select /Game by default
	FSourcesData DefaultSourcesData;
	TArray<FString> SelectedPaths;
	DefaultSourcesData.PackagePaths.Add(TEXT("/Game"));
	SelectedPaths.Add(TEXT("/Game"));
	PathViewPtr->SetSelectedPaths(SelectedPaths);
	AssetViewPtr->SetSourcesData(DefaultSourcesData);

	// Set the initial history data
	HistoryManager.AddHistoryData();

	// Load settings if they were specified
	this->InstanceName = InInstanceName;
	LoadSettings(InInstanceName);

	// Update the breadcrumb trail path
	UpdatePath();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SContentBrowser::BindCommands()
{
	Commands = TSharedPtr< FUICommandList >(new FUICommandList);

	Commands->MapAction(FContentBrowserCommands::Get().OpenAssetsOrFolders, FUIAction(
		FExecuteAction::CreateSP(this, &SContentBrowser::OnOpenAssetsOrFolders)
		));

	Commands->MapAction(FContentBrowserCommands::Get().PreviewAssets, FUIAction(
		FExecuteAction::CreateSP(this, &SContentBrowser::OnPreviewAssets)
		));

	Commands->MapAction( FContentBrowserCommands::Get().DirectoryUp, FUIAction(
		FExecuteAction::CreateSP( this, &SContentBrowser::OnDirectoryUp )
		));
}

FText SContentBrowser::GetHighlightedText() const
{
	return TextFilter->GetRawFilterText();
}

void SContentBrowser::CreateNewAsset(const FString& DefaultAssetName, const FString& PackagePath, UClass* AssetClass, UFactory* Factory)
{
	AssetViewPtr->CreateNewAsset(DefaultAssetName, PackagePath, AssetClass, Factory);
}

FText SContentBrowser::GetImportTooltipText() const
{
	FString CurrentPath = GetCurrentPath();

	FText ImportAssetLabel;
	if ( !CurrentPath.IsEmpty() )
	{
		ImportAssetLabel = FText::Format( LOCTEXT( "ImportAsset", "Import to {0}..." ), FText::FromString( CurrentPath ) );
	}
	else
	{
		ImportAssetLabel = LOCTEXT( "ImportAsset_NoPath", "Import" );
	}

	return ImportAssetLabel;
}

FReply SContentBrowser::HandleImportClicked()
{
	FString CurrentPath = GetCurrentPath();

	if ( ensure( !CurrentPath.IsEmpty() ) )
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>( "AssetTools" );
		AssetToolsModule.Get().ImportAssets( CurrentPath );
	}

	return FReply::Handled();
}

void SContentBrowser::SyncToAssets( const TArray<FAssetData>& AssetDataList, const bool bAllowImplicitSync )
{
	// Check to see if any of the assets require certain folders to be visible
	const UContentBrowserSettings* tmp = GetDefault<UContentBrowserSettings>();
	bool bDisplayDev = GetDefault<UContentBrowserSettings>()->GetDisplayDevelopersFolder();
	bool bDisplayEngine = GetDefault<UContentBrowserSettings>()->GetDisplayEngineFolder();
	if ( !bDisplayDev || !bDisplayEngine )
	{
		for (int32 AssetIdx = AssetDataList.Num() - 1; AssetIdx >= 0 && ( !bDisplayDev || !bDisplayEngine ); --AssetIdx)
		{
			const FAssetData& Item = AssetDataList[AssetIdx];
			if ( !bDisplayDev && ContentBrowserUtils::IsDevelopersFolder( Item.PackagePath.ToString() ) )
			{
				bDisplayDev = true;
				GetMutableDefault<UContentBrowserSettings>()->SetDisplayDevelopersFolder(true, true);
			}
			else if ( !bDisplayEngine && ContentBrowserUtils::IsEngineFolder( Item.PackagePath.ToString() ) )
			{
				bDisplayEngine = true;
				GetMutableDefault<UContentBrowserSettings>()->SetDisplayEngineFolder(true, true);
			}
		}

		// If we have auto-enabled any flags, force a refresh
		if ( bDisplayDev || bDisplayEngine )
		{
			PathViewPtr->Populate();
		}
	}

	FilterListPtr->DisableFiltersThatHideAssets(AssetDataList);

	// Tell the sources view first so the asset view will be up to date by the time we request the sync
	PathViewPtr->SyncToAssets(AssetDataList, bAllowImplicitSync);
	SearchBoxPtr->SetText(FText::GetEmpty());
	AssetViewPtr->SyncToAssets(AssetDataList);
}

void SContentBrowser::SetIsPrimaryContentBrowser (bool NewIsPrimary)
{
	bIsPrimaryBrowser = NewIsPrimary;

	if ( bIsPrimaryBrowser )
	{
		SyncGlobalSelectionSet();
	}
	else
	{
		USelection* EditorSelection = GEditor->GetSelectedObjects();
		if ( ensure( EditorSelection != NULL ) )
		{
			EditorSelection->DeselectAll();
		}
	}
}

TSharedPtr<FTabManager> SContentBrowser::GetTabManager() const
{
	if ( ContainingTab.IsValid() )
	{
		return ContainingTab.Pin()->GetTabManager();
	}

	return NULL;
}

void SContentBrowser::LoadSelectedObjectsIfNeeded()
{
	// Get the selected assets in the asset view
	const TArray<FAssetData>& SelectedAssets = AssetViewPtr->GetSelectedAssets();

	// Load every asset that isn't already in memory
	for ( auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt )
	{
		if ( !(*AssetIt).IsAssetLoaded() && FEditorFileUtils::IsMapPackageAsset((*AssetIt).ObjectPath.ToString()) )
		{
			// Don't load assets in map packages
			continue;
		}

		(*AssetIt).GetAsset();
	}

	// Sync the global selection set if we are the primary browser
	if ( bIsPrimaryBrowser )
	{
		SyncGlobalSelectionSet();
	}
}

void SContentBrowser::GetSelectedAssets(TArray<FAssetData>& SelectedAssets)
{
	// Make sure the asset data is up to date
	AssetViewPtr->ProcessRecentlyLoadedOrChangedAssets();

	SelectedAssets = AssetViewPtr->GetSelectedAssets();
}

void SContentBrowser::SaveSettings() const
{
	const FString& SettingsString = InstanceName.ToString();

	GConfig->SetBool(*SettingsIniSection, *(SettingsString + TEXT(".SourcesExpanded")), bSourcesViewExpanded, GEditorUserSettingsIni);
	GConfig->SetBool(*SettingsIniSection, *(SettingsString + TEXT(".Locked")), bIsLocked, GEditorUserSettingsIni);

	for(int32 SlotIndex = 0; SlotIndex < PathAssetSplitterPtr->GetChildren()->Num(); SlotIndex++)
	{
		float SplitterSize = PathAssetSplitterPtr->SlotAt(SlotIndex).SizeValue.Get();
		GConfig->SetFloat(*SettingsIniSection, *(SettingsString + FString::Printf(TEXT(".VerticalSplitter.SlotSize%d"), SlotIndex)), SplitterSize, GEditorUserSettingsIni);
	}
	
	for(int32 SlotIndex = 0; SlotIndex < PathCollectionSplitterPtr->GetChildren()->Num(); SlotIndex++)
	{
		float SplitterSize = PathCollectionSplitterPtr->SlotAt(SlotIndex).SizeValue.Get();
		GConfig->SetFloat(*SettingsIniSection, *(SettingsString + FString::Printf(TEXT(".HorizontalSplitter.SlotSize%d"), SlotIndex)), SplitterSize, GEditorUserSettingsIni);
	}

	// Save all our data using the settings string as a key in the user settings ini
	FilterListPtr->SaveSettings(GEditorUserSettingsIni, SettingsIniSection, SettingsString);
	PathViewPtr->SaveSettings(GEditorUserSettingsIni, SettingsIniSection, SettingsString);
	CollectionViewPtr->SaveSettings(GEditorUserSettingsIni, SettingsIniSection, SettingsString);
	AssetViewPtr->SaveSettings(GEditorUserSettingsIni, SettingsIniSection, SettingsString);
}

const FName SContentBrowser::GetInstanceName() const
{
	return InstanceName;
}

bool SContentBrowser::IsLocked() const
{
	return bIsLocked;
}

void SContentBrowser::SetKeyboardFocusOnSearch() const
{
	// Focus on the search box
	FSlateApplication::Get().SetKeyboardFocus( SearchBoxPtr, EKeyboardFocusCause::SetDirectly );
}

FReply SContentBrowser::OnKeyDown( const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent )
{
	if( Commands->ProcessCommandBindings( InKeyboardEvent ) )
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SContentBrowser::OnPreviewMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// Clicking in a content browser will shift it to be the primary browser
	FContentBrowserSingleton::Get().SetPrimaryContentBrowser(SharedThis(this));

	return FReply::Unhandled();
}

FReply SContentBrowser::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// Mouse back and forward buttons traverse history
	if ( MouseEvent.GetEffectingButton() == EKeys::ThumbMouseButton)
	{
		HistoryManager.GoBack();
		return FReply::Handled();
	}
	else if ( MouseEvent.GetEffectingButton() == EKeys::ThumbMouseButton2)
	{
		HistoryManager.GoForward();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SContentBrowser::OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	// Mouse back and forward buttons traverse history
	if ( InMouseEvent.GetEffectingButton() == EKeys::ThumbMouseButton)
	{
		HistoryManager.GoBack();
		return FReply::Handled();
	}
	else if ( InMouseEvent.GetEffectingButton() == EKeys::ThumbMouseButton2)
	{
		HistoryManager.GoForward();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SContentBrowser::OnContainingTabSavingVisualState() const
{
	SaveSettings();
}

void SContentBrowser::OnContainingTabClosed(TSharedRef<SDockTab> DockTab)
{
	FContentBrowserSingleton::Get().ContentBrowserClosed( SharedThis(this) );
}

void SContentBrowser::OnContainingTabActivated(TSharedRef<SDockTab> DockTab, ETabActivationCause::Type InActivationCause)
{
	if(InActivationCause == ETabActivationCause::UserClickedOnTab)
	{
		FContentBrowserSingleton::Get().SetPrimaryContentBrowser(SharedThis(this));
	}
}

void SContentBrowser::LoadSettings(const FName& InInstanceName)
{
	FString SettingsString = InInstanceName.ToString();

	// Test to see if we should load legacy settings from a previous instance name
	// First make sure there aren't any existing settings with the given instance name
	bool TestBool;
	if ( !GConfig->GetBool(*SettingsIniSection, *(SettingsString + TEXT(".SourcesExpanded")), TestBool, GEditorUserSettingsIni) )
	{
		// If there were not any settings and we are Content Browser 1, see if we have any settings under the legacy name "LevelEditorContentBrowser"
		if ( InInstanceName.ToString() == TEXT("ContentBrowserTab1") && GConfig->GetBool(*SettingsIniSection, TEXT("LevelEditorContentBrowser.SourcesExpanded"), TestBool, GEditorUserSettingsIni) )
		{
			// We have found some legacy settings with the old ID, use them. These settings will be saved out to the new id later
			SettingsString = TEXT("LevelEditorContentBrowser");
		}
		// else see if we are Content Browser 2, and see if we have any settings under the legacy name "MajorContentBrowserTab"
		else if ( InInstanceName.ToString() == TEXT("ContentBrowserTab2") && GConfig->GetBool(*SettingsIniSection, TEXT("MajorContentBrowserTab.SourcesExpanded"), TestBool, GEditorUserSettingsIni) )
		{
			// We have found some legacy settings with the old ID, use them. These settings will be saved out to the new id later
			SettingsString = TEXT("MajorContentBrowserTab");
		}
	}

	// Now that we have determined the appropriate settings string, actually load the settings
	GConfig->GetBool(*SettingsIniSection, *(SettingsString + TEXT(".SourcesExpanded")), bSourcesViewExpanded, GEditorUserSettingsIni);
	GConfig->GetBool(*SettingsIniSection, *(SettingsString + TEXT(".Locked")), bIsLocked, GEditorUserSettingsIni);

	for(int32 SlotIndex = 0; SlotIndex < PathAssetSplitterPtr->GetChildren()->Num(); SlotIndex++)
	{
		float SplitterSize = PathAssetSplitterPtr->SlotAt(SlotIndex).SizeValue.Get();
		GConfig->GetFloat(*SettingsIniSection, *(SettingsString + FString::Printf(TEXT(".VerticalSplitter.SlotSize%d"), SlotIndex)), SplitterSize, GEditorUserSettingsIni);
		PathAssetSplitterPtr->SlotAt(SlotIndex).SizeValue = SplitterSize;
	}
	
	for(int32 SlotIndex = 0; SlotIndex < PathCollectionSplitterPtr->GetChildren()->Num(); SlotIndex++)
	{
		float SplitterSize = PathCollectionSplitterPtr->SlotAt(SlotIndex).SizeValue.Get();
		GConfig->GetFloat(*SettingsIniSection, *(SettingsString + FString::Printf(TEXT(".HorizontalSplitter.SlotSize%d"), SlotIndex)), SplitterSize, GEditorUserSettingsIni);
		PathCollectionSplitterPtr->SlotAt(SlotIndex).SizeValue = SplitterSize;
	}

	// Save all our data using the settings string as a key in the user settings ini
	FilterListPtr->LoadSettings(GEditorUserSettingsIni, SettingsIniSection, SettingsString);
	PathViewPtr->LoadSettings(GEditorUserSettingsIni, SettingsIniSection, SettingsString);
	CollectionViewPtr->LoadSettings(GEditorUserSettingsIni, SettingsIniSection, SettingsString);
	AssetViewPtr->LoadSettings(GEditorUserSettingsIni, SettingsIniSection, SettingsString);
}

void SContentBrowser::SourcesChanged(const TArray<FString>& SelectedPaths, const TArray<FCollectionNameType>& SelectedCollections)
{
	FString NewSource = SelectedPaths.Num() > 0 ? SelectedPaths[0] : (SelectedCollections.Num() > 0 ? SelectedCollections[0].Name.ToString() : TEXT("None"));
	UE_LOG(LogContentBrowser, Verbose, TEXT("The content browser source was changed by the sources view to '%s'"), *NewSource);

	FSourcesData SourcesData;
	for (int32 PathIdx = 0; PathIdx < SelectedPaths.Num(); ++PathIdx)
	{
		SourcesData.PackagePaths.Add(FName(*SelectedPaths[PathIdx]));
	}
	
	SourcesData.Collections = SelectedCollections;

	// Update the current history data to preserve selection
	HistoryManager.UpdateHistoryData();

	// Change the filter for the asset view
	AssetViewPtr->SetSourcesData(SourcesData);

	// Add a new history data now that the source has changed
	HistoryManager.AddHistoryData();

	// Update the breadcrumb trail path
	UpdatePath();
}

void SContentBrowser::FolderEntered(const FString& FolderPath)
{
	// set the path view to the incoming path
	TArray<FString> SelectedPaths;
	SelectedPaths.Add(FolderPath);
	PathViewPtr->SetSelectedPaths(SelectedPaths);

	PathSelected(FolderPath);
}

void SContentBrowser::PathSelected(const FString& FolderPath)
{
	// You may not select both collections and paths
	CollectionViewPtr->ClearSelection();

	TArray<FString> SelectedPaths = PathViewPtr->GetSelectedPaths();
	TArray<FCollectionNameType> SelectedCollections;
	SourcesChanged(SelectedPaths, SelectedCollections);

	// Notify 'asset path changed' delegate
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>( TEXT("ContentBrowser") );
	FContentBrowserModule::FOnAssetPathChanged& PathChangedDelegate = ContentBrowserModule.GetOnAssetPathChanged();
	if(PathChangedDelegate.IsBound())
	{
		PathChangedDelegate.Broadcast(FolderPath);
	}
}

TSharedRef<FExtender> SContentBrowser::GetPathContextMenuExtender(const TArray<FString>& SelectedPaths) const
{
	return PathContextMenu->MakePathViewContextMenuExtender( SelectedPaths );
}

void SContentBrowser::CollectionSelected(const FCollectionNameType& SelectedCollection)
{
	// You may not select both collections and paths
	PathViewPtr->ClearSelection();

	TArray<FCollectionNameType> SelectedCollections = CollectionViewPtr->GetSelectedCollections();
	TArray<FString> SelectedPaths;

	if( SelectedCollections.Num() == 0  )
	{
		// just select the game folder
		SelectedPaths.Add(TEXT("/Game"));
		SourcesChanged(SelectedPaths, SelectedCollections);
	}
	else
	{
		SourcesChanged(SelectedPaths, SelectedCollections);
	}

}

void SContentBrowser::PathPickerPathSelected(const FString& FolderPath)
{
	PathPickerButton->SetIsOpen(false);

	if ( !FolderPath.IsEmpty() )
	{
		TArray<FString> Paths;
		Paths.Add(FolderPath);
		PathViewPtr->SetSelectedPaths(Paths);
	}

	PathSelected(FolderPath);
}

void SContentBrowser::PathPickerCollectionSelected(const FCollectionNameType& SelectedCollection)
{
	PathPickerButton->SetIsOpen(false);

	TArray<FCollectionNameType> Collections;
	Collections.Add(SelectedCollection);
	CollectionViewPtr->SetSelectedCollections(Collections);

	CollectionSelected(SelectedCollection);
}

void SContentBrowser::OnApplyHistoryData ( const FHistoryData& History )
{
	PathViewPtr->ApplyHistoryData(History);
	CollectionViewPtr->ApplyHistoryData(History);
	AssetViewPtr->ApplyHistoryData(History);

	// Update the breadcrumb trail path
	UpdatePath();
}

void SContentBrowser::OnUpdateHistoryData(FHistoryData& HistoryData) const
{
	const FSourcesData& SourcesData = AssetViewPtr->GetSourcesData();
	const TArray<FAssetData>& SelectedAssets = AssetViewPtr->GetSelectedAssets();

	const FString NewSource = SourcesData.PackagePaths.Num() > 0 ? SourcesData.PackagePaths[0].ToString() : (SourcesData.Collections.Num() > 0 ? SourcesData.Collections[0].Name.ToString() : LOCTEXT("AllAssets", "All Assets").ToString());

	HistoryData.HistoryDesc = NewSource;
	HistoryData.SourcesData = SourcesData;
	HistoryData.SelectedAssets.Empty();

	for ( auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt )
	{
		HistoryData.SelectedAssets.Add(AssetIt->ObjectPath);
	}
}

void SContentBrowser::NewAssetRequested(const FString& SelectedPath, TWeakObjectPtr<UClass> FactoryClass)
{
	if ( ensure(SelectedPath.Len() > 0) && ensure(FactoryClass.IsValid()) )
	{
		UFactory* NewFactory = ConstructObject<UFactory>( FactoryClass.Get() );
		FEditorDelegates::OnConfigureNewAssetProperties.Broadcast(NewFactory);
		if ( NewFactory->ConfigureProperties() )
		{
			FString DefaultAssetName;
			FString PackageNameToUse;

			static FName AssetToolsModuleName = FName("AssetTools");
			FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>(AssetToolsModuleName);
			AssetToolsModule.Get().CreateUniqueAssetName(SelectedPath + TEXT("/New") + NewFactory->GetSupportedClass()->GetName(), TEXT(""), PackageNameToUse, DefaultAssetName);
			CreateNewAsset(DefaultAssetName, SelectedPath, NewFactory->GetSupportedClass(), NewFactory);
		}
	}
}

void SContentBrowser::NewFolderRequested(const FString& SelectedPath)
{
	if( ensure(SelectedPath.Len() > 0) && AssetViewPtr.IsValid() )
	{
		CreateNewFolder(SelectedPath, FOnCreateNewFolder::CreateSP(AssetViewPtr.Get(), &SAssetView::OnCreateNewFolder));
	}
}

void SContentBrowser::OnSearchBoxChanged(const FText& InSearchText)
{
	TextFilter->SetRawFilterText( InSearchText );
	if(InSearchText.IsEmpty())
	{
		AssetViewPtr->SetUserSearching(false);
	}
	else
	{
		AssetViewPtr->SetUserSearching(true);
	}

	// Broadcast 'search box changed' delegate
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>( TEXT("ContentBrowser") );
	ContentBrowserModule.GetOnSearchBoxChanged().Broadcast(InSearchText, bIsPrimaryBrowser);	
}

void SContentBrowser::OnSearchBoxCommitted(const FText& InSearchText, ETextCommit::Type CommitInfo)
{
	TextFilter->SetRawFilterText( InSearchText );
	if(InSearchText.IsEmpty())
	{
		AssetViewPtr->SetUserSearching(false);
	}
	else
	{
		AssetViewPtr->SetUserSearching(true);
	}
}

void SContentBrowser::OnPathClicked( const FString& CrumbData )
{
	FSourcesData SourcesData = AssetViewPtr->GetSourcesData();

	if ( SourcesData.Collections.Num() > 0 )
	{
		// Collection crumb was clicked. Since we don't have a hierarchy of collections, this does nothing.
	}
	else if ( SourcesData.PackagePaths.Num() == 0 )
	{
		// No collections or paths are selected. This is "All Assets". Don't change the path when this is clicked.
	}
	else if ( SourcesData.PackagePaths.Num() > 1 || SourcesData.PackagePaths[0].ToString() != CrumbData )
	{
		// More than one path is selected or the crumb that was clicked is not the same path as the current one. Change the path.
		TArray<FString> SelectedPaths;
		SelectedPaths.Add(CrumbData);
		PathViewPtr->SetSelectedPaths(SelectedPaths);
		SourcesChanged(SelectedPaths, TArray<FCollectionNameType>());
	}
}

void SContentBrowser::OnPathMenuItemClicked(FString ClickedPath)
{
	OnPathClicked( ClickedPath );
}

TSharedPtr<SWidget> SContentBrowser::OnGetCrumbDelimiterContent(const FString& CrumbData) const
{
	FSourcesData SourcesData = AssetViewPtr->GetSourcesData();

	TSharedPtr<SWidget> Widget = SNullWidget::NullWidget;

	if( SourcesData.PackagePaths.Num() > 0 )
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		TArray<FString> SubPaths;
		const bool bRecurse = false;
		AssetRegistry.GetSubPaths( CrumbData, SubPaths, bRecurse );

		if( SubPaths.Num() > 0 )
		{
			FMenuBuilder MenuBuilder( true, NULL );

			for( int32 PathIndex = 0; PathIndex < SubPaths.Num(); ++PathIndex )
			{
				const FString& SubPath = SubPaths[PathIndex];

				// For displaying in the menu cut off the parent path since it is redundant
				FString PathWithoutParent = SubPath.RightChop( CrumbData.Len() + 1 );
				MenuBuilder.AddMenuEntry(
					FText::FromString(PathWithoutParent),
					FText::GetEmpty(),
					FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.BreadcrumbPathPickerFolder"),
					FUIAction(FExecuteAction::CreateSP(this, &SContentBrowser::OnPathMenuItemClicked, SubPath)));
			}


			// Do not allow the menu to become too large if there are many directories
			Widget =
				SNew( SVerticalBox )
				+SVerticalBox::Slot()
				.MaxHeight( 400.0f )
				[
					MenuBuilder.MakeWidget()
				];
		}
	}

	return Widget;
}

TSharedRef<SWidget> SContentBrowser::GetPathPickerContent()
{
	FPathPickerConfig PathPickerConfig;

	FSourcesData SourcesData = AssetViewPtr->GetSourcesData();
	if ( SourcesData.PackagePaths.Num() > 0 )
	{
		PathPickerConfig.DefaultPath = SourcesData.PackagePaths[0].ToString();
	}
	
	PathPickerConfig.OnPathSelected = FOnPathSelected::CreateSP(this, &SContentBrowser::PathPickerPathSelected);
	PathPickerConfig.bAllowContextMenu = false;

	return SNew(SBox)
		.WidthOverride(300)
		.HeightOverride(500)
		.Padding(4)
		[
			SNew(SVerticalBox)

			// Path Picker
			+SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				FContentBrowserSingleton::Get().CreatePathPicker(PathPickerConfig)
			]

			// Collection View
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 6, 0, 0)
			[
				SNew(SCollectionView)
				.AllowCollectionButtons(false)
				.OnCollectionSelected(this, &SContentBrowser::PathPickerCollectionSelected)
				.AllowContextMenu(false)
			]
		];
}

FString SContentBrowser::GetCurrentPath() const
{
	FString CurrentPath;
	const FSourcesData& SourcesData = AssetViewPtr->GetSourcesData();
	if ( SourcesData.PackagePaths.Num() > 0 && SourcesData.PackagePaths[0] != NAME_None )
	{
		CurrentPath = SourcesData.PackagePaths[0].ToString();
	}

	return CurrentPath;
}

TSharedRef<SWidget> SContentBrowser::MakeCreateAssetContextMenu()
{
	FString CurrentPath = GetCurrentPath();

	// Get all menu extenders for this context menu from the content browser module
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>( TEXT("ContentBrowser") );
	TArray<FContentBrowserMenuExtender> MenuExtenderDelegates = ContentBrowserModule.GetAllAssetContextMenuExtenders();

	TArray<TSharedPtr<FExtender>> Extenders;
	for (int32 i = 0; i < MenuExtenderDelegates.Num(); ++i)
	{
		if (MenuExtenderDelegates[i].IsBound())
		{
			Extenders.Add(MenuExtenderDelegates[i].Execute());
		}
	}
	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, NULL, MenuExtender);
		
	FNewAssetContextMenu::MakeContextMenu(
		MenuBuilder, 
		CurrentPath, 
		FNewAssetContextMenu::FOnNewAssetRequested::CreateSP(this, &SContentBrowser::NewAssetRequested),
		FNewAssetContextMenu::FOnNewFolderRequested::CreateSP(this, &SContentBrowser::NewFolderRequested));

	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetDisplayMetrics( DisplayMetrics );

	const FVector2D DisplaySize(
		DisplayMetrics.PrimaryDisplayWorkAreaRect.Right - DisplayMetrics.PrimaryDisplayWorkAreaRect.Left,
		DisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom - DisplayMetrics.PrimaryDisplayWorkAreaRect.Top );

	return 
		SNew(SVerticalBox)

		+SVerticalBox::Slot()
		.MaxHeight(DisplaySize.Y * 0.5)
		[
			MenuBuilder.MakeWidget()
		];
}

FString SContentBrowser::GetNewAssetToolTipText() const
{
	const FSourcesData& SourcesData = AssetViewPtr->GetSourcesData();

	// At least one source is selected
	if (SourcesData.PackagePaths.Num() > 0)
	{
		return FString::Printf( *LOCTEXT("CreateAssetToolTip", "Create an asset in %s.").ToString(), *SourcesData.PackagePaths[0].ToString() );
	}
	else
	{
		return FString();
	}
}

TSharedRef<SWidget> SContentBrowser::MakeAddFilterMenu()
{
	return FilterListPtr->ExternalMakeAddFilterMenu();
}

TSharedPtr<SWidget> SContentBrowser::GetFilterContextMenu()
{
	return FilterListPtr->ExternalMakeAddFilterMenu();
}

FReply SContentBrowser::OnSaveClicked()
{
	ContentBrowserUtils::SaveDirtyPackages();
	return FReply::Handled();
}

void SContentBrowser::OnAssetSelectionChanged(const FAssetData& SelectedAsset)
{
	if ( bIsPrimaryBrowser )
	{
		SyncGlobalSelectionSet();
	}

	// Notify 'asset selection changed' delegate
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>( TEXT("ContentBrowser") );
	FContentBrowserModule::FOnAssetSelectionChanged& AssetSelectionChangedDelegate = ContentBrowserModule.GetOnAssetSelectionChanged();
	if(AssetSelectionChangedDelegate.IsBound())
	{
		const TArray<FAssetData>& SelectedAssets = AssetViewPtr->GetSelectedAssets();
		AssetSelectionChangedDelegate.Broadcast(SelectedAssets, bIsPrimaryBrowser);
	}
}

void SContentBrowser::OnAssetsActivated(const TArray<FAssetData>& ActivatedAssets, EAssetTypeActivationMethod::Type ActivationMethod)
{
	TMap< TSharedRef<IAssetTypeActions>, TArray<UObject*> > TypeActionsToObjects;
	TArray<UObject*> ObjectsWithoutTypeActions;

	// Iterate over all activated assets to map them to AssetTypeActions.
	// This way individual asset type actions will get a batched list of assets to operate on
	for ( auto AssetIt = ActivatedAssets.CreateConstIterator(); AssetIt; ++AssetIt )
	{
		if ( !(*AssetIt).IsAssetLoaded() && FEditorFileUtils::IsMapPackageAsset((*AssetIt).ObjectPath.ToString()) )
		{
			// Skip unloaded assets in map packages, it is illegal to load them now
			continue;
		}

		UObject* Asset = (*AssetIt).GetAsset();

		if ( Asset != NULL )
		{
			FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
			TWeakPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(Asset->GetClass());
			if ( AssetTypeActions.IsValid() )
			{
				// Add this asset to the list associated with the asset type action object
				TArray<UObject*>& ObjList = TypeActionsToObjects.FindOrAdd(AssetTypeActions.Pin().ToSharedRef());
				ObjList.AddUnique(Asset);
			}
			else
			{
				ObjectsWithoutTypeActions.AddUnique(Asset);
			}
		}
	}

	// Now that we have created our map, activate all the lists of objects for each asset type action.
	for ( auto TypeActionsIt = TypeActionsToObjects.CreateConstIterator(); TypeActionsIt; ++TypeActionsIt )
	{
		const TSharedRef<IAssetTypeActions>& TypeActions = TypeActionsIt.Key();
		const TArray<UObject*>& ObjList = TypeActionsIt.Value();

		TypeActions->AssetsActivated(ObjList, ActivationMethod);
	}

	// Finally, open a simple asset editor for all assets which do not have asset type actions if activating with enter or double click
	if ( ActivationMethod == EAssetTypeActivationMethod::DoubleClicked || ActivationMethod == EAssetTypeActivationMethod::Opened )
	{
		ContentBrowserUtils::OpenEditorForAsset(ObjectsWithoutTypeActions);
	}
}

TSharedPtr<SWidget> SContentBrowser::OnGetAssetContextMenu(const TArray<FAssetData>& SelectedAssets)
{
	// If a class is selected do not open a context menu
	for(auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt)
	{
		if( AssetIt->AssetClass == NAME_Class )
		{
			return NULL;
		}
	}

	// If the Classes folder is selected do not open a context menu
	const TArray<FString> SelectedPaths = PathViewPtr->GetSelectedPaths();
	if(SelectedPaths.Contains(TEXT("/Classes")))
	{
		return NULL;
	}

	if ( SelectedAssets.Num() == 0 )
	{
		return MakeCreateAssetContextMenu();
	}
	else
	{
		return AssetContextMenu->MakeContextMenu(SelectedAssets, AssetViewPtr->GetSourcesData(), Commands);
	}
}

FReply SContentBrowser::ToggleLockClicked()
{
	bIsLocked = !bIsLocked;

	return FReply::Handled();
}

const FSlateBrush* SContentBrowser::GetToggleLockImage() const
{
	if ( bIsLocked )
	{
		return FEditorStyle::GetBrush("ContentBrowser.LockButton_Locked");
	}
	else
	{
		return FEditorStyle::GetBrush("ContentBrowser.LockButton_Unlocked");
	}
}

EVisibility SContentBrowser::GetSourcesViewVisibility() const
{
	return bSourcesViewExpanded ? EVisibility::Visible : EVisibility::Collapsed;
}

const FSlateBrush* SContentBrowser::GetSourcesToggleImage() const
{
	if ( bSourcesViewExpanded )
	{
		return FEditorStyle::GetBrush("ContentBrowser.HideSourcesView");
	}
	else
	{
		return FEditorStyle::GetBrush("ContentBrowser.ShowSourcesView");
	}
}

FReply SContentBrowser::SourcesViewExpandClicked()
{
	bSourcesViewExpanded = !bSourcesViewExpanded;

	// Notify 'Soureces View Expanded' delegate
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>( TEXT("ContentBrowser") );
	FContentBrowserModule::FOnSourcesViewChanged& SourcesViewChangedDelegate = ContentBrowserModule.GetOnSourcesViewChanged();
	if(SourcesViewChangedDelegate.IsBound())
	{
		SourcesViewChangedDelegate.Broadcast(bSourcesViewExpanded);
	}

	return FReply::Handled();
}

EVisibility SContentBrowser::GetPathExpanderVisibility() const
{
	return bSourcesViewExpanded ? EVisibility::Collapsed : EVisibility::Visible;
}

FReply SContentBrowser::BackClicked()
{
	HistoryManager.GoBack();

	return FReply::Handled();
}

FReply SContentBrowser::ForwardClicked()
{
	HistoryManager.GoForward();

	return FReply::Handled();
}

void SContentBrowser::OnOpenAssetsOrFolders()
{
	AssetViewPtr->OnOpenAssetsOrFolders();
}

void SContentBrowser::OnPreviewAssets()
{
	AssetViewPtr->OnPreviewAssets();
}

FReply SContentBrowser::OnDirectoryUpClicked()
{
	OnDirectoryUp();
	return FReply::Handled();
}

void SContentBrowser::OnDirectoryUp()
{
	TArray<FString> SelectedPaths = PathViewPtr->GetSelectedPaths();
	if(SelectedPaths.Num() == 1 && !ContentBrowserUtils::IsAssetRootDir(SelectedPaths[0]))
	{
		FString ParentDir = SelectedPaths[0] / TEXT("..");
		FPaths::CollapseRelativeDirectories(ParentDir);
		FolderEntered(ParentDir);
	}
}

bool SContentBrowser::IsBackEnabled() const
{
	return HistoryManager.CanGoBack();
}

bool SContentBrowser::IsForwardEnabled() const
{
	return HistoryManager.CanGoForward();
}

bool SContentBrowser::CanExecuteDirectoryUp() const
{
	TArray<FString> SelectedPaths = PathViewPtr->GetSelectedPaths();
	return (SelectedPaths.Num() == 1 && !ContentBrowserUtils::IsAssetRootDir(SelectedPaths[0]));
}

FString SContentBrowser::GetHistoryBackTooltip() const
{
	if ( HistoryManager.CanGoBack() )
	{
		return FString::Printf( *LOCTEXT("HistoryBackTooltip", "Back to %s").ToString(), *HistoryManager.GetBackDesc() );
	}
	else
	{
		return FString();
	}
}

FString SContentBrowser::GetHistoryForwardTooltip() const
{
	if ( HistoryManager.CanGoForward() )
	{
		return FString::Printf( *LOCTEXT("HistoryForwardTooltip", "Forward to %s").ToString(), *HistoryManager.GetForwardDesc() );
	}
	else
	{
		return FString();
	}
}

FText SContentBrowser::GetDirectoryUpTooltip() const
{
	TArray<FString> SelectedPaths = PathViewPtr->GetSelectedPaths();
	if(SelectedPaths.Num() == 1 && !ContentBrowserUtils::IsAssetRootDir(SelectedPaths[0]))
	{
		FString ParentDir = SelectedPaths[0] / TEXT("..");
		FPaths::CollapseRelativeDirectories(ParentDir);
		return FText::Format(LOCTEXT("DirectoryUpTooltip", "Up to {0}"), FText::FromString(ParentDir) );
	}

	return FText();
}

EVisibility SContentBrowser::GetDirectoryUpToolTipVisibility() const
{
	EVisibility ToolTipVisibility = EVisibility::Collapsed;

	// if we have text in our tooltip, make it visible. 
	if(GetDirectoryUpTooltip().IsEmpty() == false)
	{
		ToolTipVisibility = EVisibility::Visible;
	}

	return ToolTipVisibility;
}

void SContentBrowser::SyncGlobalSelectionSet()
{
	USelection* EditorSelection = GEditor->GetSelectedObjects();
	if ( !ensure( EditorSelection != NULL ) )
	{
		return;
	}

	// Get the selected assets in the asset view
	const TArray<FAssetData>& SelectedAssets = AssetViewPtr->GetSelectedAssets();

	EditorSelection->BeginBatchSelectOperation();
	{
		TSet< UObject* > SelectedObjects;
		// Lets see what the user has selected and add any new selected objects to the global selection set
		for ( auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt )
		{
			// Grab the object if it is loaded
			if ( (*AssetIt).IsAssetLoaded() )
			{
				UObject* FoundObject = (*AssetIt).GetAsset();
				if( FoundObject != NULL && FoundObject->GetClass() != UObjectRedirector::StaticClass() )
				{
					SelectedObjects.Add( FoundObject );

					// Select this object!
					EditorSelection->Select( FoundObject );
				}
			}
		}


		// Now we'll build a list of objects that need to be removed from the global selection set
		for( int32 CurEditorObjectIndex = 0; CurEditorObjectIndex < EditorSelection->Num(); ++CurEditorObjectIndex )
		{
			UObject* CurEditorObject = EditorSelection->GetSelectedObject( CurEditorObjectIndex );
			if( CurEditorObject != NULL ) 
			{
				if( !SelectedObjects.Contains( CurEditorObject ) )
				{
					EditorSelection->Deselect( CurEditorObject );
				}
			}
		}
	}
	EditorSelection->EndBatchSelectOperation();
}

void SContentBrowser::UpdatePath()
{
	FSourcesData SourcesData = AssetViewPtr->GetSourcesData();

	PathBreadcrumbTrail->ClearCrumbs();

	if ( SourcesData.PackagePaths.Num() > 0 )
	{
		TArray<FString> Crumbs;
		SourcesData.PackagePaths[0].ToString().ParseIntoArray(&Crumbs, TEXT("/"), true);

		FString CrumbPath = TEXT("/");
		for ( auto CrumbIt = Crumbs.CreateConstIterator(); CrumbIt; ++CrumbIt )
		{
			CrumbPath += *CrumbIt;
			PathBreadcrumbTrail->PushCrumb(FText::FromString(*CrumbIt), CrumbPath);
			CrumbPath += TEXT("/");
		}
	}
	else if ( SourcesData.Collections.Num() > 0 )
	{
		const FString CollectionName = SourcesData.Collections[0].Name.ToString();
		const FString CollectionType = FString::FromInt(SourcesData.Collections[0].Type);
		const FString CrumbData = CollectionName + TEXT("?") + CollectionType;

		FFormatNamedArguments Args;
		Args.Add(TEXT("CollectionName"), FText::FromString(CollectionName));
		const FText DisplayName = FText::Format(LOCTEXT("CollectionPathIndicator", "{CollectionName} (Collection)"), Args);

		PathBreadcrumbTrail->PushCrumb(DisplayName, CrumbData);
	}
	else
	{
		PathBreadcrumbTrail->PushCrumb(LOCTEXT("AllAssets", "All Assets"), TEXT(""));
	}
}

void SContentBrowser::OnFilterChanged()
{
	FARFilter Filter = FilterListPtr->GetCombinedBackendFilter();
	AssetViewPtr->SetBackendFilter( Filter );

	// Notify 'filter changed' delegate
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>( TEXT("ContentBrowser") );
	ContentBrowserModule.GetOnFilterChanged().Broadcast(Filter, bIsPrimaryBrowser);
}

FString SContentBrowser::GetPathText() const
{
	FString PathLabelText;

	if ( IsFilteredBySource() )
	{
		const FSourcesData& SourcesData = AssetViewPtr->GetSourcesData();

		// At least one source is selected
		int32 NumSources = SourcesData.PackagePaths.Num() + SourcesData.Collections.Num();

		if (NumSources > 0)
		{
			PathLabelText = SourcesData.PackagePaths.Num() > 0 ? SourcesData.PackagePaths[0].ToString() : SourcesData.Collections[0].Name.ToString();

			if (NumSources > 1)
			{
				PathLabelText += FString::Printf(*LOCTEXT("MultipleSourcesSuffix", " and %d others...").ToString(), NumSources - 1);
			}
		}
	}
	else
	{
		PathLabelText = LOCTEXT("AllAssets", "All Assets").ToString();
	}

	return PathLabelText;
}

bool SContentBrowser::IsFilteredBySource() const
{
	const FSourcesData& SourcesData = AssetViewPtr->GetSourcesData();
	return SourcesData.PackagePaths.Num() != 0 || SourcesData.Collections.Num() != 0;
}

bool SContentBrowser::IsAssetPathSelected() const
{
	return AssetViewPtr->IsAssetPathSelected();
}


void SContentBrowser::OnAssetRenameCommitted(const TArray<FAssetData>& Assets)
{
	// After a rename is committed we allow an implicit sync so as not to
	// disorientate the user if they are looking at a parent folder

	SyncToAssets(Assets, /*bAllowImplicitSync*/true);
}

void SContentBrowser::OnFindInAssetTreeRequested(const TArray<FAssetData>& AssetsToFind)
{
	SyncToAssets(AssetsToFind);
}

void SContentBrowser::OnRenameRequested(const FAssetData& AssetData)
{
	AssetViewPtr->RenameAsset(AssetData);
}

void SContentBrowser::OnRenameFolderRequested(const FString& FolderToRename)
{
	AssetViewPtr->RenameFolder(FolderToRename);
}

void SContentBrowser::OnDuplicateRequested(const TWeakObjectPtr<UObject>& OriginalObject)
{
	UObject* Object = OriginalObject.Get();

	if ( Object )
	{
		AssetViewPtr->DuplicateAsset(FPackageName::GetLongPackagePath(Object->GetOutermost()->GetName()), OriginalObject);
	}
}

void SContentBrowser::OnAssetViewRefreshRequested()
{
	AssetViewPtr->RequestListRefresh();
}

void SContentBrowser::HandleSettingChanged(FName PropertyName)
{
	if ((PropertyName == "DisplayDevelopersFolder") ||
		(PropertyName == "DisplayEngineFolder") ||
		(PropertyName == NAME_None))	// @todo: Needed if PostEditChange was called manually, for now
	{
		// If the dev or engine folder is no longer visible but we're inside it...
		const bool bDisplayDev = GetDefault<UContentBrowserSettings>()->GetDisplayDevelopersFolder();
		const bool bDisplayEngine = GetDefault<UContentBrowserSettings>()->GetDisplayEngineFolder();
		if ( !bDisplayDev || !bDisplayEngine )
		{
			const FString OldSelectedPath = PathViewPtr->GetSelectedPath();
			if ( (!bDisplayDev && ContentBrowserUtils::IsDevelopersFolder( OldSelectedPath )) || (!bDisplayEngine && ContentBrowserUtils::IsEngineFolder( OldSelectedPath )) )
			{
				// Set the folder back to the root, and refresh the contents
				TArray<FString> SelectedPaths;
				SelectedPaths.Add(TEXT("/Game"));
				PathViewPtr->SetSelectedPaths(SelectedPaths);
				SourcesChanged(SelectedPaths, TArray<FCollectionNameType>());
			}
		}

		// Update our path view so that it can include/exclude the dev folder
		PathViewPtr->Populate();

		// If the dev or engine folder has become visible and we're inside it...
		if ( bDisplayDev || bDisplayEngine )
		{
			const FString NewSelectedPath = PathViewPtr->GetSelectedPath();
			if ( (bDisplayDev && ContentBrowserUtils::IsDevelopersFolder( NewSelectedPath )) || (bDisplayEngine && ContentBrowserUtils::IsEngineFolder( NewSelectedPath )) )
			{
				// Refresh the contents
				TArray<FString> SelectedPaths;
				SelectedPaths.Add(NewSelectedPath);
				SourcesChanged(SelectedPaths, TArray<FCollectionNameType>());
			}
		}
	}
}

FText SContentBrowser::GetSearchAssetsHintText() const
{
	if (PathViewPtr.IsValid())
	{
		TArray<FString> Paths = PathViewPtr->GetSelectedPaths();
		if (Paths.Num() != 0)
		{
			FString SearchHint = "Search ";
			for(int i = 0; i < Paths.Num(); i++)
			{
				SearchHint += FPaths::GetCleanFilename(Paths[i]);
				if (i + 1 < Paths.Num())
					SearchHint += ", ";
			}

			return FText::FromString(SearchHint);
		}
	}
	
	return NSLOCTEXT( "ContentBrowser", "SearchBoxHint", "Search Assets" );
}

TArray<FString> SContentBrowser::GetAssetSearchSuggestions() const
{
	TArray<FString> AllSuggestions;

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	TArray< TWeakPtr<IAssetTypeActions> > AssetTypeActionsList;
	AssetToolsModule.Get().GetAssetTypeActionsList(AssetTypeActionsList);

	for ( auto TypeActionsIt = AssetTypeActionsList.CreateConstIterator(); TypeActionsIt; ++TypeActionsIt )
	{
		if ( (*TypeActionsIt).IsValid() )
		{
			const TSharedPtr<IAssetTypeActions> TypeActions = (*TypeActionsIt).Pin();
			AllSuggestions.Add( TypeActions->GetSupportedClass()->GetName() );
		}
	}

	return AllSuggestions;
}

TSharedPtr<SWidget> SContentBrowser::GetFolderContextMenu(const TArray<FString>& SelectedPaths, FContentBrowserMenuExtender_SelectedPaths InMenuExtender, FOnCreateNewFolder InOnCreateNewFolder)
{
	TSharedPtr<FExtender> Extender;
	if(InMenuExtender.IsBound())
	{
		Extender = InMenuExtender.Execute(SelectedPaths);
	}

	const bool bInShouldCloseWindowAfterSelection = true;
	FMenuBuilder MenuBuilder(bInShouldCloseWindowAfterSelection, Commands, Extender, true);

	// New Folder
	MenuBuilder.AddMenuEntry(
		LOCTEXT("NewFolder", "New Folder"),
		LOCTEXT("NewSubFolderTooltip", "Creates a new sub-folder in this folder."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.NewFolderIcon"),
		FUIAction( FExecuteAction::CreateSP( this, &SContentBrowser::CreateNewFolder, SelectedPaths.Num() > 0 ? SelectedPaths[0] : FString(), InOnCreateNewFolder ) ),
		"NewFolder"
		);

	return MenuBuilder.MakeWidget();
}

void SContentBrowser::CreateNewFolder(FString FolderPath, FOnCreateNewFolder InOnCreateNewFolder)
{
	// Create a valid base name for this folder
	FText DefaultFolderBaseName = LOCTEXT("DefaultFolderName", "NewFolder");
	FText DefaultFolderName = DefaultFolderBaseName;
	int32 NewFolderPostfix = 1;
	while(ContentBrowserUtils::DoesFolderExist(FolderPath / DefaultFolderName.ToString()))
	{
		DefaultFolderName = FText::Format(LOCTEXT("DefaultFolderNamePattern", "{0}{1}"), DefaultFolderBaseName, FText::AsNumber(NewFolderPostfix));
		NewFolderPostfix++;
	}

	InOnCreateNewFolder.ExecuteIfBound(DefaultFolderName.ToString(), FolderPath);
}

#undef LOCTEXT_NAMESPACE
