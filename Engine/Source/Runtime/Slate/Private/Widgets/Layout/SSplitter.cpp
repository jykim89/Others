// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"


/** The user is not allowed to make any of the splitter's children smaller than this. */
const float MinSplitterChildLength = 20.0f;

SSplitter::FSlot& SSplitter::Slot()
{
	return (*new FSlot());
}


SSplitter::FSlot& SSplitter::AddSlot( int32 AtIndex )
{
	FSlot& NewSlot = SSplitter::Slot();
	if ( AtIndex == INDEX_NONE )
	{
		// No index was specified; just add to the end of the list.
		this->Children.Add( &NewSlot );
	}
	else
	{
		// Add a slot at the desired location
		this->Children.Insert( &NewSlot, AtIndex );
	}

	return NewSlot;
}

SSplitter::FSlot& SSplitter::SlotAt( int32 SlotIndex )
{
	return Children[SlotIndex];
}

void SSplitter::RemoveAt( int32 IndexToRemove )
{
	Children.RemoveAt( IndexToRemove );
}

/**
* Construct this widget
*
* @param	InArgs	The declaration data for this widget
*/
void SSplitter::Construct( const SSplitter::FArguments& InArgs )
{
	check(InArgs._Style);

	OnSplitterFinishedResizing = InArgs._OnSplitterFinishedResizing;
	ResizeMode = InArgs._ResizeMode;
	PhysicalSplitterHandleSize = InArgs._PhysicalSplitterHandleSize;
	HitDetectionSplitterHandleSize = InArgs._HitDetectionSplitterHandleSize;
	Orientation = InArgs._Orientation;
	HoveredHandleIndex = INDEX_NONE;
	bIsResizing = false;
	Style = InArgs._Style;

	for (int32 SlotIndex = 0; SlotIndex < InArgs.Slots.Num(); ++SlotIndex )
	{
		Children.Add( InArgs.Slots[SlotIndex] );
	}
}

/**
* Panels arrange their children in a space described by the AllottedGeometry parameter. The results of the arrangement
* should be returned by appending a FArrangedWidget pair for every child widget. See StackPanel for an example
*/
void SSplitter::ArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	const int32 AxisIndex = (Orientation == Orient_Horizontal) ? 0 : 1;

	// Splitters divide the space between their children proportionately based on size coefficients.
	// The size coefficients are usually determined by a user, who grabs the handled between the child elements
	// and moves them to resize the space available to the children.
	// Some children are sized automatically based on their content; those children cannot be resized.
	//
	// e.g.   _____________________________________ Children
	//       /              /                  /
	//      v              v                  v
	//   + - - - - - + + - - - + + - - - - - - - - - - - - - - +
	//   |           | |       | |                             |
	//   | Child 0   | |Child1 | |  Child2                     |
	//   + - - - - - + + - - - + + - - - - - - - - - - - - - - +
	//                ^         ^
	//                 \_________\___________ Resize handles.

	int32 NumNonCollapsedChildren = 0;
	int32 NumResizeableChildren = 0;
	float CoefficientTotal = 0;
	// Some space is claimed by non-resizeable elements (auto-sized elements)
	float NonResizeableSpace = 0;
	{
		for (int32 ChildIndex=0; ChildIndex < Children.Num(); ++ChildIndex)	
		{
			if (Children[ChildIndex].Widget->GetVisibility() != EVisibility::Collapsed)
			{
				++NumNonCollapsedChildren;

				if ( Children[ChildIndex].SizingRule == SSplitter::SizeToContent )
				{
					NonResizeableSpace += Children[ChildIndex].Widget->GetDesiredSize()[AxisIndex];
				}
				else // SizingRule == SSplitter::FractionOfParent
				{
					CoefficientTotal += Children[ChildIndex].SizeValue.Get();
				}
			}			
		}
	}



	// The user-sizeable children must make room for the resize handles and for auto-sized children.
	const float SpaceNeededForHandles = FMath::Max(0, NumNonCollapsedChildren-1) * PhysicalSplitterHandleSize;
	const float ResizeableSpace = AllottedGeometry.Size.Component(AxisIndex) - SpaceNeededForHandles - NonResizeableSpace;

	// Arrange the children horizontally or vertically.
	float XOffset = 0;
	for (int32 ChildIndex=0; ChildIndex < Children.Num(); ++ChildIndex)
	{
		const FSlot& CurSlot = Children[ChildIndex];

		const float ChildSpace = ( CurSlot.SizingRule == SSplitter::SizeToContent )
			? CurSlot.Widget->GetDesiredSize()[AxisIndex]
		: ResizeableSpace * CurSlot.SizeValue.Get() / CoefficientTotal;

		const EVisibility ChildVisibility = CurSlot.Widget->GetVisibility();

		// If the output array wants arranged children
		if ( ArrangedChildren.Accepts(ChildVisibility) )
		{
			if (Orientation == Orient_Horizontal)
			{
				ArrangedChildren.AddWidget( ChildVisibility, AllottedGeometry.MakeChild( CurSlot.Widget, FVector2D(XOffset, 0), FVector2D( ChildSpace, AllottedGeometry.Size.Y ) ) );
			}
			else
			{
				ArrangedChildren.AddWidget( ChildVisibility, AllottedGeometry.MakeChild( CurSlot.Widget, FVector2D(0, XOffset), FVector2D( AllottedGeometry.Size.X, ChildSpace ) ) );
			}
		}

		// Advance to the next slot. If the child is collapsed, it takes up no room and does not need a splitter
		if ( ChildVisibility != EVisibility::Collapsed)
		{
			XOffset += ChildSpace + PhysicalSplitterHandleSize;
		}
	}
}


int32 SSplitter::OnPaint( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	FArrangedChildren ArrangedChildren( EVisibility::Visible );
	ArrangeChildren( AllottedGeometry, ArrangedChildren );

	int32 MaxLayerId = PaintArrangedChildren( ArrangedChildren, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled );

	const FSlateBrush* NormalHandleBrush = &Style->HandleNormalBrush;

	// Draw the splitter above any children
	MaxLayerId += 1;

	for( int32 ChildIndex = 0; ChildIndex < ArrangedChildren.Num(); ++ChildIndex )
	{
		const FGeometry& GeometryAfterSplitter = ArrangedChildren( FMath::Clamp(ChildIndex + 1, 0, ArrangedChildren.Num()-1) ).Geometry;

		const float HalfHitDetectionSplitterHandleSize = ( HitDetectionSplitterHandleSize / 2 );
		const float HalfPhysicalSplitterHandleSize = ( PhysicalSplitterHandleSize / 2 );

		FVector2D HandleSize;		
		FVector2D HandlePosition;
		if ( Orientation == Orient_Horizontal )
		{
			HandleSize.Set( PhysicalSplitterHandleSize, GeometryAfterSplitter.Size.Y );
			HandlePosition.Set( -(HalfHitDetectionSplitterHandleSize + HalfPhysicalSplitterHandleSize), 0 );
		}
		else
		{
			HandleSize.Set( GeometryAfterSplitter.Size.X, PhysicalSplitterHandleSize );
			HandlePosition.Set( 0, -(HalfHitDetectionSplitterHandleSize + HalfPhysicalSplitterHandleSize) );
		}

		if (HoveredHandleIndex != ChildIndex)
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				MaxLayerId,
				GeometryAfterSplitter.ToPaintGeometry( HandlePosition, HandleSize, 1.0f ),
				NormalHandleBrush,
				MyClippingRect,
				ShouldBeEnabled( bParentEnabled ),
				InWidgetStyle.GetColorAndOpacityTint()
			);
		}
		else
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				MaxLayerId,
				GeometryAfterSplitter.ToPaintGeometry( HandlePosition, HandleSize, 1.0f ),
				&Style->HandleHighlightBrush,
				MyClippingRect,
				ShouldBeEnabled( bParentEnabled ),
				InWidgetStyle.GetColorAndOpacityTint()
			);	
		}
	}

	return MaxLayerId;
}

template<EOrientation Orientation>
static FVector2D ComputeDesiredSizeForSplitter( const float PhysicalSplitterHandleSize, const TPanelChildren<SSplitter::FSlot>& Children )
{
	FVector2D MyDesiredSize(0,0);

	int32 NumNonCollapsed = 0;
	for (int32 ChildIndex=0; ChildIndex < Children.Num(); ++ChildIndex)
	{
		const SSplitter::FSlot& CurSlot = Children[ChildIndex];
		const EVisibility ChildVisibility = CurSlot.Widget->GetVisibility();
		if ( ChildVisibility != EVisibility::Collapsed )
		{
			++NumNonCollapsed;

			FVector2D ChildDesiredSize( CurSlot.Widget->GetDesiredSize() );
			if ( Orientation == Orient_Horizontal )
			{
				MyDesiredSize.X += ChildDesiredSize.X;
				MyDesiredSize.Y = FMath::Max(ChildDesiredSize.Y, MyDesiredSize.Y);
			}
			else
			{
				MyDesiredSize.X = FMath::Max(ChildDesiredSize.X, MyDesiredSize.X);				
				MyDesiredSize.Y += ChildDesiredSize.Y;
			}
		}
	}

	float SpaceNeededForHandles = FMath::Max(0, NumNonCollapsed-1) * PhysicalSplitterHandleSize;
	if (Orientation == Orient_Horizontal)
	{
		MyDesiredSize.X += SpaceNeededForHandles;
	}
	else
	{
		MyDesiredSize.Y += SpaceNeededForHandles;
	}

	return MyDesiredSize;
}

/**
* A Panel's desired size in the space required to arrange of its children on the screen while respecting all of
* the children's desired sizes and any layout-related options specified by the user. See StackPanel for an example.
*/
FVector2D SSplitter::ComputeDesiredSize() const
{
	FVector2D MyDesiredSize = (Orientation == Orient_Horizontal)
		? ComputeDesiredSizeForSplitter<Orient_Horizontal>( PhysicalSplitterHandleSize, Children )
		: ComputeDesiredSizeForSplitter<Orient_Vertical>( PhysicalSplitterHandleSize, Children );

	return MyDesiredSize;
}

/**
* All widgets must provide a way to access their children in a layout-agnostic way.
* Panels store their children in Slots, which creates a dilemma. Most panels
* can store their children in a TPanelChildren<Slot>, where the Slot class
* provides layout information about the child it stores. In that case
* GetChildren should simply return the TPanelChildren<Slot>. See StackPanel for an example.
*/
FChildren* SSplitter::GetChildren()
{
	return &Children;
}

/**
* The system calls this method to notify the widget that a mouse button was pressed within it. This event is bubbled.
*
* @param MyGeometry The Geometry of the widget receiving the event
* @param MouseEvent Information about the input event
*
* @return Whether the event was handled along with possible requests for the system to take action.
*/
FReply SSplitter::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && HoveredHandleIndex != INDEX_NONE )
	{
		bIsResizing = true;
		return FReply::Handled().CaptureMouse( SharedThis(this) );
	}
	else
	{
		return FReply::Unhandled();
	}
}

/**
* The system calls this method to notify the widget that a mouse button was release within it. This event is bubbled.
*
* @param MyGeometry The Geometry of the widget receiving the event
* @param MouseEvent Information about the input event
*
* @return Whether the event was handled along with possible requests for the system to take action.
*/
FReply SSplitter::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bIsResizing == true )
	{
		OnSplitterFinishedResizing.ExecuteIfBound();

		bIsResizing = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

/**
* The system calls this method to notify the widget that a mouse moved within it. This event is bubbled.
*
* @param MyGeometry The Geometry of the widget receiving the event
* @param MouseEvent Information about the input event
*
* @return Whether the event was handled along with possible requests for the system to take action.
*/
FReply SSplitter::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	const FVector2D LocalMousePosition = MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() );

	FArrangedChildren ArrangedChildren(EVisibility::All);
	ArrangeChildren( MyGeometry, ArrangedChildren );

	if ( bIsResizing )
	{
		if ( !MouseEvent.GetCursorDelta().IsZero() )
		{
			if (Orientation == Orient_Horizontal)
			{
				HandleResizing<Orient_Horizontal>( PhysicalSplitterHandleSize, ResizeMode, HoveredHandleIndex, LocalMousePosition, Children, ArrangedChildren );
			}
			else
			{
				HandleResizing<Orient_Vertical>( PhysicalSplitterHandleSize, ResizeMode, HoveredHandleIndex, LocalMousePosition, Children, ArrangedChildren );
			}
		}

		return FReply::Handled();
	}
	else
	{	
		// Hit test which handle we are hovering over.		
		HoveredHandleIndex = (Orientation == Orient_Horizontal)
			? GetHandleBeingResizedFromMousePosition<Orient_Horizontal>( PhysicalSplitterHandleSize, HitDetectionSplitterHandleSize, LocalMousePosition, ArrangedChildren )
			: GetHandleBeingResizedFromMousePosition<Orient_Vertical>( PhysicalSplitterHandleSize, HitDetectionSplitterHandleSize, LocalMousePosition, ArrangedChildren );

		if (HoveredHandleIndex != INDEX_NONE)
		{
			if ( FindResizeableSlotBeforeHandle(HoveredHandleIndex, Children) <= INDEX_NONE || FindResizeableSlotAfterHandle(HoveredHandleIndex, Children) >= Children.Num() )
			{
				HoveredHandleIndex = INDEX_NONE;
			}
		}

		return FReply::Unhandled();
	}

}

void SSplitter::OnMouseLeave( const FPointerEvent& MouseEvent )
{
	if ( !bIsResizing )
	{
		HoveredHandleIndex = INDEX_NONE;
	}
}

/**
* The system asks each widget under the mouse to provide a cursor. This event is bubbled.
* 
* @return FCursorReply::Unhandled() if the event is not handled; return FCursorReply::Cursor() otherwise.
*/
FCursorReply SSplitter::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const
{
	const FVector2D LocalMousePosition = MyGeometry.AbsoluteToLocal( CursorEvent.GetScreenSpacePosition() );

	FArrangedChildren ArrangedChildren(EVisibility::All);
	ArrangeChildren( MyGeometry, ArrangedChildren );

	// Hit test which handle we are hovering over.		
	const int32 CurrentHoveredHandleIndex = (Orientation == Orient_Horizontal)
		? GetHandleBeingResizedFromMousePosition<Orient_Horizontal>( PhysicalSplitterHandleSize, HitDetectionSplitterHandleSize, LocalMousePosition, ArrangedChildren )
		: GetHandleBeingResizedFromMousePosition<Orient_Vertical>( PhysicalSplitterHandleSize, HitDetectionSplitterHandleSize, LocalMousePosition, ArrangedChildren );

	if (CurrentHoveredHandleIndex != INDEX_NONE)
	{
		return ( Orientation == Orient_Horizontal )
			? FCursorReply::Cursor( EMouseCursor::ResizeLeftRight )
			: FCursorReply::Cursor( EMouseCursor::ResizeUpDown );
	}
	else
	{
		return FCursorReply::Unhandled();
	}
}

/**
* Change the orientation of the splitter
*
* @param NewOrientation  Should the splitter be horizontal or vertical
*/
void SSplitter::SetOrientation( EOrientation NewOrientation )
{
	Orientation = NewOrientation;
}

/**
* @return the current orientation of the splitter.
*/
EOrientation SSplitter::GetOrientation() const
{
	return Orientation;
}


SSplitter::SSplitter()
	: HoveredHandleIndex( INDEX_NONE )
	, bIsResizing( false )
	, Orientation( Orient_Horizontal )
	, Style( nullptr )
{
}


int32 SSplitter::FindResizeableSlotBeforeHandle( int32 DraggedHandle, const TPanelChildren<FSlot>& Children )
{
	// Resizing collapsed or autosizing slots does not make sense (their size is predetermined).
	// Search out from the DraggedHandle to find the first non-collapsed, non-autosizing slot we can resize.
	int32 SlotBeforeDragHandle = DraggedHandle;
	while ( SlotBeforeDragHandle >= 0 && (Children[SlotBeforeDragHandle].Widget->GetVisibility() == EVisibility::Collapsed || Children[SlotBeforeDragHandle].SizingRule.Get() == SSplitter::SizeToContent ) )
	{
		--SlotBeforeDragHandle;
	}

	return SlotBeforeDragHandle;
}


int32 SSplitter::FindResizeableSlotAfterHandle( int32 DraggedHandle, const TPanelChildren<FSlot>& Children )
{
	const int32 NumChildren = Children.Num();

	// But the slots list does contain collapsed children! Make sure that we are not resizing a collapsed slot.
	// We also cannot resize auto-sizing slots.
	int32 SlotAfterDragHandle = DraggedHandle+1;
	{
		while( SlotAfterDragHandle < NumChildren && (Children[SlotAfterDragHandle].Widget->GetVisibility() == EVisibility::Collapsed || Children[SlotAfterDragHandle].SizingRule.Get() == SSplitter::SizeToContent) )
		{
			++SlotAfterDragHandle;
		}
	}

	return SlotAfterDragHandle;
}


void SSplitter::FindAllResizeableSlotsAfterHandle( int32 DraggedHandle, const TPanelChildren<FSlot>& Children, TArray< int32 >& OutSlotIndicies )
{
	const int32 NumChildren = Children.Num();

	for (int SlotIndex = DraggedHandle+1; SlotIndex < NumChildren; SlotIndex++)
	{
		if (Children[ SlotIndex ].Widget->GetVisibility() == EVisibility::Collapsed || Children[ SlotIndex ].SizingRule.Get() == SSplitter::SizeToContent)
		{
			continue;
		}

		OutSlotIndicies.Add( SlotIndex );
	}
};


template<EOrientation SplitterOrientation>
void SSplitter::HandleResizing( const float PhysicalSplitterHandleSize, const ESplitterResizeMode::Type ResizeMode, int32 DraggedHandle, const FVector2D& LocalMousePos, TPanelChildren<FSlot>& Children, FArrangedChildren& ChildGeometries )
{
	const int32 NumChildren = Children.Num();

	const int32 AxisIndex = (SplitterOrientation == Orient_Horizontal) ? 0 : 1;

	// Note:
	//  - Prev vs. Next refers to the widgets in the order they are laid out (left->right, top->bottom).
	//  - New vs. Old refers to the Old values for width/height vs. the post-resize values.

	const float HandlePos = ChildGeometries(DraggedHandle+1).Geometry.Position.Component(AxisIndex) - PhysicalSplitterHandleSize / 2;
	float Delta = LocalMousePos.Component(AxisIndex) - HandlePos;

	const int32 SlotBeforeDragHandle = FindResizeableSlotBeforeHandle( DraggedHandle, Children );

	TArray< int32 > SlotsAfterDragHandleIndicies;
	if ( ResizeMode == ESplitterResizeMode::Fixed )
	{
		const int32 SlotAfterDragHandle = FindResizeableSlotAfterHandle( DraggedHandle, Children );

		if ( SlotAfterDragHandle < NumChildren )
		{
			SlotsAfterDragHandleIndicies.Add( SlotAfterDragHandle );
		}
	}
	else if ( ResizeMode == ESplitterResizeMode::Fill )
	{
		FindAllResizeableSlotsAfterHandle( DraggedHandle, Children, /*OUT*/ SlotsAfterDragHandleIndicies );
	}

	if ( SlotBeforeDragHandle >= 0 && SlotsAfterDragHandleIndicies.Num() > 0 )
	{
		struct FSlotInfo 
		{
			FSlot* Slot;
			FGeometry* Geometry;
			float NewSize;
		};

		TArray< FSlotInfo > SlotsAfterDragHandle;
		for (int SlotIndex = 0; SlotIndex < SlotsAfterDragHandleIndicies.Num(); SlotIndex++)
		{
			FSlotInfo SlotInfo;

			SlotInfo.Slot = &Children[ SlotsAfterDragHandleIndicies[ SlotIndex ] ];
			SlotInfo.Geometry = &ChildGeometries( SlotsAfterDragHandleIndicies[ SlotIndex ] ).Geometry;
			SlotInfo.NewSize = SlotInfo.Geometry->Size.Component( AxisIndex );

			SlotsAfterDragHandle.Add( SlotInfo );
		}

		// Get references the prev and next children and their layout settings so that we can modify them.
		FSlot& PrevChild = Children[SlotBeforeDragHandle];
		FGeometry& PrevChildGeom = ChildGeometries(SlotBeforeDragHandle).Geometry;

		// Compute the new sizes of the children
		float NewPrevChildLength = ClampChild( PrevChildGeom.Size.Component(AxisIndex) + Delta );
		Delta = NewPrevChildLength - PrevChildGeom.Size.Component(AxisIndex);

		// Distribute the Delta across the affected slots after the drag handle
		float UnusedDelta = Delta;
		for (int DistributionCount = 0; DistributionCount < SlotsAfterDragHandle.Num() && UnusedDelta != 0; DistributionCount++)
		{
			float DividedDelta = UnusedDelta / SlotsAfterDragHandle.Num();
			UnusedDelta = 0;
			for (int SlotIndex = 0; SlotIndex < SlotsAfterDragHandle.Num(); SlotIndex++)
			{
				FSlotInfo& SlotInfo = SlotsAfterDragHandle[ SlotIndex ];

				float CurrentSize = ClampChild( SlotInfo.Geometry->Size.Component(AxisIndex) );
				SlotInfo.NewSize = ClampChild( CurrentSize - DividedDelta );

				// If one of the slots couldn't be fully adjusted by the delta due to min/max constraints then
				// the leftover delta needs to be evenly distributed to all of the other slots
				UnusedDelta += SlotInfo.NewSize - ( CurrentSize - DividedDelta );
			}
		}

		Delta = Delta - UnusedDelta;

		// PrevChildLength needs to be updated: it's value has to take into account the next child's min/max restrictions
		NewPrevChildLength = ClampChild( PrevChildGeom.Size.Component(AxisIndex) + Delta );

		// Cells being resized are both stretch values -> redistribute the stretch coefficients proportionately
		// to match the new child sizes on the screen.
		{
			float TotalLength = NewPrevChildLength;
			float TotalStretchCoefficients = PrevChild.SizeValue.Get();

			for (int SlotIndex = 0; SlotIndex < SlotsAfterDragHandle.Num(); SlotIndex++)
			{
				FSlotInfo SlotInfo = SlotsAfterDragHandle[ SlotIndex ];

				TotalLength += SlotInfo.NewSize;
				TotalStretchCoefficients += SlotInfo.Slot->SizeValue.Get();
			}

			const float NewPrevChildSize = ( TotalStretchCoefficients * NewPrevChildLength / TotalLength );

			if (PrevChild.OnSlotResized_Handler.IsBound())
			{
				PrevChild.OnSlotResized_Handler.Execute( NewPrevChildSize );
			}
			else
			{
				PrevChild.SizeValue = NewPrevChildSize;
			}

			for (int SlotIndex = 0; SlotIndex < SlotsAfterDragHandle.Num(); SlotIndex++)
			{
				FSlotInfo SlotInfo = SlotsAfterDragHandle[ SlotIndex ];

				const float NewNextChildSize = ( TotalStretchCoefficients * SlotInfo.NewSize / TotalLength );

				if (SlotInfo.Slot->OnSlotResized_Handler.IsBound())
				{
					SlotInfo.Slot->OnSlotResized_Handler.Execute(NewNextChildSize);
				}
				else
				{
					SlotInfo.Slot->SizeValue = NewNextChildSize;
				}
			}
		}
	}
}

/**
* @param ProposedSize  A size that a child would like to be
*
* @return A size that is clamped against the minimum size allowed for children.
*/
float SSplitter::ClampChild( float ProposedSize )
{
	return FMath::Max( MinSplitterChildLength, ProposedSize );
}


template<EOrientation SplitterOrientation>
int32 SSplitter::GetHandleBeingResizedFromMousePosition( float PhysicalSplitterHandleSize, float HitDetectionSplitterHandleSize, FVector2D LocalMousePos, FArrangedChildren& ChildGeometries )
{
	const int32 AxisIndex = (SplitterOrientation == Orient_Horizontal) ? 0 : 1;
	const float HalfHitDetectionSplitterHandleSize = ( HitDetectionSplitterHandleSize / 2 );
	const float HalfPhysicalSplitterHandleSize = ( PhysicalSplitterHandleSize / 2 );

	// Search for the two widgets between which the cursor currently resides.
	for ( int32 ChildIndex = 1; ChildIndex < ChildGeometries.Num(); ++ChildIndex )
	{
		FGeometry& PrevChild = ChildGeometries(ChildIndex - 1).Geometry;
		FGeometry& NextChild = ChildGeometries(ChildIndex).Geometry;
		float PrevBound = PrevChild.Position.Component(AxisIndex) + PrevChild.Size.Component(AxisIndex) - HalfHitDetectionSplitterHandleSize + HalfPhysicalSplitterHandleSize;
		float NextBound = NextChild.Position.Component(AxisIndex) + HalfHitDetectionSplitterHandleSize - HalfPhysicalSplitterHandleSize;

		if ( LocalMousePos.Component(AxisIndex) > PrevBound && LocalMousePos.Component(AxisIndex) < NextBound )
		{
			return ChildIndex - 1;
		}
	}

	return INDEX_NONE;
}

/********************************************************************
* SSplitter2x2														*
* A splitter which has exactly 4 children and allows simultaneous	*		
* of all children along an axis as well as resizing all children	* 
* by dragging the center of the splitter.							*
********************************************************************/

void SSplitter2x2::Construct( const FArguments& InArgs )
{
	Children.Add( new FSlot(InArgs._TopLeft.Widget) );
	Children.Add( new FSlot(InArgs._BottomLeft.Widget) );
	Children.Add( new FSlot(InArgs._TopRight.Widget) );
	Children.Add( new FSlot(InArgs._BottomRight.Widget) );

	SplitterHandleSize = 5.0f;
	bIsResizing = false;
	ResizingAxis = INDEX_NONE;
}


void SSplitter2x2::ArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	int32 NumNonCollapsedChildren = 0;
	FVector2D CoefficientTotal(0,0);

	check( Children.Num() == 4 );

	// The allotted space for our children is our geometry minus a little space to show splitter handles
	const FVector2D SpaceAllottedForChildren = AllottedGeometry.Size - FVector2D(SplitterHandleSize,SplitterHandleSize);

	// The current offset that the next child should be positioned at.
	FVector2D Offset(0,0);

	for (int32 ChildIndex=0; ChildIndex < Children.Num(); ++ChildIndex)
	{
		const FSlot& CurSlot = Children[ChildIndex];

		// Calculate the amount of space that this child should take up.  
		// It is based on the current percentage of space it should take up which is defined by a user moving the splitters
		FVector2D ChildSpace = SpaceAllottedForChildren * CurSlot.PercentageAttribute.Get();

		const EVisibility ChildVisibility = CurSlot.Widget->GetVisibility();

		// If the child is visible, put them in their spot
		if ( ArrangedChildren.Accepts( ChildVisibility ) )
		{
			ArrangedChildren.AddWidget( ChildVisibility, AllottedGeometry.MakeChild( CurSlot.Widget, Offset, ChildSpace ) );
		}

		// Advance to the next slot. If the child is collapsed, it takes up no room and does not need a splitter
		if( ChildIndex == 1 )
		{
			// ChildIndex of 1 means we are starting the next column so reset the Y offset.
			Offset.Y = 0.0f;
			Offset += FVector2D( ChildSpace.X + SplitterHandleSize, 0);
		}
		else
		{
			Offset += FVector2D( 0, ChildSpace.Y + SplitterHandleSize );
		}
	}
}



FVector2D SSplitter2x2::ComputeDesiredSize() const
{
	return FVector2D(100,100);
}


FChildren* SSplitter2x2::GetChildren()
{
	return &Children;
}


FReply SSplitter2x2::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	FReply Reply = FReply::Unhandled();
	if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		const FVector2D LocalMousePos = MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() );
		ResizingAxis = CalculateResizingAxis( MyGeometry, LocalMousePos );
		if ( ResizingAxis != INDEX_NONE )
		{
			bIsResizing = true;
			Reply = FReply::Handled().CaptureMouse( SharedThis(this) );
		}
	}

	return Reply;
}


FReply SSplitter2x2::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bIsResizing == true )
	{
		bIsResizing = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}


FReply SSplitter2x2::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	const FVector2D LocalMousePos = MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() );

	if ( bIsResizing && FSlateApplication::Get().GetMouseCaptor() == AsShared() )
	{	
		FArrangedChildren ArrangedChildren(EVisibility::Visible);
		ArrangeChildren( MyGeometry, ArrangedChildren );

		ResizeChildren( ArrangedChildren, LocalMousePos );
		return FReply::Handled();
	}
	else
	{	
		ResizingAxis = CalculateResizingAxis( MyGeometry, LocalMousePos );
		return FReply::Unhandled();
	}
}


FCursorReply SSplitter2x2::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const 
{
	if( ResizingAxis == 0 )
	{
		return FCursorReply::Cursor( EMouseCursor::ResizeLeftRight );
	}
	else if( ResizingAxis == 1 )
	{
		return FCursorReply::Cursor( EMouseCursor::ResizeUpDown );
	}
	else if( ResizingAxis == 2)
	{
		return FCursorReply::Cursor( EMouseCursor::CardinalCross );
	}

	return FCursorReply::Unhandled();
}

void SSplitter2x2::ResizeChildren( FArrangedChildren& ArrangedChildren, FVector2D LocalMousePos )
{
	// Compute the handle position.  The last child is used because it is always the furthest away from the origin
	const FVector2D HandlePos = ArrangedChildren(3).Geometry.Position - (FVector2D( SplitterHandleSize , SplitterHandleSize ) * .5f);
	FVector2D Delta = LocalMousePos - HandlePos;

	FGeometry& TopLeftGeom = ArrangedChildren(0).Geometry;
	FGeometry& BotLeftGeom = ArrangedChildren(1).Geometry;
	FGeometry& TopRightGeom = ArrangedChildren(2).Geometry;
	FGeometry& BotRightGeom = ArrangedChildren(3).Geometry;

	FSlot& TopLeft = Children[0];
	FSlot& BotLeft = Children[1];
	FSlot& TopRight = Children[2];
	FSlot& BotRight = Children[3];

	if( ResizingAxis == 0 )
	{
		// Ensure deltas along the Y axis are not taken into account
		Delta.Y = 0;
	}
	else if( ResizingAxis == 1 )
	{
		// Ensure deltas along the X axis are not taken into account
		Delta.X = 0;
	}

	// The new size of each child
	FVector2D NewSizeTL;
	FVector2D NewSizeBL;
	FVector2D NewSizeTR;
	FVector2D NewSizeBR;

	if( ResizingAxis == 0)
	{
		NewSizeTL = TopLeftGeom.Size + Delta;
		NewSizeBL = BotLeftGeom.Size + Delta;
		NewSizeTR = TopRightGeom.Size - Delta;
		NewSizeBR = BotRightGeom.Size - Delta;
	}
	else if( ResizingAxis == 1 )
	{
		NewSizeTL = TopLeftGeom.Size + Delta;
		NewSizeBL = BotLeftGeom.Size - Delta;
		NewSizeTR = TopRightGeom.Size + Delta;
		NewSizeBR = BotRightGeom.Size - Delta;
	}
	else
	{
		// Resize X and Y independently as they have different rules for X and Y
		NewSizeTL.X = TopLeftGeom.Size.X + Delta.X;
		NewSizeBL.X = BotLeftGeom.Size.X + Delta.X;
		NewSizeTR.X = TopRightGeom.Size.X - Delta.X;
		NewSizeBR.X = BotRightGeom.Size.X - Delta.X;


		NewSizeTL.Y = TopLeftGeom.Size.Y + Delta.Y;
		NewSizeBL.Y = BotLeftGeom.Size.Y - Delta.Y;
		NewSizeTR.Y = TopRightGeom.Size.Y + Delta.Y;
		NewSizeBR.Y = BotRightGeom.Size.Y - Delta.Y;
	}

	// Clamp all values so they cant be too small
	// Must be done independently on each axis because of how FVector2D handles greater than
	NewSizeTL.X = FMath::Max(MinSplitterChildLength, NewSizeTL.X);
	NewSizeBL.X = FMath::Max(MinSplitterChildLength, NewSizeBL.X);
	NewSizeTR.X = FMath::Max(MinSplitterChildLength, NewSizeTR.X);
	NewSizeBR.X = FMath::Max(MinSplitterChildLength, NewSizeBR.X);

	NewSizeTL.Y = FMath::Max(MinSplitterChildLength, NewSizeTL.Y);
	NewSizeBL.Y = FMath::Max(MinSplitterChildLength, NewSizeBL.Y);
	NewSizeTR.Y = FMath::Max(MinSplitterChildLength, NewSizeTR.Y);
	NewSizeBR.Y = FMath::Max(MinSplitterChildLength, NewSizeBR.Y);

	// Set the percentage space within the allotted area that each child should take up
	FVector2D TotalLength = NewSizeTL+NewSizeBR;
	TopLeft.SetPercentage( NewSizeTL / TotalLength );
	TopRight.SetPercentage( NewSizeTR / TotalLength );
	BotLeft.SetPercentage( NewSizeBL / TotalLength );
	BotRight.SetPercentage( NewSizeBR / TotalLength );

}

int32 SSplitter2x2::CalculateResizingAxis( const FGeometry& MyGeometry, const FVector2D& LocalMousePos ) const
{
	int32 Axis = INDEX_NONE;

	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	ArrangeChildren( MyGeometry, ArrangedChildren );

	// The axis is in the center if it passes all hit tests
	bool bInCenter = true;
	// Search for the two widgets between which the cursor currently resides.
	for ( int32 ChildIndex = 1; ChildIndex < ArrangedChildren.Num(); ++ChildIndex )
	{
		FGeometry& PrevChild = ArrangedChildren(ChildIndex - 1).Geometry;
		FGeometry& NextChild = ArrangedChildren(ChildIndex).Geometry;
		FVector2D PrevBound = PrevChild.Position + PrevChild.Size;
		FVector2D NextBound = NextChild.Position;

		if( LocalMousePos.X > PrevBound.X && LocalMousePos.X < NextBound.X )
		{
			// The mouse is in between two viewports vertically
			// Resizing axis is the X axis
			Axis = 0;
		}
		else if( LocalMousePos.Y > PrevBound.Y && LocalMousePos.Y < NextBound.Y )
		{
			// The mouse is in between two viewports horizontally
			// Resizing axis is the Y axis
			Axis = 1;
		}
		else
		{
			// Failed a hit test
			bInCenter = false;
		}
	}

	if( bInCenter )
	{
		Axis = 2;
	}

	return Axis;
}

TSharedRef< SWidget > SSplitter2x2::GetTopLeftContent()
{
	return Children[ 0 ].Widget;
}


TSharedRef< SWidget > SSplitter2x2::GetBottomLeftContent()
{
	return Children[ 1 ].Widget;
}


TSharedRef< SWidget > SSplitter2x2::GetTopRightContent()
{
	return Children[ 2 ].Widget;
}


TSharedRef< SWidget > SSplitter2x2::GetBottomRightContent()
{
	return Children[ 3 ].Widget;
}


void SSplitter2x2::SetTopLeftContent( TSharedRef< SWidget > TopLeftContent )
{
	Children[ 0 ].Widget = TopLeftContent;
}


void SSplitter2x2::SetBottomLeftContent( TSharedRef< SWidget > BottomLeftContent )
{
	Children[ 1 ].Widget = BottomLeftContent;
}


void SSplitter2x2::SetTopRightContent( TSharedRef< SWidget > TopRightContent )
{
	Children[ 2 ].Widget = TopRightContent;
}


void SSplitter2x2::SetBottomRightContent( TSharedRef< SWidget > BottomRightContent )
{
	Children[ 3 ].Widget = BottomRightContent;
}

void SSplitter2x2::GetSplitterPercentages( TArray< FVector2D >& OutPercentages ) const
{
	OutPercentages.Empty();
	for (int32 i = 0; i < 4; ++i)
	{
		OutPercentages.Add(Children[i].PercentageAttribute.Get());
	}
}

void SSplitter2x2::SetSplitterPercentages( const TArray< FVector2D >& InPercentages )
{
	for (int32 i = 0; i < 4; ++i)
	{
		Children[i].SetPercentage(InPercentages[i]);
	}
}
