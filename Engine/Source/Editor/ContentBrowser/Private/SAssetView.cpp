// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "ContentBrowserPCH.h"
#include "SScrollBorder.h"
#include "EditorWidgets.h"
#include "AssetViewTypes.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "DragAndDrop/AssetPathDragDropOp.h"
#include "AssetThumbnail.h"
#include "AssetViewWidgets.h"
#include "FileHelpers.h"
#include "ContentBrowserModule.h"
#include "ObjectTools.h"
#include "KismetEditorUtilities.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

#define MAX_THUMBNAIL_SIZE 4096
#define MAX_CLASS_NAME_LENGTH 32 // Enforce a reasonable class name length so the path is not too long for PLATFORM_MAX_FILEPATH_LENGTH

#define MAX_PROJECTED_COOKING_PATH 165

const double SAssetView::FQuickJumpData::JumpDelaySeconds = 0.6;

SAssetView::~SAssetView()
{
	// Load the asset registry module to unregister delegates
	if ( FModuleManager::Get().IsModuleLoaded("AssetRegistry") )
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().OnAssetAdded().RemoveAll( this );
		AssetRegistryModule.Get().OnAssetRemoved().RemoveAll( this );
		AssetRegistryModule.Get().OnAssetRenamed().RemoveAll( this );
		AssetRegistryModule.Get().OnPathAdded().RemoveAll( this );
		AssetRegistryModule.Get().OnPathRemoved().RemoveAll( this );
	}

	// Unregister listener for asset loading and object property changes
	FCoreDelegates::OnAssetLoaded.RemoveAll( this );
	FCoreDelegates::OnObjectPropertyChanged.RemoveAll( this );

	// Remove the listener for when view settings are changed
	UContentBrowserSettings::OnSettingChanged().RemoveAll(this);

	if ( FrontendFilters.IsValid() )
	{
		// Clear the frontend filter changed delegate
		FrontendFilters->OnChanged().RemoveAll( this );
	}

	// Release all rendering resources being held onto
	AssetThumbnailPool->ReleaseResources();
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SAssetView::Construct( const FArguments& InArgs )
{
	IsWorking = false;
	TotalAmortizeTime = 0;
	AmortizeStartTime = 0;
	MaxSecondsPerFrame = 0.015;

	bFillEmptySpaceInTileView = InArgs._FillEmptySpaceInTileView;
	FillScale = 1.0f;

	ThumbnailHintFadeInSequence.JumpToStart();
	ThumbnailHintFadeInSequence.AddCurve(0, 0.5f, ECurveEaseFunction::Linear);

	// Load the asset registry module to listen for updates
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnAssetAdded().AddSP( this, &SAssetView::OnAssetAdded );
	AssetRegistryModule.Get().OnAssetRemoved().AddSP( this, &SAssetView::OnAssetRemoved );
	AssetRegistryModule.Get().OnAssetRenamed().AddSP( this, &SAssetView::OnAssetRenamed );
	AssetRegistryModule.Get().OnPathAdded().AddSP( this, &SAssetView::OnAssetRegistryPathAdded );
	AssetRegistryModule.Get().OnPathRemoved().AddSP( this, &SAssetView::OnAssetRegistryPathRemoved );

	FCollectionManagerModule& CollectionManagerModule = FModuleManager::LoadModuleChecked<FCollectionManagerModule>(TEXT("CollectionManager"));
	CollectionManagerModule.Get().OnAssetsAdded().AddSP( this, &SAssetView::OnAssetsAddedToCollection );
	CollectionManagerModule.Get().OnAssetsRemoved().AddSP( this, &SAssetView::OnAssetsRemovedFromCollection );
	CollectionManagerModule.Get().OnCollectionRenamed().AddSP( this, &SAssetView::OnCollectionRenamed );

	// Listen for when assets are loaded or changed to update item data
	FCoreDelegates::OnAssetLoaded.AddSP(this, &SAssetView::OnAssetLoaded);
	FCoreDelegates::OnObjectPropertyChanged.AddSP(this, &SAssetView::OnObjectPropertyChanged);

	// Listen for when view settings are changed
	UContentBrowserSettings::OnSettingChanged().AddSP(this, &SAssetView::HandleSettingChanged);

	// Get desktop metrics
	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetDisplayMetrics( DisplayMetrics );

	const FVector2D DisplaySize(
		DisplayMetrics.PrimaryDisplayWorkAreaRect.Right - DisplayMetrics.PrimaryDisplayWorkAreaRect.Left,
		DisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom - DisplayMetrics.PrimaryDisplayWorkAreaRect.Top );

	const float ThumbnailScaleRangeScalar = ( DisplaySize.Y / 1080 );

	// Create a thumbnail pool for rendering thumbnails	
	AssetThumbnailPool = MakeShareable( new FAssetThumbnailPool(1024, InArgs._AreRealTimeThumbnailsAllowed) );
	NumOffscreenThumbnails = 64;
	ListViewThumbnailResolution = 128;
	ListViewThumbnailSize = 64;
	ListViewThumbnailPadding = 4;
	TileViewThumbnailResolution = 256;
	TileViewThumbnailSize = 128;
	TileViewThumbnailPadding = 5;
	TileViewNameHeight = 36;
	ThumbnailScaleSliderValue = InArgs._ThumbnailScale; 
	ThumbnailScaleChanged = InArgs._OnThumbnailScaleChanged;

	if ( !ThumbnailScaleSliderValue.IsBound() )
	{
		ThumbnailScaleSliderValue = FMath::Clamp<float>(ThumbnailScaleSliderValue.Get(), 0.0f, 1.0f);
	}

	MinThumbnailScale = 0.6f * ThumbnailScaleRangeScalar;
	MaxThumbnailScale = 2.0f * ThumbnailScaleRangeScalar;

	bCanShowClasses = InArgs._CanShowClasses;

	bCanShowFolders = InArgs._CanShowFolders;

	bCanShowOnlyAssetsInSelectedFolders = InArgs._CanShowOnlyAssetsInSelectedFolders;
		
	bCanShowRealTimeThumbnails = InArgs._CanShowRealTimeThumbnails;

	bCanShowDevelopersFolder = InArgs._CanShowDevelopersFolder;

	bPreloadAssetsForContextMenu = InArgs._PreloadAssetsForContextMenu;

	SelectionMode = InArgs._SelectionMode;

	bPendingUpdateThumbnails = false;
	CurrentThumbnailSize = TileViewThumbnailSize;

	SourcesData = InArgs._InitialSourcesData;
	BackendFilter = InArgs._InitialBackendFilter;
	DynamicFilters = InArgs._DynamicFilters;
	if ( DynamicFilters.IsValid() )
	{
		DynamicFilters->OnChanged().AddSP( this, &SAssetView::OnDynamicFiltersChanged );
	}

	FrontendFilters = InArgs._FrontendFilters;
	if ( FrontendFilters.IsValid() )
	{
		FrontendFilters->OnChanged().AddSP( this, &SAssetView::OnFrontendFiltersChanged );
	}

	OnShouldFilterAsset = InArgs._OnShouldFilterAsset;
	OnAssetClicked = InArgs._OnAssetClicked;
	OnAssetSelected = InArgs._OnAssetSelected;
	OnAssetsActivated = InArgs._OnAssetsActivated;
	OnGetAssetContextMenu = InArgs._OnGetAssetContextMenu;
	OnGetFolderContextMenu = InArgs._OnGetFolderContextMenu;
	OnGetPathContextMenuExtender = InArgs._OnGetPathContextMenuExtender;
	OnFindInAssetTreeRequested = InArgs._OnFindInAssetTreeRequested;
	OnAssetRenameCommitted = InArgs._OnAssetRenameCommitted;
	OnAssetTagWantsToBeDisplayed = InArgs._OnAssetTagWantsToBeDisplayed;
	OnAssetDragged = InArgs._OnAssetDragged;
	HighlightedText = InArgs._HighlightedText;
	LabelVisibility = InArgs._LabelVisibility;
	ThumbnailLabel = InArgs._ThumbnailLabel;
	AllowThumbnailHintLabel = InArgs._AllowThumbnailHintLabel;
	ConstructToolTipForAsset = InArgs._ConstructToolTipForAsset;
	AssetShowWarningText = InArgs._AssetShowWarningText;
	bAllowDragging = InArgs._AllowDragging;
	bAllowFocusOnSync = InArgs._AllowFocusOnSync;
	OnPathSelected = InArgs._OnPathSelected;

	if ( InArgs._InitialViewType >= 0 && InArgs._InitialViewType < EAssetViewType::MAX )
	{
		CurrentViewType = InArgs._InitialViewType;
	}
	else
	{
		CurrentViewType = EAssetViewType::Tile;
	}

	bPendingSortFilteredItems = false;
	LastSortTime = 0;
	SortDelaySeconds = 8;

	LastProcessAddsTime = 0;

	bBulkSelecting = false;
	bAllowThumbnailEditMode = InArgs._AllowThumbnailEditMode;
	bThumbnailEditMode = false;
	bUserSearching = false;
	bPendingFocusOnSync = false;

	TagColumnRenames.Add("ResourceSize", TEXT("Size (kb)"));

	FEditorWidgetsModule& EditorWidgetsModule = FModuleManager::LoadModuleChecked<FEditorWidgetsModule>("EditorWidgets");
	TSharedRef<SWidget> AssetDiscoveryIndicator = EditorWidgetsModule.CreateAssetDiscoveryIndicator(EAssetDiscoveryIndicatorScaleMode::Scale_Vertical);

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);

	ChildSlot
	[
		VerticalBox
	];

	// Assets area
	VerticalBox->AddSlot()
	.FillHeight(1.f)
	[
		SNew( SVerticalBox ) 

		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding( 0, 0, 0, 0 )
		[
			SNew( SBox )
			.HeightOverride( 2 )
			[
				SNew(SProgressBar)
				.Percent( this, &SAssetView::GetIsWorkingProgressBarState )
				.Style( FEditorStyle::Get(), "WorkingBar" )
				.BorderPadding( FVector2D(0,0) )
			]
		]

		+SVerticalBox::Slot()
		.FillHeight(1.f)
		.Padding( 0, 0, 0, 0 )
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				// Container for the view types
				SAssignNew(ViewContainer, SBorder)
				.Padding(0)
				.BorderImage(FEditorStyle::GetBrush("NoBorder"))
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.Padding(FMargin(0, 14, 0, 0))
			[
				// A warning to display when there are no assets to show
				SAssignNew( WarningTextWidget, SRichTextBlock )
				.Justification( ETextJustify::Center )
				.Visibility( this, &SAssetView::IsAssetShowWarningTextVisible )
				.AutoWrapText( true )
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(24, 0, 24, 0))
			[
				// Asset discovery indicator
				AssetDiscoveryIndicator
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(8, 0))
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ErrorReporting.EmptyBox"))
				.BorderBackgroundColor(this, &SAssetView::GetQuickJumpColor)
				.Visibility(this, &SAssetView::IsQuickJumpVisible)
				[
					SNew(STextBlock)
					.Text(this, &SAssetView::GetQuickJumpTerm)
				]
			]
		]
	];

	// Thumbnail edit mode banner
	VerticalBox->AddSlot()
	.AutoHeight()
	.Padding(0, 4)
	[
		SNew(SBorder)
		.Visibility( this, &SAssetView::GetEditModeLabelVisibility )
		.BorderImage( FEditorStyle::GetBrush("ContentBrowser.EditModeLabelBorder") )
		.Content()
		[
			SNew( SHorizontalBox )

			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(4, 0, 0, 0)
			.FillWidth(1.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ThumbnailEditModeLabel", "Editing Thumbnails. Drag a thumbnail to rotate it if there is a 3D environment."))
				.TextStyle( FEditorStyle::Get(), "ContentBrowser.EditModeLabelFont" )
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.Text( LOCTEXT("EndThumbnailEditModeButton", "Done Editing") )
				.OnClicked( this, &SAssetView::EndThumbnailEditModeClicked )
			]
		]
	];

	if (InArgs._ShowBottomToolbar)
	{
		//// Separator
		//VerticalBox->AddSlot()
		//.AutoHeight()
		//.Padding(0, 0, 0, 1)
		//[
		//	SNew(SSeparator)
		//];

		// Bottom panel
		VerticalBox->AddSlot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			// Asset count
			+SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			.Padding(8, 0)
			[
				SNew(STextBlock) .Text(this, &SAssetView::GetAssetCountText)
			]

			// View mode combo button
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew( ViewOptionsComboButton, SComboButton )
				.ContentPadding(0)
				.ForegroundColor( this, &SAssetView::GetViewButtonForegroundColor )
				.ButtonStyle( FEditorStyle::Get(), "ToggleButton" ) // Use the tool bar item style for this button
				.OnGetMenuContent( this, &SAssetView::GetViewButtonContent )
				.ButtonContent()
				[
					SNew(SHorizontalBox)
 
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SImage).Image( FEditorStyle::GetBrush("GenericViewButton") )
					]
 
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0, 0, 0)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text( LOCTEXT("ViewButton", "View Options") )
					]
				]
			]
		];
	}

	CreateCurrentView();

	if( InArgs._InitialAssetSelection.IsValid() )
	{
		// sync to the initial item without notifying of selection
		TArray<FAssetData> AssetsToSync;
		AssetsToSync.Add( InArgs._InitialAssetSelection );
		SyncToAssets( AssetsToSync );
	}
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TOptional< float > SAssetView::GetIsWorkingProgressBarState() const
{
	return IsWorking ? TOptional< float >() : 0.0; 
}

void SAssetView::SetSourcesData(const FSourcesData& InSourcesData)
{
	// Update the path and collection lists
	SourcesData = InSourcesData;
	bRefreshSourceItemsRequested = true;
	ClearSelection();
}

const FSourcesData& SAssetView::GetSourcesData() const
{
	return SourcesData;
}

bool SAssetView::IsAssetPathSelected() const
{
	return SourcesData.PackagePaths.Num() > 0 && !SourcesData.PackagePaths[0].ToString().StartsWith(TEXT("/Classes"));
}

void SAssetView::SetBackendFilter(const FARFilter& InBackendFilter)
{
	// Update the path and collection lists
	BackendFilter = InBackendFilter;
	bRefreshSourceItemsRequested = true;
}

void SAssetView::OnCreateNewFolder(const FString& FolderName, const FString& FolderPath)
{
	// we should only be creating one deferred folder per tick
	check(!DeferredFolderToCreate.IsValid());

	// Make sure we are showing the location of the new folder (we may have created it in a folder)
	OnPathSelected.Execute(FolderPath);

	DeferredFolderToCreate = MakeShareable(new FCreateDeferredFolderData());
	DeferredFolderToCreate->FolderName = FolderName;
	DeferredFolderToCreate->FolderPath = FolderPath;
}

void SAssetView::DeferredCreateNewFolder()
{
	if(DeferredFolderToCreate.IsValid())
	{
		TSharedPtr<FAssetViewFolder> NewItem = MakeShareable(new FAssetViewFolder(DeferredFolderToCreate->FolderPath / DeferredFolderToCreate->FolderName));
		NewItem->bNewFolder = true;
		NewItem->bRenameWhenScrolledIntoview = true;
		FilteredAssetItems.Insert( NewItem, 0 );

		SetSelection(NewItem);
		RequestScrollIntoView(NewItem);

		DeferredFolderToCreate.Reset();
	}
}

void SAssetView::CreateNewAsset(const FString& DefaultAssetName, const FString& PackagePath, UClass* AssetClass, UFactory* Factory)
{
	if ( !ensure(AssetClass || Factory) )
	{
		return;
	}

	if ( AssetClass && Factory && !ensure(AssetClass->IsChildOf(Factory->GetSupportedClass())) )
	{
		return;
	}
	
	// we should only be creating one deferred asset per tick
	check(!DeferredAssetToCreate.IsValid());

	// Make sure we are showing the location of the new asset (we may have created it in a folder)
	OnPathSelected.Execute(PackagePath);

	// Defer asset creation until next tick, so we get a chance to refresh the view
	DeferredAssetToCreate = MakeShareable(new FCreateDeferredAssetData());
	DeferredAssetToCreate->DefaultAssetName = DefaultAssetName;
	DeferredAssetToCreate->PackagePath = PackagePath;
	DeferredAssetToCreate->AssetClass = AssetClass;
	DeferredAssetToCreate->Factory = Factory;
}

void SAssetView::DeferredCreateNewAsset()
{
	if(DeferredAssetToCreate.IsValid())
	{
		FString PackageNameStr = DeferredAssetToCreate->PackagePath + "/" + DeferredAssetToCreate->DefaultAssetName;
		FName PackageName = FName(*PackageNameStr);
		FName PackagePathFName = FName(*DeferredAssetToCreate->PackagePath);
		FName AssetName = FName(*DeferredAssetToCreate->DefaultAssetName);
		FName AssetClassName = DeferredAssetToCreate->AssetClass->GetFName();
		TMap<FName, FString> EmptyTags;
		TArray<int32> EmptyChunkIDs;
	
		FAssetData NewAssetData(PackageName, PackagePathFName, NAME_None, AssetName, AssetClassName, EmptyTags, EmptyChunkIDs);
		TSharedPtr<FAssetViewItem> NewItem = MakeShareable(new FAssetViewCreation(NewAssetData, DeferredAssetToCreate->AssetClass, DeferredAssetToCreate->Factory));

		NewItem->bRenameWhenScrolledIntoview = true;
		FilteredAssetItems.Insert( NewItem, 0 );
		SortManager.SortList(FilteredAssetItems, MajorityAssetType);

		SetSelection(NewItem);
		RequestScrollIntoView(NewItem);

		FEditorDelegates::OnNewAssetCreated.Broadcast(DeferredAssetToCreate->Factory);

		DeferredAssetToCreate.Reset();
	}
}

void SAssetView::DuplicateAsset(const FString& PackagePath, const TWeakObjectPtr<UObject>& OriginalObject)
{
	if ( !ensure(OriginalObject.IsValid()) )
	{
		return;
	}

	FString AssetNameStr;
	FString PackageNameStr;

	// Find a unique default name for the duplicated asset
	static FName AssetToolsModuleName = FName("AssetTools");
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>(AssetToolsModuleName);
	AssetToolsModule.Get().CreateUniqueAssetName(PackagePath + TEXT("/") + OriginalObject->GetName(), TEXT(""), PackageNameStr, AssetNameStr);

	FName PackageName = FName(*PackageNameStr);
	FName PackagePathFName = FName(*PackagePath);
	FName AssetName = FName(*AssetNameStr);
	FName AssetClass = OriginalObject->GetClass()->GetFName();
	TMap<FName, FString> EmptyTags;
	TArray<int32> EmptyChunkIDs;

	FAssetData NewAssetData(PackageName, PackagePathFName, NAME_None, AssetName, AssetClass, EmptyTags, EmptyChunkIDs);
	TSharedPtr<FAssetViewItem> NewItem = MakeShareable(new FAssetViewDuplication(NewAssetData, OriginalObject));
	NewItem->bRenameWhenScrolledIntoview = true;

	// Insert into the list and sort
	FilteredAssetItems.Insert( NewItem, 0 );
	SortManager.SortList(FilteredAssetItems, MajorityAssetType);

	SetSelection(NewItem);
	RequestScrollIntoView(NewItem);
}

void SAssetView::RenameAsset(const FAssetData& ItemToRename)
{
	if ( !FEditorFileUtils::IsMapPackageAsset(ItemToRename.ObjectPath.ToString()) )
	{
		for ( auto ItemIt = FilteredAssetItems.CreateConstIterator(); ItemIt; ++ItemIt )
		{
			const TSharedPtr<FAssetViewItem>& Item = *ItemIt;
			if ( Item.IsValid() && Item->GetType() != EAssetItemType::Folder )	
			{
				const TSharedPtr<FAssetViewAsset>& ItemAsAsset = StaticCastSharedPtr<FAssetViewAsset>(Item);
				if ( ItemAsAsset->Data.ObjectPath == ItemToRename.ObjectPath )
				{
					ItemAsAsset->bRenameWhenScrolledIntoview = true;

					SetSelection(Item);
					RequestScrollIntoView(Item);
					break;
				}
			}
		}
	}
}

void SAssetView::RenameFolder(const FString& FolderToRename)
{
	for ( auto ItemIt = FilteredAssetItems.CreateConstIterator(); ItemIt; ++ItemIt )
	{
		const TSharedPtr<FAssetViewItem>& Item = *ItemIt;
		if ( Item.IsValid() && Item->GetType() == EAssetItemType::Folder )	
		{
			const TSharedPtr<FAssetViewFolder>& ItemAsFolder = StaticCastSharedPtr<FAssetViewFolder>(Item);
			if ( ItemAsFolder->FolderPath == FolderToRename )
			{
				ItemAsFolder->bRenameWhenScrolledIntoview = true;

				SetSelection(Item);
				RequestScrollIntoView(Item);
				break;
			}
		}
	}
}

void SAssetView::SyncToAssets( const TArray<FAssetData>& AssetDataList, const bool bFocusOnSync )
{
	PendingSyncAssets.Empty();
	for ( auto AssetIt = AssetDataList.CreateConstIterator(); AssetIt; ++AssetIt )
	{
		PendingSyncAssets.Add(AssetIt->ObjectPath);
	}

	bPendingFocusOnSync = bFocusOnSync;
}

void SAssetView::ApplyHistoryData ( const FHistoryData& History )
{
	SetSourcesData(History.SourcesData);
	PendingSyncAssets = History.SelectedAssets;
	bPendingFocusOnSync = true;
}

TArray<TSharedPtr<FAssetViewItem>> SAssetView::GetSelectedItems() const
{
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::List: return ListView->GetSelectedItems();
		case EAssetViewType::Tile: return TileView->GetSelectedItems();
		case EAssetViewType::Column: return ColumnView->GetSelectedItems();
		default:
		ensure(0); // Unknown list type
		return TArray<TSharedPtr<FAssetViewItem>>();
	}
}

TArray<FAssetData> SAssetView::GetSelectedAssets() const
{
	TArray<TSharedPtr<FAssetViewItem>> SelectedItems = GetSelectedItems();
	TArray<FAssetData> SelectedAssets;
	for ( auto ItemIt = SelectedItems.CreateConstIterator(); ItemIt; ++ItemIt )
	{
		const TSharedPtr<FAssetViewItem>& Item = *ItemIt;

		// Only report non-temporary & non-folder items
		if ( Item.IsValid() && !Item->IsTemporaryItem() && Item->GetType() != EAssetItemType::Folder )	
		{
			SelectedAssets.Add(StaticCastSharedPtr<FAssetViewAsset>(Item)->Data);
		}
	}

	return SelectedAssets;
}

TArray<FString> SAssetView::GetSelectedFolders() const
{
	TArray<TSharedPtr<FAssetViewItem>> SelectedItems = GetSelectedItems();
	TArray<FString> SelectedFolders;
	for ( auto ItemIt = SelectedItems.CreateConstIterator(); ItemIt; ++ItemIt )
	{
		const TSharedPtr<FAssetViewItem>& Item = *ItemIt;
		if ( Item.IsValid() && Item->GetType() == EAssetItemType::Folder )	
		{
			SelectedFolders.Add(StaticCastSharedPtr<FAssetViewFolder>(Item)->FolderPath);
		}
	}

	return SelectedFolders;
}

void SAssetView::RequestListRefresh()
{
	bRefreshSourceItemsRequested = true;
}

void SAssetView::SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const
{
	GConfig->SetFloat(*IniSection, *(SettingsString + TEXT(".ThumbnailScale")), ThumbnailScaleSliderValue.Get(), IniFilename);
	GConfig->SetInt(*IniSection, *(SettingsString + TEXT(".CurrentViewType")), CurrentViewType, IniFilename);
}

void SAssetView::LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString)
{
	float Scale = 0.f;
	if ( GConfig->GetFloat(*IniSection, *(SettingsString + TEXT(".ThumbnailScale")), Scale, IniFilename) )
	{
		// Clamp value to normal range and update state
		Scale = FMath::Clamp<float>(Scale, 0.f, 1.f);
		SetThumbnailScale(Scale);
	}

	int32 ViewType = EAssetViewType::Tile;
	if ( GConfig->GetInt(*IniSection, *(SettingsString + TEXT(".CurrentViewType")), ViewType, IniFilename) )
	{
		// Clamp value to normal range and update state
		if ( ViewType < 0 || ViewType >= EAssetViewType::MAX)
		{
			ViewType = EAssetViewType::Tile;
		}
		SetCurrentViewType( (EAssetViewType::Type)ViewType );
	}
}

// Adjusts the selected asset by the selection delta, which should be +1 or -1)
void SAssetView::AdjustActiveSelection(int32 SelectionDelta)
{
	// Find the index of the first selected item
	TArray<TSharedPtr<FAssetViewItem>> SelectionSet = GetSelectedItems();
	
	int32 SelectedSuggestion = INDEX_NONE;

	if (SelectionSet.Num() > 0)
	{
		if (!FilteredAssetItems.Find(SelectionSet[0], /*out*/ SelectedSuggestion))
		{
			// Should never happen
			ensureMsgf(false, TEXT("SAssetView has a selected item that wasn't in the filtered list"));
			return;
		}
	}
	else
	{
		SelectedSuggestion = 0;
		SelectionDelta = 0;
	}

	if (FilteredAssetItems.Num() > 0)
	{
		// Move up or down one, wrapping around
		SelectedSuggestion = (SelectedSuggestion + SelectionDelta + FilteredAssetItems.Num()) % FilteredAssetItems.Num();

		// Pick the new asset
		const TSharedPtr<FAssetViewItem>& NewSelection = FilteredAssetItems[SelectedSuggestion];

		RequestScrollIntoView(NewSelection);
		SetSelection(NewSelection);
	}
	else
	{
		ClearSelection();
	}
}

void SAssetView::ProcessRecentlyLoadedOrChangedAssets()
{
	if ( RecentlyLoadedOrChangedAssets.Num() > 0 )
	{
		TMap< FName, TWeakObjectPtr<UObject> > NextRecentlyLoadedOrChangedMap = RecentlyLoadedOrChangedAssets;

		for (int32 AssetIdx = FilteredAssetItems.Num() - 1; AssetIdx >= 0; --AssetIdx)
		{
			if(FilteredAssetItems[AssetIdx]->GetType() != EAssetItemType::Folder)
			{
				const TSharedPtr<FAssetViewAsset>& ItemAsAsset = StaticCastSharedPtr<FAssetViewAsset>(FilteredAssetItems[AssetIdx]);
				const FName ObjectPath = ItemAsAsset->Data.ObjectPath;
				const TWeakObjectPtr<UObject>* WeakAssetPtr = RecentlyLoadedOrChangedAssets.Find( ObjectPath );
				if ( WeakAssetPtr && (*WeakAssetPtr).IsValid() )
				{
					NextRecentlyLoadedOrChangedMap.Remove(ObjectPath);

					// Found the asset in the filtered items list, update it
					const UObject* Asset = (*WeakAssetPtr).Get();
					FAssetData AssetData(Asset);

					bool bShouldRemoveAsset = false;
					TArray<FAssetData> AssetDataThatPassesFilter;
					AssetDataThatPassesFilter.Add(AssetData);
					RunAssetsThroughBackendFilter(AssetDataThatPassesFilter);
					if ( AssetDataThatPassesFilter.Num() == 0 )
					{
						bShouldRemoveAsset = true;
					}

					if ( !bShouldRemoveAsset && OnShouldFilterAsset.IsBound() && OnShouldFilterAsset.Execute(AssetData) )
					{
						bShouldRemoveAsset = true;
					}

					if ( !bShouldRemoveAsset && (IsFrontendFilterActive() && !PassesCurrentFrontendFilter(AssetData)) )
					{
						bShouldRemoveAsset = true;
					}

					if ( bShouldRemoveAsset )
					{
						FilteredAssetItems.RemoveAt(AssetIdx);
					}
					else
					{
						// Update the asset data on the item
							ItemAsAsset->SetAssetData( AssetData );
					}

					RefreshList();
				}
			}
		}

		if( FilteredRecentlyAddedAssets.Num() > 0 || RecentlyAddedAssets.Num() > 0 )
		{
			//Keep unprocessed items as we are still processing assets
			RecentlyLoadedOrChangedAssets = NextRecentlyLoadedOrChangedMap;
		}
		else
		{
			//No more assets coming in so if we haven't found them now we aren't going to
			RecentlyLoadedOrChangedAssets.Empty();
		}
	}
}

void SAssetView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	CalculateFillScale( AllottedGeometry );

	CurrentTime = InCurrentTime;

	// If there were any assets that were recently added via the asset registry, process them now
	ProcessRecentlyAddedAssets();

	// If there were any assets loaded since last frame that we are currently displaying thumbnails for, push them on the render stack now.
	ProcessRecentlyLoadedOrChangedAssets();

	CalculateThumbnailHintColorAndOpacity();

	if ( bPendingUpdateThumbnails )
	{
		UpdateThumbnails();
		bPendingUpdateThumbnails = false;
	}

	if ( bRefreshSourceItemsRequested )
	{
		ResetQuickJump();
		RefreshSourceItems();
		RefreshFilteredItems();
		RefreshFolders();
		// Don't sync to selection if we are just going to do it below
		SortList(!PendingSyncAssets.Num());
		bRefreshSourceItemsRequested = false;
	}

	if ( QueriedAssetItems.Num() > 0 )
	{
		check( OnShouldFilterAsset.IsBound() );
		double TickStartTime = FPlatformTime::Seconds();

		// Mark the first amortize time
		if ( AmortizeStartTime == 0 )
		{
			AmortizeStartTime = FPlatformTime::Seconds();
			IsWorking = true;
		}

		ProcessQueriedItems( TickStartTime );

		if ( QueriedAssetItems.Num() == 0 )
		{
			TotalAmortizeTime += FPlatformTime::Seconds() - AmortizeStartTime;
			AmortizeStartTime = 0;
			IsWorking = false;
		}
	}

	if ( PendingSyncAssets.Num() )
	{
		if (bPendingSortFilteredItems)
		{
			// Don't sync to selection because we are just going to do it below
			SortList(/*bSyncToSelection=*/false);
		}

		bBulkSelecting = true;
		ClearSelection();
		bool bFoundScrollIntoViewTarget = false;
		for ( auto ItemIt = FilteredAssetItems.CreateConstIterator(); ItemIt; ++ItemIt )
		{
			const auto& Item = *ItemIt;
			if(Item.IsValid() && Item->GetType() != EAssetItemType::Folder)
			{
				const TSharedPtr<FAssetViewAsset>& ItemAsAsset = StaticCastSharedPtr<FAssetViewAsset>(Item);
				if ( PendingSyncAssets.Contains(ItemAsAsset->Data.ObjectPath) )
				{
					SetItemSelection(*ItemIt, true, ESelectInfo::OnNavigation);

					// Scroll the first item in the list that can be shown into view
					if ( !bFoundScrollIntoViewTarget )
					{
						RequestScrollIntoView(Item);
						bFoundScrollIntoViewTarget = true;
					}
				}
			}
		}
		
		bBulkSelecting = false;

		PendingSyncAssets.Empty();

		if (bAllowFocusOnSync && bPendingFocusOnSync)
		{
			FocusList();
		}
	}

	if ( IsHovered() )
	{
		// This prevents us from sorting the view immediately after the cursor leaves it
		LastSortTime = CurrentTime;
	}
	else if ( bPendingSortFilteredItems && InCurrentTime > LastSortTime + SortDelaySeconds )
	{
		SortList();
	}

	// create any assets & folders we need to now
	DeferredCreateNewAsset();
	DeferredCreateNewFolder();

	AssetThumbnailPool->Tick(InDeltaTime);

	// Do quick-jump last as the Tick function might have canceled it
	if(QuickJumpData.bHasChangedSinceLastTick)
	{
		QuickJumpData.bHasChangedSinceLastTick = false;

		const bool bWasJumping = QuickJumpData.bIsJumping;
		QuickJumpData.bIsJumping = true;

		QuickJumpData.LastJumpTime = InCurrentTime;
		QuickJumpData.bHasValidMatch = PerformQuickJump(bWasJumping);
	}
	else if(QuickJumpData.bIsJumping && InCurrentTime > QuickJumpData.LastJumpTime + FQuickJumpData::JumpDelaySeconds)
	{
		ResetQuickJump();
	}

	if ( IsAssetShowWarningTextVisible() == EVisibility::Visible )
	{
		FText WarningText = GetAssetShowWarningText();
		if ( WarningText.CompareTo( CachedWarningText ) != 0)
		{
			CachedWarningText = WarningText;
			WarningTextWidget->SetText( WarningText );
		}
	}
}

void SAssetView::CalculateFillScale( const FGeometry& AllottedGeometry )
{
	if ( bFillEmptySpaceInTileView && CurrentViewType == EAssetViewType::Tile )
	{
		float ItemWidth = GetTileViewItemBaseWidth();

		// Scrollbars are 16, but we add 1 to deal with half pixels.
		const float ScrollbarWidth = 16 + 1;
		float TotalWidth = AllottedGeometry.Size.X - ( ScrollbarWidth / AllottedGeometry.Scale );
		float Coverage = TotalWidth / ItemWidth;
		int32 Items = (int)( TotalWidth / ItemWidth );

		// If there isn't enough room to support even a single item, don't apply a fill scale.
		if ( Items > 0 )
		{
			float GapSpace = ItemWidth * ( Coverage - Items );
			float ExpandAmount = GapSpace / (float)Items;
			FillScale = ( ItemWidth + ExpandAmount ) / ItemWidth;
			FillScale = FMath::Max( 1.0f, FillScale );
		}
		else
		{
			FillScale = 1.0f;
		}
	}
	else
	{
		FillScale = 1.0f;
	}
}

void SAssetView::CalculateThumbnailHintColorAndOpacity()
{
	if ( HighlightedText.Get().IsEmpty() )
	{
		if ( ThumbnailHintFadeInSequence.IsPlaying() )
		{
			if ( ThumbnailHintFadeInSequence.IsForward() )
			{
				ThumbnailHintFadeInSequence.Reverse();
			}
		}
		else if ( ThumbnailHintFadeInSequence.IsAtEnd() ) 
		{
			ThumbnailHintFadeInSequence.PlayReverse();
		}
	}
	else 
	{
		if ( ThumbnailHintFadeInSequence.IsPlaying() )
		{
			if ( ThumbnailHintFadeInSequence.IsInReverse() )
			{
				ThumbnailHintFadeInSequence.Reverse();
			}
		}
		else if ( ThumbnailHintFadeInSequence.IsAtStart() ) 
		{
			ThumbnailHintFadeInSequence.Play();
		}
	}

	const float Opacity = ThumbnailHintFadeInSequence.GetLerp();
	ThumbnailHintColorAndOpacity = FLinearColor( 1.0, 1.0, 1.0, Opacity );
}

void SAssetView::ProcessQueriedItems( const double TickStartTime )
{
	const bool bFlushFullBuffer = TickStartTime < 0;

	bool ListNeedsRefresh = false;
	int32 AssetIndex = 0;
	for ( AssetIndex = QueriedAssetItems.Num() - 1; AssetIndex >= 0 ; AssetIndex--)
	{
		if ( !OnShouldFilterAsset.Execute( QueriedAssetItems[AssetIndex] ) )
		{
			AssetItems.Add( QueriedAssetItems[AssetIndex] );

			if ( !IsFrontendFilterActive() )
			{
				const FAssetData& AssetData = QueriedAssetItems[AssetIndex];
				FilteredAssetItems.Add(MakeShareable(new FAssetViewAsset(AssetData)));
				ListNeedsRefresh = true;
				bPendingSortFilteredItems = true;
			}
			else if ( PassesCurrentFrontendFilter( QueriedAssetItems[AssetIndex] ) )
			{
				const FAssetData& AssetData = QueriedAssetItems[AssetIndex];
				FilteredAssetItems.Add(MakeShareable(new FAssetViewAsset(AssetData)));
				ListNeedsRefresh = true;
				bPendingSortFilteredItems = true;
			}
		}

		// Check to see if we have run out of time in this tick
		if ( !bFlushFullBuffer && (FPlatformTime::Seconds() - TickStartTime) > MaxSecondsPerFrame)
		{
			break;
		}
	}

	// Trim the results array
	if (AssetIndex > 0)
	{
		QueriedAssetItems.RemoveAt( AssetIndex, QueriedAssetItems.Num() - AssetIndex );
	}
	else
	{
		QueriedAssetItems.Empty();
	}

	if ( ListNeedsRefresh )
	{
		RefreshList();
	}
}

void SAssetView::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	TSharedPtr< FAssetDragDropOp > DragAssetOp = DragDropEvent.GetOperationAs< FAssetDragDropOp >();
	if( DragAssetOp.IsValid() )
	{
		DragAssetOp->ResetToDefaultToolTip();
	}
}

FReply SAssetView::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr< FExternalDragOperation > DragDropOp = DragDropEvent.GetOperationAs< FExternalDragOperation >();
	if ( DragDropOp.IsValid() )
	{
		if ( DragDropOp->HasFiles() )
		{
			return FReply::Handled();
		}
	}
	else if ( HasSingleCollectionSource() )
	{
		TArray< FAssetData > AssetDatas = AssetUtil::ExtractAssetDataFromDrag( DragDropEvent );

		if ( AssetDatas.Num() > 0 )
		{
			TSharedPtr< FAssetDragDropOp > DragAssetOp = DragDropEvent.GetOperationAs< FAssetDragDropOp >();
			if( DragAssetOp.IsValid() )
			{
				TArray< FName > ObjectPaths;
				FCollectionManagerModule& CollectionManagerModule = FModuleManager::LoadModuleChecked<FCollectionManagerModule>(TEXT("CollectionManager"));
				CollectionManagerModule.Get().GetObjectsInCollection( SourcesData.Collections[0].Name, SourcesData.Collections[0].Type, ObjectPaths );

				bool IsValidDrop = false;
				for (int Index = 0; Index < AssetDatas.Num(); Index++)
				{
					if ( !ObjectPaths.Contains( AssetDatas[Index].ObjectPath ) )
					{
						IsValidDrop = true;
						break;
					}
				}

				if ( IsValidDrop )
				{
					DragAssetOp->SetToolTip( NSLOCTEXT( "AssetView", "OnDragOverCollection", "Add to Collection" ), FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK"))) ;
				}
			}

			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

FReply SAssetView::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	// Handle drag drop for import
	if ( IsAssetPathSelected() )
	{
		TSharedPtr<FExternalDragOperation> DragDropOp = DragDropEvent.GetOperationAs<FExternalDragOperation>();
		if (DragDropOp.IsValid())
		{
			if ( DragDropOp->HasFiles() )
			{
				FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
				AssetToolsModule.Get().ImportAssets( DragDropOp->GetFiles(), SourcesData.PackagePaths[0].ToString() );
			}

			return FReply::Handled();
		}
	}
	else if ( HasSingleCollectionSource() )
	{
		TArray< FAssetData > SelectedAssetDatas = AssetUtil::ExtractAssetDataFromDrag( DragDropEvent );

		if ( SelectedAssetDatas.Num() > 0 )
		{
			TArray< FName > ObjectPaths;
			for (int Index = 0; Index < SelectedAssetDatas.Num(); Index++)
			{
				ObjectPaths.Add( SelectedAssetDatas[Index].ObjectPath );
			}

			FCollectionManagerModule& CollectionManagerModule = FModuleManager::LoadModuleChecked<FCollectionManagerModule>(TEXT("CollectionManager"));
			CollectionManagerModule.Get().AddToCollection( SourcesData.Collections[0].Name, SourcesData.Collections[0].Type, ObjectPaths );

			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

FReply SAssetView::OnKeyChar( const FGeometry& MyGeometry,const FCharacterEvent& InCharacterEvent )
{
	const bool bTestOnly = false;
	if(HandleQuickJumpKeyDown(InCharacterEvent.GetCharacter(), InCharacterEvent.IsControlDown(), InCharacterEvent.IsAltDown(), bTestOnly).IsEventHandled())
	{
		return FReply::Handled();
	}

	// If the user pressed a key we couldn't handle, reset the quick-jump search
	ResetQuickJump();

	return FReply::Unhandled();
}

FReply SAssetView::OnKeyDown( const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent )
{
	{
		// Swallow the key-presses used by the quick-jump in OnKeyChar to avoid other things (such as the viewport commands) getting them instead
		// eg) Pressing "W" without this would set the viewport to "translate" mode
		const bool bTestOnly = true;
		if(HandleQuickJumpKeyDown(InKeyboardEvent.GetCharacter(), InKeyboardEvent.IsControlDown(), InKeyboardEvent.IsAltDown(), bTestOnly).IsEventHandled())
		{
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

FReply SAssetView::OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if( MouseEvent.IsControlDown() )
	{
		const float DesiredScale = FMath::Clamp<float>(GetThumbnailScale() + ( MouseEvent.GetWheelDelta() * 0.05f ), 0.0f, 1.0f);
		if ( DesiredScale != GetThumbnailScale() )
		{
			SetThumbnailScale( DesiredScale );
		}		
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SAssetView::OnKeyboardFocusChanging( const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath )
{
	ResetQuickJump();
}

TSharedRef<SAssetTileView> SAssetView::CreateTileView()
{
	return SNew(SAssetTileView)
		.SelectionMode( SelectionMode )
		.ListItemsSource(&FilteredAssetItems)
		.OnGenerateTile(this, &SAssetView::MakeTileViewWidget)
		.OnItemScrolledIntoView(this, &SAssetView::ItemScrolledIntoView)
		.OnContextMenuOpening(this, &SAssetView::OnGetContextMenuContent)
		.OnMouseButtonDoubleClick(this, &SAssetView::OnListMouseButtonDoubleClick)
		.OnSelectionChanged(this, &SAssetView::AssetSelectionChanged)
		.ItemHeight(this, &SAssetView::GetTileViewItemHeight)
		.ItemWidth(this, &SAssetView::GetTileViewItemWidth);
}

TSharedRef<SAssetListView> SAssetView::CreateListView()
{
	return SNew(SAssetListView)
		.SelectionMode( SelectionMode )
		.ListItemsSource(&FilteredAssetItems)
		.OnGenerateRow(this, &SAssetView::MakeListViewWidget)
		.OnItemScrolledIntoView(this, &SAssetView::ItemScrolledIntoView)
		.OnContextMenuOpening(this, &SAssetView::OnGetContextMenuContent)
		.OnMouseButtonDoubleClick(this, &SAssetView::OnListMouseButtonDoubleClick)
		.OnSelectionChanged(this, &SAssetView::AssetSelectionChanged)
		.ItemHeight(this, &SAssetView::GetListViewItemHeight);
}

TSharedRef<SAssetColumnView> SAssetView::CreateColumnView()
{
	return SNew(SAssetColumnView)
		.SelectionMode( SelectionMode )
		.ListItemsSource(&FilteredAssetItems)
		.OnGenerateRow(this, &SAssetView::MakeColumnViewWidget)
		.OnItemScrolledIntoView(this, &SAssetView::ItemScrolledIntoView)
		.OnContextMenuOpening(this, &SAssetView::OnGetContextMenuContent)
		.OnMouseButtonDoubleClick(this, &SAssetView::OnListMouseButtonDoubleClick)
		.OnSelectionChanged(this, &SAssetView::AssetSelectionChanged)
		.Visibility(this, &SAssetView::GetColumnViewVisibility)
		.HeaderRow
		(
			SNew(SHeaderRow)
			+ SHeaderRow::Column(SortManager.NameColumnId)
			.FillWidth(300)
			.SortMode( TAttribute< EColumnSortMode::Type >::Create( TAttribute< EColumnSortMode::Type >::FGetter::CreateSP( this, &SAssetView::GetColumnSortMode, SortManager.NameColumnId ) ) )
			.OnSort( FOnSortModeChanged::CreateSP( this, &SAssetView::OnSortColumnHeader ) )
			.DefaultLabel( LOCTEXT("Column_Name", "Name") )
			//@TODO: Query the OnAssetTagWantsToBeDisplayed column filter here too, in case the user wants to bury the type column
			+ SHeaderRow::Column(SortManager.ClassColumnId)
			.FillWidth(160)
			.SortMode( TAttribute< EColumnSortMode::Type >::Create( TAttribute< EColumnSortMode::Type >::FGetter::CreateSP( this, &SAssetView::GetColumnSortMode, SortManager.ClassColumnId ) ) )
			.OnSort( FOnSortModeChanged::CreateSP( this, &SAssetView::OnSortColumnHeader ) )
			.DefaultLabel( LOCTEXT("Column_Class", "Type") )
		);
}

bool SAssetView::IsValidSearchToken(const FString& Token) const
{
	if ( Token.Len() == 0 )
	{
		return false;
	}

	// A token may not be only apostrophe only, or it will match every asset because the text filter compares against the pattern Class'ObjectPath'
	if ( Token.Len() == 1 && Token[0] == '\'' )
	{
		return false;
	}

	return true;
}

void SAssetView::RefreshSourceItems()
{
	// Load the asset registry module
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	RecentlyLoadedOrChangedAssets.Empty();
	RecentlyAddedAssets.Empty();
	FilteredRecentlyAddedAssets.Empty();
	QueriedAssetItems.Empty();
	AssetItems.Empty();
	FilteredAssetItems.Empty();
	VisibleItems.Empty();
	RelevantThumbnails.Empty();
	Folders.Empty();

	TArray<FAssetData>& Items = OnShouldFilterAsset.IsBound() ? QueriedAssetItems : AssetItems;

	const bool bShowAll = SourcesData.IsEmpty() && BackendFilter.IsEmpty();

	bool bWantToShowShowClasses = false;

	if ( bShowAll )
	{
		AssetRegistryModule.Get().GetAllAssets(Items);
		bWantToShowShowClasses = true;
	}
	else
	{
		// Assemble the filter using the current sources
		// force recursion when the user is searching
		const bool bRecurse = ShouldFilterRecursively();
		const bool bUsingFolders = GetDefault<UContentBrowserSettings>()->ShowOnlyAssetsInSelectedFolders || IsShowingFolders();
		FARFilter Filter = SourcesData.MakeFilter(bRecurse, bUsingFolders);

		// Remove the classes path if it is in the list. We will add classes to the results later
		bWantToShowShowClasses = Filter.PackagePaths.Remove(TEXT("/Classes")) > 0;

		if ( SourcesData.Collections.Num() > 0 && Filter.ObjectPaths.Num() == 0 )
		{
			// This is an empty collection, no asset will pass the check
		}
		else
		{
			// Add the backend filters from the filter list
			Filter.Append(BackendFilter);

			// Add assets found in the asset registry
			AssetRegistryModule.Get().GetAssets(Filter, Items);
		}

		TArray< FName > ClassPaths;
		FCollectionManagerModule& CollectionManagerModule = FModuleManager::GetModuleChecked<FCollectionManagerModule>(TEXT("CollectionManager"));
		for (int Index = 0; Index < SourcesData.Collections.Num(); Index++)
		{
			CollectionManagerModule.Get().GetClassesInCollection( SourcesData.Collections[Index].Name, SourcesData.Collections[Index].Type, ClassPaths );
		}

		for (int Index = 0; Index < ClassPaths.Num(); Index++)
		{
			UClass* Class = FindObject<UClass>(ANY_PACKAGE, *ClassPaths[Index].ToString());

			if ( Class != NULL )
			{
				Items.Add( Class );
			}
		}
	}

	// If we are showing classes in the asset list...
	if (bWantToShowShowClasses && bCanShowClasses)
	{
		// Make a map of UClasses to ActorFactories that support them
		const TArray< UActorFactory *>& ActorFactories = GEditor->ActorFactories;
		TMap<UClass*, UActorFactory*> ActorFactoryMap;
		for ( int32 FactoryIdx = 0; FactoryIdx < ActorFactories.Num(); ++FactoryIdx )
		{
			UActorFactory* ActorFactory = ActorFactories[FactoryIdx];

			if ( ActorFactory )
			{
				ActorFactoryMap.Add(ActorFactory->GetDefaultActorClass( FAssetData() ), ActorFactory);
			}
		}

		// Add loaded classes
		FText UnusedErrorMessage;
		FAssetData NoAssetData;
		for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
		{
			// Don't offer skeleton classes
			bool bIsSkeletonClass = FKismetEditorUtilities::IsClassABlueprintSkeleton(*ClassIt);

			if ( !ClassIt->HasAllClassFlags(CLASS_NotPlaceable) &&
				!ClassIt->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) &&
				ClassIt->IsChildOf(AActor::StaticClass()) &&
				(!ClassIt->IsChildOf(ABrush::StaticClass()) || ClassIt->IsChildOf(AVolume::StaticClass())) &&
				!bIsSkeletonClass )
			{
				UActorFactory** ActorFactory = ActorFactoryMap.Find(*ClassIt);

				if ( !ActorFactory || (*ActorFactory)->CanCreateActorFrom( NoAssetData, UnusedErrorMessage ) )
				{
					Items.Add(FAssetData(*ClassIt));
				}
			}
		}
	}

	// Remove any assets that should be filtered out any redirectors and non-assets
	const bool bDisplayEngine = GetDefault<UContentBrowserSettings>()->GetDisplayEngineFolder();
	for (int32 AssetIdx = Items.Num() - 1; AssetIdx >= 0; --AssetIdx)
	{
		const FAssetData& Item = Items[AssetIdx];
		if ( Item.AssetClass == UObjectRedirector::StaticClass()->GetFName() && !Item.IsUAsset() )
		{
			// Do not show redirectors if they are not the main asset in the uasset file.
			Items.RemoveAt(AssetIdx);
		}
		else if ( !bDisplayEngine && ContentBrowserUtils::IsEngineFolder(Item.PackagePath.ToString()) )
		{
			// If this is an engine folder, and we don't want to show them, remove
			Items.RemoveAt(AssetIdx);
		}
	}
}

bool SAssetView::ShouldFilterRecursively() const
{
	// Quick check for conditions which force recursive filtering
	if (bUserSearching || !BackendFilter.IsEmpty())
	{
		return true;
	}

	// Otherwise, check if there are any non-inverse frontend filters selected
	if (FrontendFilters.IsValid())
	{
		for (int32 FilterIndex = 0; FilterIndex < FrontendFilters->Num(); ++FilterIndex)
		{
			const auto* Filter = static_cast<FFrontendFilter*>(FrontendFilters->GetFilterAtIndex(FilterIndex).Get());
			if (Filter)
			{
				if (!Filter->IsInverseFilter())
				{
					return true;
				}
			}
		}
	}

	// No filters, do not override folder view with recursive filtering
	return false;
}

void SAssetView::RefreshFilteredItems()
{
	//Build up a map of the existing AssetItems so we can preserve them while filtering
	TMap< FName, TSharedPtr< FAssetViewAsset > > ItemToObjectPath;
	for (int Index = 0; Index < FilteredAssetItems.Num(); Index++)
	{
		if(FilteredAssetItems[Index].IsValid() && FilteredAssetItems[Index]->GetType() != EAssetItemType::Folder)
		{
			TSharedPtr<FAssetViewAsset> Item = StaticCastSharedPtr<FAssetViewAsset>(FilteredAssetItems[Index]);
			ItemToObjectPath.Add( Item->Data.ObjectPath, Item );
		}
	}

	// Empty all the filtered lists
	FilteredAssetItems.Empty();
	VisibleItems.Empty();
	RelevantThumbnails.Empty();
	Folders.Empty();

	// true if the results from the asset registry query are filtered further by the content browser
	const bool bIsFrontendFilterActive = IsFrontendFilterActive();

	// true if we are looking at columns so we need to determine the majority asset type
	const bool bGatherAssetTypeCount = CurrentViewType == EAssetViewType::Column;
	TMap<FName, int32> AssetTypeCount;

	if ( bIsFrontendFilterActive && FrontendFilters.IsValid() )
	{
		const bool bRecurse = ShouldFilterRecursively();
		const bool bUsingFolders = GetDefault<UContentBrowserSettings>()->ShowOnlyAssetsInSelectedFolders || IsShowingFolders();
		FARFilter CombinedFilter = SourcesData.MakeFilter(bRecurse, bUsingFolders);
		CombinedFilter.Append(BackendFilter);

		// Let the frontend filters know the currently used filter in case it is necessary to conditionally filter based on path or class filters
		for ( int32 FilterIdx = 0; FilterIdx < FrontendFilters->Num(); ++FilterIdx )
		{
			// There are only FFrontendFilters in this collection
			const TSharedPtr<FFrontendFilter>& Filter = StaticCastSharedPtr<FFrontendFilter>( FrontendFilters->GetFilterAtIndex(FilterIdx) );
			if ( Filter.IsValid() )
			{
				Filter->SetCurrentFilter(CombinedFilter);
			}
		}
	}

	if ( bIsFrontendFilterActive && bGatherAssetTypeCount )
	{
		// Check the frontend filter for every asset and keep track of how many assets were found of each type
		for (int32 AssetIdx = 0; AssetIdx < AssetItems.Num(); ++AssetIdx)
		{
			const FAssetData& AssetData = AssetItems[AssetIdx];
			if ( PassesCurrentFrontendFilter(AssetData) )
			{
				const TSharedPtr< FAssetViewAsset >* AssetItem = ItemToObjectPath.Find( AssetData.ObjectPath );

				if ( AssetItem != NULL )
				{
					FilteredAssetItems.Add(*AssetItem);
				}
				else
				{
					FilteredAssetItems.Add(MakeShareable(new FAssetViewAsset(AssetData)));
				}

				int32* TypeCount = AssetTypeCount.Find(AssetData.AssetClass);
				if ( TypeCount )
				{
					(*TypeCount)++;
				}
				else
				{
					AssetTypeCount.Add(AssetData.AssetClass, 1);
				}
			}
		}
	}
	else if ( bIsFrontendFilterActive && !bGatherAssetTypeCount )
	{
		// Check the frontend filter for every asset and don't worry about asset type counts
		for (int32 AssetIdx = 0; AssetIdx < AssetItems.Num(); ++AssetIdx)
		{
			const FAssetData& AssetData = AssetItems[AssetIdx];
			if ( PassesCurrentFrontendFilter(AssetData) )
			{
				const TSharedPtr< FAssetViewAsset >* AssetItem = ItemToObjectPath.Find( AssetData.ObjectPath );

				if ( AssetItem != NULL )
				{
					FilteredAssetItems.Add(*AssetItem);
				}
				else
				{
					FilteredAssetItems.Add(MakeShareable(new FAssetViewAsset(AssetData)));
				}
			}
		}
	}
	else if ( !bIsFrontendFilterActive && bGatherAssetTypeCount )
	{
		// Don't need to check the frontend filter for every asset but keep track of how many assets were found of each type
		for (int32 AssetIdx = 0; AssetIdx < AssetItems.Num(); ++AssetIdx)
		{
			const FAssetData& AssetData = AssetItems[AssetIdx];
			const TSharedPtr< FAssetViewAsset >* AssetItem = ItemToObjectPath.Find( AssetData.ObjectPath );

			if ( AssetItem != NULL )
			{
				FilteredAssetItems.Add(*AssetItem);
			}
			else
			{
				FilteredAssetItems.Add(MakeShareable(new FAssetViewAsset(AssetData)));
			}

			int32* TypeCount = AssetTypeCount.Find(AssetData.AssetClass);
			if ( TypeCount )
			{
				(*TypeCount)++;
			}
			else
			{
				AssetTypeCount.Add(AssetData.AssetClass, 1);
			}
		}
	}
	else if ( !bIsFrontendFilterActive && !bGatherAssetTypeCount )
	{
		// Don't check the frontend filter and don't count the number of assets of each type. Just add all assets.
		for (int32 AssetIdx = 0; AssetIdx < AssetItems.Num(); ++AssetIdx)
		{
			const FAssetData& AssetData = AssetItems[AssetIdx];
			const TSharedPtr< FAssetViewAsset >* AssetItem = ItemToObjectPath.Find( AssetData.ObjectPath );

			if ( AssetItem != NULL )
			{
				FilteredAssetItems.Add(*AssetItem);
			}
			else
			{
				FilteredAssetItems.Add(MakeShareable(new FAssetViewAsset(AssetData)));
			}
		}
	}
	else
	{
		// The above cases should handle all combinations of bIsFrontendFilterActive and bGatherAssetTypeCount
		ensure(0);
	}

	if ( bGatherAssetTypeCount )
	{
		int32 HighestCount = 0;
		FName HighestType;
		for ( auto TypeIt = AssetTypeCount.CreateConstIterator(); TypeIt; ++TypeIt )
		{
			if ( TypeIt.Value() > HighestCount )
			{
				HighestType = TypeIt.Key();
				HighestCount = TypeIt.Value();
			}
		}

		SetMajorityAssetType(HighestType);
	}
}

void SAssetView::RefreshFolders()
{
	if(IsShowingFolders() && !ShouldFilterRecursively())
	{
		const bool bDisplayDev = GetDefault<UContentBrowserSettings>()->GetDisplayDevelopersFolder();
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		for(auto SourcePathIt(SourcesData.PackagePaths.CreateConstIterator()); SourcePathIt; SourcePathIt++)
		{		
			TArray<FString> SubPaths;
			AssetRegistryModule.Get().GetSubPaths((*SourcePathIt).ToString(), SubPaths, false);
			for(auto SubPathIt(SubPaths.CreateConstIterator()); SubPathIt; SubPathIt++)	
			{
				// If this is a developer folder, and we don't want to show them try the next path
				if ( !bDisplayDev && ContentBrowserUtils::IsDevelopersFolder(*SubPathIt) )
				{
					continue;
				}

				if(!Folders.Contains(*SubPathIt))
				{
					FilteredAssetItems.Add(MakeShareable(new FAssetViewFolder(*SubPathIt)));
					Folders.Add(*SubPathIt);
					bPendingSortFilteredItems = true;
				}
			}
		}
	}
}

void SAssetView::SetMajorityAssetType(FName NewMajorityAssetType)
{
	if ( NewMajorityAssetType != MajorityAssetType )
	{
		UE_LOG(LogContentBrowser, Verbose, TEXT("The majority of assets in the view are of type: %s"), *NewMajorityAssetType.ToString());

		MajorityAssetType = NewMajorityAssetType;

		// Since the asset type has changed, remove all columns except name and class
		const TIndirectArray<SHeaderRow::FColumn>& Columns = ColumnView->GetHeaderRow()->GetColumns();

		for ( int32 ColumnIdx = Columns.Num() - 1; ColumnIdx >= 0; --ColumnIdx )
		{
			const FName ColumnId = Columns[ColumnIdx].ColumnId;
			if ( ColumnId != SortManager.NameColumnId && ColumnId != SortManager.ClassColumnId && ColumnId != NAME_None )
			{
				ColumnView->GetHeaderRow()->RemoveColumn(ColumnId);
			}
		}

		// Keep track of the current column name to see if we need to change it now that columns are being removed
		// Name, Class, and Path are always relevant
		const FName CurrentSortColumn = SortManager.GetSortColumnId();
		bool bSortColumnStillRelevant = CurrentSortColumn == FAssetViewSortManager::NameColumnId
									|| CurrentSortColumn == FAssetViewSortManager::ClassColumnId
									|| CurrentSortColumn == FAssetViewSortManager::PathColumnId;

		// If we have a new majority type, add the new type's columns
		if ( NewMajorityAssetType != NAME_None )
		{
			// Determine the columns by querying the CDO for the tag map
			UClass* TypeClass = FindObject<UClass>(ANY_PACKAGE, *NewMajorityAssetType.ToString());
			if ( TypeClass )
			{
				UObject* CDO = TypeClass->GetDefaultObject();
				if ( CDO )
				{
					TArray<UObject::FAssetRegistryTag> AssetRegistryTags;
					CDO->GetAssetRegistryTags(AssetRegistryTags);

					// Add a column for every tag that isn't hidden
					for ( auto TagIt = AssetRegistryTags.CreateConstIterator(); TagIt; ++TagIt )
					{
						if ( TagIt->Type != UObject::FAssetRegistryTag::TT_Hidden )
						{
							const FName& Tag = TagIt->Name;

							if ( !OnAssetTagWantsToBeDisplayed.IsBound() || OnAssetTagWantsToBeDisplayed.Execute(NewMajorityAssetType, Tag) )
							{
								const FString* DisplayNamePtr = TagColumnRenames.Find(Tag);
								FText DisplayName;
								if ( DisplayNamePtr )
								{
									DisplayName = FText::FromString(*DisplayNamePtr);
								}
								else
								{
									DisplayName = FText::FromName(Tag);
								}

								ColumnView->GetHeaderRow()->AddColumn(
										SHeaderRow::Column(Tag)
										.SortMode( TAttribute< EColumnSortMode::Type >::Create( TAttribute< EColumnSortMode::Type >::FGetter::CreateSP( this, &SAssetView::GetColumnSortMode, Tag ) ) )
										.OnSort( FOnSortModeChanged::CreateSP( this, &SAssetView::OnSortColumnHeader ) )
										.DefaultLabel( DisplayName )
										.HAlignCell( (TagIt->Type == UObject::FAssetRegistryTag::TT_Numerical) ? HAlign_Right : HAlign_Left )
										.FillWidth(180)
									);

								// If we found a tag the matches the column we are currently sorting on, there will be no need to change the column
								if ( Tag == CurrentSortColumn )
								{
									bSortColumnStillRelevant = true;
								}
							}
						}
					}
				}			
			}	
		}

		if ( !bSortColumnStillRelevant )
		{
			// If the current sort column is no longer relevant, revert to "Name" and resort when convenient
			SortManager.SetOrToggleSortColumn(FAssetViewSortManager::NameColumnId);
			bPendingSortFilteredItems = true;
		}
	}
}

void SAssetView::OnAssetsAddedToCollection( const FCollectionNameType& Collection, const TArray< FName >& ObjectPaths )
{
	if ( !SourcesData.Collections.Contains( Collection ) )
	{
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	for (int Index = 0; Index < ObjectPaths.Num(); Index++)
	{
		OnAssetAdded( AssetRegistryModule.Get().GetAssetByObjectPath( ObjectPaths[Index] ) );
	}
}

void SAssetView::OnAssetAdded(const FAssetData& AssetData)
{
	RecentlyAddedAssets.Add(AssetData);
}

void SAssetView::ProcessRecentlyAddedAssets()
{
	if ( FilteredRecentlyAddedAssets.Num() > 0 )
	{
		const static float MaxSecondsPerFrame = 0.015;
		double TickStartTime = FPlatformTime::Seconds();

		TSet<FName> ExistingObjectPaths;
		for ( auto AssetIt = AssetItems.CreateConstIterator(); AssetIt; ++AssetIt )
		{
			ExistingObjectPaths.Add((*AssetIt).ObjectPath);
		}

		for ( auto AssetIt = QueriedAssetItems.CreateConstIterator(); AssetIt; ++AssetIt )
		{
			ExistingObjectPaths.Add((*AssetIt).ObjectPath);
		}

		int32 AssetIdx = 0;
		for ( ; AssetIdx < FilteredRecentlyAddedAssets.Num(); ++AssetIdx )
		{
			const FAssetData& AssetData = FilteredRecentlyAddedAssets[AssetIdx];
			if ( !ExistingObjectPaths.Contains(AssetData.ObjectPath) )
			{
				if ( AssetData.AssetClass != UObjectRedirector::StaticClass()->GetFName() || AssetData.IsUAsset() )
				{
					if ( !OnShouldFilterAsset.IsBound() || !OnShouldFilterAsset.Execute(AssetData) )
					{
						// Add the asset to the list
						int32 AddedAssetIdx = AssetItems.Add(AssetData);
						if ( !IsFrontendFilterActive() || PassesCurrentFrontendFilter(AssetItems[AddedAssetIdx]) )
						{
							FilteredAssetItems.Add(MakeShareable(new FAssetViewAsset(AssetItems[AddedAssetIdx])));
							bPendingSortFilteredItems = true;
							bRefreshSourceItemsRequested = true;

							RefreshList();
						}
					}
				}
			}

			if ( (FPlatformTime::Seconds() - TickStartTime) > MaxSecondsPerFrame)
			{
				// Increment the index to properly trim the buffer below
				++AssetIdx;
				break;
			}
		}

		// Trim the results array
		if (AssetIdx > 0)
		{
			FilteredRecentlyAddedAssets.RemoveAt(0, AssetIdx);
		}
	}
	else if (
		( RecentlyAddedAssets.Num() > 2048 ) ||
		( RecentlyAddedAssets.Num() > 0 && FPlatformTime::Seconds() - LastProcessAddsTime > 4 )
		)
	{
		RunAssetsThroughBackendFilter(RecentlyAddedAssets);
		FilteredRecentlyAddedAssets.Append(RecentlyAddedAssets);
		RecentlyAddedAssets.Empty();
		LastProcessAddsTime = FPlatformTime::Seconds();
	}
}

void SAssetView::OnAssetsRemovedFromCollection( const FCollectionNameType& Collection, const TArray< FName >& ObjectPaths )
{
	if ( !SourcesData.Collections.Contains( Collection ) )
	{
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	for (int Index = 0; Index < ObjectPaths.Num(); Index++)
	{
		OnAssetRemoved( AssetRegistryModule.Get().GetAssetByObjectPath( ObjectPaths[Index] ) );
	}
}

void SAssetView::OnAssetRemoved(const FAssetData& AssetData)
{
	RemoveAssetByPath( AssetData.ObjectPath );
}

void SAssetView::OnAssetRegistryPathAdded(const FString& Path)
{
	if(IsShowingFolders() && !ShouldFilterRecursively())
	{
		// If this isn't a developer folder or we want to show them, continue
		const bool bDisplayDev = GetDefault<UContentBrowserSettings>()->GetDisplayDevelopersFolder();
		if ( bDisplayDev || !ContentBrowserUtils::IsDevelopersFolder(Path) )
		{
			for(auto SourcePathIt(SourcesData.PackagePaths.CreateConstIterator()); SourcePathIt; SourcePathIt++)
			{
				const FString SourcePath = (*SourcePathIt).ToString();
				if(Path.StartsWith(SourcePath))
				{
					const FString SubPath = Path.RightChop(SourcePath.Len());
					
					TArray<FString> SubPathItemList;
					SubPath.ParseIntoArray(&SubPathItemList, TEXT("/"), /*InCullEmpty=*/true);

					if(SubPathItemList.Num() > 0)
					{
						const FString NewSubFolder = SourcePath / SubPathItemList[0];
						if(!Folders.Contains(NewSubFolder))
						{
							FilteredAssetItems.Add(MakeShareable(new FAssetViewFolder(NewSubFolder)));
							Folders.Add(NewSubFolder);
							bPendingSortFilteredItems = true;
						}
					}
				}
			}
		}
	}	
}

void SAssetView::OnAssetRegistryPathRemoved(const FString& Path)
{
	FString* Folder = Folders.Find(Path);
	if(Folder != NULL)
	{
		Folders.Remove(Path);

		for (int32 AssetIdx = 0; AssetIdx < FilteredAssetItems.Num(); ++AssetIdx)
		{
			if(FilteredAssetItems[AssetIdx]->GetType() == EAssetItemType::Folder)
			{
				if ( StaticCastSharedPtr<FAssetViewFolder>(FilteredAssetItems[AssetIdx])->FolderPath == Path )
				{
					// Found the folder in the filtered items list, remove it
					FilteredAssetItems.RemoveAt(AssetIdx);
					RefreshList();
					break;
				}
			}
		}
	}
}

void SAssetView::RemoveAssetByPath( const FName& ObjectPath )
{
	bool bFoundAsset = false;
	for (int32 AssetIdx = 0; AssetIdx < AssetItems.Num(); ++AssetIdx)
	{
		if ( AssetItems[AssetIdx].ObjectPath == ObjectPath )
		{
			// Found the asset in the cached list, remove it
			AssetItems.RemoveAt(AssetIdx);
			bFoundAsset = true;
			break;
		}
	}

	if ( bFoundAsset )
	{
		// If it was in the AssetItems list, see if it is also in the FilteredAssetItems list
		for (int32 AssetIdx = 0; AssetIdx < FilteredAssetItems.Num(); ++AssetIdx)
		{
			if(FilteredAssetItems[AssetIdx].IsValid() && FilteredAssetItems[AssetIdx]->GetType() != EAssetItemType::Folder)
			{
				if ( StaticCastSharedPtr<FAssetViewAsset>(FilteredAssetItems[AssetIdx])->Data.ObjectPath == ObjectPath && !FilteredAssetItems[AssetIdx]->IsTemporaryItem() )
				{
					// Found the asset in the filtered items list, remove it
					FilteredAssetItems.RemoveAt(AssetIdx);
					RefreshList();
					break;
				}
			}
		}
	}
	else
	{
		//Make sure we don't have the item still queued up for processing
		for (int32 AssetIdx = 0; AssetIdx < QueriedAssetItems.Num(); ++AssetIdx)
		{
			if ( QueriedAssetItems[AssetIdx].ObjectPath == ObjectPath )
			{
				// Found the asset in the cached list, remove it
				QueriedAssetItems.RemoveAt(AssetIdx);
				bFoundAsset = true;
				break;
			}
		}
	}
}

void SAssetView::OnCollectionRenamed( const FCollectionNameType& OriginalCollection, const FCollectionNameType& NewCollection )
{
	int32 FoundIndex = INDEX_NONE;
	if ( SourcesData.Collections.Find( OriginalCollection, FoundIndex ) )
	{
		SourcesData.Collections[ FoundIndex ] = NewCollection;
	}
}

void SAssetView::OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath)
{
	// Remove the old asset, if it exists
	RemoveAssetByPath( FName( *OldObjectPath ) );

	// Add the new asset, if it should be in the cached list
	OnAssetAdded( AssetData );
}

void SAssetView::OnAssetLoaded(UObject* Asset)
{
	if ( Asset != NULL )
	{
		RecentlyLoadedOrChangedAssets.Add( FName(*Asset->GetPathName()), Asset );
	}
}

void SAssetView::OnObjectPropertyChanged(UObject* Asset, FPropertyChangedEvent& PropertyChangedEvent)
{
	if ( Asset != NULL )
	{
		RecentlyLoadedOrChangedAssets.Add( FName(*Asset->GetPathName()), Asset );
	}
}

void SAssetView::OnDynamicFiltersChanged()
{
	ResetQuickJump();
	RefreshFilteredItems();
	RefreshFolders();
	SortList();
}

void SAssetView::OnFrontendFiltersChanged()
{
	bRefreshSourceItemsRequested = true;
}

bool SAssetView::IsFrontendFilterActive() const
{
	return ( FrontendFilters.IsValid() && FrontendFilters->Num() > 0 ) || ( DynamicFilters.IsValid() && DynamicFilters->Num() > 0 );
}

bool SAssetView::PassesCurrentFrontendFilter(const FAssetData& Item) const
{
	// Check the frontend filters list
	if ( ( FrontendFilters.IsValid() && !FrontendFilters->PassesAllFilters(Item) ) ||
		 ( DynamicFilters.IsValid() && !DynamicFilters->PassesAllFilters( Item ) ) )
	{
		return false;
	}

	return true;
}

void SAssetView::RunAssetsThroughBackendFilter(TArray<FAssetData>& InOutAssetDataList) const
{
	const bool bRecurse = ShouldFilterRecursively();
	const bool bUsingFolders = GetDefault<UContentBrowserSettings>()->ShowOnlyAssetsInSelectedFolders || IsShowingFolders();
	FARFilter Filter = SourcesData.MakeFilter(bRecurse, bUsingFolders);
	
	if ( SourcesData.Collections.Num() > 0 && Filter.ObjectPaths.Num() == 0 )
	{
		// This is an empty collection, no asset will pass the check
		InOutAssetDataList.Empty();
	}
	else
	{
		// Actually append the backend filter
		Filter.Append(BackendFilter);

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().RunAssetsThroughFilter(InOutAssetDataList, Filter);

		if ( SourcesData.Collections.Num() > 0 )
		{
			FCollectionManagerModule& CollectionManagerModule = FModuleManager::GetModuleChecked<FCollectionManagerModule>(TEXT("CollectionManager"));
			TArray< FName > CollectionObjectPaths;
			for (int Index = 0; Index < SourcesData.Collections.Num(); Index++)
			{
				CollectionManagerModule.Get().GetObjectsInCollection(SourcesData.Collections[Index].Name, SourcesData.Collections[Index].Type, CollectionObjectPaths);
			}

			for ( int32 AssetDataIdx = InOutAssetDataList.Num() - 1; AssetDataIdx >= 0; --AssetDataIdx )
			{
				const FAssetData& AssetData = InOutAssetDataList[AssetDataIdx];

				if ( !CollectionObjectPaths.Contains( AssetData.ObjectPath ) )
				{
					InOutAssetDataList.RemoveAt(AssetDataIdx);
				}
			}
		}
	}
}

void SAssetView::SortList(bool bSyncToSelection)
{
	if ( !IsRenamingAsset() )
	{
		SortManager.SortList(FilteredAssetItems, MajorityAssetType);

		// Update the thumbnails we were using since the order has changed
		bPendingUpdateThumbnails = true;

		if ( bSyncToSelection )
		{
			// Make sure the selection is in view
			TArray<FAssetData> SelectedAssets = GetSelectedAssets();
			if ( SelectedAssets.Num() > 0 )
			{
				const bool bFocusOnSync = false;
				SyncToAssets(SelectedAssets, bFocusOnSync);
			}
		}

		RefreshList();
		bPendingSortFilteredItems = false;
		LastSortTime = CurrentTime;
	}
	else
	{
		bPendingSortFilteredItems = true;
	}
}

FLinearColor SAssetView::GetThumbnailHintColorAndOpacity() const
{
	//We update this color in tick instead of here as an optimization
	return ThumbnailHintColorAndOpacity;
}

FSlateColor SAssetView::GetViewButtonForegroundColor() const
{
	return ViewOptionsComboButton->IsHovered() ? FEditorStyle::GetSlateColor("InvertedForeground") : FEditorStyle::GetSlateColor("DefaultForeground");
}

TSharedRef<SWidget> SAssetView::GetViewButtonContent()
{
	// Get all menu extenders for this context menu from the content browser module
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>( TEXT("ContentBrowser") );
	TArray<FContentBrowserMenuExtender> MenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewViewMenuExtenders();

	TArray<TSharedPtr<FExtender>> Extenders;
	for (int32 i = 0; i < MenuExtenderDelegates.Num(); ++i)
	{
		if (MenuExtenderDelegates[i].IsBound())
		{
			Extenders.Add(MenuExtenderDelegates[i].Execute());
		}
	}
	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, NULL, MenuExtender, /*bCloseSelfOnly=*/ true);

	MenuBuilder.BeginSection("AssetViewType", LOCTEXT("ViewTypeHeading", "View Type"));
	{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("TileViewOption", "Tiles"),
		LOCTEXT("TileViewOptionToolTip", "View assets as tiles in a grid."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &SAssetView::SetCurrentViewType, EAssetViewType::Tile ),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP( this, &SAssetView::IsCurrentViewType, EAssetViewType::Tile )
			),
		NAME_None,
		EUserInterfaceActionType::RadioButton
		);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ListViewOption", "List"),
		LOCTEXT("ListViewOptionToolTip", "View assets in a list with thumbnails."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &SAssetView::SetCurrentViewType, EAssetViewType::List ),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP( this, &SAssetView::IsCurrentViewType, EAssetViewType::List )
			),
		NAME_None,
		EUserInterfaceActionType::RadioButton
		);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ColumnViewOption", "Columns"),
		LOCTEXT("ColumnViewOptionToolTip", "View assets in a list with columns of details."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &SAssetView::SetCurrentViewType, EAssetViewType::Column ),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP( this, &SAssetView::IsCurrentViewType, EAssetViewType::Column )
			),
		NAME_None,
		EUserInterfaceActionType::RadioButton
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Folders", LOCTEXT("FoldersHeading", "Folders"));
	{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("ShowFoldersOption", "Show Folders"),
		LOCTEXT("ShowFoldersOptionToolTip", "Show folders in the view as well as assets."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &SAssetView::ToggleShowFolders ),
			FCanExecuteAction::CreateSP( this, &SAssetView::IsToggleShowFoldersAllowed ),
			FIsActionChecked::CreateSP( this, &SAssetView::IsShowingFolders )
			),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
		);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ShowOnlyAssetsInSelectedFolders", "Show Only Assets in Selected Folders"),
		LOCTEXT("ShowOnlyAssetsInSelectedFoldersToolTip", "Only displays the assets of the selected folders"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &SAssetView::ToggleShowOnlyAssetsInSelectedFolders ),
			FCanExecuteAction::CreateSP( this, &SAssetView::CanShowOnlyAssetsInSelectedFolders ),
			FIsActionChecked::CreateSP( this, &SAssetView::IsShowingOnlyAssetsInSelectedFolders )
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
		);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ShowDevelopersFolderOption", "Show Developers Folder"),
		LOCTEXT("ShowDevelopersFolderOptionToolTip", "Show the developers folder in the view."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &SAssetView::ToggleShowDevelopersFolder ),
			FCanExecuteAction::CreateSP( this, &SAssetView::IsToggleShowDevelopersFolderAllowed ),
			FIsActionChecked::CreateSP( this, &SAssetView::IsShowingDevelopersFolder )
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
		);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ShowEngineFolderOption", "Show Engine Content"),
		LOCTEXT("ShowEngineFolderOptionToolTip", "Show the engine content in the view."),
		FSlateIcon(),
		FUIAction(
		FExecuteAction::CreateSP( this, &SAssetView::ToggleShowEngineFolder ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SAssetView::IsShowingEngineFolder )
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("AssetThumbnails", LOCTEXT("ThumbnailsHeading", "Thumbnails"));
	{
	MenuBuilder.AddWidget(
		SNew(SSlider)
			.ToolTipText( LOCTEXT("ThumbnailScaleToolTip", "Adjust the size of thumbnails.") )
			.Value( this, &SAssetView::GetThumbnailScale )
			.OnValueChanged( this, &SAssetView::SetThumbnailScale )
			.Locked( this, &SAssetView::IsThumbnailScalingLocked ),
		LOCTEXT("ThumbnailScaleLabel", "Scale"),
		/*bNoIndent=*/true
		);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ThumbnailEditModeOption", "Thumbnail Edit Mode"),
		LOCTEXT("ThumbnailEditModeOptionToolTip", "Toggle thumbnail editing mode. When in this mode you can rotate the camera on 3D thumbnails by dragging them."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &SAssetView::ToggleThumbnailEditMode ),
			FCanExecuteAction::CreateSP( this, &SAssetView::IsThumbnailEditModeAllowed ),
			FIsActionChecked::CreateSP( this, &SAssetView::IsThumbnailEditMode )
			),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
		);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("RealTimeThumbnailsOption", "Real-Time Thumbnails"),
		LOCTEXT("RealTimeThumbnailsOptionToolTip", "Renders the assets thumbnails in real-time"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &SAssetView::ToggleRealTimeThumbnails ),
			FCanExecuteAction::CreateSP( this, &SAssetView::CanShowRealTimeThumbnails ),
			FIsActionChecked::CreateSP( this, &SAssetView::IsShowingRealTimeThumbnails )
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SAssetView::ToggleShowFolders()
{
	check( IsToggleShowFoldersAllowed() );
	GetMutableDefault<UContentBrowserSettings>()->DisplayFolders = !GetDefault<UContentBrowserSettings>()->DisplayFolders;
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
}

bool SAssetView::IsToggleShowFoldersAllowed() const
{
	return bCanShowFolders;
}

bool SAssetView::IsShowingFolders() const
{
	return IsToggleShowFoldersAllowed() ? GetDefault<UContentBrowserSettings>()->DisplayFolders : false;
}

void SAssetView::ToggleShowOnlyAssetsInSelectedFolders()
{
	check( CanShowOnlyAssetsInSelectedFolders() );
	GetMutableDefault<UContentBrowserSettings>()->ShowOnlyAssetsInSelectedFolders = !GetDefault<UContentBrowserSettings>()->ShowOnlyAssetsInSelectedFolders;
	bRefreshSourceItemsRequested = true;
}

bool SAssetView::CanShowOnlyAssetsInSelectedFolders() const
{
	 return bCanShowOnlyAssetsInSelectedFolders;
}

bool SAssetView::IsShowingOnlyAssetsInSelectedFolders() const
{
	return CanShowOnlyAssetsInSelectedFolders() ? GetDefault<UContentBrowserSettings>()->ShowOnlyAssetsInSelectedFolders : false;
}

void SAssetView::ToggleRealTimeThumbnails()
{
	check( CanShowRealTimeThumbnails() );
	GetMutableDefault<UContentBrowserSettings>()->RealTimeThumbnails = !GetDefault<UContentBrowserSettings>()->RealTimeThumbnails;
	bRefreshSourceItemsRequested = true;
}

bool SAssetView::CanShowRealTimeThumbnails() const
{
	return bCanShowRealTimeThumbnails;
}

bool SAssetView::IsShowingRealTimeThumbnails() const
{
	return CanShowRealTimeThumbnails() ? GetDefault<UContentBrowserSettings>()->RealTimeThumbnails : false;
}

void SAssetView::ToggleShowEngineFolder()
{
	bool bDisplayEngine = GetDefault<UContentBrowserSettings>()->GetDisplayEngineFolder();
	bool bRawDisplayEngine = GetDefault<UContentBrowserSettings>()->GetDisplayEngineFolder( true );

	// Only if both these flags are false when toggling we want to enable the flag, otherwise we're toggling off
	if ( !bDisplayEngine && !bRawDisplayEngine )
	{
		GetMutableDefault<UContentBrowserSettings>()->SetDisplayEngineFolder( true );
	}
	else
	{
		GetMutableDefault<UContentBrowserSettings>()->SetDisplayEngineFolder( false );
		GetMutableDefault<UContentBrowserSettings>()->SetDisplayEngineFolder( false, true );
	}	
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
}

bool SAssetView::IsShowingEngineFolder() const
{
	return GetDefault<UContentBrowserSettings>()->GetDisplayEngineFolder();
}

void SAssetView::ToggleShowDevelopersFolder()
{
	bool bDisplayDev = GetDefault<UContentBrowserSettings>()->GetDisplayDevelopersFolder();
	bool bRawDisplayDev = GetDefault<UContentBrowserSettings>()->GetDisplayDevelopersFolder( true );

	// Only if both these flags are false when toggling we want to enable the flag, otherwise we're toggling off
	if ( !bDisplayDev && !bRawDisplayDev )
	{
		GetMutableDefault<UContentBrowserSettings>()->SetDisplayDevelopersFolder( true );
	}
	else
	{
		GetMutableDefault<UContentBrowserSettings>()->SetDisplayDevelopersFolder( false );
		GetMutableDefault<UContentBrowserSettings>()->SetDisplayDevelopersFolder( false, true );
	}	
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
}

bool SAssetView::IsToggleShowDevelopersFolderAllowed() const
{
	return bCanShowDevelopersFolder;
}

bool SAssetView::IsShowingDevelopersFolder() const
{
	return GetDefault<UContentBrowserSettings>()->GetDisplayDevelopersFolder();
}

void SAssetView::SetCurrentViewType(EAssetViewType::Type NewType)
{
	if ( ensure(NewType != EAssetViewType::MAX) && NewType != CurrentViewType )
	{
		TArray<FAssetData> SelectedAssets = GetSelectedAssets();
		
		ResetQuickJump();

		CurrentViewType = NewType;
		CreateCurrentView();

		SyncToAssets(SelectedAssets);

		// Clear relevant thumbnails to render fresh ones in the new view if needed
		RelevantThumbnails.Empty();
		VisibleItems.Empty();

		if ( NewType == EAssetViewType::Tile )
		{
			CurrentThumbnailSize = TileViewThumbnailSize;
			bPendingUpdateThumbnails = true;
		}
		else if ( NewType == EAssetViewType::List )
		{
			CurrentThumbnailSize = ListViewThumbnailSize;
			bPendingUpdateThumbnails = true;
		}
		else if ( NewType == EAssetViewType::Column )
		{
			// No thumbnails, but we do need to refresh filtered items to determine a majority asset type
			MajorityAssetType = NAME_None;
			RefreshFilteredItems();
			RefreshFolders();
			SortList();
		}
	}
}

void SAssetView::CreateCurrentView()
{
	TileView.Reset();
	ListView.Reset();
	ColumnView.Reset();

	TSharedRef<SWidget> NewView = SNullWidget::NullWidget;
	switch (CurrentViewType)
	{
		case EAssetViewType::Tile:
			TileView = CreateTileView();
			NewView = CreateShadowOverlay(TileView.ToSharedRef());
			break;
		case EAssetViewType::List:
			ListView = CreateListView();
			NewView = CreateShadowOverlay(ListView.ToSharedRef());
			break;
		case EAssetViewType::Column:
			ColumnView = CreateColumnView();
			NewView = CreateShadowOverlay(ColumnView.ToSharedRef());
			break;
	}
	
	ViewContainer->SetContent( NewView );
}

TSharedRef<SWidget> SAssetView::CreateShadowOverlay( TSharedRef<STableViewBase> Table )
{
	return SNew(SScrollBorder, Table)
		[
			Table
		];
}

EAssetViewType::Type SAssetView::GetCurrentViewType() const
{
	return CurrentViewType;
}

bool SAssetView::IsCurrentViewType(EAssetViewType::Type ViewType) const
{
	return GetCurrentViewType() == ViewType;
}

void SAssetView::FocusList() const
{
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::List: FSlateApplication::Get().SetKeyboardFocus(ListView, EKeyboardFocusCause::SetDirectly); break;
		case EAssetViewType::Tile: FSlateApplication::Get().SetKeyboardFocus(TileView, EKeyboardFocusCause::SetDirectly); break;
		case EAssetViewType::Column: FSlateApplication::Get().SetKeyboardFocus(ColumnView, EKeyboardFocusCause::SetDirectly); break;
	}
}

void SAssetView::RefreshList()
{
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::List: ListView->RequestListRefresh(); break;
		case EAssetViewType::Tile: TileView->RequestListRefresh(); break;
		case EAssetViewType::Column: ColumnView->RequestListRefresh(); break;
	}
}

void SAssetView::SetSelection(const TSharedPtr<FAssetViewItem>& Item)
{
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::List: ListView->SetSelection(Item); break;
		case EAssetViewType::Tile: TileView->SetSelection(Item); break;
		case EAssetViewType::Column: ColumnView->SetSelection(Item); break;
	}
}

void SAssetView::SetItemSelection(const TSharedPtr<FAssetViewItem>& Item, bool bSelected, const ESelectInfo::Type SelectInfo)
{
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::List: ListView->SetItemSelection(Item, bSelected, SelectInfo); break;
		case EAssetViewType::Tile: TileView->SetItemSelection(Item, bSelected, SelectInfo); break;
		case EAssetViewType::Column: ColumnView->SetItemSelection(Item, bSelected, SelectInfo); break;
	}
}

void SAssetView::RequestScrollIntoView(const TSharedPtr<FAssetViewItem>& Item)
{
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::List: ListView->RequestScrollIntoView(Item); break;
		case EAssetViewType::Tile: TileView->RequestScrollIntoView(Item); break;
		case EAssetViewType::Column: ColumnView->RequestScrollIntoView(Item); break;
	}
}

void SAssetView::OnOpenAssetsOrFolders()
{
	TArray<FAssetData> SelectedAssets = GetSelectedAssets();
	TArray<FString> SelectedFolders = GetSelectedFolders();
	if (SelectedAssets.Num() > 0 && SelectedFolders.Num() == 0)
	{
		OnAssetsActivated.ExecuteIfBound(SelectedAssets, EAssetTypeActivationMethod::Opened);
	}
	else if (SelectedAssets.Num() == 0 && SelectedFolders.Num() > 0)
	{
		OnPathSelected.ExecuteIfBound(SelectedFolders[0]);
	}
}

void SAssetView::OnPreviewAssets()
{
	OnAssetsActivated.ExecuteIfBound(GetSelectedAssets(), EAssetTypeActivationMethod::Previewed);
}

void SAssetView::ClearSelection()
{
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::List: ListView->ClearSelection(); break;
		case EAssetViewType::Tile: TileView->ClearSelection(); break;
		case EAssetViewType::Column: ColumnView->ClearSelection(); break;
	}
}

TSharedRef<ITableRow> SAssetView::MakeListViewWidget(TSharedPtr<FAssetViewItem> AssetItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	if ( !ensure(AssetItem.IsValid()) )
	{
		return SNew( STableRow<TSharedPtr<FAssetViewAsset>>, OwnerTable );
	}

	VisibleItems.Add(AssetItem);
	bPendingUpdateThumbnails = true;

	if(AssetItem->GetType() == EAssetItemType::Folder)
	{
		TSharedPtr< STableRow<TSharedPtr<FAssetViewItem>> > TableRowWidget;
		SAssignNew( TableRowWidget, STableRow<TSharedPtr<FAssetViewItem>>, OwnerTable )
			.Style(FEditorStyle::Get(), "ContentBrowser.AssetListView.TableRow")
			.Cursor( bAllowDragging ? EMouseCursor::GrabHand : EMouseCursor::Default )
			.OnDragDetected( this, &SAssetView::OnDraggingAssetItem );

		TSharedRef<SAssetListItem> Item =
			SNew(SAssetListItem)
			.AssetItem(AssetItem)
			.ItemHeight(this, &SAssetView::GetListViewItemHeight)
			.OnRenameBegin(this, &SAssetView::AssetRenameBegin)
			.OnRenameCommit(this, &SAssetView::AssetRenameCommit)
			.OnVerifyRenameCommit(this, &SAssetView::AssetVerifyRenameCommit)
			.OnItemDestroyed(this, &SAssetView::AssetItemWidgetDestroyed)
			.ShouldAllowToolTip(this, &SAssetView::ShouldAllowToolTips)
			.HighlightText(HighlightedText)
			.ConstructToolTip( ConstructToolTipForAsset )
			.IsSelected( FIsSelected::CreateSP(TableRowWidget.Get(), &STableRow<TSharedPtr<FAssetViewItem>>::IsSelectedExclusively) );

		TableRowWidget->SetContent(Item);

		return TableRowWidget.ToSharedRef();
	}
	else
	{
		TSharedPtr<FAssetViewAsset> AssetItemAsAsset = StaticCastSharedPtr<FAssetViewAsset>(AssetItem);

		TSharedPtr<FAssetThumbnail>* AssetThumbnailPtr = RelevantThumbnails.Find(AssetItemAsAsset);
		TSharedPtr<FAssetThumbnail> AssetThumbnail;
		if ( AssetThumbnailPtr )
		{
			AssetThumbnail = *AssetThumbnailPtr;
		}
		else
		{
			const float ThumbnailResolution = ListViewThumbnailResolution;
			AssetThumbnail = MakeShareable( new FAssetThumbnail( AssetItemAsAsset->Data, ThumbnailResolution, ThumbnailResolution, AssetThumbnailPool ) );
			RelevantThumbnails.Add( AssetItemAsAsset, AssetThumbnail );
			AssetThumbnail->GetViewportRenderTargetTexture(); // Access the texture once to trigger it to render
		}

		TSharedPtr< STableRow<TSharedPtr<FAssetViewItem>> > TableRowWidget;
		SAssignNew( TableRowWidget, STableRow<TSharedPtr<FAssetViewItem>>, OwnerTable )
		.Style(FEditorStyle::Get(), "ContentBrowser.AssetListView.TableRow")
		.Cursor( bAllowDragging ? EMouseCursor::GrabHand : EMouseCursor::Default )
		.OnDragDetected( this, &SAssetView::OnDraggingAssetItem );

		TSharedRef<SAssetListItem> Item =
			SNew(SAssetListItem)
			.AssetThumbnail(AssetThumbnail)
			.AssetItem(AssetItem)
			.ThumbnailPadding(ListViewThumbnailPadding)
			.ItemHeight(this, &SAssetView::GetListViewItemHeight)
			.OnRenameBegin(this, &SAssetView::AssetRenameBegin)
			.OnRenameCommit(this, &SAssetView::AssetRenameCommit)
			.OnVerifyRenameCommit(this, &SAssetView::AssetVerifyRenameCommit)
			.OnItemDestroyed(this, &SAssetView::AssetItemWidgetDestroyed)
			.ShouldAllowToolTip(this, &SAssetView::ShouldAllowToolTips)
			.HighlightText(HighlightedText)
			.ThumbnailEditMode(this, &SAssetView::IsThumbnailEditMode)
			.ConstructToolTip( ConstructToolTipForAsset )
			.ThumbnailLabel( ThumbnailLabel )
			.ThumbnailHintColorAndOpacity( this, &SAssetView::GetThumbnailHintColorAndOpacity )
			.AllowThumbnailHintLabel( AllowThumbnailHintLabel )
			.IsSelected( FIsSelected::CreateSP(TableRowWidget.Get(), &STableRow<TSharedPtr<FAssetViewItem>>::IsSelectedExclusively) );

		TableRowWidget->SetContent(Item);

		return TableRowWidget.ToSharedRef();
	}
}

TSharedRef<ITableRow> SAssetView::MakeTileViewWidget(TSharedPtr<FAssetViewItem> AssetItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	if ( !ensure(AssetItem.IsValid()) )
	{
		return SNew( STableRow<TSharedPtr<FAssetViewAsset>>, OwnerTable );
	}

	VisibleItems.Add(AssetItem);
	bPendingUpdateThumbnails = true;

	if(AssetItem->GetType() == EAssetItemType::Folder)
	{
		TSharedPtr< STableRow<TSharedPtr<FAssetViewItem>> > TableRowWidget;
		SAssignNew( TableRowWidget, STableRow<TSharedPtr<FAssetViewItem>>, OwnerTable )
			.Style( FEditorStyle::Get(), "ContentBrowser.AssetListView.TableRow" )
			.Cursor( bAllowDragging ? EMouseCursor::GrabHand : EMouseCursor::Default )
			.OnDragDetected( this, &SAssetView::OnDraggingAssetItem );

		TSharedRef<SAssetTileItem> Item =
			SNew(SAssetTileItem)
			.AssetItem(AssetItem)
			.ItemWidth(this, &SAssetView::GetTileViewItemWidth)
			.OnRenameBegin(this, &SAssetView::AssetRenameBegin)
			.OnRenameCommit(this, &SAssetView::AssetRenameCommit)
			.OnVerifyRenameCommit(this, &SAssetView::AssetVerifyRenameCommit)
			.OnItemDestroyed(this, &SAssetView::AssetItemWidgetDestroyed)
			.ShouldAllowToolTip(this, &SAssetView::ShouldAllowToolTips)
			.HighlightText( HighlightedText )
			.LabelVisibility( LabelVisibility )
			.ConstructToolTip( ConstructToolTipForAsset )
			.IsSelected( FIsSelected::CreateSP(TableRowWidget.Get(), &STableRow<TSharedPtr<FAssetViewItem>>::IsSelectedExclusively) )
			.OnAssetsDragDropped(this, &SAssetView::OnAssetsDragDropped)
			.OnPathsDragDropped(this, &SAssetView::OnPathsDragDropped)
			.OnFilesDragDropped(this, &SAssetView::OnFilesDragDropped);

		TableRowWidget->SetContent(Item);

		return TableRowWidget.ToSharedRef();
	}
	else
	{
		TSharedPtr<FAssetViewAsset> AssetItemAsAsset = StaticCastSharedPtr<FAssetViewAsset>(AssetItem);

		TSharedPtr<FAssetThumbnail>* AssetThumbnailPtr = RelevantThumbnails.Find(AssetItemAsAsset);
		TSharedPtr<FAssetThumbnail> AssetThumbnail;
		if ( AssetThumbnailPtr )
		{
			AssetThumbnail = *AssetThumbnailPtr;
		}
		else
		{
			const float ThumbnailResolution = TileViewThumbnailResolution;
			AssetThumbnail = MakeShareable( new FAssetThumbnail( AssetItemAsAsset->Data, ThumbnailResolution, ThumbnailResolution, AssetThumbnailPool ) );
			RelevantThumbnails.Add( AssetItemAsAsset, AssetThumbnail );
			AssetThumbnail->GetViewportRenderTargetTexture(); // Access the texture once to trigger it to render
		}

		TSharedPtr< STableRow<TSharedPtr<FAssetViewItem>> > TableRowWidget;
		SAssignNew( TableRowWidget, STableRow<TSharedPtr<FAssetViewItem>>, OwnerTable )
		.Style(FEditorStyle::Get(), "ContentBrowser.AssetListView.TableRow")
		.Cursor( bAllowDragging ? EMouseCursor::GrabHand : EMouseCursor::Default )
		.OnDragDetected( this, &SAssetView::OnDraggingAssetItem );

		TSharedRef<SAssetTileItem> Item =
			SNew(SAssetTileItem)
			.AssetThumbnail(AssetThumbnail)
			.AssetItem(AssetItem)
			.ThumbnailPadding(TileViewThumbnailPadding)
			.ItemWidth(this, &SAssetView::GetTileViewItemWidth)
			.OnRenameBegin(this, &SAssetView::AssetRenameBegin)
			.OnRenameCommit(this, &SAssetView::AssetRenameCommit)
			.OnVerifyRenameCommit(this, &SAssetView::AssetVerifyRenameCommit)
			.OnItemDestroyed(this, &SAssetView::AssetItemWidgetDestroyed)
			.ShouldAllowToolTip(this, &SAssetView::ShouldAllowToolTips)
			.HighlightText( HighlightedText )
			.ThumbnailEditMode(this, &SAssetView::IsThumbnailEditMode)
			.LabelVisibility( LabelVisibility )
			.ThumbnailLabel( ThumbnailLabel )
			.ThumbnailHintColorAndOpacity( this, &SAssetView::GetThumbnailHintColorAndOpacity )
			.ConstructToolTip( ConstructToolTipForAsset )
			.AllowThumbnailHintLabel( AllowThumbnailHintLabel )
			.IsSelected( FIsSelected::CreateSP(TableRowWidget.Get(), &STableRow<TSharedPtr<FAssetViewItem>>::IsSelectedExclusively) );

		TableRowWidget->SetContent(Item);

		return TableRowWidget.ToSharedRef();
	}
}

TSharedRef<ITableRow> SAssetView::MakeColumnViewWidget(TSharedPtr<FAssetViewItem> AssetItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	if ( !ensure(AssetItem.IsValid()) )
	{
		return SNew( STableRow<TSharedPtr<FAssetViewItem>>, OwnerTable )
			.Style(FEditorStyle::Get(), "ContentBrowser.AssetListView.TableRow");
	}

	return
		SNew( SAssetColumnViewRow, OwnerTable )
		.OnDragDetected( this, &SAssetView::OnDraggingAssetItem )
		.Cursor( bAllowDragging ? EMouseCursor::GrabHand : EMouseCursor::Default )
		.AssetColumnItem(
			SNew(SAssetColumnItem)
				.AssetItem(AssetItem)
				.OnRenameBegin(this, &SAssetView::AssetRenameBegin)
				.OnRenameCommit(this, &SAssetView::AssetRenameCommit)
				.OnVerifyRenameCommit(this, &SAssetView::AssetVerifyRenameCommit)
				.OnItemDestroyed(this, &SAssetView::AssetItemWidgetDestroyed)
				.HighlightText( HighlightedText )
				.ConstructToolTip( ConstructToolTipForAsset )
				.OnAssetsDragDropped(this, &SAssetView::OnAssetsDragDropped)
				.OnPathsDragDropped(this, &SAssetView::OnPathsDragDropped)
				.OnFilesDragDropped(this, &SAssetView::OnFilesDragDropped)
		);
}

UObject* SAssetView::CreateAssetFromTemporary(FString InName, const TSharedPtr<FAssetViewAsset>& InItem, FText& OutErrorText)
{
	UObject* Asset = NULL;

	const EAssetItemType::Type ItemType = InItem->GetType();
	if ( ItemType == EAssetItemType::Creation )
	{
		// Committed creation
		TSharedPtr<FAssetViewCreation> CreationItem = StaticCastSharedPtr<FAssetViewCreation>(InItem);
		UFactory* Factory = CreationItem->Factory;
		UClass* AssetClass = CreationItem->AssetClass;
		FString PackagePath = CreationItem->Data.PackagePath.ToString();

		// Remove the temporary item before we do any work to ensure the new item creation is not prevented.
		FilteredAssetItems.Remove(InItem);
		RefreshList();

		if ( AssetClass || Factory )
		{
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
			Asset = AssetToolsModule.Get().CreateAsset(InName, PackagePath, AssetClass, Factory, FName("ContentBrowserNewAsset"));
		}

		if ( Asset == NULL )
		{
			OutErrorText = LOCTEXT("AssetCreationFailed", "Failed to create asset.");
		}
	}
	else if ( ItemType == EAssetItemType::Duplication )
	{
		// Committed duplication
		TSharedPtr<FAssetViewDuplication> DuplicationItem = StaticCastSharedPtr<FAssetViewDuplication>(InItem);
		UObject* SourceObject = DuplicationItem->SourceObject.Get();
		FString PackagePath = DuplicationItem->Data.PackagePath.ToString();

		// Remove the temporary item before we do any work to ensure the new item creation is not prevented.
		FilteredAssetItems.Remove(InItem);
		RefreshList();

		if ( SourceObject )
		{
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
			Asset = AssetToolsModule.Get().DuplicateAsset(InName, PackagePath, SourceObject);
		}

		if ( Asset == NULL )
		{
			OutErrorText = LOCTEXT("AssetCreationFailed", "Failed to create asset.");
		}
	}

	return Asset;
}

void SAssetView::AssetItemWidgetDestroyed(const TSharedPtr<FAssetViewItem>& Item)
{
	if(RenamingAsset.Pin().Get() == Item.Get())
	{
		/* Check if the item is in a temp state and if it is, commit using the default name so that it does not entirely vanish on the user.
		   This keeps the functionality consistent for content to never be in a temporary state */
		if ( Item.IsValid() && Item->IsTemporaryItem() && Item->GetType() != EAssetItemType::Folder )
		{
			FText OutErrorText;
			const TSharedPtr<FAssetViewAsset>& ItemAsAsset = StaticCastSharedPtr<FAssetViewAsset>(Item);
			CreateAssetFromTemporary(ItemAsAsset->Data.AssetName.ToString(), ItemAsAsset, OutErrorText);

			// Remove the temporary item.
			FilteredAssetItems.Remove(Item);
			RefreshList();
		}

		RenamingAsset.Reset();
	}

	if ( VisibleItems.Remove(Item) != INDEX_NONE )
	{
		bPendingUpdateThumbnails = true;
	}
}

void SAssetView::UpdateThumbnails()
{
	int32 MinItemIdx = INDEX_NONE;
	int32 MaxItemIdx = INDEX_NONE;
	int32 MinVisibleItemIdx = INDEX_NONE;
	int32 MaxVisibleItemIdx = INDEX_NONE;

	const int32 HalfNumOffscreenThumbnails = NumOffscreenThumbnails * 0.5;
	for ( auto ItemIt = VisibleItems.CreateConstIterator(); ItemIt; ++ItemIt )
	{
		int32 ItemIdx = FilteredAssetItems.Find(*ItemIt);
		if ( ItemIdx != INDEX_NONE )
		{
			const int32 ItemIdxLow = FMath::Max<int32>(0, ItemIdx - HalfNumOffscreenThumbnails);
			const int32 ItemIdxHigh = FMath::Min<int32>(FilteredAssetItems.Num() - 1, ItemIdx + HalfNumOffscreenThumbnails);
			if ( MinItemIdx == INDEX_NONE || ItemIdxLow < MinItemIdx )
			{
				MinItemIdx = ItemIdxLow;
			}
			if ( MaxItemIdx == INDEX_NONE || ItemIdxHigh > MaxItemIdx )
			{
				MaxItemIdx = ItemIdxHigh;
			}
			if ( MinVisibleItemIdx == INDEX_NONE || ItemIdx < MinVisibleItemIdx )
			{
				MinVisibleItemIdx = ItemIdx;
			}
			if ( MaxVisibleItemIdx == INDEX_NONE || ItemIdx > MaxVisibleItemIdx )
			{
				MaxVisibleItemIdx = ItemIdx;
			}
		}
	}

	if ( MinItemIdx != INDEX_NONE && MaxItemIdx != INDEX_NONE && MinVisibleItemIdx != INDEX_NONE && MaxVisibleItemIdx != INDEX_NONE )
	{
		// We have a new min and a new max, compare it to the old min and max so we can create new thumbnails
		// when appropriate and remove old thumbnails that are far away from the view area.
		TMap< TSharedPtr<FAssetViewAsset>, TSharedPtr<FAssetThumbnail> > NewRelevantThumbnails;

		// Operate on offscreen items that are furthest away from the visible items first since the thumbnail pool processes render requests in a LIFO order.
		while (MinItemIdx < MinVisibleItemIdx || MaxItemIdx > MaxVisibleItemIdx)
		{
			const int32 LowEndDistance = MinVisibleItemIdx - MinItemIdx;
			const int32 HighEndDistance = MaxItemIdx - MaxVisibleItemIdx;

			if ( HighEndDistance > LowEndDistance )
			{
				if(FilteredAssetItems.IsValidIndex(MaxItemIdx) && FilteredAssetItems[MaxItemIdx]->GetType() != EAssetItemType::Folder)
				{
					AddItemToNewThumbnailRelevancyMap( StaticCastSharedPtr<FAssetViewAsset>(FilteredAssetItems[MaxItemIdx]), NewRelevantThumbnails );
				}
				MaxItemIdx--;
			}
			else
			{
				if(FilteredAssetItems.IsValidIndex(MinItemIdx) && FilteredAssetItems[MinItemIdx]->GetType() != EAssetItemType::Folder)
				{
					AddItemToNewThumbnailRelevancyMap( StaticCastSharedPtr<FAssetViewAsset>(FilteredAssetItems[MinItemIdx]), NewRelevantThumbnails );
				}
				MinItemIdx++;
			}
		}

		// Now operate on VISIBLE items then prioritize them so they are rendered first
		TArray< TSharedPtr<FAssetThumbnail> > ThumbnailsToPrioritize;
		for ( int32 ItemIdx = MinVisibleItemIdx; ItemIdx <= MaxVisibleItemIdx; ++ItemIdx )
		{
			if(FilteredAssetItems.IsValidIndex(ItemIdx) && FilteredAssetItems[ItemIdx]->GetType() != EAssetItemType::Folder)
			{
				TSharedPtr<FAssetThumbnail> Thumbnail = AddItemToNewThumbnailRelevancyMap( StaticCastSharedPtr<FAssetViewAsset>(FilteredAssetItems[ItemIdx]), NewRelevantThumbnails );
				if ( Thumbnail.IsValid() )
				{
					ThumbnailsToPrioritize.Add(Thumbnail);
				}
			}
		}

		// Now prioritize all thumbnails there were in the visible range
		if ( ThumbnailsToPrioritize.Num() > 0 )
		{
			AssetThumbnailPool->PrioritizeThumbnails(ThumbnailsToPrioritize, CurrentThumbnailSize, CurrentThumbnailSize);
		}

		// Assign the new map of relevant thumbnails. This will remove any entries that were no longer relevant.
		RelevantThumbnails = NewRelevantThumbnails;
	}
}

TSharedPtr<FAssetThumbnail> SAssetView::AddItemToNewThumbnailRelevancyMap(const TSharedPtr<FAssetViewAsset>& Item, TMap< TSharedPtr<FAssetViewAsset>, TSharedPtr<FAssetThumbnail> >& NewRelevantThumbnails)
{
	const TSharedPtr<FAssetThumbnail>* Thumbnail = RelevantThumbnails.Find(Item);
	if ( Thumbnail )
	{
		// The thumbnail is still relevant, add it to the new list
		NewRelevantThumbnails.Add(Item, *Thumbnail);

		return *Thumbnail;
	}
	else
	{
		if ( !ensure(CurrentThumbnailSize > 0 && CurrentThumbnailSize <= MAX_THUMBNAIL_SIZE) )
		{
			// Thumbnail size must be in a sane range
			CurrentThumbnailSize = 64;
		}

		// The thumbnail newly relevant, create a new thumbnail
		const float ThumbnailResolution = CurrentThumbnailSize * MaxThumbnailScale;
		TSharedPtr<FAssetThumbnail> NewThumbnail = MakeShareable( new FAssetThumbnail( Item->Data, ThumbnailResolution, ThumbnailResolution, AssetThumbnailPool ) );
		NewRelevantThumbnails.Add( Item, NewThumbnail );
		NewThumbnail->GetViewportRenderTargetTexture(); // Access the texture once to trigger it to render

		return NewThumbnail;
	}
}

void SAssetView::AssetSelectionChanged( TSharedPtr< struct FAssetViewItem > AssetItem, ESelectInfo::Type SelectInfo )
{
	if ( !bBulkSelecting )
	{
		if ( AssetItem.IsValid() && AssetItem->GetType() != EAssetItemType::Folder )
		{
			OnAssetSelected.ExecuteIfBound(StaticCastSharedPtr<FAssetViewAsset>(AssetItem)->Data);
		}
		else
		{
			OnAssetSelected.ExecuteIfBound(FAssetData());
		}
	}
}

void SAssetView::ItemScrolledIntoView(TSharedPtr<struct FAssetViewItem> AssetItem, const TSharedPtr<ITableRow>& Widget )
{
	if ( AssetItem->bRenameWhenScrolledIntoview )
	{
		// Make sure we have window focus to avoid the inline text editor from canceling itself if we try to click on it
		// This can happen if creating an asset opens an intermediary window which steals our focus, 
		// eg, the blueprint and slate widget style class windows (TTP# 314240)
		TSharedPtr<SWindow> OwnerWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		if(OwnerWindow.IsValid())
		{
			OwnerWindow->BringToFront();
		}

		if ( Widget.IsValid() && Widget->GetContent().IsValid() )
		{
			AssetItem->RenamedRequestEvent.ExecuteIfBound();
		}

		AssetItem->bRenameWhenScrolledIntoview = false;
	}
}

TSharedPtr<SWidget> SAssetView::OnGetContextMenuContent()
{
	if ( CanOpenContextMenu() )
	{
		const TArray<FString> SelectedFolders = GetSelectedFolders();
		if(SelectedFolders.Num() > 0)
		{
			return OnGetFolderContextMenu.Execute(SelectedFolders, OnGetPathContextMenuExtender, FOnCreateNewFolder::CreateSP(this, &SAssetView::OnCreateNewFolder));
		}
		else
		{
			return OnGetAssetContextMenu.Execute(GetSelectedAssets());
		}
	}
	
	return NULL;
}

bool SAssetView::CanOpenContextMenu() const
{
	if ( !OnGetAssetContextMenu.IsBound() )
	{
		// You can only a summon a context menu if one is set up
		return false;
	}

	if ( IsThumbnailEditMode() )
	{
		// You can not summon a context menu for assets when in thumbnail edit mode because right clicking may happen inadvertently while adjusting thumbnails.
		return false;
	}

	TArray<FAssetData> SelectedAssets = GetSelectedAssets();

	// Detect if at least one temporary item was selected. If there were no valid assets selected and a temporary one was, then deny the context menu.
	TArray<TSharedPtr<FAssetViewItem>> SelectedItems = GetSelectedItems();
	bool bAtLeastOneTemporaryItemFound = false;
	for ( auto ItemIt = SelectedItems.CreateConstIterator(); ItemIt; ++ItemIt )
	{
		const TSharedPtr<FAssetViewItem>& Item = *ItemIt;
		if ( Item->IsTemporaryItem() )
		{
			bAtLeastOneTemporaryItemFound = true;
		}
	}

	// If there were no valid assets found, but some invalid assets were found, deny the context menu
	if ( SelectedAssets.Num() == 0 && bAtLeastOneTemporaryItemFound )
	{
		return false;
	}

	// Build a list of selected object paths
	TArray<FString> ObjectPaths;
	for(auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt)
	{
		ObjectPaths.Add( AssetIt->ObjectPath.ToString() );
	}

	bool bLoadSuccessful = true;
	bool bShouldPromptToLoadAssets = false;

	if ( bPreloadAssetsForContextMenu )
	{
		// Should the user be asked to load unloaded assets
		TArray<FString> UnloadedObjects;
		bShouldPromptToLoadAssets = ContentBrowserUtils::ShouldPromptToLoadAssets(ObjectPaths, UnloadedObjects);

		bool bShouldLoadAssets = false;
		if ( bShouldPromptToLoadAssets )
		{
			// The user should be prompted to loaded assets
			bShouldLoadAssets = ContentBrowserUtils::PromptToLoadAssets(UnloadedObjects);
		}
		else
		{
			// The user should not be prompted to load assets but assets should still be loaded
			bShouldLoadAssets = true;
		}

		if ( bShouldLoadAssets )
		{
			// Load assets that are unloaded
			TArray<UObject*> LoadedObjects;
			const bool bAllowedToPrompt = false;
			bLoadSuccessful = ContentBrowserUtils::LoadAssetsIfNeeded(ObjectPaths, LoadedObjects, bAllowedToPrompt);
		}
	}

	// Do not show the context menu if we prompted the user to load assets or if the load failed
	return !bShouldPromptToLoadAssets && bLoadSuccessful;
}

void SAssetView::OnListMouseButtonDoubleClick(TSharedPtr<FAssetViewItem> AssetItem)
{
	if ( !ensure(AssetItem.IsValid()) )
	{
		return;
	}

	if ( IsThumbnailEditMode() )
	{
		// You can not activate assets when in thumbnail edit mode because double clicking may happen inadvertently while adjusting thumbnails.
		return;
	}

	if ( AssetItem->GetType() == EAssetItemType::Folder )
	{
		OnPathSelected.ExecuteIfBound(StaticCastSharedPtr<FAssetViewFolder>(AssetItem)->FolderPath);
		return;
	}

	if ( AssetItem->IsTemporaryItem() )
	{
		// You may not activate temporary items, they are just for display.
		return;
	}

	TArray<FAssetData> ActivatedAssets;
	ActivatedAssets.Add(StaticCastSharedPtr<FAssetViewAsset>(AssetItem)->Data);
	OnAssetsActivated.ExecuteIfBound( ActivatedAssets, EAssetTypeActivationMethod::DoubleClicked );
}

FReply SAssetView::OnDraggingAssetItem( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( bAllowDragging && MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton ) )
	{
		TArray<FAssetData> AssetDataList = GetSelectedAssets();

		if (AssetDataList.Num())
		{
			// We have some items selected, start a drag-drop
			TArray<FAssetData> InAssetData;

			for (int32 AssetIdx = 0; AssetIdx < AssetDataList.Num(); ++AssetIdx)
			{
				const FAssetData& AssetData = AssetDataList[AssetIdx];

				if ( !AssetData.IsValid() || AssetData.AssetClass == UObjectRedirector::StaticClass()->GetFName() )
				{
					// Skip invalid assets and redirectors
					continue;
				}

				if (AssetData.AssetClass == UClass::StaticClass()->GetFName())
				{
					// If dragging a class, send though an FAssetData whose name is null and class is this class' name
					InAssetData.Add(AssetData);
				}
				else if ( AssetData.IsAssetLoaded() || !FEditorFileUtils::IsMapPackageAsset(AssetData.ObjectPath.ToString()) )
				{
					InAssetData.Add(AssetData);
				}
			}
			
			if ( InAssetData.Num() > 0 )
			{
				FReply Reply = FReply::Unhandled();
				if ( OnAssetDragged.IsBound() )
				{
					Reply = OnAssetDragged.Execute( InAssetData );
				}

				if ( !Reply.IsEventHandled() )
				{
					Reply = FReply::Handled().BeginDragDrop(FAssetDragDropOp::New(InAssetData));
				}

				return Reply;
			}
		}
		else
		{
			// are we dragging some folders?
			TArray<FString> SelectedFolders = GetSelectedFolders();
			if(SelectedFolders.Num() > 0)
			{
				return FReply::Handled().BeginDragDrop(FAssetPathDragDropOp::New(SelectedFolders));
			}
		}
	}

	return FReply::Unhandled();
}

bool SAssetView::AssetVerifyRenameCommit(const TSharedPtr<FAssetViewItem>& Item, const FText& NewName, const FSlateRect& MessageAnchor, FText& OutErrorMessage)
{
	// Everything other than a folder is considered an asset, including "Creation" and "Duplication"
	// See FAssetViewCreation and FAssetViewDuplication
	const bool bIsAssetType = Item->GetType() != EAssetItemType::Folder;

	FString NewNameString = NewName.ToString();
	if ( bIsAssetType )
	{
		const TSharedPtr<FAssetViewAsset>& ItemAsAsset = StaticCastSharedPtr<FAssetViewAsset>(Item);
		if ( !Item->IsTemporaryItem() && NewNameString == ItemAsAsset->Data.AssetName.ToString() )
		{
			return true;
		}
	}

	if ( bIsAssetType )
	{
		// Make sure the name is not already a class or otherwise invalid for saving
		if ( !FEditorFileUtils::IsFilenameValidForSaving(NewNameString, OutErrorMessage) )
		{
			// Return false to indicate that the user should enter a new name
			return false;
		}

		// Make sure the new name only contains valid characters
		if ( !FName(*NewNameString).IsValidXName( INVALID_OBJECTNAME_CHARACTERS INVALID_LONGPACKAGE_CHARACTERS, &OutErrorMessage ) )
		{
			// Return false to indicate that the user should enter a new name
			return false;
		}

		const TSharedPtr<FAssetViewAsset>& ItemAsAsset = StaticCastSharedPtr<FAssetViewAsset>(Item);

		// Prepare the object path for the new name
		const FString NewPackageName = FString::Printf(TEXT("%s/%s"), *ItemAsAsset->Data.PackagePath.ToString(), *NewNameString);
		FString ObjectPathStr = NewPackageName + TEXT(".");
		if ( ItemAsAsset->Data.GroupNames != NAME_None )
		{
			ObjectPathStr += ItemAsAsset->Data.GroupNames.ToString() + TEXT(".");
		}
		ObjectPathStr += NewNameString;

		// Make sure we are not creating an FName that is too large
		if ( ObjectPathStr.Len() > NAME_SIZE )
		{
			// This asset already exists at this location, inform the user and continue
			OutErrorMessage = OutErrorMessage = LOCTEXT("AssetNameTooLong", "This asset name is too long. Please choose a shorter name.");
			// Return false to indicate that the user should enter a new name
			return false;
		}

		// The following checks are done mostly to prevent / alleviate the problems that "long" paths are causing with the BuildFarm and cooked builds.
		// The BuildFarm buildmachines use a verbose path to encode extra information to provide more information when things fail, however
		// this makes the path limitation (260 chars on Windows) a problem. It doubles up the GGameName and does the cooking in another
		// sub-folder, one of the "saved/sandboxes", with folder duplication.

		// Get the SubPath containing folders without the "game name" folder itself
		const FString GameNameStr(GGameName);
		FString SubPath = FPaths::GameDir();
		FPaths::NormalizeDirectoryName(SubPath);
		SubPath = SubPath.Replace(*(FString(TEXT("../../../")) + GameNameStr), TEXT(""));
		FPaths::RemoveDuplicateSlashes(SubPath);

		// Calculate the maximum path length this will generate when doing a cooked build.
		const int32 PathCalcLen = SubPath.Len() + (2 * GameNameStr.Len()) + (NewPackageName + FPackageName::GetAssetPackageExtension()).Len();
		if ( PathCalcLen >= MAX_PROJECTED_COOKING_PATH )
		{
			// The projected length of the path for cooking is too long
			OutErrorMessage = FText::Format( LOCTEXT("AssetCookingPathTooLong", 
				"The path to the asset is too long for cooking, the maximum is '{0}' characters.\nPlease choose a shorter name for the asset or create it in a shallower folder structure with shorter folder names."),
				FText::FromString(FString::Printf(TEXT("%d"), MAX_PROJECTED_COOKING_PATH)) );
			// Return false to indicate that the user should enter a new name
			return false;
		}

		// Make sure we are not creating an path that is too long for the OS
		const FString RelativePathFilename = FPackageName::LongPackageNameToFilename(NewPackageName, FPackageName::GetAssetPackageExtension());	// full relative path with name + extension
		const FString FullPath = FPaths::ConvertRelativePathToFull(RelativePathFilename);	// path to file on disk
		if ( ObjectPathStr.Len() > (PLATFORM_MAX_FILEPATH_LENGTH - MAX_CLASS_NAME_LENGTH) || FullPath.Len() > PLATFORM_MAX_FILEPATH_LENGTH )
		{
			// The full path for the asset is too long
			OutErrorMessage = FText::Format( LOCTEXT("AssetPathTooLong", 
				"The full path for the asset is too deep, the maximum is '{0}'. \nPlease choose a shorter name for the asset or create it in a shallower folder structure."), 
				FText::FromString(FString::Printf(TEXT("%d"), PLATFORM_MAX_FILEPATH_LENGTH)) );
			// Return false to indicate that the user should enter a new name
			return false;
		}

		FName NewObjectPath = FName(*ObjectPathStr);

		// Check if the input is valid before we proceed with the rename.
		if ( IsPathInAssetItemsList(NewObjectPath) )
		{
			// This asset already exists at this location, inform the user and continue
			OutErrorMessage = FText::Format( LOCTEXT("RenameAssetAlreadyExists", "An asset already exists at this location with the name '{0}'."), FText::FromString( NewNameString ) );

			// Return false to indicate that the user should enter a new name
			return false;
		}
	}
	else
	{
		const TSharedPtr<FAssetViewFolder>& ItemAsFolder = StaticCastSharedPtr<FAssetViewFolder>(Item);

		if ( !ContentBrowserUtils::IsValidFolderName(NewName.ToString(), OutErrorMessage) )
		{
			return false;
		}

		const FString NewPath = FPaths::GetPath(ItemAsFolder->FolderPath) / NewName.ToString();
		if ( ContentBrowserUtils::DoesFolderExist(NewPath) )
		{
			OutErrorMessage = LOCTEXT("RenameFolderAlreadyExists", "A folder already exists at this location with this name.");
			return false;		
		}

		// Make sure we are not creating a folder path that is too long
		if ( NewPath.Len() > PLATFORM_MAX_FILEPATH_LENGTH - MAX_CLASS_NAME_LENGTH )
		{
			// The full path for the folder is too long
			OutErrorMessage = FText::Format( LOCTEXT("RenameFolderPathTooLong", 
				"The full path for the folder is too deep, the maximum is '{0}'. Please choose a shorter name for the folder or create it in a shallower folder structure."), 
				FText::FromString(FString::Printf(TEXT("%d"), PLATFORM_MAX_FILEPATH_LENGTH)) );
			// Return false to indicate that the user should enter a new name for the folder
			return false;
		}
	}

	return true;
}

void SAssetView::AssetRenameBegin(const TSharedPtr<FAssetViewItem>& Item, const FString& NewName, const FSlateRect& MessageAnchor)
{
	check(!RenamingAsset.IsValid());
	RenamingAsset = Item;
}

void SAssetView::AssetRenameCommit(const TSharedPtr<FAssetViewItem>& Item, const FString& NewName, const FSlateRect& MessageAnchor, const ETextCommit::Type CommitType)
{
	const EAssetItemType::Type ItemType = Item->GetType();

	// If the item had a factory, create a new object, otherwise rename
	bool bSuccess = false;
	UObject* Asset = NULL;
	FText ErrorMessage;
	if ( ItemType == EAssetItemType::Normal )
	{
		const TSharedPtr<FAssetViewAsset>& ItemAsAsset = StaticCastSharedPtr<FAssetViewAsset>(Item);

		// Check if the name is different
		if( NewName == ItemAsAsset->Data.AssetName.ToString() )
		{
			RenamingAsset.Reset();
			return;
		}

		// Committed rename
		Asset = ItemAsAsset->Data.GetAsset();
		ContentBrowserUtils::RenameAsset(Asset, NewName, ErrorMessage);
		bSuccess = true;
	}
	else if ( ItemType == EAssetItemType::Creation || ItemType == EAssetItemType::Duplication )
	{
		if (CommitType == ETextCommit::OnCleared)
		{
			// Clearing the rename box on a newly created asset cancels the entire creation process
			FilteredAssetItems.Remove(Item);
			bRefreshSourceItemsRequested = true;
		}
		else
		{
			Asset = CreateAssetFromTemporary(NewName, StaticCastSharedPtr<FAssetViewAsset>(Item), ErrorMessage);
			bSuccess = Asset != NULL;
		}
	}
	else if( ItemType == EAssetItemType::Folder )
	{
		const TSharedPtr<FAssetViewFolder>& ItemAsFolder = StaticCastSharedPtr<FAssetViewFolder>(Item);
		if(ItemAsFolder->bNewFolder)
		{
			ItemAsFolder->bNewFolder = false;

			const FString NewPath = FPaths::GetPath(ItemAsFolder->FolderPath) / NewName;
			FText ErrorText;
			if( ContentBrowserUtils::IsValidFolderName(NewName, ErrorText) &&
				!ContentBrowserUtils::DoesFolderExist(NewPath))
			{
				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
				bSuccess = AssetRegistryModule.Get().AddPath(NewPath);
			}

			// remove this temp item - a new one will have been added by the asset registry callback
			FilteredAssetItems.Remove(Item);
			bRefreshSourceItemsRequested = true;

			if(!bSuccess)
			{
				ErrorMessage = LOCTEXT("CreateFolderFailed", "Failed to create folder.");
			}
		}
		else if(NewName != ItemAsFolder->FolderName.ToString())
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

			// first create the new folder
			const FString NewPath = FPaths::GetPath(ItemAsFolder->FolderPath) / NewName;
			FText ErrorText;
			if( ContentBrowserUtils::IsValidFolderName(NewName, ErrorText) &&
				!ContentBrowserUtils::DoesFolderExist(NewPath))
			{
				bSuccess = AssetRegistryModule.Get().AddPath(NewPath);
			}

			if(bSuccess)
			{
				// move any assets in our folder
				TArray<FAssetData> AssetsInFolder;
				AssetRegistryModule.Get().GetAssetsByPath(*ItemAsFolder->FolderPath, AssetsInFolder, true);
				TArray<UObject*> ObjectsInFolder;
				ContentBrowserUtils::GetObjectsInAssetData(AssetsInFolder, ObjectsInFolder);
				ContentBrowserUtils::MoveAssets(ObjectsInFolder, NewPath, ItemAsFolder->FolderPath);

				// Now check to see if the original folder is empty, if so we can delete it
				TArray<FAssetData> AssetsInOriginalFolder;
				AssetRegistryModule.Get().GetAssetsByPath(*ItemAsFolder->FolderPath, AssetsInOriginalFolder, true);
				if(AssetsInOriginalFolder.Num() == 0)
				{
					TArray<FString> FoldersToDelete;
					FoldersToDelete.Add(ItemAsFolder->FolderPath);
					ContentBrowserUtils::DeleteFolders(FoldersToDelete);
				}
			}

			bRefreshSourceItemsRequested = true;
		}		
	}
	else
	{
		// Unknown AssetItemType
		ensure(0);
	}

	if ( bSuccess && ItemType != EAssetItemType::Folder )
	{
		if ( ensure(Asset != NULL) )
		{
			// Sort in the new item
			bPendingSortFilteredItems = true;
			bRefreshSourceItemsRequested = true;

			// Refresh the thumbnail
			const TSharedPtr<FAssetThumbnail>* AssetThumbnail = RelevantThumbnails.Find(StaticCastSharedPtr<FAssetViewAsset>(Item));
			if ( AssetThumbnail )
			{
				AssetThumbnailPool->RefreshThumbnail(*AssetThumbnail);
			}

			// Sync to its location
			TArray<FAssetData> AssetDataList;
			new(AssetDataList) FAssetData(Asset);

			if ( OnAssetRenameCommitted.IsBound() )
			{
				// If our parent wants to potentially handle the sync, let it
				OnAssetRenameCommitted.Execute(AssetDataList);
			}
			else
			{
				// Otherwise, sync just the view
				SyncToAssets(AssetDataList);
			}
		}
	}
	else if ( !ErrorMessage.IsEmpty() )
	{
		// Prompt the user with the reason the rename/creation failed
		ContentBrowserUtils::DisplayMessage(ErrorMessage, MessageAnchor, SharedThis(this));
	}

	RenamingAsset.Reset();
}

bool SAssetView::IsRenamingAsset() const
{
	return RenamingAsset.IsValid();
}

bool SAssetView::ShouldAllowToolTips() const
{
	bool bIsRightClickScrolling = false;
	switch( CurrentViewType )
	{
		case EAssetViewType::List:
			bIsRightClickScrolling = ListView->IsRightClickScrolling();
			break;

		case EAssetViewType::Tile:
			bIsRightClickScrolling = TileView->IsRightClickScrolling();
			break;

		case EAssetViewType::Column:
			bIsRightClickScrolling = ColumnView->IsRightClickScrolling();
			break;

		default:
			bIsRightClickScrolling = false;
			break;
	}

	return !bIsRightClickScrolling && !IsThumbnailEditMode() && !IsRenamingAsset();
}

bool SAssetView::IsThumbnailEditMode() const
{
	return IsThumbnailEditModeAllowed() && bThumbnailEditMode;
}

bool SAssetView::IsThumbnailEditModeAllowed() const
{
	return bAllowThumbnailEditMode && GetCurrentViewType() != EAssetViewType::Column;
}

FReply SAssetView::EndThumbnailEditModeClicked()
{
	bThumbnailEditMode = false;

	return FReply::Handled();
}

FString SAssetView::GetAssetCountText() const
{
	const int32 NumAssets = FilteredAssetItems.Num();
	const int32 NumSelectedAssets = GetSelectedItems().Num();

	FText AssetCount;
	if ( NumSelectedAssets == 0 )
	{
		if ( NumAssets == 1 )
		{
			AssetCount = LOCTEXT("AssetCountLabelSingular", "1 item");
		}
		else
		{
			AssetCount = FText::Format( LOCTEXT("AssetCountLabelPlural", "{0} items"), FText::AsNumber(NumAssets) );
		}
	}
	else
	{
		if ( NumAssets == 1 )
		{
			AssetCount = FText::Format( LOCTEXT("AssetCountLabelSingularPlusSelection", "1 item ({0} selected)"), FText::AsNumber(NumSelectedAssets) );
		}
		else
		{
			AssetCount = FText::Format( LOCTEXT("AssetCountLabelPluralPlusSelection", "{0} items ({1} selected)"), FText::AsNumber(NumAssets), FText::AsNumber(NumSelectedAssets) );
		}
	}

	return AssetCount.ToString();
}

EVisibility SAssetView::GetEditModeLabelVisibility() const
{
	return IsThumbnailEditMode() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SAssetView::GetListViewVisibility() const
{
	return GetCurrentViewType() == EAssetViewType::List ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SAssetView::GetTileViewVisibility() const
{
	return GetCurrentViewType() == EAssetViewType::Tile ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SAssetView::GetColumnViewVisibility() const
{
	return GetCurrentViewType() == EAssetViewType::Column ? EVisibility::Visible : EVisibility::Collapsed;
}

void SAssetView::ToggleThumbnailEditMode()
{
	bThumbnailEditMode = !bThumbnailEditMode;
}

float SAssetView::GetThumbnailScale() const
{
	return ThumbnailScaleSliderValue.Get();
}

void SAssetView::SetThumbnailScale( float NewValue )
{
	if ( ThumbnailScaleSliderValue.IsBound() )
	{
		ThumbnailScaleChanged.ExecuteIfBound( NewValue );
	}
	else
	{
		ThumbnailScaleSliderValue = NewValue;
	}

	RefreshList();
}

bool SAssetView::IsThumbnailScalingLocked() const
{
	return GetCurrentViewType() == EAssetViewType::Column;
}

float SAssetView::GetListViewItemHeight() const
{
	return (ListViewThumbnailSize + ListViewThumbnailPadding * 2) * FMath::Lerp(MinThumbnailScale, MaxThumbnailScale, GetThumbnailScale());
}

float SAssetView::GetTileViewItemHeight() const
{
	float Height = GetTileViewItemBaseHeight() * FillScale;

	if ( LabelVisibility.Get() != EVisibility::Collapsed )
	{
		Height += TileViewNameHeight;
	}

	return Height;
}

float SAssetView::GetTileViewItemBaseHeight() const
{
	return (TileViewThumbnailSize + TileViewThumbnailPadding * 2) * FMath::Lerp(MinThumbnailScale, MaxThumbnailScale, GetThumbnailScale());
}

float SAssetView::GetTileViewItemWidth() const
{
	return GetTileViewItemBaseWidth() * FillScale;
}

float SAssetView::GetTileViewItemBaseWidth() const
{
	return ( TileViewThumbnailSize + TileViewThumbnailPadding * 2 ) * FMath::Lerp( MinThumbnailScale, MaxThumbnailScale, GetThumbnailScale() );
}

EColumnSortMode::Type SAssetView::GetColumnSortMode(FName ColumnId) const
{
	if ( ColumnId == SortManager.GetSortColumnId() )
	{
		return SortManager.GetSortMode();
	}
	else
	{
		return EColumnSortMode::None;
	}
}

void SAssetView::OnSortColumnHeader( const FName& ColumnId, EColumnSortMode::Type NewSortMode )
{
	SortManager.SetSortColumnId( ColumnId );
	SortManager.SetSortMode( NewSortMode );
	SortList();
}

bool SAssetView::IsPathInAssetItemsList(FName ObjectPath) const
{
	for ( auto AssetIt = AssetItems.CreateConstIterator(); AssetIt; ++AssetIt )
	{
		if ( (*AssetIt).ObjectPath == ObjectPath )
		{
			return true;
		}
	}

	return false;
}

EVisibility SAssetView::IsAssetShowWarningTextVisible() const
{
	return FilteredAssetItems.Num() > 0 ? EVisibility::Collapsed : EVisibility::Visible;
}

FText SAssetView::GetAssetShowWarningText() const
{
	if (AssetShowWarningText.IsBound())
	{
		return AssetShowWarningText.Get();
	}
	
	FText NothingToShowText, DropText;
	if (ShouldFilterRecursively())
	{
		NothingToShowText = LOCTEXT( "NothingToShowCheckFilter", "No results, check your filter." );
	}

	if ( SourcesData.Collections.Num() > 0 )
	{
		DropText = LOCTEXT( "DragAssetsHere", "Drag and drop assets here to add them to the collection." );
	}
	else if ( OnGetAssetContextMenu.IsBound() )
	{
		DropText = LOCTEXT( "DropFilesOrRightClick", "Drop files here or right click to create content." );
	}
	
	return NothingToShowText.IsEmpty() ? DropText : FText::Format(LOCTEXT("NothingToShowPattern", "{0}\n\n{1}"), NothingToShowText, DropText);
}

bool SAssetView::HasSingleCollectionSource() const
{
	return ( SourcesData.Collections.Num() == 1 && SourcesData.PackagePaths.Num() == 0 );
}

void SAssetView::OnAssetsDragDropped(const TArray<FAssetData>& AssetList, const FString& DestinationPath)
{
	// Do not display the menu if any of the assets are classes as they cannot be moved or copied
	for( int32 AssetIndex = 0; AssetIndex < AssetList.Num(); AssetIndex++ )
	{
		const FAssetData& Asset = AssetList[AssetIndex];
		if ( Asset.AssetClass == "Class" )
		{
			const FText MessageText = LOCTEXT("AssetTreeDropClassError", "The selection contains one or more 'Class' type assets, these cannot be moved or copied.");
			FMessageDialog::Open(EAppMsgType::Ok, MessageText);
			return;
		}
	}

	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, NULL);
	const FText MoveCopyHeaderString = FText::Format( LOCTEXT("AssetViewDropMenuHeading", "Move/Copy to {0}"), FText::FromString( DestinationPath ) );
	MenuBuilder.BeginSection("PathAssetMoveCopy", MoveCopyHeaderString);
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DragDropCopy", "Copy Here"),
			LOCTEXT("DragDropCopyTooltip", "Creates a copy of all dragged files in this folder."),
			FSlateIcon(),
			FUIAction(
			FExecuteAction::CreateSP( this, &SAssetView::ExecuteDropCopy, AssetList, DestinationPath ),
			FCanExecuteAction()
			)
			);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("DragDropMove", "Move Here"),
			LOCTEXT("DragDropMoveTooltip", "Moves all dragged files to this folder."),
			FSlateIcon(),
			FUIAction(
			FExecuteAction::CreateSP( this, &SAssetView::ExecuteDropMove, AssetList, DestinationPath ),
			FCanExecuteAction()
			)
			);
	}
	MenuBuilder.EndSection();

	TWeakPtr< SWindow > ContextMenuWindow = FSlateApplication::Get().PushMenu(
		SharedThis( this ),
		MenuBuilder.MakeWidget(),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu )
		);
}

void SAssetView::OnPathsDragDropped(const TArray<FString>& PathNames, const FString& DestinationPath)
{
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, NULL);
	MenuBuilder.BeginSection("PathFolderMoveCopy", FText::Format(LOCTEXT("AssetViewDropMenuHeading", "Move/Copy to {0}"), FText::FromString(DestinationPath)));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DragDropCopyFolder", "Copy Folder Here"),
			LOCTEXT("DragDropCopyFolderTooltip", "Creates a copy of all assets in the dragged folders to this folder, preserving folder structure."),
			FSlateIcon(),
			FUIAction( FExecuteAction::CreateSP( this, &SAssetView::ExecuteDropCopyFolder, PathNames, DestinationPath ) )
			);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("DragDropMoveFolder", "Move Folder Here"),
			LOCTEXT("DragDropMoveFolderTooltip", "Moves all assets in the dragged folders to this folder, preserving folder structure."),
			FSlateIcon(),
			FUIAction( FExecuteAction::CreateSP( this, &SAssetView::ExecuteDropMoveFolder, PathNames, DestinationPath ) )
			);
	}
	MenuBuilder.EndSection();

	TWeakPtr< SWindow > ContextMenuWindow = FSlateApplication::Get().PushMenu(
		SharedThis( this ),
		MenuBuilder.MakeWidget(),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu )
		);
}

void SAssetView::OnFilesDragDropped(const TArray<FString>& AssetList, const FString& DestinationPath)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().ImportAssets( AssetList, DestinationPath );
}

void SAssetView::ExecuteDropCopy(TArray<FAssetData> AssetList, FString DestinationPath)
{
	TArray<UObject*> DroppedObjects;
	ContentBrowserUtils::GetObjectsInAssetData(AssetList, DroppedObjects);

	TArray<UObject*> NewObjects;
	ObjectTools::DuplicateObjects(DroppedObjects, TEXT(""), DestinationPath, /*bOpenDialog=*/false, &NewObjects);

	// If any objects were duplicated, report the success
	if ( NewObjects.Num() )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("Number"), NewObjects.Num() );
		const FText Message = FText::Format( LOCTEXT("AssetsDroppedCopy", "{Number} asset(s) copied"), Args );
		const FVector2D& CursorPos = FSlateApplication::Get().GetCursorPos();
		FSlateRect MessageAnchor(CursorPos.X, CursorPos.Y, CursorPos.X, CursorPos.Y);
		ContentBrowserUtils::DisplayMessage(Message, MessageAnchor, SharedThis(this));
	}
}

void SAssetView::ExecuteDropMove(TArray<FAssetData> AssetList, FString DestinationPath)
{
	TArray<UObject*> DroppedObjects;
	ContentBrowserUtils::GetObjectsInAssetData(AssetList, DroppedObjects);

	ContentBrowserUtils::MoveAssets(DroppedObjects, DestinationPath);
}

void SAssetView::ExecuteDropCopyFolder(TArray<FString> PathNames, FString DestinationPath)
{
	ContentBrowserUtils::CopyFolders(PathNames, DestinationPath);
}

void SAssetView::ExecuteDropMoveFolder(TArray<FString> PathNames, FString DestinationPath)
{
	ContentBrowserUtils::MoveFolders(PathNames, DestinationPath);
}

void SAssetView::SetUserSearching(bool bInSearching)
{
	if(bUserSearching != bInSearching)
	{
		bRefreshSourceItemsRequested = true;
	}
	bUserSearching = bInSearching;
}

void SAssetView::HandleSettingChanged(FName PropertyName)
{
	if ((PropertyName == "ShowOnlyAssetsInSelectedFolders") ||
		(PropertyName == "DisplayFolders") ||
		(PropertyName == "DisplayDevelopersFolder") ||
		(PropertyName == "DisplayEngineFolder") ||
		(PropertyName == NAME_None))	// @todo: Needed if PostEditChange was called manually, for now
	{
		bRefreshSourceItemsRequested = true;
	}
}

FText SAssetView::GetQuickJumpTerm() const
{
	return FText::FromString(QuickJumpData.JumpTerm);
}

EVisibility SAssetView::IsQuickJumpVisible() const
{
	return (QuickJumpData.JumpTerm.IsEmpty()) ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
}

FSlateColor SAssetView::GetQuickJumpColor() const
{
	return FEditorStyle::GetColor((QuickJumpData.bHasValidMatch) ? "InfoReporting.BackgroundColor" : "ErrorReporting.BackgroundColor");
}

void SAssetView::ResetQuickJump()
{
	QuickJumpData.JumpTerm.Empty();
	QuickJumpData.bIsJumping = false;
	QuickJumpData.bHasChangedSinceLastTick = false;
	QuickJumpData.bHasValidMatch = false;
}

FReply SAssetView::HandleQuickJumpKeyDown(const TCHAR InCharacter, const bool bIsControlDown, const bool bIsAltDown, const bool bTestOnly)
{
	// Check for special characters
	if(bIsControlDown || bIsAltDown)
	{
		return FReply::Unhandled();
	}

	// Check for invalid characters
	for(int InvalidCharIndex = 0; InvalidCharIndex < ARRAY_COUNT(INVALID_OBJECTNAME_CHARACTERS) - 1; ++InvalidCharIndex)
	{
		if(InCharacter == INVALID_OBJECTNAME_CHARACTERS[InvalidCharIndex])
		{
			return FReply::Unhandled();
		}
	}

	switch(InCharacter)
	{
	// Ignore some other special characters that we don't want to be entered into the buffer
	case 0:		// Any non-character key press, e.g. f1-f12, Delete, Pause/Break, etc.
				// These should be explicitly not handled so that their input bindings are handled higher up the chain.

	case 8:		// Backspace
	case 13:	// Enter
	case 27:	// Esc
		return FReply::Unhandled();

	default:
		break;
	}

	// Any other character!
	if(!bTestOnly)
	{
		QuickJumpData.JumpTerm.AppendChar(InCharacter);
		QuickJumpData.bHasChangedSinceLastTick = true;
	}

	return FReply::Handled();
}

bool SAssetView::PerformQuickJump(const bool bWasJumping)
{
	auto GetAssetViewItemName = [](const TSharedPtr<FAssetViewItem> &Item) -> FString
	{
		switch(Item->GetType())
		{
		case EAssetItemType::Normal:
			{
				const TSharedPtr<FAssetViewAsset>& ItemAsAsset = StaticCastSharedPtr<FAssetViewAsset>(Item);
				return ItemAsAsset->Data.AssetName.ToString();
			}

		case EAssetItemType::Folder:
			{
				const TSharedPtr<FAssetViewFolder>& ItemAsFolder = StaticCastSharedPtr<FAssetViewFolder>(Item);
				return ItemAsFolder->FolderName.ToString();
			}

		default:
			return FString();
		}
	};

	auto JumpToNextMatch = [this, &GetAssetViewItemName](const int StartIndex, const int EndIndex) -> bool
	{
		check(StartIndex >= 0);
		check(EndIndex <= FilteredAssetItems.Num());

		for(int NewSelectedItemIndex = StartIndex; NewSelectedItemIndex < EndIndex; ++NewSelectedItemIndex)
		{
			TSharedPtr<FAssetViewItem>& NewSelectedItem = FilteredAssetItems[NewSelectedItemIndex];
			const FString NewSelectedItemName = GetAssetViewItemName(NewSelectedItem);
			if(NewSelectedItemName.StartsWith(QuickJumpData.JumpTerm, ESearchCase::IgnoreCase))
			{
				SetSelection(NewSelectedItem);
				RequestScrollIntoView(NewSelectedItem);
				return true;
			}
		}

		return false;
	};

	TArray<TSharedPtr<FAssetViewItem>> SelectedItems = GetSelectedItems();
	TSharedPtr<FAssetViewItem> SelectedItem = (SelectedItems.Num()) ? SelectedItems[0] : nullptr;

	// If we have a selection, and we were already jumping, first check to see whether 
	// the current selection still matches the quick-jump term; if it does, we do nothing
	if(bWasJumping && SelectedItem.IsValid())
	{
		const FString SelectedItemName = GetAssetViewItemName(SelectedItem);
		if(SelectedItemName.StartsWith(QuickJumpData.JumpTerm, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	// We need to move on to the next match in FilteredAssetItems that starts with the given quick-jump term
	const int SelectedItemIndex = (SelectedItem.IsValid()) ? FilteredAssetItems.Find(SelectedItem) : INDEX_NONE;
	const int StartIndex = (SelectedItemIndex == INDEX_NONE) ? 0 : SelectedItemIndex + 1;
	
	bool ValidMatch = JumpToNextMatch(StartIndex, FilteredAssetItems.Num());
	if(!ValidMatch && StartIndex > 0)
	{
		// If we didn't find a match, we need to loop around and look again from the start (assuming we weren't already)
		return JumpToNextMatch(0, StartIndex);
	}

	return ValidMatch;
}

#undef LOCTEXT_NAMESPACE
