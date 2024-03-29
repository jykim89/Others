// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "MultiBox.h"
#include "SButtonRowBlock.h"
#include "SToolBarButtonBlock.h"
#include "SMenuEntryBlock.h"
#include "SWidgetBlock.h"
#include "MultiBoxCustomization.h"
#include "SClippingHorizontalBox.h"


TAttribute<bool> FMultiBoxSettings::UseSmallToolBarIcons;
TAttribute<bool> FMultiBoxSettings::DisplayMultiboxHooks;
FMultiBoxSettings::FConstructToolTip FMultiBoxSettings::ToolTipConstructor = FConstructToolTip::CreateStatic( &FMultiBoxSettings::ConstructDefaultToolTip );

bool FMultiBoxSettings::bInToolbarEditMode;


FMultiBoxSettings::FMultiBoxSettings()
{
	ResetToolTipConstructor();
}

TSharedRef< SToolTip > FMultiBoxSettings::ConstructDefaultToolTip( const TAttribute<FText>& ToolTipText, const TSharedPtr<SWidget>& OverrideContent, const TSharedPtr<const FUICommandInfo>& Action )
{
	if ( OverrideContent.IsValid() )
	{
		return SNew( SToolTip )
		[
			OverrideContent.ToSharedRef()
		];
	}

	return SNew( SToolTip ).Text( ToolTipText );
}

void FMultiBoxSettings::ResetToolTipConstructor()
{
	ToolTipConstructor = FConstructToolTip::CreateStatic( &FMultiBoxSettings::ConstructDefaultToolTip );
}

void FMultiBoxSettings::ToggleToolbarEditing()
{
	bool bCanBeEnabled = false;
	if( GIsEditor )
	{
		GConfig->GetBool(TEXT("/Script/UnrealEd.EditorExperimentalSettings"), TEXT("bToolbarCustomization"), bCanBeEnabled, GEditorUserSettingsIni);
	}

	bInToolbarEditMode = !bInToolbarEditMode && bCanBeEnabled;
}

const FMultiBoxCustomization FMultiBoxCustomization::None( NAME_None );


void SMultiBlockBaseWidget::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	if ( DragDropEvent.GetOperationAs<FUICommandDragDropOp>().IsValid() )
	{
		OwnerMultiBoxWidget.Pin()->OnCustomCommandDragEnter( MultiBlock.ToSharedRef(), MyGeometry, DragDropEvent );
	}
}

FReply SMultiBlockBaseWidget::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	if ( DragDropEvent.GetOperationAs<FUICommandDragDropOp>().IsValid() )
	{
		OwnerMultiBoxWidget.Pin()->OnCustomCommandDragged( MultiBlock.ToSharedRef(), MyGeometry, DragDropEvent );
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SMultiBlockBaseWidget::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	if ( DragDropEvent.GetOperationAs<FUICommandDragDropOp>().IsValid() )
	{
		OwnerMultiBoxWidget.Pin()->OnCustomCommandDropped();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

/**
 * Creates a MultiBlock widget for this MultiBlock
 *
 * @param	InOwnerMultiBoxWidget	The widget that will own the new MultiBlock widget
 * @param	InLocation				The location information for the MultiBlock widget
 *
 * @return  MultiBlock widget object
 */
TSharedRef< IMultiBlockBaseWidget > FMultiBlock::MakeWidget( TSharedRef< SMultiBoxWidget > InOwnerMultiBoxWidget, EMultiBlockLocation::Type InLocation ) const
{
	TSharedRef< IMultiBlockBaseWidget > NewMultiBlockWidget = ConstructWidget();

	// Tell the widget about its parent MultiBox widget
	NewMultiBlockWidget->SetOwnerMultiBoxWidget( InOwnerMultiBoxWidget );

	// Assign ourselves to the MultiBlock widget
	NewMultiBlockWidget->SetMultiBlock( AsShared() );

	// Pass location information to widget.
	NewMultiBlockWidget->SetMultiBlockLocation(InLocation);

	// Work out what style the widget should be using
	const ISlateStyle* const StyleSet = InOwnerMultiBoxWidget->GetStyleSet();
	const FName& StyleName = InOwnerMultiBoxWidget->GetStyleName();

	// Build up the widget
	NewMultiBlockWidget->BuildMultiBlockWidget(StyleSet, StyleName);

	return NewMultiBlockWidget;
}

/**
 * Constructor
 *
 * @param	InType	Type of MultiBox
 * @param	bInShouldCloseWindowAfterMenuSelection	Sets whether or not the window that contains this multibox should be destroyed after the user clicks on a menu item in this box
 */
FMultiBox::FMultiBox( const EMultiBoxType::Type InType, FMultiBoxCustomization InCustomization, const bool bInShouldCloseWindowAfterMenuSelection )
	: CustomizationData( new FMultiBoxCustomizationData( InCustomization.GetCustomizationName() ) )
	, CommandLists()
	, Blocks()
	, StyleSet( &FCoreStyle::Get() )
	, StyleName( "ToolBar" )
	, Type( InType )
	, bShouldCloseWindowAfterMenuSelection( bInShouldCloseWindowAfterMenuSelection )
{

}

FMultiBox::~FMultiBox()
{
}

TSharedRef<FMultiBox> FMultiBox::Create( const EMultiBoxType::Type InType, FMultiBoxCustomization InCustomization, const bool bInShouldCloseWindowAfterMenuSelection )
{
	TSharedRef<FMultiBox> NewBox = MakeShareable( new FMultiBox( InType, InCustomization, bInShouldCloseWindowAfterMenuSelection ) );

	return NewBox;
}

/**
 * Adds a MultiBlock to this MultiBox, to the end of the list
 */
void FMultiBox::AddMultiBlock( TSharedRef< const FMultiBlock > InBlock )
{
#if UE_BUILD_DEBUG
	check( !Blocks.Contains( InBlock ) );
#endif

	if( InBlock->GetActionList().IsValid() )
	{
		CommandLists.AddUnique( InBlock->GetActionList() );
	}

	Blocks.Add( InBlock );
}

void FMultiBox::RemoveCustomMultiBlock( TSharedRef< const FMultiBlock> InBlock )
{
	if( IsCustomizable() )
	{
		int32 Index = Blocks.Find( InBlock );

		// Remove the block from the visual list
		if( Index != INDEX_NONE )
		{
			Blocks.RemoveAt( Index );

			// Remove the block from the customization data
			CustomizationData->BlockRemoved( InBlock, Index, Blocks );
		}

	}
}

void FMultiBox::InsertCustomMultiBlock( TSharedRef<const FMultiBlock> InBlock, int32 Index )
{
	if( IsCustomizable() && ensure( InBlock->GetAction().IsValid() ) )
	{
		int32 ExistingIndex = Blocks.Find( InBlock );
		if( ExistingIndex != INDEX_NONE )
		{
			Blocks.RemoveAt( ExistingIndex );

			CustomizationData->BlockRemoved( InBlock, ExistingIndex, Blocks );

			if( ExistingIndex < Index )
			{
				--Index;
			}
		}

		Blocks.Insert( InBlock, Index );

		CustomizationData->BlockAdded( InBlock, Index, Blocks );
	}
}

/**
 * Creates a MultiBox widget for this MultiBox
 *
 * @return  MultiBox widget object
 */
TSharedRef< SMultiBoxWidget > FMultiBox::MakeWidget()
{	
	ApplyCustomizedBlocks();

	TSharedRef< SMultiBoxWidget > NewMultiBoxWidget =
		SNew( SMultiBoxWidget );

	// Assign ourselves to the MultiBox widget
	NewMultiBoxWidget->SetMultiBox( AsShared() );

	// Build up the widget
	NewMultiBoxWidget->BuildMultiBoxWidget();

	return NewMultiBoxWidget;
}

bool FMultiBox::IsCustomizable() const
{
	bool bIsCustomizable = false;
	if( CustomizationData->GetCustomizationName() != NAME_None )
	{
		GConfig->GetBool(TEXT("/Script/UnrealEd.EditorExperimentalSettings"), TEXT("bToolbarCustomization"), bIsCustomizable, GEditorUserSettingsIni);
	}

	return bIsCustomizable;
}

void FMultiBox::ApplyCustomizedBlocks()
{
	if( IsCustomizable() )
	{
		CustomizationData->LoadCustomizedBlocks();

		// Build a map of commands to existing blocks,  we'll try to use existing blocks before creating new ones
		TMap< TSharedPtr<const FUICommandInfo>, TSharedPtr<const FMultiBlock> > CommandToBlockMap;

		for( int32 BlockIndex = 0; BlockIndex < Blocks.Num(); ++BlockIndex )
		{
			TSharedPtr<const FMultiBlock> Block = Blocks[BlockIndex];
			if( Block->GetAction().IsValid() )
			{
				CommandToBlockMap.Add( Block->GetAction(), Block );
			}
		}

		// Rebuild the users customized box by executing the transactions the user made to get the 
		// box to its customized state
		for( uint32 TransIndex = 0; TransIndex < CustomizationData->GetNumTransactions(); ++TransIndex )
		{
			const FCustomBlockTransaction& Transaction = CustomizationData->GetTransaction(TransIndex);
		
			// Try and find the block in the default map;
			TSharedPtr<const FMultiBlock> Block = CommandToBlockMap.FindRef( Transaction.Command.Pin() );

			if( Transaction.TransactionType == FCustomBlockTransaction::Add )
			{
			
				if( !Block.IsValid() )
				{
					Block = MakeMultiBlockFromCommand( Transaction.Command.Pin(), false );
				}

				if( Block.IsValid() )
				{
					Blocks.Insert( Block.ToSharedRef(), FMath::Clamp( Transaction.BlockIndex, 0, Blocks.Num() ) );
				}
			}
			else
			{
				if( Block.IsValid() )
				{
					Blocks.Remove( Block.ToSharedRef() );
				}
			}
		}
	}
}

FName FMultiBox::GetCustomizationName() const
{
	return CustomizationData->GetCustomizationName(); 
}

TSharedPtr<FMultiBlock> FMultiBox::MakeMultiBlockFromCommand( TSharedPtr<const FUICommandInfo> CommandInfo, bool bCommandMustBeBound ) const
{
	TSharedPtr<FMultiBlock> NewBlock;

	// Find the command list that processes this command
	TSharedPtr<const FUICommandList> CommandList;

	for (int32 CommandListIndex = 0; CommandListIndex < CommandLists.Num(); ++CommandListIndex )
	{
		TSharedPtr<const FUICommandList> TestCommandList = CommandLists[CommandListIndex];
		if( TestCommandList->GetActionForCommand( CommandInfo.ToSharedRef() ) != NULL )
		{
			CommandList = TestCommandList;
			break;
		}
	}

	
	if( !bCommandMustBeBound && !CommandList.IsValid() && CommandLists.Num() > 0 )
	{
		// The first command list is the main command list and other are commandlists added from extension points
		// Use the main command list if one was not found
		CommandList = CommandLists[0];
	}

	if( !bCommandMustBeBound || CommandList.IsValid() )
	{
		// Only toolbars and menu buttons are supported currently
		switch ( Type )
		{
		case EMultiBoxType::ToolBar:
			{
				NewBlock = MakeShareable( new FToolBarButtonBlock( CommandInfo, CommandList ) );
			}
			break;
		case EMultiBoxType::Menu:
			{
				NewBlock = MakeShareable( new FMenuEntryBlock( NAME_None, CommandInfo, CommandList ) );
			}
			break;
		}
	}

	return NewBlock;

}

TSharedPtr<const FMultiBlock> FMultiBox::FindBlockFromCommand( TSharedPtr<const FUICommandInfo> Command ) const
{
	for (TArray< TSharedRef< const FMultiBlock > >::TConstIterator It(Blocks); It; ++It)
	{
		const TSharedRef< const FMultiBlock >& Block = *It;
		if( Block->GetAction() == Command )
		{
			return Block;
		}
	}

	return NULL;
}


void SMultiBoxWidget::Construct( const FArguments& InArgs )
{
	ContentScale = InArgs._ContentScale;
}

TSharedRef<ITableRow> SMultiBoxWidget::GenerateTiles(TSharedPtr<SWidget> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow< TSharedPtr<SWidget> >, OwnerTable)
		[
			Item.ToSharedRef()
		];
}

float SMultiBoxWidget::GetItemWidth() const
{
	float MaxWidth = 0;
	for (int32 i = 0; i < TileViewWidgets.Num(); ++i)
	{
		MaxWidth = FMath::Max(TileViewWidgets[i]->GetDesiredSize().X, MaxWidth);
	}
	return MaxWidth;
}

float SMultiBoxWidget::GetItemHeight() const
{
	float MaxHeight = 0;
	for (int32 i = 0; i < TileViewWidgets.Num(); ++i)
	{
		MaxHeight = FMath::Max(TileViewWidgets[i]->GetDesiredSize().Y, MaxHeight);
	}
	return MaxHeight;
}

bool SMultiBoxWidget::IsBlockBeingDragged( TSharedPtr<const FMultiBlock> Block ) const
{
	if( DragPreview.PreviewBlock.IsValid() )
	{
		return DragPreview.PreviewBlock->GetActualBlock() == Block;
	}

	return false;
}

void SMultiBoxWidget::AddBlockWidget( const FMultiBlock& Block, TSharedPtr<SHorizontalBox> HorizontalBox, TSharedPtr<SVerticalBox> VerticalBox, EMultiBlockLocation::Type InLocation )
{
	check( MultiBox.IsValid() );

	bool bDisplayExtensionHooks = FMultiBoxSettings::DisplayMultiboxHooks.Get() && Block.GetExtensionHook() != NAME_None;

	TSharedRef<SWidget> BlockWidget = Block.MakeWidget( SharedThis( this ), InLocation)->AsWidget();

	TWeakPtr<SWidget> BlockWidgetWeakPtr = BlockWidget;
	TWeakPtr<const FMultiBlock> BlockWeakPtr = Block.AsShared();

	const ISlateStyle* const StyleSet = MultiBox->GetStyleSet();

	TSharedRef<SWidget> FinalWidget =
	SNew( SOverlay )
	+ SOverlay::Slot()
	[
		BlockWidget
	]
	+ SOverlay::Slot()
	[
		// This overlay prevents users from clicking on the actual block when in edit mode and also allows new blocks
		// to be dropped on disabled blocks
		SNew( SMultiBlockDragHandle, SharedThis( this ), Block.AsShared(), MultiBox->GetCustomizationName() )
		.Visibility( this, &SMultiBoxWidget::GetCustomizationVisibility, BlockWeakPtr, BlockWidgetWeakPtr )
	]
	+ SOverlay::Slot()
	.HAlign( HAlign_Right )
	.VAlign( VAlign_Top )
	.Padding( FMargin(0.0f, 2.0f, 1.0f, 0.0f ) )
	[
		// The delete button for removing blocks is only visible when in edit mode
		SNew( SButton )
		.Visibility( this, &SMultiBoxWidget::GetCustomizationVisibility, BlockWeakPtr, BlockWidgetWeakPtr )
		.ContentPadding(0)
		.OnClicked( this, &SMultiBoxWidget::OnDeleteBlockClicked, BlockWeakPtr )
		.ButtonStyle( StyleSet, "MultiBox.DeleteButton" )
	];

	switch (MultiBox->GetType())
	{
	case EMultiBoxType::MenuBar:
	case EMultiBoxType::ToolBar:
		{
			HorizontalBox->AddSlot()
			.AutoWidth()
			.Padding( 0 )
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				.AutoHeight()
				[
					SNew(STextBlock)
					.Visibility(bDisplayExtensionHooks ? EVisibility::Visible : EVisibility::Collapsed)
					.ColorAndOpacity(StyleSet->GetColor("MultiboxHookColor"))
					.Text(Block.GetExtensionHook().ToString())
				]
				+SVerticalBox::Slot()
				[
					FinalWidget
				]
			];
		}
		break;
	case EMultiBoxType::VerticalToolBar:
		{
			VerticalBox->AddSlot()
				.AutoHeight()
				.Padding( 0.0f, 1.0f, 0.0f, 1.0f )
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.HAlign(HAlign_Center)
					.AutoHeight()
					[
						SNew(STextBlock)
						.Visibility(bDisplayExtensionHooks ? EVisibility::Visible : EVisibility::Collapsed)
						.ColorAndOpacity(StyleSet->GetColor("MultiboxHookColor"))
						.Text(Block.GetExtensionHook().ToString())
					]
					+SVerticalBox::Slot()
					[
						FinalWidget
					]
				];
		}
		break;
	case EMultiBoxType::ButtonRow:
		{
			TileViewWidgets.Add( FinalWidget );
		}
		break;
	case EMultiBoxType::Menu:
		{
			VerticalBox->AddSlot()
			.AutoHeight()
			.Padding( 1.0f, 0.0f, 1.0f, 0.0f )
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Visibility(bDisplayExtensionHooks ? EVisibility::Visible : EVisibility::Collapsed)
					.ColorAndOpacity(StyleSet->GetColor("MultiboxHookColor"))
					.Text(Block.GetExtensionHook().ToString())
				]
				+SHorizontalBox::Slot()
				[
					FinalWidget
				]
			];
		}
		break;
	}
}


/**
 * Builds this MultiBox widget up from the MultiBox associated with it
 */
void SMultiBoxWidget::BuildMultiBoxWidget()
{
	check( MultiBox.IsValid() );

	// Grab the list of blocks, early out if there's nothing to fill the widget with
	const TArray< TSharedRef< const FMultiBlock > >& Blocks = MultiBox->GetBlocks();
	if ( Blocks.Num() == 0 )
	{
		return;
	}

	// Select background brush based on the type of multibox.
	const ISlateStyle* const StyleSet = MultiBox->GetStyleSet();
	const FName& StyleName = MultiBox->GetStyleName();
	const FSlateBrush* BackgroundBrush = StyleSet->GetBrush( StyleName, ".Background" );

	// Create a box panel that the various multiblocks will resides within
	// @todo Slate MultiBox: Expose margins and other useful bits
	TSharedPtr< SHorizontalBox > HorizontalBox;
	TSharedPtr< SVerticalBox > VerticalBox;
	TSharedPtr< SWidget > MainWidget;

	/** The current row of buttons for if the multibox type is a button row */
	TSharedPtr<SHorizontalBox> ButtonRow;

	TSharedPtr< STileView< TSharedPtr<SWidget> > > TileView;

	switch (MultiBox->GetType())
	{
	case EMultiBoxType::MenuBar:
	case EMultiBoxType::ToolBar:
		{
				MainWidget = HorizontalBox = ClippedHorizontalBox = SNew( SClippingHorizontalBox )
					.BackgroundBrush(BackgroundBrush)
					.OnWrapButtonClicked(FOnGetContent::CreateSP(this, &SMultiBoxWidget::OnWrapButtonClicked))
					.StyleSet( StyleSet )
					.StyleName( StyleName );
		}
		break;
	case EMultiBoxType::VerticalToolBar:
		{
			MainWidget = VerticalBox = SNew( SVerticalBox );
		}
		break;
	case EMultiBoxType::ButtonRow:
		{
			MainWidget = TileView = SNew(STileView< TSharedPtr<SWidget> >)
				.OnGenerateTile(this, &SMultiBoxWidget::GenerateTiles)
				.ListItemsSource(&TileViewWidgets)
				.ItemWidth(this, &SMultiBoxWidget::GetItemWidth)
				.ItemHeight(this, &SMultiBoxWidget::GetItemHeight)
				.SelectionMode(ESelectionMode::None);
		}
		break;
	case EMultiBoxType::Menu:
		{
			// wrap menu content in a scrollbox to support vertical scrolling if needed
			MainWidget = SNew(SScrollBox)
				+SScrollBox::Slot()
				[
					SAssignNew( VerticalBox, SVerticalBox )
				];
		}
		break;
	}
	
	bool bInsideGroup = false;

	// Start building up the actual UI from each block in this MultiBox
	bool bIsNextToStartBlock = false;
	bool bIsNextToEndBlock = false;
	for( int32 Index = 0; Index < Blocks.Num(); Index++ )
	{
		const FMultiBlock& Block = *Blocks[Index];
		EMultiBlockLocation::Type Location = EMultiBlockLocation::None;

		// Determine the location of the current block, used for group styling information
		{
			// Check if we are a start or end block
			if (Block.IsGroupStartBlock())
			{
				bInsideGroup = true;
			}
			else if (Block.IsGroupEndBlock())
			{
				bInsideGroup = false;
			}

			// Check if we are next to a start or end block
			bIsNextToStartBlock = false;
			bIsNextToEndBlock = false;
			if (Index + 1 < Blocks.Num())
			{
				const FMultiBlock& NextBlock = *Blocks[Index + 1];
				if ( NextBlock.IsGroupEndBlock() )
				{
					bIsNextToEndBlock = true;
				}
			}
			if (Index > 0)
			{
				const FMultiBlock& PrevBlock = *Blocks[Index - 1];
				if ( PrevBlock.IsGroupStartBlock() )
				{
					bIsNextToStartBlock = true;
				}
			}

			// determine location
			if (bInsideGroup)
			{
				// assume we are in the middle of a group
				Location = EMultiBlockLocation::Middle;

				// We are the start of a group
				if (bIsNextToStartBlock && !bIsNextToEndBlock)
				{
					Location = EMultiBlockLocation::Start;
				}
				// we are the end of a group
				else if (!bIsNextToStartBlock && bIsNextToEndBlock)
				{
					Location = EMultiBlockLocation::End;
				}
				// we are the only block in a group
				else if (bIsNextToStartBlock && bIsNextToEndBlock)
				{
					Location = EMultiBlockLocation::None;
				}
			}
		}
	

		if( DragPreview.IsValid() && DragPreview.InsertIndex == Index )
		{
			// Add the drag preview before if we have it. This block shows where the custom block will be 
			// added if the user drops it
			AddBlockWidget( *DragPreview.PreviewBlock, HorizontalBox, VerticalBox, EMultiBlockLocation::None );
		}
		
		// Do not add a block if it is being dragged
		if( !IsBlockBeingDragged( Blocks[Index] ) )
		{
			AddBlockWidget( Block, HorizontalBox, VerticalBox, Location );
		}
	}

	// Add the wrap button as the final block
	if (ClippedHorizontalBox.IsValid())
	{
		ClippedHorizontalBox->AddWrapButton();
	}

	// Setup the root border widget
	TSharedPtr< SBorder > RootBorder;
	switch (MultiBox->GetType())
	{
	case EMultiBoxType::MenuBar:
	case EMultiBoxType::ToolBar:
		{
			RootBorder =
				SNew( SBorder )
				.Padding(0)
				.BorderImage( FCoreStyle::Get().GetBrush("NoBorder") )
				// Assign the box panel as the child
				[
					MainWidget.ToSharedRef()
				];
		}
		break;
	default:
		{
			RootBorder =
				SNew( SBorder )
				.Padding(0)
				.BorderImage( BackgroundBrush )
				.ForegroundColor( FCoreStyle::Get().GetSlateColor("DefaultForeground") )
				// Assign the box panel as the child
				[
					MainWidget.ToSharedRef()
				];
		}
		break;
	}

	// Prevent tool-tips spawned by child widgets from drawing on top of our main widget
	RootBorder->EnableToolTipForceField( true );

	ChildSlot
	[
		RootBorder.ToSharedRef()
	];

}


void SMultiBoxWidget::SetSummonedMenu( TSharedRef< SMenuAnchor > InMenuAnchor )
{
	SummonedMenuAnchor = InMenuAnchor;
}

	
/**
 * For menu bar multibox widgets, returns the currently open pull-down menu, if there is one open
 *
 * @return	Pull-down menu anchor, or null pointer
 */
TSharedPtr< const SMenuAnchor > SMultiBoxWidget::GetOpenMenu() const
{
	if( SummonedMenuAnchor.IsValid() && SummonedMenuAnchor.Pin()->IsOpen() )
	{
		return SummonedMenuAnchor.Pin();
	}

	// No open menus
	return TSharedPtr< SMenuAnchor >();
}



/**
 * For menu bar multibox widget, closes any open pull-down menus
 */
void SMultiBoxWidget::CloseSummonedMenus()
{
	if( GetOpenMenu().IsValid() )
	{
		SummonedMenuAnchor.Pin()->SetIsOpen( false );

		// Menu was closed, so we no longer need a weak reference to it
		SummonedMenuAnchor = NULL;
	}
}


TSharedRef<SWidget> SMultiBoxWidget::OnWrapButtonClicked()
{
	FMenuBuilder MenuBuilder(true, NULL, TSharedPtr<FExtender>(), false, GetStyleSet());
	{ 
		// Iterate through the array of blocks telling each one to add itself to the menu
		const TArray< TSharedRef< const FMultiBlock > >& Blocks = MultiBox->GetBlocks();
		for (int32 BlockIdx = ClippedHorizontalBox->GetClippedIndex(); BlockIdx < Blocks.Num(); ++BlockIdx)
		{
			Blocks[BlockIdx]->CreateMenuEntry(MenuBuilder);
		}
	}

	return MenuBuilder.MakeWidget();
}

void SMultiBoxWidget::UpdateDropAreaPreviewBlock( TSharedRef<const FMultiBlock> MultiBlock, TSharedPtr<FUICommandDragDropOp> DragDropContent, const FGeometry& DragAreaGeometry, const FVector2D& DragPos )
{
	TSharedPtr<const FUICommandInfo> UICommand = DragDropContent->UICommand;
	FName OriginMultiBox = DragDropContent->OriginMultiBox;

	FVector2D LocalDragPos = DragAreaGeometry.AbsoluteToLocal( DragPos );

	FVector2D DrawSize = DragAreaGeometry.GetDrawSize();

	bool bAddedNewBlock = false;
	bool bValidCommand = true;
	if( DragPreview.UICommand != UICommand )
	{	
		TSharedPtr<const FMultiBlock> ExistingBlock = MultiBox->FindBlockFromCommand( UICommand );

		// Check that the command does not already exist and that we can create it or that we are dragging an exisiting block in this box
		if( !ExistingBlock.IsValid() || ( ExistingBlock.IsValid() && OriginMultiBox == MultiBox->GetCustomizationName() ) )
		{

			TSharedPtr<const FMultiBlock> NewBlock = ExistingBlock;

			if( !ExistingBlock.IsValid() )
			{
				NewBlock = MultiBox->MakeMultiBlockFromCommand( UICommand, true );
			}

			if( NewBlock.IsValid() )
			{
				DragPreview.Reset();
				DragPreview.UICommand = UICommand;
				DragPreview.PreviewBlock = 
					MakeShareable(
						new FDropPreviewBlock( 
							NewBlock.ToSharedRef(), 
							NewBlock->MakeWidget( SharedThis(this), EMultiBlockLocation::None ) )
					);

				bAddedNewBlock = true;
			}


		}
		else
		{
			// this command cannot be dropped here
			bValidCommand = false;
		}
	}

	if( bValidCommand )
	{
		// determine whether or not to insert before or after
		bool bInsertBefore = false;
		if( MultiBox->GetType() == EMultiBoxType::ToolBar )
		{
			DragPreview.InsertOrientation  = EOrientation::Orient_Horizontal;
			if( LocalDragPos.X < DrawSize.X / 2 )
			{
				// Insert before horizontally
				bInsertBefore = true;
			}
			else
			{
				// Insert after horizontally
				bInsertBefore = false;
			}
		}
		else 
		{
			DragPreview.InsertOrientation  = EOrientation::Orient_Vertical;
			if( LocalDragPos.Y < DrawSize.Y / 2 )
			{
				// Insert before vertically
				bInsertBefore = true;
			}
			else
			{
				// Insert after vertically
				bInsertBefore = false;
			}
		}


		int32 CurrentIndex = DragPreview.InsertIndex;
		DragPreview.InsertIndex = INDEX_NONE;
		// Find the index of the multiblock being dragged over. This is where we will insert the new block
		if( DragPreview.PreviewBlock.IsValid() )
		{
			const TArray< TSharedRef< const FMultiBlock > >& Blocks = MultiBox->GetBlocks();
			for (int32 BlockIdx = 0; BlockIdx < Blocks.Num(); ++BlockIdx)
			{
				TSharedRef<const FMultiBlock> Block = Blocks[BlockIdx];
				if( Block == MultiBlock )
				{
					if( bInsertBefore)
					{
						DragPreview.InsertIndex = BlockIdx;
					}
					else
					{
						DragPreview.InsertIndex = FMath::Min(Blocks.Num()-1, BlockIdx+1);
					}

					break;
				}
			}
		}

		if( CurrentIndex != DragPreview.InsertIndex && DragPreview.InsertIndex != INDEX_NONE )
		{
			BuildMultiBoxWidget();
		}
	}

}

EVisibility SMultiBoxWidget::GetCustomizationVisibility( TWeakPtr<const FMultiBlock> BlockWeakPtr, TWeakPtr<SWidget> BlockWidgetWeakPtr ) const
{
	if( MultiBox->IsInEditMode() && BlockWidgetWeakPtr.IsValid() && BlockWeakPtr.IsValid() && (!DragPreview.PreviewBlock.IsValid() || BlockWeakPtr.Pin() != DragPreview.PreviewBlock->GetActualBlock() ) )
	{
		// If in edit mode and this is not the block being dragged, the customization widget should be visible if the default block beging customized would have been visible
		return BlockWeakPtr.Pin()->GetAction().IsValid() && BlockWidgetWeakPtr.Pin()->GetVisibility() == EVisibility::Visible ? EVisibility::Visible : EVisibility::Collapsed;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}

FReply SMultiBoxWidget::OnDeleteBlockClicked( TWeakPtr<const FMultiBlock> BlockWeakPtr )
{
	if( BlockWeakPtr.IsValid() )
	{
		MultiBox->RemoveCustomMultiBlock( BlockWeakPtr.Pin().ToSharedRef() );
		BuildMultiBoxWidget();
	}

	return FReply::Handled();
}

void SMultiBoxWidget::OnCustomCommandDragEnter( TSharedRef<const FMultiBlock> MultiBlock, const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	if( MultiBlock != DragPreview.PreviewBlock && MultiBox->IsInEditMode() )
	{
		TSharedPtr<FUICommandDragDropOp> DragDropContent = StaticCastSharedPtr<FUICommandDragDropOp>( DragDropEvent.GetOperation() );

		UpdateDropAreaPreviewBlock( MultiBlock, DragDropContent, MyGeometry, DragDropEvent.GetScreenSpacePosition() );
	}
}


void SMultiBoxWidget::OnCustomCommandDragged( TSharedRef<const FMultiBlock> MultiBlock, const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	if( MultiBlock != DragPreview.PreviewBlock && MultiBox->IsInEditMode() )
	{
		TSharedPtr<FUICommandDragDropOp> DragDropContent = StaticCastSharedPtr<FUICommandDragDropOp>( DragDropEvent.GetOperation() );

		UpdateDropAreaPreviewBlock( MultiBlock, DragDropContent, MyGeometry, DragDropEvent.GetScreenSpacePosition() );
	}
}

void SMultiBoxWidget::OnCustomCommandDropped()
{
	if( DragPreview.IsValid() )
	{	

		// Check that the command does not already exist and that we can create it or that we are dragging an exisiting block in this box
		TSharedPtr<const FMultiBlock> Block = MultiBox->FindBlockFromCommand( DragPreview.UICommand );
		if( !Block.IsValid() )
		{
			Block = MultiBox->MakeMultiBlockFromCommand( DragPreview.UICommand, true );
		}

		if( Block.IsValid() )
		{
			MultiBox->InsertCustomMultiBlock( Block.ToSharedRef(), DragPreview.InsertIndex );
		}

		DragPreview.Reset();

		BuildMultiBoxWidget();
	}
}

void SMultiBoxWidget::OnDropExternal()
{
	// The command was not dropped in this widget
	if( DragPreview.IsValid() )
	{
		DragPreview.Reset();

		BuildMultiBoxWidget();
	}
}

FReply SMultiBoxWidget::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	if ( DragDropEvent.GetOperationAs<FUICommandDragDropOp>().IsValid() && MultiBox->IsInEditMode() )
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SMultiBoxWidget::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	if ( DragDropEvent.GetOperationAs<FUICommandDragDropOp>().IsValid() )
	{
		OnCustomCommandDropped();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

bool SMultiBoxWidget::SupportsKeyboardFocus() const
{
	return true;
}

FReply SMultiBoxWidget::FocusNextWidget( EFocusMoveDirection::Type MoveDirection )
{
	TSharedPtr<SWidget> FocusWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
	if(FocusWidget.IsValid())
	{
		FWidgetPath FocusPath;
		FSlateApplication::Get().GeneratePathToWidgetUnchecked( FocusWidget.ToSharedRef(), FocusPath );
		FWeakWidgetPath WeakFocusPath = FocusPath;
		FWidgetPath NextFocusPath = WeakFocusPath.ToNextFocusedPath( MoveDirection );
		if ( NextFocusPath.Widgets.Num() > 0 )
		{
			return FReply::Handled().SetKeyboardFocus( NextFocusPath.Widgets.Last().Widget, EKeyboardFocusCause::Keyboard );
		}
	}

	return FReply::Unhandled();
}

FReply SMultiBoxWidget::OnKeyboardFocusReceived( const FGeometry& MyGeometry, const FKeyboardFocusEvent& InKeyboardFocusEvent )
{
	if(InKeyboardFocusEvent.GetCause() == EKeyboardFocusCause::Keyboard)
	{
		// forward focus to children
		return FocusNextWidget( EFocusMoveDirection::Next );
	}

	return FReply::Unhandled();
}

FReply SMultiBoxWidget::OnKeyDown( const FGeometry& MyGeometry, const FKeyboardEvent& KeyboardEvent )
{
	SCompoundWidget::OnKeyDown(MyGeometry, KeyboardEvent);

	// allow use of up and down keys to transfer focus/hover state
	if(KeyboardEvent.GetKey() == EKeys::Up || KeyboardEvent.GetKey() == EKeys::Down)
	{
		return FocusNextWidget( EFocusMoveDirection::Next );
	}

	return FReply::Unhandled();
}
