// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

/** A single item in the asset tree. Represents a folder. */
class SAssetTreeItem : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_TwoParams( FOnNameChanged, const TSharedPtr<FTreeItem>& /*TreeItem*/, const FVector2D& /*MessageLocation*/);
	DECLARE_DELEGATE_RetVal_ThreeParams( bool, FOnVerifyNameChanged, const FText& /*InName*/, FText& /*OutErrorMessage*/, const FString& /*FolderPath*/);
	DECLARE_DELEGATE_TwoParams( FOnAssetsDragDropped, const TArray<FAssetData>& /*AssetList*/, const TSharedPtr<FTreeItem>& /*TreeItem*/);
	DECLARE_DELEGATE_TwoParams( FOnPathsDragDropped, const TArray<FString>& /*PathNames*/, const TSharedPtr<FTreeItem>& /*TreeItem*/);
	DECLARE_DELEGATE_TwoParams( FOnFilesDragDropped, const TArray<FString>& /*FileNames*/, const TSharedPtr<FTreeItem>& /*TreeItem*/);

	SLATE_BEGIN_ARGS( SAssetTreeItem )
		: _TreeItem( TSharedPtr<FTreeItem>() )
		, _IsItemExpanded( false )
	{}

	/** Data for the folder this item represents */
	SLATE_ARGUMENT( TSharedPtr<FTreeItem>, TreeItem )

		/** Delegate for when the user commits a new name to the folder */
		SLATE_EVENT( FOnNameChanged, OnNameChanged )

		/** Delegate for when the user is typing a new name for the folder */
		SLATE_EVENT( FOnVerifyNameChanged, OnVerifyNameChanged )

		/** Delegate for when assets are dropped on this folder */
		SLATE_EVENT( FOnAssetsDragDropped, OnAssetsDragDropped )

		/** Delegate for when asset paths are dropped on this folder */
		SLATE_EVENT( FOnPathsDragDropped, OnPathsDragDropped )

		/** Delegate for when a list of files is dropped on this folder from an external source */
		SLATE_EVENT( FOnFilesDragDropped, OnFilesDragDropped )

		/** True when this item has children and is expanded */
		SLATE_ATTRIBUTE( bool, IsItemExpanded )

		/** The string in the title to highlight (used when searching folders) */
		SLATE_ATTRIBUTE( FText, HighlightText)

		/** Callback to check if the widget is selected, should only be hooked up if parent widget is handling selection or focus. */
		SLATE_EVENT( FIsSelected, IsSelected )
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs );

	~SAssetTreeItem();

	virtual void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) OVERRIDE;
	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) OVERRIDE;
	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) OVERRIDE;
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) OVERRIDE;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) OVERRIDE;

private:
	/** Handles verifying name changes */
	bool VerifyNameChanged(const FText& InName, FText& OutError) const;

	/** Handles committing a name change */
	void HandleNameCommitted( const FText& NewText, ETextCommit::Type /*CommitInfo*/ );

	/** Returns false if this folder is in the process of being created */
	bool IsReadOnly() const;

	/** Returns true if this folder is special and not a real path (like the Classes folder) */
	bool IsValidAssetPath() const;

	/** Gets the brush used to draw the folder icon */
	const FSlateBrush* GetFolderIcon() const;

	/** Gets the color used to draw the folder icon */
	FSlateColor GetFolderColor() const;

	/** Returns the text of the folder name */
	FText GetNameText() const;

	/** Returns the image for the border around this item. Used for drag/drop operations */
	const FSlateBrush* GetBorderImage() const;

	/** Returns the visibility of the editable folder name */
	EVisibility GetEditableTextVisibility() const;

	/** Returns the visibility of the non-editable folder name */
	EVisibility GetStaticTextVisibility() const;

private:
	/** The data for this item */
	TWeakPtr<FTreeItem> TreeItem;

	/** The name of the asset as an editable text box */
	TSharedPtr<SEditableTextBox> EditableName;

	/** Delegate for when a list of assets is dropped on this folder */
	FOnAssetsDragDropped OnAssetsDragDropped;

	/** Delegate for when a list of folder paths is dropped on this folder */
	FOnPathsDragDropped OnPathsDragDropped;

	/** Delegate for when a list of files is dropped on this folder from an external source */
	FOnFilesDragDropped OnFilesDragDropped;

	/** Delegate for when the user commits a new name to the folder */
	FOnNameChanged OnNameChanged;

	/** Delegate for when a user is typing a name for the folder */
	FOnVerifyNameChanged OnVerifyNameChanged;

	/** True when this item has children and is expanded */
	TAttribute<bool> IsItemExpanded;

	/** The geometry last frame. Used when telling popup messages where to appear. */
	FGeometry LastGeometry;

	/** Brushes for the different folder states */
	const FSlateBrush* FolderOpenBrush;
	const FSlateBrush* FolderClosedBrush;
	const FSlateBrush* FolderDeveloperBrush;

	/** True when a drag is over this item with a valid operation for drop */
	bool bDraggedOver;

	/** True when this item represents a folder which is in the developer folder or is the developer folder itself */
	bool bDeveloperFolder;

	/** Widget to display the name of the asset item and allows for renaming */
	TSharedPtr< SInlineEditableTextBlock > InlineRenameWidget;
};

/** A single item in the collection list. */
class SCollectionListItem : public SCompoundWidget
{
public:
	/** Delegate for when a collection is renamed. If returning false, OutWarningMessage will be displayed over the collection. */
	DECLARE_DELEGATE_OneParam( FOnBeginNameChange, const TSharedPtr<FCollectionItem>& /*Item*/);
	DECLARE_DELEGATE_RetVal_FourParams( bool, FOnNameChangeCommit, const TSharedPtr<FCollectionItem>& /*Item*/, const FString& /*NewName*/, bool /*bChangeConfirmed*/, FText& /*OutWarningMessage*/);
	DECLARE_DELEGATE_RetVal_FourParams( bool, FOnVerifyRenameCommit, const TSharedPtr<FCollectionItem>& /*Item*/, const FString& /*NewName*/, const FSlateRect& /*MessageAnchor*/, FText& /*OutErrorMessage*/)
	DECLARE_DELEGATE_ThreeParams( FOnAssetsDragDropped, const TArray<FAssetData>& /*AssetList*/, const TSharedPtr<FCollectionItem>& /*CollectionItem*/, FText& /*OutMessage*/);

	SLATE_BEGIN_ARGS( SCollectionListItem )
		: _ParentWidget()
		, _CollectionItem( TSharedPtr<FCollectionItem>() )
	{}

	/** Data for the collection this item represents */
	SLATE_ARGUMENT( TSharedPtr<FCollectionItem>, CollectionItem )

		/** The parent widget */
		SLATE_ARGUMENT( TSharedPtr<SWidget>, ParentWidget )

		/** Delegate for when the user begins to rename the item */
		SLATE_EVENT( FOnBeginNameChange, OnBeginNameChange )

		/** Delegate for when the user commits a new name to the folder */
		SLATE_EVENT( FOnNameChangeCommit, OnNameChangeCommit )

		/** Delegate for when a collection name has been entered for an item to verify the name before commit */
		SLATE_EVENT( FOnVerifyRenameCommit, OnVerifyRenameCommit )

		/** Delegate for when assets are dropped on this folder */
		SLATE_EVENT( FOnAssetsDragDropped, OnAssetsDragDropped )

		/** Callback to check if the widget is selected, should only be hooked up if parent widget is handling selection or focus. */
		SLATE_EVENT( FIsSelected, IsSelected )

		/** True if the item is read-only. It will not be able to be renamed if read-only */
		SLATE_ATTRIBUTE( bool, IsReadOnly )

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs );

	~SCollectionListItem();

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) OVERRIDE;
	virtual void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) OVERRIDE;
	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) OVERRIDE;
	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) OVERRIDE;
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) OVERRIDE;

private:
	/** Handles beginning a name change */
	void HandleBeginNameChange( const FText& OldText );

	/** Handles committing a name change */
	void HandleNameCommitted( const FText& NewText, ETextCommit::Type CommitInfo );

	/** Handles verifying a name change */
	bool HandleVerifyNameChanged( const FText& NewText, FText& OutErrorMessage );

	/** Returns the text of the collection name */
	FText GetNameText() const;

	/** Returns the color of the collection name */
	FSlateColor GetCollectionColor() const;

	/** Returns the image for the border around this item. Used for drag/drop operations */
	const FSlateBrush* GetBorderImage() const;

private:
	/** A shared pointer to the parent widget. */
	TSharedPtr<SWidget> ParentWidget;

	/** The data for this item */
	TWeakPtr<FCollectionItem> CollectionItem;

	/** The name of the asset as an editable text box */
	TSharedPtr<SEditableTextBox> EditableName;

	/** True when a drag is over this item with a valid operation for drop */
	bool bDraggedOver;

	/** Delegate for when a list of assets */
	FOnAssetsDragDropped OnAssetsDragDropped;

	/** The geometry as of the last frame. Used to open warning messages over the item */
	FGeometry CachedGeometry;

	/** Delegate for when the user starts to rename an item */
	FOnBeginNameChange OnBeginNameChange;

	/** Delegate for when the user commits a new name to the collection */
	FOnNameChangeCommit OnNameChangeCommit;

	/** Delegate for when a collection name has been entered for an item to verify the name before commit */
	FOnVerifyRenameCommit OnVerifyRenameCommit;

	/** Widget to display the name of the collection item and allows for renaming */
	TSharedPtr< SInlineEditableTextBlock > InlineRenameWidget;
};

/** A tab which may not be dragged */
class SSourcesTab : public SDockableTab
{

};