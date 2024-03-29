// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

/*-----------------------------------------------------------------------------
	Type definitions
-----------------------------------------------------------------------------*/

/** Type definition for shared pointers to instances of FEventGraphColumn. */
typedef TSharedPtr<class FEventGraphColumn> FEventGraphColumnPtr;

/** Type definition for shared references to instances of FEventGraphColumn. */
typedef TSharedRef<class FEventGraphColumn> FEventGraphColumnRef;

/*-----------------------------------------------------------------------------
	Enumerators
-----------------------------------------------------------------------------*/

/** Enumerates event graph view modes. */
namespace EEventGraphViewModes
{
	enum Type
	{
		/** Hierarchical list of the events. */
		Hierarchical,

		/** Flat list of the events based on the inclusive time, sorted by the inclusive time. */
		FlatInclusive,

		/** Flat list of the events based on the inclusive time coalesced by the event name, sorted by the inclusive time. */
		FlatInclusiveCoalesced,

		/** Flat list of the events based on the exclusive time, sorted by the exclusive time. */
		FlatExclusive,

		/** Flat list of the events based on the exclusive time coalesced by the event name, sorted by the exclusive time. */
		FlatExclusiveCoalesced,
		
		/** For the specified class shows an aggregated hierarchy @TBD. */
		ClassAggregate,

		/** Invalid enum type, may be used as a number of enumerations. */
		InvalidOrMax,
	};

	/**
	 * @param EventGraphViewMode - The value to get the string for.
	 *
	 * @return string representation of the specified EEventGraphViewModes value.
	 */
	FText ToName( const EEventGraphViewModes::Type EventGraphViewMode );

	/**
	 * @param EventGraphViewMode - The value to get the string for.
	 *
	 * @return string representation with more detailed explanation of the specified EEventGraphViewModes value.
	 */
	FText ToDescription( const EEventGraphViewModes::Type EventGraphViewMode );

	FName ToBrushName( const Type EventGraphViewMode );
}

// @TODO: Sort properties by size.

/** Holds information about a column in the event graph widget. */
class FEventGraphColumn /*: public FNoncopyable*/
{
public:

	FEventGraphColumn
	(
		const EEventPropertyIndex::Type InIndex,
		const FName InSearchID,
		const FString InShortName,
		const FString InDescription,
		const bool bInCanBeHidden,
		const bool bInIsVisible,
		const bool bInCanBeSorted,
		const bool bInCanBeFiltered,
		const bool bInCanBeCulled,
		const EHorizontalAlignment InHorizontalAlignment,
		const float InFixedColumnWidth
	)
		: Index( InIndex )
		, ID( FEventGraphSample::GetEventPropertyByIndex(InIndex).Name )
		, SearchID( InSearchID )
		, ShortName( InShortName )
		, Description( InDescription )
		, bCanBeHidden( bInCanBeHidden )
		, bIsVisible( bInIsVisible )
		, bCanBeSorted( bInCanBeSorted )
		, bCanBeFiltered( bInCanBeFiltered )
		, bCanBeCulled( bInCanBeCulled )
		, HorizontalAlignment( InHorizontalAlignment )
		, FixedColumnWidth( InFixedColumnWidth )
	{}

public:
	/** Index of the event's property, also means the index of the column @see EEventPropertyIndex. */
	EEventPropertyIndex::Type Index;

	/** Name of the column, name of the property. */
	FName ID;

	/** Name of the column used by the searching system. */
	FName SearchID;

	/** Short name of the column, displayed in the event graph header. */
	FString ShortName;

	/** Long name of the column, displayed in the column tooltip. */
	FString Description;

	/** Whether this column can be hidden. */
	bool bCanBeHidden;

	/** Is this column visible?. */
	bool bIsVisible;

	/** Whether this column cab be used for sorting. */
	bool bCanBeSorted;

	/** Where this column can be used to filtering displayed results. */
	bool bCanBeFiltered;

	/** Where this column can be used to culling displayed results. */
	bool bCanBeCulled;

	/** Horizontal alignment of the content in this column. */
	EHorizontalAlignment HorizontalAlignment;

	/** If greater than 0.0f, this column has fixed width and cannot be resized. */
	float FixedColumnWidth;
};

/*-----------------------------------------------------------------------------
	Declarations
-----------------------------------------------------------------------------*/

/** Interface for the event graph. */
class IEventGraph
{
public:
	virtual void ExpandCulledEvents( FEventGraphSamplePtr EventPtr ) = 0;
};

/** A custom event graph widget used to visualize profiling data. */
class SEventGraph : public SCompoundWidget, public IEventGraph
{
	struct ESelectedEventTypes
	{
		enum Type
		{
			AllEvents,
			SelectedEvents,
			SelectedThreadEvents,
		};
	};

public:
	SLATE_BEGIN_ARGS( SEventGraph ){}
	SLATE_END_ARGS()

	/** Default constructor. */
	SEventGraph();

	/** Destructor. */
	~SEventGraph();

	/**
	 * Construct this widget.
	 *
	 * @param	InArgs		- the declaration data for this widget.
	 */
	void Construct( const FArguments& InArgs );

protected:
	virtual void ExpandCulledEvents( FEventGraphSamplePtr EventPtr );

protected:
	/*-----------------------------------------------------------------------------
		Helper for constructing sub-widgets of this event graph widget.
	-----------------------------------------------------------------------------*/
	TSharedRef<SWidget> GetWidgetForEventGraphTypes();
	TSharedRef<SWidget> GetWidgetForEventGraphViewModes();
	TSharedRef<SWidget> GetWidgetBoxForOptions();
	TSharedRef<SWidget> GetToggleButtonForEventGraphType( const EEventGraphTypes::Type EventGraphType, const FName BrushName );
	TSharedRef<SWidget> GetToggleButtonForEventGraphViewMode( const EEventGraphViewModes::Type EventGraphViewMode );

	/*-----------------------------------------------------------------------------
		Settings
	-----------------------------------------------------------------------------*/

	EVisibility EventGraphViewMode_GetVisibility( const EEventGraphViewModes::Type ViewMode ) const;

	/*-----------------------------------------------------------------------------
		Events
	-----------------------------------------------------------------------------*/

public:
	/**
	 * The event to execute when the event graph state has been restored from the history.
	 *
	 * @param FrameStartIndex	- <describe FrameStartIndex>
	 * @param FrameEndIndex		- <describe FrameEndIndex>
	 *
	 */
	DECLARE_EVENT_TwoParams( SEventGraph, FEventGraphRestoredFromHistoryEvent, uint32 /*FrameStartIndex*/, uint32 /*FrameEndIndex*/ );
	FEventGraphRestoredFromHistoryEvent& OnEventGraphRestoredFromHistory()
	{
		return EventGraphRestoredFromHistoryEvent;
	}

protected:
	void ProfilerManager_OnViewModeChanged( EProfilerViewMode::Type NewViewMode );
	
protected:
	/** The event to execute when the event graph has been restored from the history. */
	FEventGraphRestoredFromHistoryEvent EventGraphRestoredFromHistoryEvent;

	/** Called by STreeView to generate a table row for the specified item. */
	TSharedRef< ITableRow > EventGraph_OnGenerateRow( FEventGraphSamplePtr EventPtr, const TSharedRef< STableViewBase >& OwnerTable );

	/*
	 * Called by STreeView to retrieves the children for the specified parent item.
	 * @param InParent		- The parent node to retrieve the children from.
	 * @param out_Children	- List of children for the parent node.
	 */
	void EventGraph_OnGetChildren( FEventGraphSamplePtr InParent, TArray< FEventGraphSamplePtr >& out_Children );

	void EventGraph_OnSelectionChanged( FEventGraphSamplePtr SelectedItem, ESelectInfo::Type SelectInfo );
	void EventGraph_OnExpansionChanged( FEventGraphSamplePtr SelectedItem, bool bIsExpanded );

	void TreeView_SetItemsExpansion_Recurrent( TArray<FEventGraphSamplePtr>& InEventGraphSamples, const bool bShouldExpand );

	void SetHierarchicalSelectedEvents( const TArray<FEventGraphSamplePtr>& HierarchicalSelectedEvents );
	void GetHierarchicalSelectedEvents( TArray< FEventGraphSamplePtr >& out_HierarchicalSelectedEvents, const TArray<FEventGraphSamplePtr>* SelectedEvents = NULL ) const;

	void SetHierarchicalExpandedEvents( const TSet<FEventGraphSamplePtr>& HierarchicalExpandedEvents );
	void GetHierarchicalExpandedEvents( TSet<FEventGraphSamplePtr>& out_HierarchicalExpandedEvents ) const;
	
	void ShowEventsInViewMode( const TArray<FEventGraphSamplePtr>& EventsToSynchronize, const EEventGraphViewModes::Type NewViewMode );
	void ScrollToTheSlowestSelectedEvent( EEventPropertyIndex::Type ColumnIndex );

	void CreateEvents();
	void SortEvents();
	void TreeView_Refresh();
	void SetTreeItemsForViewMode( const EEventGraphViewModes::Type NewViewMode, EEventGraphTypes::Type NewEventGraphType );

	void EventGraphViewMode_OnCheckStateChanged( ESlateCheckBoxState::Type NewRadioState, const EEventGraphViewModes::Type InViewMode );
	ESlateCheckBoxState::Type EventGraphViewMode_IsChecked( const EEventGraphViewModes::Type InViewMode ) const;

	void EventGraphType_OnCheckStateChanged( ESlateCheckBoxState::Type NewRadioState, const EEventGraphTypes::Type NewEventGraphType );
	ESlateCheckBoxState::Type EventGraphType_IsChecked( const EEventGraphTypes::Type InEventGraphType ) const;
	bool EventGraphType_IsEnabled( const EEventGraphTypes::Type InEventGraphType ) const;

	void FilteringSearchBox_OnTextChanged( const FText& InFilterText );
	bool FilteringSearchBox_IsEnabled() const;

	TSharedPtr<SWidget> EventGraph_GetMenuContent() const;
	void EventGraph_BuildSortByMenu( FMenuBuilder& MenuBuilder );
	void EventGraph_BuildViewColumnMenu( FMenuBuilder& MenuBuilder );

	FReply ExpandHotPath_OnClicked();
	void HighlightHotPath_OnCheckStateChanged( ESlateCheckBoxState::Type InState );

	void InitializeAndShowHeaderColumns();
	void TreeViewHeaderRow_OnSortModeChanged( const FName& ColumnID, EColumnSortMode::Type );

	void SetSortModeForColumn( const FName& ColumnID, EColumnSortMode::Type SortMode );

	EColumnSortMode::Type TreeViewHeaderRow_GetSortModeForColumn( const FName ColumnID ) const;
	TSharedRef<SWidget> TreeViewHeaderRow_GenerateColumnMenu( const FEventGraphColumn& Column );

	bool EventGraphTableRow_IsColumnVisible( const FName ColumnID ) const;
	void EventGraphTableRow_SetHoveredTableCell( const FName ColumnID, const FEventGraphSamplePtr EventPtr );
	FName EventGraphRow_GetHighlightedEventName() const;
	EHorizontalAlignment EventGraphRow_GetColumnOutlineHAlignment( const FName ColumnID ) const;

	void TreeViewHeaderRow_ShowColumn( const FName ColumnID );
	void TreeViewHeaderRow_CreateColumnArgs( const uint32 ColumnIndex );

	/**	Binds our UI commands to delegates. */
	void BindCommands();

	/*-----------------------------------------------------------------------------
		SelectAllFrames
	-----------------------------------------------------------------------------*/
	
public:
	/** Maps UI command info SelectAllFrames with the specified UI command list. */
	void Map_SelectAllFrames_Global();
	
	/** Add comment here */
	const FUIAction SelectAllFrames_Custom() const
	{
		FUIAction UIAction;
		UIAction.ExecuteAction = FExecuteAction::CreateSP( this, &SEventGraph::SelectAllFrames_Execute );
		UIAction.CanExecuteAction = FCanExecuteAction::CreateSP( this, &SEventGraph::SelectAllFrames_CanExecute );
		UIAction.IsCheckedDelegate = FIsActionChecked();
		UIAction.IsActionVisibleDelegate = FIsActionButtonVisible();
		return UIAction;
	}
		
protected:
	/** Handles FExecuteAction for SelectAllFrames. */
	void SelectAllFrames_Execute();
	/** Handles FCanExecuteAction for SelectAllFrames. */
	bool SelectAllFrames_CanExecute() const;

	// Dummy implementation, used to verify that all actions have been properly implemented.
	void ContextMenu_ExecuteDummy( const FName ActionName );
	bool ContextMenu_CanExecuteDummy( const FName ActionName ) const;
	bool ContextMenu_IsCheckedDummy( const FName ActionName ) const;


	// Action_ExpandHotPath
	void ContextMenu_ExpandHotPath_Execute();
	bool ContextMenu_ExpandHotPath_CanExecute() const;

	// Action_CopySelectedToClipboard
	void ContextMenu_CopySelectedToClipboard_Execute();
	bool ContextMenu_CopySelectedToClipboard_CanExecute() const;

	// Action_SelectStack
	void ContextMenu_SelectStack_Execute();
	bool ContextMenu_SelectStack_CanExecute() const;

	// Action_SortByColumn
	void ContextMenu_SortByColumn_Execute( const FName ColumnID );
	bool ContextMenu_SortByColumn_CanExecute( const FName ColumnID ) const;
	bool ContextMenu_SortByColumn_IsChecked( const FName ColumnID );

	// Action_ToggleColumn
	void ContextMenu_ToggleColumn_Execute( const FName ColumnID );
	bool ContextMenu_ToggleColumn_CanExecute( const FName ColumnID ) const;
	bool ContextMenu_ToggleColumn_IsChecked( const FName ColumnID );

	// Action_SortMode
	void ContextMenu_SortMode_Execute( const EColumnSortMode::Type InSortMode );
	bool ContextMenu_SortMode_CanExecute( const EColumnSortMode::Type InSortMode ) const;
	bool ContextMenu_SortMode_IsChecked( const EColumnSortMode::Type InSortMode );

	// Action_ResetColumns
	void ContextMenu_ResetColumns_Execute();
	bool ContextMenu_ResetColumns_CanExecute() const;

	// Header Action_SortMode
	void HeaderMenu_SortMode_Execute( const FName ColumnID, const EColumnSortMode::Type InSortMode );
	bool HeaderMenu_SortMode_CanExecute( const FName ColumnID, const EColumnSortMode::Type InSortMode ) const;
	bool HeaderMenu_SortMode_IsChecked( const FName ColumnID, const EColumnSortMode::Type InSortMode );

	// Header Action_HideColumn
	void HeaderMenu_HideColumn_Execute( const FName ColumnID );
	bool HeaderMenu_HideColumn_CanExecute( const FName ColumnID ) const;

	/*-----------------------------------------------------------------------------
		SetRoot
	-----------------------------------------------------------------------------*/
	
public:
	/** Add comment here */
	const FUIAction SetRoot_Custom() const
	{
		FUIAction UIAction;
		UIAction.ExecuteAction = FExecuteAction::CreateSP( this, &SEventGraph::SetRoot_Execute );
		UIAction.CanExecuteAction = FCanExecuteAction::CreateSP( this, &SEventGraph::SetRoot_CanExecute );
		UIAction.IsCheckedDelegate = FIsActionChecked();
		UIAction.IsActionVisibleDelegate = FIsActionButtonVisible();
		return UIAction;
	}
		
protected:
	/** Handles FExecuteAction for SetRoot. */
	void SetRoot_Execute();
	/** Handles FCanExecuteAction for SetRoot. */
	bool SetRoot_CanExecute() const;

	/*-----------------------------------------------------------------------------
		ClearHistory
	-----------------------------------------------------------------------------*/
	
public:

	/** Add comment here */
	const FUIAction ClearHistory_Custom() const
	{
		FUIAction UIAction;
		UIAction.ExecuteAction = FExecuteAction::CreateSP( this, &SEventGraph::ClearHistory_Execute );
		UIAction.CanExecuteAction = FCanExecuteAction::CreateSP( this, &SEventGraph::ClearHistory_CanExecute );
		UIAction.IsCheckedDelegate = FIsActionChecked();
		UIAction.IsActionVisibleDelegate = FIsActionButtonVisible();
		return UIAction;
	}
		
protected:
	/** Handles FExecuteAction for ClearHistory. */
	void ClearHistory_Execute();
	/** Handles FCanExecuteAction for ClearHistory. */
	bool ClearHistory_CanExecute() const;

	/*-----------------------------------------------------------------------------
		ShowSelectedEventsInViewMode
	-----------------------------------------------------------------------------*/
	
public:
	/** Add comment here */
	const FUIAction ShowSelectedEventsInViewMode_Custom( EEventGraphViewModes::Type NewViewMode ) const
	{
		FUIAction UIAction;
		UIAction.ExecuteAction = FExecuteAction::CreateSP( this, &SEventGraph::ShowSelectedEventsInViewMode_Execute, NewViewMode );
		UIAction.CanExecuteAction = FCanExecuteAction::CreateSP( this, &SEventGraph::ShowSelectedEventsInViewMode_CanExecute, NewViewMode );
		UIAction.IsCheckedDelegate = FIsActionChecked::CreateSP( this, &SEventGraph::ShowSelectedEventsInViewMode_IsChecked, NewViewMode );
		UIAction.IsActionVisibleDelegate = FIsActionButtonVisible();
		return UIAction;
	}
		
protected:
	/** Handles FExecuteAction for ShowSelectedEventsInViewMode. */
	void ShowSelectedEventsInViewMode_Execute(EEventGraphViewModes::Type NewViewMode);
	/** Handles FCanExecuteAction for ShowSelectedEventsInViewMode. */
	bool ShowSelectedEventsInViewMode_CanExecute(EEventGraphViewModes::Type NewViewMode) const;
	/** Handles FIsActionChecked for ShowSelectedEventsInViewMode. */
	bool ShowSelectedEventsInViewMode_IsChecked(EEventGraphViewModes::Type NewViewMode) const;

	/*-----------------------------------------------------------------------------
		FilterOutByProperty
	-----------------------------------------------------------------------------*/
	
public:
	/** Add comment here */
	const FUIAction FilterOutByProperty_Custom( const FEventGraphSamplePtr EventPtr, const FName PropertyName, const bool bReset ) const
	{
		FUIAction UIAction;
		UIAction.ExecuteAction = FExecuteAction::CreateSP( this, &SEventGraph::FilterOutByProperty_Execute, EventPtr, PropertyName, bReset );
		UIAction.CanExecuteAction = FCanExecuteAction::CreateSP( this, &SEventGraph::FilterOutByProperty_CanExecute, EventPtr, PropertyName, bReset );
		UIAction.IsActionVisibleDelegate = FIsActionButtonVisible();
		return UIAction;
	}
		
protected:
	/** Handles FExecuteAction for FilterOutByProperty. */
	void FilterOutByProperty_Execute( const FEventGraphSamplePtr EventPtr, const FName PropertyName, const bool bReset );
	/** Handles FCanExecuteAction for FilterOutByProperty. */
	bool FilterOutByProperty_CanExecute( const FEventGraphSamplePtr EventPtr, const FName PropertyName, const bool bReset ) const;

	/*-----------------------------------------------------------------------------
		CullByProperty
	-----------------------------------------------------------------------------*/
	
public:
	
	/** Add comment here */
	const FUIAction CullByProperty_Custom( const FEventGraphSamplePtr EventPtr, const FName PropertyName, const bool bReset ) const
	{
		FUIAction UIAction;
		UIAction.ExecuteAction = FExecuteAction::CreateSP( this, &SEventGraph::CullByProperty_Execute, EventPtr, PropertyName, bReset );
		UIAction.CanExecuteAction = FCanExecuteAction::CreateSP( this, &SEventGraph::CullByProperty_CanExecute, EventPtr, PropertyName, bReset );
		UIAction.IsActionVisibleDelegate = FIsActionButtonVisible();
		return UIAction;
	}
		
protected:
	/** Handles FExecuteAction for CullByProperty. */
	void CullByProperty_Execute( const FEventGraphSamplePtr EventPtr, const FName PropertyName, const bool bReset );
	/** Handles FCanExecuteAction for CullByProperty. */
	bool CullByProperty_CanExecute( const FEventGraphSamplePtr EventPtr, const FName PropertyName, const bool bReset ) const;

	/*-----------------------------------------------------------------------------
		HistoryList_GoTo
	-----------------------------------------------------------------------------*/
	
public:
	/** Add comment here */
	const FUIAction HistoryList_GoTo_Custom( int32 StateIndex ) const
	{
		FUIAction UIAction;
		UIAction.ExecuteAction = FExecuteAction::CreateSP( this, &SEventGraph::HistoryList_GoTo_Execute, StateIndex );
		UIAction.CanExecuteAction = FCanExecuteAction();
		UIAction.IsCheckedDelegate = FIsActionChecked::CreateSP( this, &SEventGraph::HistoryList_GoTo_IsChecked, StateIndex );
		UIAction.IsActionVisibleDelegate = FIsActionButtonVisible();
		return UIAction;
	}
		
protected:
	void HistoryList_GoTo_ExecuteRadioState( ESlateCheckBoxState::Type NewRadioState, int32 StateIndex )
	{
		if( NewRadioState == ESlateCheckBoxState::Checked )
		{
			HistoryList_GoTo_Execute( StateIndex );
		}
	}

	/** Handles FExecuteAction for HistoryList_GoTo. */
	void HistoryList_GoTo_Execute( int32 StateIndex );

	ESlateCheckBoxState::Type HistoryList_GoTo_IsCheckedRadioState( int32 StateIndex ) const
	{
		return HistoryList_GoTo_IsChecked( StateIndex ) ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
	}

	/** Handles FIsActionChecked for HistoryList_GoTo. */
	bool HistoryList_GoTo_IsChecked( int32 StateIndex ) const
	{
		return StateIndex == CurrentStateIndex;
	}

	/*-----------------------------------------------------------------------------
		SetExpansionForEvents
	-----------------------------------------------------------------------------*/
	
public:
	/** Add comment here */
	const FUIAction SetExpansionForEvents_Custom( const ESelectedEventTypes::Type SelectedEventType, bool bShouldExpand ) const
	{
		FUIAction UIAction;
		UIAction.ExecuteAction = FExecuteAction::CreateSP( this, &SEventGraph::SetExpansionForEvents_Execute, SelectedEventType, bShouldExpand );
		UIAction.CanExecuteAction = FCanExecuteAction::CreateSP( this, &SEventGraph::SetExpansionForEvents_CanExecute, SelectedEventType, bShouldExpand );
		UIAction.IsCheckedDelegate = FIsActionChecked();
		UIAction.IsActionVisibleDelegate = FIsActionButtonVisible();
		return UIAction;
	}
		
protected:
	void GetEventsForChangingExpansion( TArray<FEventGraphSamplePtr>& out_Events, const ESelectedEventTypes::Type SelectedEventType );

	/** Handles FExecuteAction for SetExpansionForEvents. */
	void SetExpansionForEvents_Execute( const ESelectedEventTypes::Type SelectedEventType, bool bShouldExpand );
	/** Handles FCanExecuteAction for SetExpansionForEvents. */
	bool SetExpansionForEvents_CanExecute( const ESelectedEventTypes::Type SelectedEventType, bool bShouldExpand ) const;

protected:
	/** All events coalesced by the event name, stored as FName -> FEventGraphSamplePtr. */
	TMultiMap< FName, FEventGraphSamplePtr > HierarchicalToFlatCoalesced;

	/** An array of samples to be displayed in this widget. */
	TArray<FEventGraphSamplePtr> Events_Flat;
	TArray<FEventGraphSamplePtr> Events_FlatCoalesced;

	/** How we sort the event graph?. */
	EColumnSortMode::Type ColumnSortMode;

	/** Name of the column currently being sorted, NAME_None if sorting is disabled. */
	FName ColumnBeingSorted;

	typedef TSharedPtr< STreeView<FEventGraphSamplePtr> > FTreeViewOfEventGraphSamples;
	
	/** Holds the tree view widget which display event graph samples. */
	FTreeViewOfEventGraphSamples TreeView_Base;

	/** External scrollbar used to synchronize tree view position. */
	TSharedPtr<SScrollBar> ExternalScrollbar;

	TSharedPtr<SBox> FunctionDetailsBox;

	/** Holds the tree view header row widget which display all columns in the tree view. */
	TSharedPtr< SHeaderRow > TreeViewHeaderRow;

	/** The search box widget used to filter items displayed in this widget. */
	TSharedPtr< SSearchBox > FilteringSearchBox;

	/** Column metadata used to initialize column arguments, stored as PropertyName -> FEventGraphColumn. */
	TMap< FName, FEventGraphColumn > TreeViewHeaderColumns;

	/** Column arguments used to initialize a new header column in the tree view, stored as column name to column arguments mapping. */
	TMap< FName, SHeaderRow::FColumn::FArguments > TreeViewHeaderColumnArgs;

	/** Name of the column currently being hovered by the mouse. */
	FName HoveredColumnID;

	/** A shared pointer to the event currently being hovered by the mouse. */
	FEventGraphSamplePtr HoveredSamplePtr;

	/*-----------------------------------------------------------------------------
		History management
	-----------------------------------------------------------------------------*/
protected:
	struct EEventHistoryTypes
	{
		enum Type
		{
			NewEventGraph,
			Rooted,
			Culled,
			Filtered,
		};
	};

	class FEventGraphState
	{
		struct FCulledTag{};
		struct FFilteredTag{};

	public:
		/** New event graph state constructor. */
		FEventGraphState( FEventGraphDataRef InAverageEventGraph, FEventGraphDataRef InMaximumEventGraph )
			: AverageEventGraph( InAverageEventGraph )
			, MaximumEventGraph( InMaximumEventGraph )
			, FakeRoot( FEventGraphSample::CreateNamedEvent( FEventGraphConsts::FakeRoot ) )
			, CullPropertyName( NAME_None )
			, CullEventPtr( nullptr )
			, FilterPropertyName( NAME_None )
			, FilterEventPtr( nullptr )
			, CreationTime( FPlatformTime::Seconds() )
			, HistoryType( EEventHistoryTypes::NewEventGraph )
			, ViewMode( EEventGraphViewModes::Hierarchical )
			, EventGraphType( InAverageEventGraph->GetNumFrames() == 1 ? EEventGraphTypes::OneFrame : EEventGraphTypes::Average )
		{
			CreateOneToOneMapping();
		}

		FEventGraphState* CreateCopyWithNewRoot( const TArray<FEventGraphSamplePtr>& UniqueLeafs )
		{
			return new FEventGraphState( *this, UniqueLeafs );
		}

		FEventGraphState* CreateCopyWithCulling( const FName InCullPropertyName, const FEventGraphSamplePtr InCullEventPtr )
		{
			return new FEventGraphState( *this, InCullPropertyName, InCullEventPtr, FCulledTag() );
		}

		FEventGraphState* CreateCopyWithFiltering( const FName InFilterPropertyName, const FEventGraphSamplePtr InFilterEventPtr )
		{
			return new FEventGraphState( *this, InFilterPropertyName, InFilterEventPtr, FFilteredTag() );
		}

	protected:
		/** Copy constructor for setting new root. */
		FEventGraphState( const FEventGraphState& EventGraphState, const TArray<FEventGraphSamplePtr>& UniqueLeafs )
			: AverageEventGraph( EventGraphState.AverageEventGraph )
			, MaximumEventGraph( EventGraphState.MaximumEventGraph )
			, AverageToMaximumMapping( EventGraphState.AverageToMaximumMapping )
			, MaximumToAverageMapping( EventGraphState.MaximumToAverageMapping )
			, ExpandedEvents( EventGraphState.ExpandedEvents )
			, SelectedEvents( EventGraphState.SelectedEvents )
			, FakeRoot( FEventGraphSample::CreateNamedEvent( FEventGraphConsts::FakeRoot ) )
			, CullPropertyName( EventGraphState.CullPropertyName )
			, CullEventPtr( EventGraphState.CullEventPtr )
			, ExpandedCulledEvents( EventGraphState.ExpandedCulledEvents )
			, FilterPropertyName( EventGraphState.FilterPropertyName )
			, FilterEventPtr( EventGraphState.FilterEventPtr )
			, CreationTime( FPlatformTime::Seconds() )
			, HistoryType( EEventHistoryTypes::Rooted )
			, ViewMode( EventGraphState.ViewMode )
			, EventGraphType( EventGraphState.EventGraphType )
		{
			// Set new root.
			SetNewRoot( UniqueLeafs );
		}

		/** Copy constructor for culling. */
		FEventGraphState( const FEventGraphState& EventGraphState, const FName InCullPropertyName, const FEventGraphSamplePtr InCullEventPtr, FCulledTag )
			: AverageEventGraph( EventGraphState.AverageEventGraph )
			, MaximumEventGraph( EventGraphState.MaximumEventGraph )
			, AverageToMaximumMapping( EventGraphState.AverageToMaximumMapping )
			, MaximumToAverageMapping( EventGraphState.MaximumToAverageMapping )
			, ExpandedEvents( EventGraphState.ExpandedEvents )
			, SelectedEvents( EventGraphState.SelectedEvents )
			, FakeRoot( FEventGraphSample::CreateNamedEvent( FEventGraphConsts::FakeRoot ) )
			, CullPropertyName( InCullPropertyName )
			, CullEventPtr( InCullEventPtr )
			, ExpandedCulledEvents()
			, FilterPropertyName( EventGraphState.FilterPropertyName )
			, FilterEventPtr( EventGraphState.FilterEventPtr )
			, CreationTime( FPlatformTime::Seconds() )
			, HistoryType( EEventHistoryTypes::Culled )
			, ViewMode( EventGraphState.ViewMode )
			, EventGraphType( EventGraphState.EventGraphType )
		{
			// Copy fake root.
			SetNewRoot( EventGraphState.FakeRoot->GetChildren() );
		}

		/** Copy constructor for filtering. */
		FEventGraphState( const FEventGraphState& EventGraphState, const FName InFilterPropertyName, const FEventGraphSamplePtr InFilterEventPtr, FFilteredTag )
			: AverageEventGraph( EventGraphState.AverageEventGraph )
			, MaximumEventGraph( EventGraphState.MaximumEventGraph )
			, AverageToMaximumMapping( EventGraphState.AverageToMaximumMapping )
			, MaximumToAverageMapping( EventGraphState.MaximumToAverageMapping )
			, ExpandedEvents( EventGraphState.ExpandedEvents )
			, SelectedEvents( EventGraphState.SelectedEvents )
			, FakeRoot( FEventGraphSample::CreateNamedEvent( FEventGraphConsts::FakeRoot ) )
			, CullPropertyName( EventGraphState.CullPropertyName )
			, CullEventPtr( EventGraphState.CullEventPtr )
			, ExpandedCulledEvents( EventGraphState.ExpandedCulledEvents )
			, FilterPropertyName( InFilterPropertyName )
			, FilterEventPtr( InFilterEventPtr )
			, CreationTime( FPlatformTime::Seconds() )
			, HistoryType( EEventHistoryTypes::Filtered )
			, ViewMode( EventGraphState.ViewMode )
			, EventGraphType( EventGraphState.EventGraphType )
		{
			// Copy fake root.
			SetNewRoot( EventGraphState.FakeRoot->GetChildren() );
		}

	public:
		const FEventGraphDataRef AverageEventGraph;
		const FEventGraphDataRef MaximumEventGraph;

		TMap<FEventGraphSamplePtr,FEventGraphSamplePtr> AverageToMaximumMapping;
		TMap<FEventGraphSamplePtr,FEventGraphSamplePtr> MaximumToAverageMapping;

		/** Only for hierarchical events, states for coalesced events are generated on demand. */
		TSet<FEventGraphSamplePtr> ExpandedEvents;
		TArray<FEventGraphSamplePtr> SelectedEvents;

		/** Fake root event used to limit the event graph to the specified events and its children. */
		FEventGraphSamplePtr FakeRoot;

		/** Name of the property used to cull the event graph. */
		FName CullPropertyName;
		/** Value of the property used to cull the event graph. */
		FEventGraphSamplePtr CullEventPtr;

		/** Events that were culled, but later the user decided to expand them. */
		TArray<FEventGraphSamplePtr> ExpandedCulledEvents;

		/** Name of the property used to filter out the event graph. */
		FName FilterPropertyName;
		/** Value of the property used to filter out the event graph. */
		FEventGraphSamplePtr FilterEventPtr;

		const double CreationTime;
		EEventHistoryTypes::Type HistoryType;

		/** Event graph view mode. */
		EEventGraphViewModes::Type ViewMode;

		/** Event graph type. */
		EEventGraphTypes::Type EventGraphType;

		void CreateOneToOneMapping();

		const bool IsCulled() const
		{
			return CullPropertyName != NAME_None;
		}

		const bool IsFiltered() const
		{
			return FilterPropertyName != NAME_None;
		}

		const bool IsRooted() const
		{
			return FakeRoot->GetChildren().Num() > 0;
		}

		/**
		 * @return the number of frames used to create this event graph data state.
		 */
		const uint32 GetNumFrames() const
		{
			return AverageEventGraph->GetNumFrames();
		}

		FText GetFullDescription() const;

		FText GetRootedDesc() const;

		FText GetCullingDesc() const;

		FText GetFilteringDesc() const;

		FText GetHistoryDesc() const;

		const FEventGraphDataRef& GetEventGraph() const
		{
			return EventGraphType == EEventGraphTypes::Average ? AverageEventGraph : MaximumEventGraph;
		}

		FEventGraphSamplePtr GetRoot() const
		{
			return IsRooted() ? FakeRoot : GetEventGraph()->GetRoot();
		}

		FEventGraphSamplePtr GetRealRoot() const
		{
			return GetEventGraph()->GetRoot();
		}

		void SetNewRoot( const TArray<FEventGraphSamplePtr>& NewRootEvents )
		{
			for( int32 Nx = 0; Nx < NewRootEvents.Num(); ++Nx )
			{
				FakeRoot->AddChildPtr( NewRootEvents[Nx] );
			}
		}

		void ApplyCulling()
		{
			if( IsCulled() )
			{
				// Apply culling.
				FEventArrayBooleanOp::ExecuteOperation
				( 
					GetRoot(), EEventPropertyIndex::bIsCulled, 
					CullEventPtr, FEventGraphSample::GetEventPropertyByName(CullPropertyName).Index, 
					EEventCompareOps::Less
				);

				// Update not culled children.
				GetRoot()->SetBooleanStateForAllChildren<EEventPropertyIndex::bNeedNotCulledChildrenUpdate>(true);
			}
			else
			{
				// Reset culling.
				GetRoot()->SetBooleanStateForAllChildren<EEventPropertyIndex::bIsCulled>(false);
			}
		}

		void ApplyFiltering()
		{
			if( IsFiltered() )
			{
				// Apply filtering.
				FEventArrayBooleanOp::ExecuteOperation
				( 
					GetRoot(), EEventPropertyIndex::bIsFiltered, 
					FilterEventPtr, FEventGraphSample::GetEventPropertyByName(FilterPropertyName).Index, 
					EEventCompareOps::Less
				);
			}
			else
			{
				// Reset filtering.
				GetRoot()->SetBooleanStateForAllChildren<EEventPropertyIndex::bIsFiltered>(false);
			}
		}

		/** Hacky method to update this saved state so it can be used with the new event graph type, mostly temporary. */
		void UpdateToNewEventGraphType( const EEventGraphTypes::Type NewEventGraphType )
		{
			if( EventGraphType != NewEventGraphType )
			{
				const TMap<FEventGraphSamplePtr,FEventGraphSamplePtr>& OneToOneMapping = NewEventGraphType == EEventGraphTypes::Maximum ? AverageToMaximumMapping : MaximumToAverageMapping;

				// Copy selected events.
				TArray<FEventGraphSamplePtr> NewSelectedEvents;
				for( int32 Nx = 0; Nx < SelectedEvents.Num(); ++Nx )
				{
					const FEventGraphSamplePtr& EventRef = OneToOneMapping.FindRef( SelectedEvents[Nx] );
					NewSelectedEvents.Add( EventRef );
				}

				// Cope expanded events.
				TSet<FEventGraphSamplePtr> NewExpandedEvents;
				for( auto It = ExpandedEvents.CreateConstIterator(); It; ++It )
				{
					const FEventGraphSamplePtr& EventRef = OneToOneMapping.FindRef( *It );
					NewExpandedEvents.Add( EventRef );
				}

				// Copy fake root's children.
				FEventGraphSamplePtr NewFakeRoot = FEventGraphSample::CreateNamedEvent( FEventGraphConsts::FakeRoot );

				TArray<FEventGraphSamplePtr>& Children = FakeRoot->GetChildren();
				for( int32 Nx = 0; Nx < Children.Num(); ++Nx )
				{
					const FEventGraphSamplePtr& EventRef = OneToOneMapping.FindRef( Children[Nx] );
					NewFakeRoot->AddChildPtr( EventRef );
				}

				// Switch to new data.
				Exchange( SelectedEvents, NewSelectedEvents );
				Exchange( ExpandedEvents, NewExpandedEvents );
				FakeRoot = NewFakeRoot;

				EventGraphType = NewEventGraphType;
			}
		}
	};

	/** Type definition for shared pointers to instances of FEventGraphState. */
	typedef TSharedPtr<class FEventGraphState> FEventGraphStatePtr;

	/** Type definition for shared references to instances of FEventGraphState. */
	typedef TSharedRef<class FEventGraphState> FEventGraphStateRef;

	FEventGraphStateRef GetCurrentState() const
	{
		return EventGraphStatesHistory[CurrentStateIndex];
	}

	/**
	 * @return the current event graph view mode.
	 */
	EEventGraphViewModes::Type GetCurrentStateViewMode() const
	{
		if( IsEventGraphStatesHistoryValid() )
		{
			return GetCurrentState()->ViewMode;
		}
		return EEventGraphViewModes::InvalidOrMax;
	}

	/**
	 * @return the current event graph type.
	 */
	EEventGraphTypes::Type GetCurrentStateEventGraphType() const
	{
		return GetCurrentState()->EventGraphType;
	}

public:
	/** Updates top level of the event graph, but only if there is no any other selection. */
	void SetNewEventGraphState( const FEventGraphDataRef AverageEventGraph, const FEventGraphDataRef MaximumEventGraph, bool bInitial );

protected:
	void SaveCurrentEventGraphState();
	void RestoreEventGraphStateFrom( const FEventGraphStateRef EventGraphState, const bool bRestoredFromHistoryEvent = true );
	void SwitchToEventGraphState( int32 StateIndex );

	void SetEventGraphFromStateInternal( const FEventGraphStateRef& EventGraphState );

	bool EventGraph_IsEnabled() const;
	FReply HistoryBack_OnClicked();
	bool HistoryBack_IsEnabled() const;
	FText HistoryBack_GetToolTipText() const;

	bool HistoryForward_IsEnabled() const;
	FReply HistoryForward_OnClicked();
	FText HistoryForward_GetToolTipText() const;

	bool HistoryList_IsEnabled() const;
	TSharedRef<SWidget> HistoryList_GetMenuContent() const;

	bool IsEventGraphStatesHistoryValid() const
	{
		return EventGraphStatesHistory.Num() > 0;
	}

	/** Array of all operations that have been done in this event graph. */
	TArray<FEventGraphStateRef> EventGraphStatesHistory;

	/** The current operation index. */
	int32 CurrentStateIndex;

	/*-----------------------------------------------------------------------------
		Function details
	-----------------------------------------------------------------------------*/
protected:
	struct FEventPtrAndMisc
	{
		FEventPtrAndMisc( FEventGraphSamplePtr InEventPtr, float InIncTimeToTotalPct, float InHeightPct )
			: EventPtr( InEventPtr )
			, IncTimeToTotalPct( InIncTimeToTotalPct )
			, HeightPct( InHeightPct )
		{}

		FEventGraphSamplePtr EventPtr;
		float IncTimeToTotalPct;
		float HeightPct;	
	};

	TSharedRef<SVerticalBox> GetVerticalBoxForFunctionDetails( TSharedPtr<SVerticalBox>& out_VerticalBoxTopFuncions, const FText& Caption );
	TSharedRef<SVerticalBox> GetVerticalBoxForCurrentFunction();

	void UpdateFunctionDetailsForEvent( FEventGraphSamplePtr SelectedEvent );
	void DisableFunctionDetails();
	void UpdateFunctionDetails();


	void RecreateWidgetsForTopEvents( const TSharedPtr<SVerticalBox>& DestVerticalBox, const TArray<FEventPtrAndMisc>& TopEvents );

	void GenerateCallerCalleeGraph( FEventGraphSamplePtr SelectedEvent );
	void GenerateTopEvents( const TSet< FEventGraphSamplePtr >& EventPtrSet, TArray<FEventPtrAndMisc>& out_Results );
	void CalculateEventWeights( TArray<FEventPtrAndMisc>& Events );

	FString GetEventDescription( FEventGraphSamplePtr EventPtr, float Pct, const bool bSimple );
	TSharedRef<SHorizontalBox> GetContentForEvent( FEventGraphSamplePtr EventPtr, float Pct, const bool bSimple );

	FReply CallingCalledFunctionButton_OnClicked( FEventGraphSamplePtr EventPtr );

	TSharedPtr<SVerticalBox> VerticalBox_TopCalled;
	TSharedPtr<SVerticalBox> VerticalBox_TopCalling;
	TSharedPtr<SVerticalBox> VerticalBox_CurrentFunction;
	SVerticalBox::FSlot* CurrentFunctionDescSlot;

	TArray<FEventPtrAndMisc> TopCallingFunctionEvents;
	TArray<FEventPtrAndMisc> TopCalledFunctionEvents;

	/** Name of the event that should be drawn as highlighted. */
	FName HighlightedEventName;
};