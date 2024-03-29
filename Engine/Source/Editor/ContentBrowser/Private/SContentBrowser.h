// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * A widget to display and work with all game and engine content
 */
class SContentBrowser : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SContentBrowser )
		: _ContainingTab()
		, _InitiallyLocked(false)
	{}
		/** The tab in which the content browser resides */
		SLATE_ARGUMENT( TSharedPtr<SDockTab>, ContainingTab )

		/** If true, this content browser will not sync from external sources. */
		SLATE_ARGUMENT( bool, InitiallyLocked )

	SLATE_END_ARGS()

	~SContentBrowser();

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs, const FName& InstanceName );

	/** Sets up an inline-name for the creation of a new asset using the specified path and the specified class and/or factory */
	void CreateNewAsset(const FString& DefaultAssetName, const FString& PackagePath, UClass* AssetClass, UFactory* Factory);

	/** Changes sources to show the specified assets and selects them in the asset view
	 *
	 *	@param AssetDataList		- A list of assets to sync the view to
	 * 
	 *	@param bAllowImplicitSync	- true to allow the view to sync to parent folders if they are already selected,
	 *								  false to force the view to select the explicit Parent folders of each asset 
	 */
	void SyncToAssets( const TArray<FAssetData>& AssetDataList, const bool bAllowImplicitSync = false );

	/** Sets this content browser as the primary browser. The primary browser is the target for asset syncs and contributes to the global selection set. */
	void SetIsPrimaryContentBrowser (bool NewIsPrimary);

	/** Gets the tab manager for the tab containing this browser */
	TSharedPtr<FTabManager> GetTabManager() const;

	/** Loads all selected assets if unloaded */
	void LoadSelectedObjectsIfNeeded();

	/** Returns all the assets that are selected in the asset view */
	void GetSelectedAssets(TArray<FAssetData>& SelectedAssets);

	/** Saves all persistent settings to config and returns a string identifier */
	void SaveSettings() const;

	/** Get the unique name of this content browser's in */
	const FName GetInstanceName() const;

	/** Returns true if this content browser does not accept syncing from an external source */
	bool IsLocked() const;

	/** Gives keyboard focus to the asset search box */
	void SetKeyboardFocusOnSearch() const;

	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent ) OVERRIDE;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE;
	virtual FReply OnPreviewMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE;
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) OVERRIDE;

private:
	/** Called to retrieve the text that should be highlighted on assets */
	FText GetHighlightedText() const;

	/** Called to retrieve the text that should be in the import tooltip */
	FText GetImportTooltipText() const;

	/** Called when a containing tab is closing, if there is one */
	void OnContainingTabSavingVisualState() const;

	/** Called when a containing tab is closed, if there is one */
	void OnContainingTabClosed(TSharedRef<SDockTab> DockTab);

	/** Called when a containing tab is activated, if there is one */
	void OnContainingTabActivated(TSharedRef<SDockTab> DockTab, ETabActivationCause::Type InActivationCause);

	/** Loads settings from config based on the browser's InstanceName*/
	void LoadSettings(const FName& InstanceName);

	/** Handler for when the sources were changed */
	void SourcesChanged(const TArray<FString>& SelectedPaths, const TArray<FCollectionNameType>& SelectedCollections);

	/** Handler for when a folder has been entered in the path view */
	void FolderEntered(const FString& FolderPath);

	/** Handler for when a path has been selected in the path view */
	void PathSelected(const FString& FolderPath);

	/** Gets the current path if one exists, otherwise returns empty string. */
	FString GetCurrentPath() const;

	/** Get the asset tree context menu */
	TSharedRef<FExtender> GetPathContextMenuExtender(const TArray<FString>& SelectedPaths) const;

	/** Handler for when a collection has been selected in the collection view */
	void CollectionSelected(const FCollectionNameType& SelectedCollection);

	/** Handler for when the sources were changed by the path picker */
	void PathPickerPathSelected(const FString& FolderPath);

	/** Handler for when the sources were changed by the path picker collection view */
	void PathPickerCollectionSelected(const FCollectionNameType& SelectedCollection);

	/** Sets the state of the browser to the one described by the supplied history data */
	void OnApplyHistoryData (const FHistoryData& History);

	/** Updates the supplied history data with current information */
	void OnUpdateHistoryData (FHistoryData& History) const;

	/** Handler for when the path view requests an asset creation */
	void NewAssetRequested(const FString& SelectedPath, TWeakObjectPtr<UClass> FactoryClass);

	/** Handler for when the path context menu requests a folder creation */
	void NewFolderRequested(const FString& SelectedPath);

	/** Called by the editable text control when the search text is changed by the user */
	void OnSearchBoxChanged(const FText& InSearchText);

	/** Called by the editable text control when the user commits a text change */
	void OnSearchBoxCommitted(const FText& InSearchText, ETextCommit::Type CommitInfo);

	/** Called when a crumb in the path breadcrumb trail or menu is clicked */
	void OnPathClicked(const FString& CrumbData);

	/** Called when item in the path delimiter arrow menu is clicked */
	void OnPathMenuItemClicked(FString ClickedPath);

	/** 
	 * Populates the delimiter arrow with a menu of directories under the current directory that can be navigated to
	 * 
	 * @param CrumbData	The current directory path
	 * @return The directory menu
	 */
	TSharedPtr<SWidget> OnGetCrumbDelimiterContent(const FString& CrumbData) const;

	/** Gets the content for the path picker combo button */
	TSharedRef<SWidget> GetPathPickerContent();

	/** Handle creating a context menu to generate a new asset */
	TSharedRef<SWidget> MakeCreateAssetContextMenu();

	/** Gets the tool tip for the new asset button */
	FString GetNewAssetToolTipText() const;

	/** Makes the filters menu */
	TSharedRef<SWidget> MakeAddFilterMenu();
	
	/** Builds the context menu for the filter list area. */
	TSharedPtr<SWidget> GetFilterContextMenu();

	/** Imports a new piece of content. */
	FReply HandleImportClicked();

	/** Saves dirty content. */
	FReply OnSaveClicked();

	/** Handler for when the selection set in the asset view has changed. */
	void OnAssetSelectionChanged(const FAssetData& SelectedAsset);

	/** Handler for when the user double clicks, presses enter, or presses space on an asset */
	void OnAssetsActivated(const TArray<FAssetData>& ActivatedAssets, EAssetTypeActivationMethod::Type ActivationMethod);

	/** Handler for when an asset context menu has been requested. */
	TSharedPtr<SWidget> OnGetAssetContextMenu(const TArray<FAssetData>& SelectedAssets);

	/** Handler for clicking the lock button */
	FReply ToggleLockClicked();

	/** Gets the brush used on the lock button */
	const FSlateBrush* GetToggleLockImage() const;

	/** Gets the visibility state of the asset tree */
	EVisibility GetSourcesViewVisibility() const;

	/** Gets the brush used to on the sources toggle button */
	const FSlateBrush* GetSourcesToggleImage() const;

	/** Handler for clicking the tree expand/collapse button */
	FReply SourcesViewExpandClicked();

	/** Gets the visibility of the path expander button */
	EVisibility GetPathExpanderVisibility() const;

	/** Handler for clicking the history back button */
	FReply BackClicked();

	/** Handler for clicking the history forward button */
	FReply ForwardClicked();

	/** Handler for opening assets or folders */
	void OnOpenAssetsOrFolders();

	/** Handler for previewing assets */
	void OnPreviewAssets();

	/** Handler for clicking the directory up button */
	void OnDirectoryUp();

	/** Handler for clicking the directory up button */
	FReply OnDirectoryUpClicked();

	/** True if the user may use the history back button */
	bool IsBackEnabled() const;

	/** True if the user may use the history forward button */
	bool IsForwardEnabled() const;

	/** True if the user may use the directory up button */
	bool CanExecuteDirectoryUp() const;

	/** Gets the tool tip text for the history back button */
	FString GetHistoryBackTooltip() const;

	/** Gets the tool tip text for the history forward button */
	FString GetHistoryForwardTooltip() const;

	/** Gets the tool tip text for the directory up button */
	FText GetDirectoryUpTooltip() const;

	/** Gets the Visibility for the directory up buttons tooltip (hidden if empty) */
	EVisibility GetDirectoryUpToolTipVisibility() const;

	/** Sets the global selection set to the asset view's selected items */
	void SyncGlobalSelectionSet();

	/** Updates the breadcrumb trail to the current path */
	void UpdatePath();

	/** Handler for when a filter in the filter list has changed */
	void OnFilterChanged();

	/** Gets the text for the path label */
	FString GetPathText() const;

	/** Returns true if currently filtering by a source */
	bool IsFilteredBySource() const;

	/** Returns true if a real asset path is selected (i.e \Engine\* or \Game\*) */
	bool IsAssetPathSelected() const;

	/** Handler for when the context menu or asset view requests to find assets in the asset tree */
	void OnFindInAssetTreeRequested(const TArray<FAssetData>& AssetsToFind);

	/** Handler for when the user has committed a rename of an asset */
	void OnAssetRenameCommitted(const TArray<FAssetData>& Assets);

	/** Handler for when the asset context menu requests to rename an asset */
	void OnRenameRequested(const FAssetData& AssetData);

	/** Handler for when the asset context menu requests to rename a folder */
	void OnRenameFolderRequested(const FString& FolderToRename);

	/** Handler for when the asset context menu requests to duplicate an asset */
	void OnDuplicateRequested(const TWeakObjectPtr<UObject>& OriginalObject);

	/** Handler for when the asset context menu requests to refresh the asset view */
	void OnAssetViewRefreshRequested();

	/** Delegate called when an editor setting is changed */
	void HandleSettingChanged(FName PropertyName);

	/** Gets all suggestions for the asset search box */
	TArray<FString> GetAssetSearchSuggestions() const;

	/** Gets the dynamic hint text for the "Search Assets" search text box */
	FText GetSearchAssetsHintText() const;

	/** Delegate called when generating the context menu for a folder */
	TSharedPtr<SWidget> GetFolderContextMenu(const TArray<FString>& SelectedPaths, FContentBrowserMenuExtender_SelectedPaths InMenuExtender, FOnCreateNewFolder OnCreateNewFolder);

	/** Sets up an inline-name for the creation of a default-named folder the specified path */
	void CreateNewFolder(FString FolderPath, FOnCreateNewFolder OnCreateNewFolder);

	/** Bind our UI commands */
	void BindCommands();

private:

	/** The tab that contains this browser */
	TWeakPtr<SDockTab> ContainingTab;

	/** The manager that keeps track of history data for this browser */
	FHistoryManager HistoryManager;

	/** A helper class to manage asset context menu options */
	TSharedPtr<class FAssetContextMenu> AssetContextMenu;

	/** The context menu manager for the path view */
	TSharedPtr<class FPathContextMenu> PathContextMenu;

	/** The asset tree widget */
	TSharedPtr<SPathView> PathViewPtr;

	/** The collection widget */
	TSharedPtr<SCollectionView> CollectionViewPtr;

	/** The asset view widget */
	TSharedPtr<SAssetView> AssetViewPtr;

	/** The breadcrumb trail representing the current path */
	TSharedPtr< SBreadcrumbTrail<FString> > PathBreadcrumbTrail;

	/** The text box used to search for assets */
	TSharedPtr<SAssetSearchBox> SearchBoxPtr;

	/** The filter list */
	TSharedPtr<SFilterList> FilterListPtr;

	/** The path picker */
	TSharedPtr<SComboButton> PathPickerButton;

	/** The expanded state of the asset tree */
	bool bSourcesViewExpanded;

	/** True if this browser is the primary content browser */
	bool bIsPrimaryBrowser;

	/** Unique name for this Content Browser. */
	FName InstanceName;

	/** True if source should not be changed from an outside source */
	bool bIsLocked;

	/** The text filter to use on the assets */
	TSharedPtr< TTextFilter< AssetFilterType > > TextFilter;

	/** Commands handled by this widget */
	TSharedPtr< FUICommandList > Commands;

	/** Delegate used to create a new folder */
	FOnCreateNewFolder OnCreateNewFolder;

	/** The splitter between the path & asset view */
	TSharedPtr<SSplitter> PathAssetSplitterPtr;

	/** The splitter between the path & collection view */
	TSharedPtr<SSplitter> PathCollectionSplitterPtr;

public: 

	/** The section of EditorUserSettings in which to save content browser settings */
	static const FString SettingsIniSection;
};
