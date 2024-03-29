// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * A ListView widget observes an array of data items and creates visual representations of these items.
 * ListView relies on the property that holding a reference to a value ensures its existence. In other words,
 * neither SListView<FString> nor SListView<FString*> are valid, while SListView< TSharedPtr<FString> > and
 * SListView< UObject* > are valid.
 *
 * A trivial use case appear below:
 *
 *   Given: TArray< FString* > Items;
 *
 *   SNew( SListView< FString* > )
 *     .ItemHeight(24)
 *     .ListItemsSource( &Items )
 *     .OnGenerateRow( SListView< TSharedPtr<FString> >::MakeOnGenerateWidget( this, &MyClass::OnGenerateRowForList ) )
 *
 * In the example we make all our widgets be 24 screen units tall. The ListView will create widgets based on data items
 * in the Items TArray. When the ListView needs to generate an item, it will do so using the OnGenerateWidgetForList method.
 *
 * A sample implementation of OnGenerateWidgetForList would simply return a TextBlock with the corresponding text:
 *
 * TSharedRef<ITableRow> OnGenerateWidgetForList( FString* InItem )
 * {
 *     return SNew(STextBlock).Text( (*InItem) )
 * }
 *
 */
template <typename ItemType>
class SListView : public STableViewBase, TListTypeTraits<ItemType>::SerializerType, public ITypedTableView< ItemType >
{
	checkAtCompileTime( TIsValidListItem<ItemType>::Value, ItemType_must_be_a_pointer_or_TSharedPtr );

public:
	typedef typename TListTypeTraits< ItemType >::NullableType NullableItemType;

	typedef typename TSlateDelegates< ItemType >::FOnGenerateRow FOnGenerateRow;
	typedef typename TSlateDelegates< ItemType >::FOnItemScrolledIntoView FOnItemScrolledIntoView;
	typedef typename TSlateDelegates< NullableItemType >::FOnSelectionChanged FOnSelectionChanged;
	typedef typename TSlateDelegates< ItemType >::FOnMouseButtonDoubleClick FOnMouseButtonDoubleClick;


public:

	class FColumnHeaderSlot
	{
	public:
		FColumnHeaderSlot& operator[]( const TSharedRef< SHeaderRow >& InColumnHeaders )
		{
			HeaderRow = InColumnHeaders;
		}			
		TSharedPtr<SHeaderRow> HeaderRow;
	};

	SLATE_BEGIN_ARGS( SListView<ItemType> )
		: _OnGenerateRow()
		, _ListItemsSource( static_cast<const TArray<ItemType>*>(NULL) ) //@todo Slate Syntax: Initializing from NULL without a cast
		, _ItemHeight(16)
		, _OnContextMenuOpening()
		, _OnMouseButtonDoubleClick()
		, _OnSelectionChanged()
		, _SelectionMode(ESelectionMode::Multi)
		, _ClearSelectionOnClick(true)
		, _ExternalScrollbar()
	{}

		SLATE_EVENT( FOnGenerateRow, OnGenerateRow )

		SLATE_EVENT( FOnItemScrolledIntoView, OnItemScrolledIntoView )

		SLATE_ARGUMENT( const TArray<ItemType>* , ListItemsSource )

		SLATE_ATTRIBUTE( float, ItemHeight )

		SLATE_EVENT( FOnContextMenuOpening, OnContextMenuOpening )

		SLATE_EVENT( FOnMouseButtonDoubleClick, OnMouseButtonDoubleClick )

		SLATE_EVENT( FOnSelectionChanged, OnSelectionChanged )

		SLATE_ATTRIBUTE( ESelectionMode::Type, SelectionMode )

		SLATE_ARGUMENT( TSharedPtr<SHeaderRow>, HeaderRow )

		SLATE_ARGUMENT ( bool, ClearSelectionOnClick )

		SLATE_ARGUMENT( TSharedPtr<SScrollBar>, ExternalScrollbar )

		SLATE_ATTRIBUTE( EVisibility, ScrollbarVisibility)

		SLATE_END_ARGS()

		/**
		 * Construct this widget
		 *
		 * @param	InArgs	The declaration data for this widget
		 */
		void Construct( const typename SListView<ItemType>::FArguments& InArgs )
	{
		this->OnGenerateRow = InArgs._OnGenerateRow;
		this->OnItemScrolledIntoView = InArgs._OnItemScrolledIntoView;

		this->ItemsSource = InArgs._ListItemsSource;
		this->OnContextMenuOpening = InArgs._OnContextMenuOpening;
		this->OnDoubleClick = InArgs._OnMouseButtonDoubleClick;
		this->OnSelectionChanged = InArgs._OnSelectionChanged;
		this->SelectionMode = InArgs._SelectionMode;

		this->bClearSelectionOnClick = InArgs._ClearSelectionOnClick;

		// Check for any parameters that the coder forgot to specify.
		FString ErrorString;
		{
			if ( !this->OnGenerateRow.IsBound() )
			{
				ErrorString += TEXT("Please specify an OnGenerateRow. \n");
			}

			if ( this->ItemsSource == NULL )
			{
				ErrorString += TEXT("Please specify a ListItemsSource. \n");
			}
		}

		if (ErrorString.Len() > 0)
		{
			// Let the coder know what they forgot
			this->ChildSlot
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(ErrorString)
				];
		}
		else
		{
			// Make the TableView
			ConstructChildren( 0, InArgs._ItemHeight, InArgs._HeaderRow, InArgs._ExternalScrollbar );
			if(ScrollBar.IsValid())
			{
				ScrollBar->SetUserVisibility(InArgs._ScrollbarVisibility);
			}

		}
	}

	SListView( ETableViewMode::Type InListMode = ETableViewMode::List )
		: STableViewBase( InListMode )
		, SelectorItem( NullableItemType(NULL) )
		, RangeSelectionStart( NullableItemType(NULL) )
		, ItemsSource( NULL )
		, ItemToScrollIntoView( NullableItemType(NULL) )
		, ItemToNotifyWhenInView( NullableItemType(NULL) ) 
	{
	}

public:
	// Inherited from SWidget
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent ) OVERRIDE
	{
		const TArray<ItemType>& ItemsSourceRef = (*this->ItemsSource);

		// Don't respond to key-presses containing "Alt" as a modifier
		if ( ItemsSourceRef.Num() > 0 && !InKeyboardEvent.IsAltDown() )
		{
			bool bWasHandled = false;
			NullableItemType ItemNavigatedTo( NULL );

			// Check for selection manipulation keys (Up, Down, Home, End, PageUp, PageDown)
			if ( InKeyboardEvent.GetKey() == EKeys::Home )
			{
				// Select the first item
				ItemNavigatedTo = ItemsSourceRef[0];
				bWasHandled = true;
			}
			else if ( InKeyboardEvent.GetKey() == EKeys::End )
			{
				// Select the last item
				ItemNavigatedTo = ItemsSourceRef.Last();
				bWasHandled = true;
			}
			else if ( InKeyboardEvent.GetKey() == EKeys::PageUp )
			{
				int32 SelectionIndex = 0;
				if( TListTypeTraits<ItemType>::IsPtrValid(SelectorItem) )
				{
					SelectionIndex = ItemsSourceRef.Find( TListTypeTraits<ItemType>::NullableItemTypeConvertToItemType( SelectorItem ) );
				}

				int32 NumItemsInAPage = GetNumLiveWidgets();
				int32 Remainder = NumItemsInAPage % GetNumItemsWide();
				NumItemsInAPage -= Remainder;

				if ( SelectionIndex >= NumItemsInAPage )
				{
					// Select an item on the previous page
					ItemNavigatedTo = ItemsSourceRef[SelectionIndex - NumItemsInAPage];
				}
				else
				{
					ItemNavigatedTo = ItemsSourceRef[0];
				}

				bWasHandled = true;
			}
			else if ( InKeyboardEvent.GetKey() == EKeys::PageDown )
			{
				int32 SelectionIndex = 0;
				if( TListTypeTraits<ItemType>::IsPtrValid(SelectorItem) )
				{
					SelectionIndex = ItemsSourceRef.Find( TListTypeTraits<ItemType>::NullableItemTypeConvertToItemType( SelectorItem ) );
				}

				int32 NumItemsInAPage = GetNumLiveWidgets();
				int32 Remainder = NumItemsInAPage % GetNumItemsWide();
				NumItemsInAPage -= Remainder;

				if ( SelectionIndex < ItemsSourceRef.Num() - NumItemsInAPage )
				{
					// Select an item on the next page
					ItemNavigatedTo = ItemsSourceRef[SelectionIndex + NumItemsInAPage];
				}
				else
				{
					ItemNavigatedTo = ItemsSourceRef.Last();
				}

				bWasHandled = true;
			}
			else if ( InKeyboardEvent.GetKey() == EKeys::Up )
			{
				int32 SelectionIndex = 0;
				if( TListTypeTraits<ItemType>::IsPtrValid(SelectorItem) )
				{
					SelectionIndex = ItemsSourceRef.Find( TListTypeTraits<ItemType>::NullableItemTypeConvertToItemType( SelectorItem ) );
				}

				int32 NumItemsWide = this->GetNumItemsWide();

				if ( SelectionIndex >= NumItemsWide )
				{
					// Select an item on the previous row
					ItemNavigatedTo = ItemsSourceRef[SelectionIndex - NumItemsWide];
				}

				bWasHandled = true;
			}
			else if ( InKeyboardEvent.GetKey() == EKeys::Down )
			{
				// Begin at INDEX_NONE so the first item will get selected
				int32 SelectionIndex = INDEX_NONE;
				if( TListTypeTraits<ItemType>::IsPtrValid(SelectorItem) )
				{
					SelectionIndex = ItemsSourceRef.Find( TListTypeTraits<ItemType>::NullableItemTypeConvertToItemType( SelectorItem ) );
				}

				int32 NumItemsWide = this->GetNumItemsWide();

				if ( SelectionIndex < ItemsSourceRef.Num() - NumItemsWide )
				{
					// Select an item on the next row
					ItemNavigatedTo = ItemsSourceRef[SelectionIndex + NumItemsWide];
				}

				bWasHandled = true;
			}

			if( TListTypeTraits<ItemType>::IsPtrValid(ItemNavigatedTo) )
			{
				ItemType ItemToSelect( TListTypeTraits<ItemType>::NullableItemTypeConvertToItemType( ItemNavigatedTo ) );
				KeyboardSelect( ItemToSelect, InKeyboardEvent );
			}
			else
			{
				// Change selected status of item.
				if( TListTypeTraits<ItemType>::IsPtrValid(SelectorItem) && InKeyboardEvent.GetKey() == EKeys::SpaceBar )
				{
					ItemType SelectorItemDereference( TListTypeTraits<ItemType>::NullableItemTypeConvertToItemType( SelectorItem ) );

					// Deselect.
					if( InKeyboardEvent.IsControlDown() || this->SelectionMode == ESelectionMode::SingleToggle )
					{
						this->Private_SetItemSelection( SelectorItemDereference, !( this->Private_IsItemSelected( SelectorItemDereference ) ), true );
						this->Private_SignalSelectionChanged( ESelectInfo::OnKeyPress );
						bWasHandled = true;
					}
					else
					{
						// Already selected, don't handle.
						if( this->Private_IsItemSelected( SelectorItemDereference ) )
						{
							bWasHandled = false;
						}
						// Select.
						else
						{
							this->Private_SetItemSelection( SelectorItemDereference, true, true );
							this->Private_SignalSelectionChanged( ESelectInfo::OnKeyPress );
							bWasHandled = true;
						}
					}

					RangeSelectionStart = SelectorItem;

					// If the selector is not in the view, scroll it into view.
					TSharedPtr<ITableRow> WidgetForItem = this->WidgetGenerator.GetWidgetForItem( SelectorItemDereference );
					if ( !WidgetForItem.IsValid() )
					{
						this->RequestScrollIntoView( SelectorItemDereference );
					}
				}
				// Select all items
				else if ( (!InKeyboardEvent.IsShiftDown() && !InKeyboardEvent.IsAltDown() && InKeyboardEvent.IsControlDown() && InKeyboardEvent.GetKey() == EKeys::A) && this->SelectionMode == ESelectionMode::Multi )
				{
					this->Private_ClearSelection();

					for ( int32 ItemIdx = 0; ItemIdx < ItemsSourceRef.Num(); ++ItemIdx )
					{
						this->Private_SetItemSelection( ItemsSourceRef[ItemIdx], true );
					}

					this->Private_SignalSelectionChanged(ESelectInfo::OnKeyPress);

					bWasHandled = true;
				}
			}

			return (bWasHandled ? FReply::Handled() : FReply::Unhandled());
		}

		return STableViewBase::OnKeyDown(MyGeometry, InKeyboardEvent);
	}

	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE
	{
		if ( bClearSelectionOnClick
			&& SelectionMode != ESelectionMode::None
			&& MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton
			&& !MouseEvent.IsControlDown()
			&& !MouseEvent.IsShiftDown()
			)
		{
			// Left clicking on a list (but not an item) will clear the selection on mouse button down.
			// Right clicking is handled on mouse up.
			if ( this->Private_GetNumSelectedItems() > 0 )
			{
				this->Private_ClearSelection();
				this->Private_SignalSelectionChanged(ESelectInfo::OnMouseClick);
			}

			return FReply::Handled();
		}

		return STableViewBase::OnMouseButtonDown(MyGeometry, MouseEvent);
	}

	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE
	{
		if ( bClearSelectionOnClick
			&& SelectionMode != ESelectionMode::None
			&& MouseEvent.GetEffectingButton() == EKeys::RightMouseButton
			&& !MouseEvent.IsControlDown()
			&& !MouseEvent.IsShiftDown()
			&& !this->IsRightClickScrolling()
			)
		{
			// Right clicking on a list (but not an item) will clear the selection on mouse button up.
			// Left clicking is handled on mouse down
			if ( this->Private_GetNumSelectedItems() > 0 )
			{
				this->Private_ClearSelection();
				this->Private_SignalSelectionChanged(ESelectInfo::OnMouseClick);
			}
		}

		return STableViewBase::OnMouseButtonUp(MyGeometry, MouseEvent);
	}

private:

	/**
	 * A WidgetGenerator is a component responsible for creating widgets from data items.
	 * It also provides mapping from currently generated widgets to the data items which they
	 * represent.
	 */
	class FWidgetGenerator
	{
	public:
		/**
		 * Find a widget for this item if it has already been constructed.
		 *
		 * @param Item  The item for which to find the widget.
		 *
		 * @return A pointer to the corresponding widget if it exists; otherwise NULL.
		 */
		TSharedPtr<ITableRow> GetWidgetForItem( const ItemType& Item )
		{
			TSharedRef<ITableRow>* LookupResult = ItemToWidgetMap.Find( Item );
			if ( LookupResult != NULL )
			{
				return *LookupResult;
			}
			else
			{
				return TSharedPtr<ITableRow>(NULL);
			}
		}

		/**
		 * Keep track of every item and corresponding widget during a generation pass.
		 *
		 * @param InItem             The DataItem which is in view.
		 * @param InGeneratedWidget  The widget generated for this item; it may have been newly generated.
		 */
		void OnItemSeen( ItemType InItem, TSharedRef<ITableRow> InGeneratedWidget)
		{
			TSharedRef<ITableRow>* LookupResult = ItemToWidgetMap.Find( InItem );
			const bool bWidgetIsNewlyGenerated = (LookupResult == NULL);
			if ( bWidgetIsNewlyGenerated )
			{
				// It's a newly generated item!
				ItemToWidgetMap.Add( InItem, InGeneratedWidget );
				WidgetMapToItem.Add( &InGeneratedWidget.Get(), InItem );		
			}

			// We should not clean up this item's widgets because it is in view.
			ItemsToBeCleanedUp.Remove(InItem);
			ItemsWithGeneratedWidgets.Add(InItem);
		}

		/**
		 * Called at the beginning of the generation pass.
		 * Begins tracking of which widgets were in view and which were not (so we can clean them up)
		 * 
		 * @param InNumDataItems   The total number of data items being observed
		 */
		void OnBeginGenerationPass()
		{
			// Assume all the previously generated items need to be cleaned up.				
			ItemsToBeCleanedUp = ItemsWithGeneratedWidgets;
			ItemsWithGeneratedWidgets.Empty();
		}

		/**
		 * Called at the end of the generation pass.
		 * Cleans up any widgets associated with items that were not in view this frame.
		 */
		void OnEndGenerationPass()
		{
			for( int32 ItemIndex = 0; ItemIndex < ItemsToBeCleanedUp.Num(); ++ItemIndex )
			{
				ItemType ItemToBeCleanedUp = ItemsToBeCleanedUp[ItemIndex];
				const TSharedRef<ITableRow>* FindResult = ItemToWidgetMap.Find( ItemToBeCleanedUp );
				if ( FindResult != NULL )
				{
					const TSharedRef<ITableRow> WidgetToCleanUp = *FindResult;
					ItemToWidgetMap.Remove( ItemToBeCleanedUp );
					WidgetMapToItem.Remove( &WidgetToCleanUp.Get() );
				}				
			}

			checkf( ItemToWidgetMap.Num() == WidgetMapToItem.Num(), TEXT("ItemToWidgetMap length (%d) does not match WidgetMapToItem length (%d)"), ItemToWidgetMap.Num(), WidgetMapToItem.Num() );
			checkf( WidgetMapToItem.Num() == ItemsWithGeneratedWidgets.Num(), TEXT("WidgetMapToItem length (%d) does not match ItemsWithGeneratedWidgets length (%d). This is often because the same item is in the list more than once."), WidgetMapToItem.Num(), ItemsWithGeneratedWidgets.Num() );
			ItemsToBeCleanedUp.Reset();
		}

		/**
		 * Clear everything so widgets will be regenerated
		*/
		void Clear()
		{
			// Clean up all the previously generated items			
			ItemsToBeCleanedUp = ItemsWithGeneratedWidgets;
			ItemsWithGeneratedWidgets.Empty();

			for( int32 ItemIndex = 0; ItemIndex < ItemsToBeCleanedUp.Num(); ++ItemIndex )
			{
				ItemType ItemToBeCleanedUp = ItemsToBeCleanedUp[ItemIndex];
				const TSharedRef<ITableRow>* FindResult = ItemToWidgetMap.Find( ItemToBeCleanedUp );
				if ( FindResult != NULL )
				{
					const TSharedRef<ITableRow> WidgetToCleanUp = *FindResult;
					ItemToWidgetMap.Remove( ItemToBeCleanedUp );
					WidgetMapToItem.Remove( &WidgetToCleanUp.Get() );
				}				
			}

			ItemsToBeCleanedUp.Reset();
		}

		/** Map of DataItems to corresponding SWidgets */
		TMap< ItemType, TSharedRef<ITableRow> > ItemToWidgetMap;
		/** Map of SWidgets to DataItems from which they were generated */
		TMap< const ITableRow*, ItemType > WidgetMapToItem;
		/** A set of Items that currently have a generated widget */
		TArray< ItemType > ItemsWithGeneratedWidgets;
		/** Total number of DataItems the last time we performed a generation pass. */
		int32 TotalItemsLastGeneration;
		/** Items that need their widgets destroyed because they are no longer on screen. */
		TArray<ItemType> ItemsToBeCleanedUp;
	};


public:
	//
	// Private Interface
	//
	// A low-level interface for use various widgets generated by ItemsWidgets(Lists, Trees, etc).
	// These handle selection, expansion, and other such properties common to ItemsWidgets.
	//

	virtual void Private_SetItemSelection( ItemType TheItem, bool bShouldBeSelected, bool bWasUserDirected = false ) OVERRIDE
	{
		if ( SelectionMode == ESelectionMode::None )
		{
			return;
		}

		if ( bShouldBeSelected )
		{
			SelectedItems.Add( TheItem );
		}
		else
		{
			SelectedItems.Remove( TheItem );
		}

		// Only move the selector item and range selection start if the user directed this change in selection.
		if( bWasUserDirected )
		{
			SelectorItem = TheItem;
			RangeSelectionStart = TheItem;
		}

		this->InertialScrollManager.ClearScrollVelocity();
	}

	virtual void Private_ClearSelection() OVERRIDE
	{
		SelectedItems.Empty();

		this->InertialScrollManager.ClearScrollVelocity();
	}

	virtual void Private_SelectRangeFromCurrentTo( ItemType InRangeSelectionEnd ) OVERRIDE
	{
		if ( SelectionMode == ESelectionMode::None )
		{
			return;
		}

		const TArray<ItemType>& ItemsSourceRef = (*ItemsSource);

		int32 RangeStartIndex = 0;
		if( TListTypeTraits<ItemType>::IsPtrValid(RangeSelectionStart) )
		{
			RangeStartIndex = ItemsSourceRef.Find( TListTypeTraits<ItemType>::NullableItemTypeConvertToItemType( RangeSelectionStart ) );
		}

		int32 RangeEndIndex = ItemsSourceRef.Find( InRangeSelectionEnd );

		RangeStartIndex = FMath::Clamp(RangeStartIndex, 0, ItemsSourceRef.Num());
		RangeEndIndex = FMath::Clamp(RangeEndIndex, 0, ItemsSourceRef.Num());

		if (RangeEndIndex < RangeStartIndex)
		{
			Swap( RangeStartIndex, RangeEndIndex );
		}

		for( int32 ItemIndex = RangeStartIndex; ItemIndex <= RangeEndIndex; ++ItemIndex )
		{
			SelectedItems.Add( ItemsSourceRef[ItemIndex] );
		}

		this->InertialScrollManager.ClearScrollVelocity();
	}

	virtual void Private_SignalSelectionChanged(ESelectInfo::Type SelectInfo) OVERRIDE
	{
		if ( SelectionMode == ESelectionMode::None )
		{
			return;
		}

		if( OnSelectionChanged.IsBound() )
		{
			NullableItemType SelectedItem = (SelectedItems.Num() > 0)
				? (*typename TSet<ItemType>::TIterator(SelectedItems))
				: TListTypeTraits< ItemType >::MakeNullPtr();

			OnSelectionChanged.ExecuteIfBound(SelectedItem, SelectInfo );
		}
	}

	virtual const ItemType* Private_ItemFromWidget( const ITableRow* TheWidget ) const OVERRIDE
	{
		ItemType const * LookupResult = WidgetGenerator.WidgetMapToItem.Find( TheWidget );
		return LookupResult == NULL ? NULL : LookupResult;
	}

	virtual bool Private_UsesSelectorFocus() const OVERRIDE
	{
		return true;
	}

	virtual bool Private_HasSelectorFocus( const ItemType& TheItem ) const OVERRIDE
	{
		return SelectorItem == TheItem;
	}

	virtual bool Private_IsItemSelected( const ItemType& TheItem ) const OVERRIDE
	{
		return NULL != SelectedItems.Find(TheItem);
	}

	virtual bool Private_IsItemExpanded( const ItemType& TheItem ) const OVERRIDE
	{
		// List View does not support item expansion.
		return false;	
	}

	virtual void Private_SetItemExpansion( ItemType TheItem, bool bShouldBeExpanded ) OVERRIDE
	{
		// Do nothing; you cannot expand an item in a list!
	}

	virtual void Private_OnExpanderArrowShiftClicked( ItemType TheItem, bool bShouldBeExpanded ) OVERRIDE
	{
		// Do nothing; you cannot expand an item in a list!
	}

	virtual bool Private_DoesItemHaveChildren( int32 ItemIndexInList ) const OVERRIDE
	{
		// List View items cannot have children
		return false;
	}

	virtual int32 Private_GetNumSelectedItems() const OVERRIDE
	{
		return SelectedItems.Num();
	}

	virtual int32 Private_GetNestingDepth( int32 ItemIndexInList ) const OVERRIDE
	{
		// List View items are not indented
		return 0;
	}

	virtual ESelectionMode::Type Private_GetSelectionMode() const OVERRIDE
	{
		return SelectionMode.Get();
	}

	virtual void Private_OnItemRightClicked( ItemType TheItem, const FPointerEvent& MouseEvent ) OVERRIDE
	{
		this->OnRightMouseButtonUp( MouseEvent.GetScreenSpacePosition() );
	}

	virtual bool Private_OnItemDoubleClicked( ItemType TheItem ) OVERRIDE
	{
		if( OnDoubleClick.ExecuteIfBound( TheItem ) )
		{
			return true;	// Handled
		}

		return false;	// Not handled
	}

	virtual ETableViewMode::Type GetTableViewMode() const OVERRIDE
	{
		return TableViewMode;
	}

	virtual TSharedRef<SWidget> AsWidget() OVERRIDE
	{
		return SharedThis(this);
	}



public:	

	/**
	 * Remove any items that are no longer in the list from the selection set.
	 */
	virtual void UpdateSelectionSet()
	{
		// Trees take care of this update in a different way.
		if ( TableViewMode != ETableViewMode::Tree )
		{
			bool bSelectionChanged = false;
			if ( ItemsSource == NULL )
			{
				// We are no longer observing items so there is no more selection.
				this->Private_ClearSelection();
				bSelectionChanged = true;
			}
			else
			{
				// We are observing some items; they are potentially different.
				// Unselect any that are no longer being observed.
				TSet< ItemType > NewSelectedItems;
				for ( int32 ItemIndex = 0; ItemIndex < ItemsSource->Num(); ++ItemIndex )
				{
					ItemType CurItem = (*ItemsSource)[ItemIndex];
					const bool bItemIsSelected = ( NULL != SelectedItems.Find( CurItem ) );
					if ( bItemIsSelected )
					{
						NewSelectedItems.Add( CurItem );
					}
				}

				// Look for items that were removed from the selection.
				TSet< ItemType > SetDifference = SelectedItems.Difference( NewSelectedItems );
				bSelectionChanged = (SetDifference.Num()) > 0;

				// Update the selection to reflect the removal of any items from the ItemsSource.
				SelectedItems = NewSelectedItems;
			}

			if (bSelectionChanged)
			{
				Private_SignalSelectionChanged(ESelectInfo::Direct);
			}
		}
	}

	/**
	 * Update generate Widgets for Items as needed and clean up any Widgets that are no longer needed.
	 * Re-arrange the visible widget order as necessary.
	 */
	virtual FReGenerateResults ReGenerateItems( const FGeometry& MyGeometry ) OVERRIDE
	{
		// Clear all the items from our panel. We will re-add them in the correct order momentarily.
		this->ClearWidgets();

		// Ensure that we always begin and clean up a generation pass.
		
		FGenerationPassGuard GenerationPassGuard(WidgetGenerator);

		const TArray<ItemType>* SourceItems = ItemsSource;
		if ( SourceItems != NULL && SourceItems->Num() > 0 )
		{
			// Items in view, including fractional items
			float ItemsInView = 0.0f;

			// Height of generated widgets that is landing in the bounds of the view.
			float ViewHeightUsedSoFar = 0.0f;

			// Total height of widgets generated so far.
			float HeightGeneratedSoFar = 0.0f;

			// Index of the item at which we start generating based on how far scrolled down we are
			// Note that we must generate at LEAST one item.
			int32 StartIndex = FMath::Clamp( FMath::FloorToInt(ScrollOffset), 0, SourceItems->Num()-1 );

			// Height of the first item that is generated. This item is at the location where the user requested we scroll
			float FirstItemHeight = 0.0f;

			// Generate widgets assuming scenario a.
			bool bGeneratedEnoughForSmoothScrolling = false;
			bool AtEndOfList = false;

			for( int32 ItemIndex = StartIndex; !bGeneratedEnoughForSmoothScrolling && ItemIndex < SourceItems->Num(); ++ItemIndex )
			{
				const ItemType& CurItem = (*SourceItems)[ItemIndex];

				const float ItemHeight = GenerateWidgetForItem( CurItem, ItemIndex, StartIndex );

				const bool IsFirstItem = ItemIndex == StartIndex;

				if (IsFirstItem)
				{
					FirstItemHeight = ItemHeight;
				}

				// Track the number of items in the view, including fractions.
				if (IsFirstItem)
				{
					ItemsInView += 1.0f - FMath::Fractional(ScrollOffset);
				}
				else if (ViewHeightUsedSoFar + ItemHeight > MyGeometry.Size.Y)
				{
					ItemsInView += (MyGeometry.Size.Y - ViewHeightUsedSoFar) / ItemHeight;
				}
				else
				{
					ItemsInView += 1;
				}

				HeightGeneratedSoFar += ItemHeight;

				ViewHeightUsedSoFar += (IsFirstItem)
					? ItemHeight * (1.0f - FMath::Fractional(ScrollOffset))
					: ItemHeight;

				if (ItemIndex >= SourceItems->Num()-1)
				{
					AtEndOfList = true;
				}

				if (ViewHeightUsedSoFar > MyGeometry.Size.Y )
				{
					bGeneratedEnoughForSmoothScrolling = true;
				}
			}

			// Handle scenario b.
			// We may have stopped because we got to the end of the items.
			// But we may still have space to fill!
			if (AtEndOfList && ViewHeightUsedSoFar < MyGeometry.Size.Y)
			{
				float NewScrollOffsetForBackfill = StartIndex + (HeightGeneratedSoFar - MyGeometry.Size.Y) / FirstItemHeight;

				for( int32 ItemIndex = StartIndex-1; HeightGeneratedSoFar < MyGeometry.Size.Y && ItemIndex >= 0; --ItemIndex )
				{
					const ItemType& CurItem = (*SourceItems)[ItemIndex];

					const float ItemHeight = GenerateWidgetForItem( CurItem, ItemIndex, StartIndex );

					if (HeightGeneratedSoFar + ItemHeight > MyGeometry.Size.Y)
					{
						// Generated the item that puts us over the top.
						// Count the fraction of this item that will stick out above the list
						NewScrollOffsetForBackfill = ItemIndex + (HeightGeneratedSoFar + ItemHeight - MyGeometry.Size.Y) / ItemHeight;
					}

					// The widget used up some of the available vertical space.
					HeightGeneratedSoFar += ItemHeight;
				}

				return FReGenerateResults(NewScrollOffsetForBackfill, HeightGeneratedSoFar, ItemsSource->Num() - NewScrollOffsetForBackfill, AtEndOfList);
			}

			return FReGenerateResults(ScrollOffset, HeightGeneratedSoFar, ItemsInView, AtEndOfList);
		}

		return FReGenerateResults(0.0f, 0.0f, 0.0f, false);

	}

	float GenerateWidgetForItem( const ItemType& CurItem, int32 ItemIndex, int32 StartIndex )
	{
		// Find a previously generated Widget for this item, if one exists.
		TSharedPtr<ITableRow> WidgetForItem = WidgetGenerator.GetWidgetForItem( CurItem );
		if ( !WidgetForItem.IsValid() )
		{
			// We couldn't find an existing widgets, meaning that this data item was not visible before.
			// Make a new widget for it.
			WidgetForItem = this->GenerateNewWidget( CurItem );
		}

		// It is useful to know the item's index that the widget was generated from.
		// Helps with even/odd coloring
		WidgetForItem->SetIndexInList(ItemIndex);

		// Let the item generator know that we encountered the current Item and associated Widget.
		WidgetGenerator.OnItemSeen( CurItem, WidgetForItem.ToSharedRef() );

		// We rely on the widgets desired size in order to determine how many will fit on screen.
		const TSharedRef<SWidget> NewlyGeneratedWidget = WidgetForItem->AsWidget();
		NewlyGeneratedWidget->SlatePrepass();		

		const bool IsFirstWidgetOnScreen = (ItemIndex == StartIndex);
		const float ItemHeight = NewlyGeneratedWidget->GetDesiredSize().Y;

		// We have a widget for this item; add it to the panel so that it is part of the UI.
		if (ItemIndex >= StartIndex)
		{
			// Generating widgets downward
			this->AppendWidget( WidgetForItem.ToSharedRef() );
		}
		else
		{
			// Backfilling widgets; going upward
			this->InsertWidget( WidgetForItem.ToSharedRef() );
		}

		return ItemHeight;
	}

	/** @return how many items there are in the TArray being observed */
	virtual int32 GetNumItemsBeingObserved() const
	{
		return ItemsSource == NULL ? 0 : ItemsSource->Num();
	}

	/**
	 * Given an data item, generate a widget corresponding to it.
	 *
	 * @param InItem  The data item which to visualize.
	 *
	 * @return A new Widget that represents the data item.
	 */
	virtual TSharedRef<ITableRow> GenerateNewWidget(ItemType InItem)
	{
		if ( OnGenerateRow.IsBound() )
		{
			return OnGenerateRow.Execute( InItem, SharedThis(this) );
		}
		else
		{
			// The programmer did not provide an OnGenerateRow() handler; let them know.
			TSharedRef< STableRow<ItemType> > NewListItemWidget = 
				SNew( STableRow<ItemType>, SharedThis(this) )
				.Content()
				[
					SNew(STextBlock) .Text( NSLOCTEXT("SListView", "BrokenUIMessage", "OnGenerateWidget() not assigned.") )
				];

			return NewListItemWidget;
		}

	}

	/**
	 * Given a Widget, find the corresponding data item.
	 * 
	 * @param WidgetToFind  An widget generated by the list view for some data item.
	 *
	 * @return the data item from which the WidgetToFind was generated
	 */
	const ItemType* ItemFromWidget( const ITableRow* WidgetToFind ) const
	{
		return Private_ItemFromWidget( WidgetToFind );
	}

	/**
	 * Test if the current item is selected.
	 *
	 * @param InItem The item to test.
	 *
	 * @return true if the item is selected in this list; false otherwise.
	 */
	bool IsItemSelected( const ItemType& InItem ) const
	{
		if ( SelectionMode == ESelectionMode::None )
		{
			return false;
		}

		return Private_IsItemSelected( InItem );
	}

	/**
	 * Set the selection state of an item.
	 *
	 * @param InItem      The Item whose selection state to modify
	 * @param bSelected   true to select the item; false to unselect
	 * @param SelectInfo  Provides context on how the selection changed
	 */
	void SetItemSelection( const ItemType& InItem, bool bSelected, ESelectInfo::Type SelectInfo = ESelectInfo::Direct )
	{
		if ( SelectionMode == ESelectionMode::None )
		{
			return;
		}

		Private_SetItemSelection( InItem, bSelected, SelectInfo != ESelectInfo::Direct);
		Private_SignalSelectionChanged(SelectInfo);
	}

	/**
	 * Empty the selection set.
	 */
	void ClearSelection()
	{
		if ( SelectionMode == ESelectionMode::None )
		{
			return;
		}

		Private_ClearSelection();
		Private_SignalSelectionChanged(ESelectInfo::Direct);
	}

	/**
	 * Gets the number of selected items.
	 *
	 * @return Number of selected items.
	 */
	int32 GetNumItemsSelected()
	{
		return SelectedItems.Num();
	}

	/**
	 * Returns a list of selected item indices, or an empty array if nothing is selected
	 *
	 * @return	List of selected item indices (in no particular order)
	 */
	TArray< ItemType > GetSelectedItems()
	{
		TArray< ItemType > SelectedItemArray;
		SelectedItemArray.Empty( SelectedItems.Num() );
		for( typename TSet< ItemType >::TConstIterator SelectedItemIt( SelectedItems ); SelectedItemIt; ++SelectedItemIt )
		{
			SelectedItemArray.Add( *SelectedItemIt );
		}
		return SelectedItemArray;
	}

	/**
	 * Checks whether the specified item is currently visible in the list view.
	 *
	 * @param Item - The item to check.
	 *
	 * @return true if the item is visible, false otherwise.
	 */
	bool IsItemVisible( ItemType Item )
	{
		return WidgetGenerator.GetWidgetForItem(Item).IsValid();
	}	

	/**
	 * Scroll an item into view. If the item is not found, fails silently.
	 *
	 * @param ItemToView  The item to scroll into view on next tick.
	 */
	void RequestScrollIntoView( ItemType ItemToView )
	{
		ItemToScrollIntoView = ItemToView;
		RequestListRefresh();
	}

	/**
	 * Set the currently selected Item.
	 *
	 * @param SoleSelectedItem   Sole item that should be selected.
	 * @param SelectInfo Provides context on how the selection changed
	 */
	void SetSelection( ItemType SoleSelectedItem, ESelectInfo::Type SelectInfo = ESelectInfo::Direct  )
	{
		SelectedItems.Empty();
		SetItemSelection( SoleSelectedItem, true, SelectInfo );
	}

	/**
	 * Find a widget for this item if it has already been constructed.
	 *
	 * @param InItem  The item for which to find the widget.
	 *
	 * @return A pointer to the corresponding widget if it exists; otherwise NULL.
	*/
	TSharedPtr<ITableRow> WidgetFromItem( const ItemType& InItem )
	{
		return WidgetGenerator.GetWidgetForItem(InItem);
	}

	/**
	 * Lists and Trees serialize items that they observe because they rely on the property
	 * that holding a reference means it will not be garbage collected.
	 *
	 * @param Ar The archive to serialize with
	 */
	virtual void AddReferencedObjects( FReferenceCollector& Collector )
	{
		TListTypeTraits<ItemType>::AddReferencedObjects( Collector, WidgetGenerator.ItemsWithGeneratedWidgets, SelectedItems );
	}

protected:

	/**
	 * If there is a pending request to scroll an item into view, do so.
	 * 
	 * @param ListViewGeometry  The geometry of the listView; can be useful for centering the item.
	 */
	virtual void ScrollIntoView( const FGeometry& ListViewGeometry ) OVERRIDE
	{
		if ( TListTypeTraits<ItemType>::IsPtrValid(ItemToScrollIntoView) && ItemsSource != NULL )
		{
			const int32 IndexOfItem = ItemsSource->Find( TListTypeTraits<ItemType>::NullableItemTypeConvertToItemType( ItemToScrollIntoView ) );
			if (IndexOfItem != INDEX_NONE)
			{
				double NumLiveWidgets = GetNumLiveWidgets();
				if (NumLiveWidgets == 0 && IsPendingRefresh())
				{
					// Use the last number of widgets on screen to estimate if we actually need to scroll.
					NumLiveWidgets = LastGenerateResults.ExactNumWidgetsOnScreen;
				}

				// Only scroll the item into view if it's not already in the visible range
				const double IndexPlusOne = IndexOfItem+1;
				if (IndexOfItem < ScrollOffset || IndexPlusOne > (ScrollOffset + NumLiveWidgets))
				{
					// Scroll the top of the listview to the item in question
					ScrollOffset = IndexOfItem;
					// Center the list view on the item in question.
					ScrollOffset -= (NumLiveWidgets / 2);
					//we also don't want the widget being chopped off if it is at the end of the list
					const double MoveBackBy = FMath::Clamp<double>(IndexPlusOne - (ScrollOffset + NumLiveWidgets), 0, FLT_MAX);
					//Move to the correct center spot
					ScrollOffset += MoveBackBy;
				}

				RequestListRefresh();

				ItemToNotifyWhenInView = ItemToScrollIntoView;
			}

			TListTypeTraits<ItemType>::ResetPtr(ItemToScrollIntoView);
		}
	}

	virtual void NotifyItemScrolledIntoView() OVERRIDE
	{
		// Notify that an item that came into view
		if ( TListTypeTraits<ItemType>::IsPtrValid( ItemToNotifyWhenInView ) )
		{
			ItemType NonNullItemToNotifyWhenInView = TListTypeTraits<ItemType>::NullableItemTypeConvertToItemType( ItemToNotifyWhenInView );
			TSharedPtr<ITableRow> Widget = WidgetGenerator.GetWidgetForItem(NonNullItemToNotifyWhenInView);
			OnItemScrolledIntoView.ExecuteIfBound( NonNullItemToNotifyWhenInView, Widget );

			TListTypeTraits<ItemType>::ResetPtr( ItemToNotifyWhenInView );
		}
	}

	virtual float ScrollBy( const FGeometry& MyGeometry, float ScrollByAmountInSlateUnits ) OVERRIDE
	{
		float AbsScrollByAmount = FMath::Abs( ScrollByAmountInSlateUnits );
		int32 StartingItemIndex = (int32)ScrollOffset;
		double NewScrollOffset = ScrollOffset;

		const TArray<ItemType>* SourceItems = ItemsSource;
		if ( SourceItems != NULL && SourceItems->Num() > 0 )
		{
			int ItemIndex = StartingItemIndex;
			while( AbsScrollByAmount != 0 && ItemIndex < SourceItems->Num() && ItemIndex >= 0 )
			{
				const ItemType CurItem = (*SourceItems)[ ItemIndex ];
				TSharedPtr<ITableRow> RowWidget = WidgetGenerator.GetWidgetForItem( CurItem );
				if ( !RowWidget.IsValid() )
				{
					// We couldn't find an existing widgets, meaning that this data item was not visible before.
					// Make a new widget for it.
					RowWidget = this->GenerateNewWidget( CurItem );

					// It is useful to know the item's index that the widget was generated from.
					// Helps with even/odd coloring
					RowWidget->SetIndexInList(ItemIndex);

					// Let the item generator know that we encountered the current Item and associated Widget.
					WidgetGenerator.OnItemSeen( CurItem, RowWidget.ToSharedRef() );

					RowWidget->AsWidget()->SlatePrepass();
				}

				if ( ScrollByAmountInSlateUnits > 0 )
				{
					FVector2D DesiredSize = RowWidget->AsWidget()->GetDesiredSize();
					const float RemainingHeight = DesiredSize.Y * ( 1.0 - FMath::Fractional( NewScrollOffset ) );

					if ( AbsScrollByAmount > RemainingHeight )
					{
						if ( ItemIndex != SourceItems->Num() )
						{
							AbsScrollByAmount -= RemainingHeight;
							NewScrollOffset = 1.0f + (int32)NewScrollOffset;
							++ItemIndex;
						}
						else
						{
							NewScrollOffset = SourceItems->Num();
							break;
						}
					} 
					else if ( AbsScrollByAmount == RemainingHeight )
					{
						NewScrollOffset = 1.0f + (int32)NewScrollOffset;
						break;
					}
					else
					{
						NewScrollOffset = (int32)NewScrollOffset + ( 1.0f - ( ( RemainingHeight - AbsScrollByAmount ) / DesiredSize.Y ) );
						break;
					}
				}
				else
				{
					FVector2D DesiredSize = RowWidget->AsWidget()->GetDesiredSize();

					float Fractional = FMath::Fractional( NewScrollOffset );
					if ( Fractional == 0 )
					{
						Fractional = 1.0f;
						--NewScrollOffset;
					}

					const float PrecedingHeight = DesiredSize.Y * Fractional;

					if ( AbsScrollByAmount > PrecedingHeight )
					{
						if ( ItemIndex != 0 )
						{
							AbsScrollByAmount -= PrecedingHeight;
							NewScrollOffset -= FMath::Fractional( NewScrollOffset );
							--ItemIndex;
						}
						else
						{
							NewScrollOffset = 0;
							break;
						}
					} 
					else if ( AbsScrollByAmount == PrecedingHeight )
					{
						NewScrollOffset -= FMath::Fractional( NewScrollOffset );
						break;
					}
					else
					{
						NewScrollOffset = (int32)NewScrollOffset + ( ( PrecedingHeight - AbsScrollByAmount ) / DesiredSize.Y );
						break;
					}
				}
			}
		}

		return ScrollTo( NewScrollOffset );
	}

protected:

	/**
	 * Selects the specified item and scrolls it into view. If shift is held, it will be a range select.
	 * 
	 * @param ItemToSelect		The item that was selected by a keystroke
	 * @param InKeyboardEvent	The keyboard event that caused this selection
	 */
	virtual void KeyboardSelect(const ItemType& ItemToSelect, const FKeyboardEvent& InKeyboardEvent, bool bCausedByNavigation=false)
	{
		if ( SelectionMode != ESelectionMode::None )
		{
			// Must be set before signaling selection changes because sometimes new items will be selected that need to stomp this value
			SelectorItem = ItemToSelect;

			if ( SelectionMode == ESelectionMode::Multi && ( InKeyboardEvent.IsShiftDown() || InKeyboardEvent.IsControlDown() ) )
			{
				// Range select.
				if ( InKeyboardEvent.IsShiftDown() )
				{
					// Holding control makes the range select bidirectional, where as it is normally unidirectional.
					if( !( InKeyboardEvent.IsControlDown() ) )
					{
						this->Private_ClearSelection();
					}

					this->Private_SelectRangeFromCurrentTo( ItemToSelect );
				}

				this->Private_SignalSelectionChanged( ESelectInfo::OnNavigation );
			}
			else
			{
				// Single select.
				this->SetSelection( ItemToSelect, ESelectInfo::OnNavigation );
			}

			// If the selector is not in the view, scroll it into view.
			TSharedPtr<ITableRow> WidgetForItem = this->WidgetGenerator.GetWidgetForItem( ItemToSelect );
			if ( !WidgetForItem.IsValid() )
			{
				this->RequestScrollIntoView( ItemToSelect );
			}
		}
	}

protected:
	/** A widget generator component */
	FWidgetGenerator WidgetGenerator;

	/** Delegate to be invoked when the list needs to generate a new widget from a data item. */
	FOnGenerateRow OnGenerateRow;

	/** Delegate to be invoked when an item has come into view after it was requested to come into view. */
	FOnItemScrolledIntoView OnItemScrolledIntoView;

	/** A set of selected data items */
	TSet< ItemType > SelectedItems;

	/** The item to manipulate selection for */
	NullableItemType SelectorItem;

	/** The item which was last manipulated; used as a start for shift-click selection */
	NullableItemType RangeSelectionStart;

	/** Pointer to the array of data items that we are observing */
	const TArray<ItemType>* ItemsSource;

	/** When not null, the list will try to scroll to this item on tick. */
	NullableItemType ItemToScrollIntoView;

	/** When set, the list will notify this item when it has been scrolled into view */
	NullableItemType ItemToNotifyWhenInView;

	/** Delegate to invoke when selection changes. */
	FOnSelectionChanged OnSelectionChanged;

	/** Called when the user double-clicks on an element in the list view with the left mouse button */
	FOnMouseButtonDoubleClick OnDoubleClick;

	/** If true, the selection will be cleared if the user clicks in empty space (not on an item) */
	bool bClearSelectionOnClick;

private:

	struct FGenerationPassGuard
	{
		FWidgetGenerator& Generator;
		FGenerationPassGuard( FWidgetGenerator& InGenerator )
			: Generator(InGenerator)
		{
			// Let the WidgetGenerator that we are starting a pass so that it can keep track of data items and widgets.
			Generator.OnBeginGenerationPass();
		}

		~FGenerationPassGuard()
		{
			// We have completed the generation pass. The WidgetGenerator will clean up unused Widgets when it goes out of scope.
			Generator.OnEndGenerationPass();
		}
	};
};
