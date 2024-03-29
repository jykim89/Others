// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once


class FUICommandInfo;
class FUICommandList;


namespace MultiBoxConstants
{	
	// @todo Slate MultiBox: Hard coded dimensions
	const float MenuIconSize = 16.0f;
	const float MenuCheckBoxSize = 16.0f;

	/** The time that a mouse should be hovered over a sub-menu before it automatically opens */
	const float SubMenuOpenTime = 0.0f;

	/** When a sub-menu is already open, the time that a mouse should be hovered over a sub-menu entry before
	    dismissing the other menu and opening this one */
	const float SubMenuClobberTime = 0.5f;

	//const FName MenuItemFont = "MenuItem.Font";
	//const FName MenuBindingFont = "MenuItem.BindingFont";

	//const FName EditableTextFont = "MenuItem.Font";

	/** Minimum widget of an editable text box within a multi-box */
	const float EditableTextMinWidth = 30.0f;
}

/**
 * MultiBlock (abstract).  Wraps a "block" of useful UI functionality that can be added to a MultiBox.
 */
class SLATE_API FMultiBlock
	: public TSharedFromThis< FMultiBlock >		// Enables this->AsShared()
{

public:

	/**
	 * Constructor
 	 *
	 * @param InCommand		The command info that describes what action to take when this block is activated
	 * @param InCommandList	The list of mappings from command info to delegates so we can find the delegates to process for the provided action
	 */
	FMultiBlock( const TSharedPtr< const FUICommandInfo > InCommand, TSharedPtr< const FUICommandList > InCommandList, FName InExtensionHook = NAME_None )
		: Action( InCommand )
		, ActionList( InCommandList )
		, ExtensionHook( InExtensionHook )
		, TutorialHighlightName( NAME_None )
	{
	}

	/**
	 * Constructor
	 *
	 * @InAction UI action delegates that should be used in place of UI commands (dynamic menu items)
	 */
	FMultiBlock( const FUIAction& InAction,  FName InExtensionHook = NAME_None )
		: DirectActions( InAction )
		, ExtensionHook( InExtensionHook )
		, TutorialHighlightName( NAME_None )
	{
	}

	virtual ~FMultiBlock()
	{
	}

	/**
	 * Returns the action list associated with this block
	 * 
	 * @return The action list or null if one does not exist for this block
	 */
	TSharedPtr< const FUICommandList > GetActionList() const { return ActionList; }

	/**
	 * Returns the action associated with this block
	 * 
	 * @return The action for this block or null if one does not exist
	 */
	TSharedPtr< const FUICommandInfo> GetAction() const { return Action; }

	/**
	 * Returns the direct actions for this block.  Delegates may be unbound if this block has a UICommand
	 *
	 * @return DirectActions for this block
	 */
	const FUIAction& GetDirectActions() const { return DirectActions; }

	/** Creates a menu entry that is representative of this block */
	virtual void CreateMenuEntry(class FMenuBuilder& MenuBuilder) const { }

	/** Group blocks interface */
	virtual bool IsGroupStartBlock()	const	{ return false; };
	virtual bool IsGroupEndBlock()		const	{ return false; };

	/** Set the tutorial highlight name for this menu entry */
	void SetTutorialHightlightName(FName InTutorialName)
	{
		TutorialHighlightName = InTutorialName;
	}

	/** Get the tutorial highlight name for this menu entry */
	FName GetTutorialHighlightName() const
	{
		return TutorialHighlightName;
	}

	/**
	 * Creates a MultiBlock widget for this MultiBlock
	 *
	 * @param	InOwnerMultiBoxWidget	The widget that will own the new MultiBlock widget
	 * @param	InLocation				The location information for the MultiBlock widget
	 *
	 * @return  MultiBlock widget object
	 */
	TSharedRef< class IMultiBlockBaseWidget > MakeWidget( TSharedRef< class SMultiBoxWidget > InOwnerMultiBoxWidget, EMultiBlockLocation::Type InLocation ) const;

private:
	/**
	 * Allocates a widget for this type of MultiBlock.  Override this in derived classes.
	 *
	 * @return  MultiBlock widget object
	 */
	virtual TSharedRef< class IMultiBlockBaseWidget > ConstructWidget() const = 0;
	
	/** Gets the extension hook so users can see what hooks are where */
	FName GetExtensionHook() const {return ExtensionHook;}

private:

	// We're friends with SMultiBoxWidget so that it can call MakeWidget() directly
	friend class SMultiBoxWidget;	

	/** Direct processing of actions. Will use these actions if there is not UICommand associated with this block that handles actions*/
	FUIAction DirectActions;

	/** The action associated with this block (can be null for some actions) */
	const TSharedPtr< const FUICommandInfo > Action;

	/** The list of mappings from command info to delegates that should be called. This is here for quick access. Can be null for some widgets*/
	const TSharedPtr< const FUICommandList > ActionList;

	/** Optional extension hook which is used for debug display purposes, so users can see what hooks are where */
	FName ExtensionHook;

	/** Name to identify a widget for tutorials */
	FName TutorialHighlightName;
};




/**
 * MultiBox.  Contains a list of MultiBlocks that provide various functionality.
 */
class SLATE_API FMultiBox
	: public TSharedFromThis< FMultiBox >		// Enables this->AsShared()
{

public:
	virtual ~FMultiBox();

	/**
	 * Creates a new multibox instance
	 */
	static TSharedRef<FMultiBox> Create( const EMultiBoxType::Type InType,  FMultiBoxCustomization InCustomization, const bool bInShouldCloseWindowAfterMenuSelection );


	/**
	 * Gets the type of this MultiBox
	 *
	 * @return	The MultiBox's type
	 */
	const EMultiBoxType::Type GetType() const
	{
		return Type;
	}


	/**
	 * Gets whether or not the window that contains this multibox should be destroyed after the user clicks on a menu item in this box
	 *
	 * @return	True if window should be closed automatically when the user clicks on a menu item, otherwise false
	 */
	bool ShouldCloseWindowAfterMenuSelection() const
	{
		return bShouldCloseWindowAfterMenuSelection;
	}



	/**
	 * Adds a MultiBlock to this MultiBox, to the end of the list
	 */
	void AddMultiBlock( TSharedRef< const FMultiBlock > InBlock );

	/**
	 * Removes a MultiBlock from the list for user customization
	 */
	void RemoveCustomMultiBlock( TSharedRef< const FMultiBlock> InBlock );

	/**
	 * Inserts a MultiBlock to the list for user customization
	 */
	void InsertCustomMultiBlock( TSharedRef<const FMultiBlock> InBlock, int32 Index );

	/**
	 * Creates a MultiBox widget for this MultiBox
	 *
	 * @return  MultiBox widget object
	 */
	TSharedRef< class SMultiBoxWidget > MakeWidget();


	/**
	 * Access this MultiBox's list of blocks
	 *
	 * @return	Our list of MultiBlocks
	 */
	const TArray< TSharedRef< const FMultiBlock > >& GetBlocks() const
	{
		return Blocks;
	}
	
	/** @return The style set used by the multibox widgets */
	const ISlateStyle* GetStyleSet() const { return StyleSet; }

	/** @return The style name used by the multibox widgets */
	const FName& GetStyleName() const { return StyleName; }

	/** Sets the style to use on the multibox widgets */
	void SetStyle( const ISlateStyle* InStyleSet, const FName& InStyleName )
	{
		StyleSet  = InStyleSet;
		StyleName = InStyleName;
	}

	/** @return The customization name for this box */
	FName GetCustomizationName() const;

	/**
	 * Creates a block from the provided command that is compatible with this box 
	 *
	 * @param Command	The UI command to create the block from
	 * @return The created multi block.  If null, this command could not be placed in this block
	 */
	TSharedPtr<FMultiBlock> MakeMultiBlockFromCommand( TSharedPtr<const FUICommandInfo> Command, bool bCommandMustBeBound ) const;

	/**
	 * Finds an existing block that handles the provided command
	 *
	 * @param Command The command to check for
	 */
	TSharedPtr<const FMultiBlock> FindBlockFromCommand( TSharedPtr<const FUICommandInfo> Command ) const;

	bool IsInEditMode() const { return FMultiBoxSettings::IsInToolbarEditMode() && IsCustomizable(); }
private:
	
	/**
	 * Constructor
	 *
	 * @param	InType	Type of MultiBox
	 * @param	bInShouldCloseWindowAfterMenuSelection	Sets whether or not the window that contains this multibox should be destroyed after the user clicks on a menu item in this box
	 */
	FMultiBox( const EMultiBoxType::Type InType,  FMultiBoxCustomization InCustomization, const bool bInShouldCloseWindowAfterMenuSelection );

	/**
	 * @return true if this box can be customized by a user
	 */
	bool IsCustomizable() const;

	/**
	 * Applies user customized blocks to the multibox
	 */
	void ApplyCustomizedBlocks();
private:

	/** Saved customization data */
	TSharedRef< class FMultiBoxCustomizationData > CustomizationData;

	/** All command lists in this box */
	TArray< TSharedPtr<const FUICommandList> > CommandLists;

	/** Ordered list of blocks */
	TArray< TSharedRef< const FMultiBlock > > Blocks;

	/** The style set to use with the widgets in the MultiBox */
	const ISlateStyle* StyleSet;

	/** The style name to use with the widgets in the MultiBox */
	FName StyleName;

	/** Type of MultiBox */
	EMultiBoxType::Type Type;

	/** True if window that owns any widgets created from this multibox should be closed automatically after the user commits to a menu choice */
	bool bShouldCloseWindowAfterMenuSelection;
};



/**
 * MultiBlock Slate widget interface
 */
class SLATE_API IMultiBlockBaseWidget
{

public:

	/**
	 * Interprets this object as a SWidget
	 *
	 * @return  Widget reference
	 */
	virtual TSharedRef< SWidget > AsWidget() = 0;


	/**
	 * Interprets this object as a SWidget
	 *
	 * @return  Widget reference
	 */
	virtual TSharedRef< const SWidget > AsWidget() const = 0;


	/**
	 * Associates the owner MultiBox widget with this widget
	 *
	 * @param	InOwnerMultiBoxWidget		The MultiBox widget that owns us
	 */
	virtual void SetOwnerMultiBoxWidget( TSharedRef< SMultiBoxWidget > InOwnerMultiBoxWidget ) = 0;

	
	/**
	 * Associates this widget with a MultiBlock
	 *
	 * @param	InMultiBlock	The MultiBlock we'll be associated with
	 */
	virtual void SetMultiBlock( TSharedRef< const FMultiBlock > InMultiBlock ) = 0;


	/**
	 * Builds this MultiBlock widget up from the MultiBlock associated with it
	 *
	 * @param	StyleSet	The Slate style to use to build the widget
	 * @param	StyleName	The style name to use from the StyleSet
	 */
	virtual void BuildMultiBlockWidget(const ISlateStyle* StyleSet, const FName& StyleName) = 0;

	/**
	 * Sets the blocks location relative to the other blocks
	 * 
	 * @param InLocation	The MultiBlocks location
	 */
	virtual void SetMultiBlockLocation(EMultiBlockLocation::Type InLocation) = 0;

	/**
	 * Returns this MultiBlocks location
	 */
	virtual EMultiBlockLocation::Type GetMultiBlockLocation() = 0;

};



/**
 * MultiBlock Slate base widget (pure virtual).  You'll derive your own MultiBlock class from this base class.
 */
class SLATE_API SMultiBlockBaseWidget
	: public IMultiBlockBaseWidget,
	  public SCompoundWidget
{

public:

	/**
	 * Interprets this object as a SWidget
	 *
	 * @return  Widget reference
	 */
	virtual TSharedRef< SWidget > AsWidget()
	{
		return this->AsShared();
	}


	/**
	 * Interprets this object as a SWidget
	 *
	 * @return  Widget reference
	 */
	virtual TSharedRef< const SWidget > AsWidget() const
	{
		return this->AsShared();
	}


	/**
	 * Associates the owner MultiBox widget with this widget
	 *
	 * @param	InOwnerMultiBoxWidget		The MultiBox widget that owns us
	 */
	void SetOwnerMultiBoxWidget( TSharedRef< SMultiBoxWidget > InOwnerMultiBoxWidget )
	{
		OwnerMultiBoxWidget = InOwnerMultiBoxWidget;
	}

	
	/**
	 * Associates this widget with a MultiBlock
	 *
	 * @param	InMultiBlock	The MultiBlock we'll be associated with
	 */
	void SetMultiBlock( TSharedRef< const FMultiBlock > InMultiBlock )
	{
		MultiBlock = InMultiBlock;
	}

	/**
	 * Sets the blocks location relative to the other blocks
	 * 
	 * @param InLocation	The MultiBlocks location
	 */
	virtual void SetMultiBlockLocation(EMultiBlockLocation::Type InLocation)
	{
		Location = InLocation;
	}

	/**
	 * Returns this MultiBlocks location
	 */
	virtual EMultiBlockLocation::Type GetMultiBlockLocation()
	{
		return Location;
	}

	/**
	 * Builds this MultiBlock widget up from the MultiBlock associated with it
	 */
	virtual void BuildMultiBlockWidget(const ISlateStyle* StyleSet, const FName& StyleName) = 0;

	/** SWidget Interface */
	virtual void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) OVERRIDE;
	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) OVERRIDE;
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) OVERRIDE;
protected:

	/** Weak reference back to the MultiBox widget that owns us */
	TWeakPtr< SMultiBoxWidget > OwnerMultiBoxWidget;

	/** The MultiBlock we're associated with */
	TSharedPtr< const FMultiBlock > MultiBlock;

	/** The MultiBlocks location relative to the other blocks in the set */
	EMultiBlockLocation::Type Location;
};

/**
 * MultiBox Slate widget
 */
class SLATE_API SMultiBoxWidget
	: public SCompoundWidget
{

public:

	SLATE_BEGIN_ARGS( SMultiBoxWidget )
		: _ContentScale( FVector2D::UnitVector )
		{}

		/** Content scaling factor */
		SLATE_ATTRIBUTE( FVector2D, ContentScale )

	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	/**
	 * Associates this widget with a MultiBox
	 *
	 * @param	InMultiBox	The MultiBox we'll be associated with
	 */
	void SetMultiBox( TSharedRef< FMultiBox > InMultiBox )
	{
		MultiBox = InMultiBox;
	}


	/**
	 * Access the MultiBox associated with this widget
	 */
	TSharedRef< const FMultiBox > GetMultiBox() const
	{
		return MultiBox.ToSharedRef();
	}

	
	/**
	 * Builds this MultiBox widget up from the MultiBox associated with it
	 */
	void BuildMultiBoxWidget();

	
	/**
	 * For menu bar multibox widgets, tells the multibox widget about a currently active pull-down menu
	 *
	 * @param	InMenuAnchor	Menu anchor for active pull-down menu or sub-menu
	 */
	void SetSummonedMenu( TSharedRef< SMenuAnchor > InMenuAnchor );
	

	/**
	 * For menu bar or sub-menu multibox widgets, returns the currently open menu, if there is one open
	 *
	 * @return	Menu anchor, or null pointer
	 */
	TSharedPtr< const SMenuAnchor > GetOpenMenu() const;

	/**
	 * For menu bar multibox widget, closes any open pull-down or sub menus
	 */
	void CloseSummonedMenus();
	

	/** Generates the tiles for an STileView for button rows */
	TSharedRef<ITableRow> GenerateTiles(TSharedPtr<SWidget> Item, const TSharedRef<STableViewBase>& OwnerTable);

	/** Gets the maximum item width and height of the consituent widgets */
	float GetItemWidth() const;
	float GetItemHeight() const;

	/** Event handler for clicking the wrap button */
	TSharedRef<SWidget> OnWrapButtonClicked();

	const ISlateStyle* GetStyleSet() const { return MultiBox->GetStyleSet(); }
	const FName& GetStyleName() const { return MultiBox->GetStyleName(); }

	/**
	 * Called when a user drags a UI command into a multiblock in this widget
	 */
	void OnCustomCommandDragEnter( TSharedRef<const FMultiBlock> MultiBlock, const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent );

	/**
	 * Called when a user drags a UI command within multiblock in this widget
	 */
	void OnCustomCommandDragged( TSharedRef<const FMultiBlock> MultiBlock, const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent );

	/**
	 * Called when a user drops a UI command in this widget
	 */
	void OnCustomCommandDropped();
	
	/**
	 * Called after a drag was initiated from this box but was dropped elsewhere
	 */
	void OnDropExternal();

	/** Helper function used to transfer focus to the next/previous widget */
	static FReply FocusNextWidget( EFocusMoveDirection::Type MoveDirection );

	/** SWidget interface */
	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) OVERRIDE;
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) OVERRIDE;
	virtual bool SupportsKeyboardFocus() const OVERRIDE;
	virtual FReply OnKeyboardFocusReceived( const FGeometry& MyGeometry, const FKeyboardFocusEvent& InKeyboardFocusEvent ) OVERRIDE;
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyboardEvent& KeyboardEvent ) OVERRIDE;
private:
	/** Adds a block Widget to this widget */
	void AddBlockWidget( const FMultiBlock& Block, TSharedPtr<SHorizontalBox> HorizontalBox, TSharedPtr<SVerticalBox> VerticalBox, EMultiBlockLocation::Type InLocation );

	/**
	 * @return True if the passed in block is being dragged
	 */
	bool IsBlockBeingDragged( TSharedPtr<const FMultiBlock> Block ) const;

	/**
	 * Updates the preview block being dragged.  The drag area is where the users dragged block will be dropped
	 */
	void UpdateDropAreaPreviewBlock( TSharedRef<const FMultiBlock> MultiBlock, TSharedPtr<FUICommandDragDropOp> DragDropContent, const FGeometry& DragArea, const FVector2D& DragPos );

	/**
   	 * @return The visibility of customization widgets for a block
	 */
	EVisibility GetCustomizationVisibility( TWeakPtr<const FMultiBlock> BlockWeakPtr, TWeakPtr<SWidget> BlockWidgetWeakPtr ) const;
	
	/** Called when a user clicks the delete button on a block */
	FReply OnDeleteBlockClicked( TWeakPtr<const FMultiBlock> BlockWeakPtr );
private:
	/** A preview of a block being dragged */
	struct FDraggedMultiBlockPreview
	{
		/** Command being dragged */
		TSharedPtr<const FUICommandInfo> UICommand;
		/** Preview block for the command */
		TSharedPtr<class FDropPreviewBlock> PreviewBlock;
		/** Index into the block list where the block will be added*/
		int32 InsertIndex;
		// Vertical for menus and vertical toolbars, horizontally otherwise
		EOrientation InsertOrientation;

		FDraggedMultiBlockPreview()
			: InsertIndex( INDEX_NONE )
		{}

		void Reset()
		{
			UICommand.Reset();
			PreviewBlock.Reset();
			InsertIndex = INDEX_NONE;
		}

		bool IsValid() const { return UICommand.IsValid() && PreviewBlock.IsValid() && InsertIndex != INDEX_NONE; }
	};
	
	/** The MultiBox we're associated with */
	TSharedPtr< FMultiBox > MultiBox;

	/** For menu bar multibox widgets, this stores a weak reference to the last pull-down or sub-menu that was summoned. */
	TWeakPtr< SMenuAnchor > SummonedMenuAnchor;

	/** An array of widgets used for an STileView if used */
	TArray< TSharedPtr<SWidget> > TileViewWidgets;

	/** Specialized box widget to handle clipping of toolbars and menubars */
	TSharedPtr<class SClippingHorizontalBox> ClippedHorizontalBox;

	/** A preview of a block being dragged inside this box */
	FDraggedMultiBlockPreview DragPreview;
};
