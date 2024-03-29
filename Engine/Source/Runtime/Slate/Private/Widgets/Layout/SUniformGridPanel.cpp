// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "LayoutUtils.h"


void SUniformGridPanel::Construct( const FArguments& InArgs )
{
	SlotPadding = InArgs._SlotPadding;
	NumColumns = 0;
	NumRows = 0;
	MinDesiredSlotWidth = InArgs._MinDesiredSlotWidth.Get();
	MinDesiredSlotHeight = InArgs._MinDesiredSlotHeight.Get();

	Children.Reserve( InArgs.Slots.Num() );
	for (int32 ChildIndex=0; ChildIndex < InArgs.Slots.Num(); ChildIndex++)
	{
		FSlot* ChildSlot = InArgs.Slots[ChildIndex];
		Children.Add( ChildSlot );
		// A single cell at (N,M) means our grid size is (N+1, M+1)
		NumColumns = FMath::Max( ChildSlot->Column+1, NumColumns );
		NumRows = FMath::Max( ChildSlot->Row+1, NumRows );
	}
}


void SUniformGridPanel::ArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	const FVector2D CellSize(AllottedGeometry.Size.X / NumColumns, AllottedGeometry.Size.Y / NumRows);
	const FMargin& CurrentSlotPadding(SlotPadding.Get());
	for (int32 ChildIndex=0; ChildIndex < Children.Num(); ++ChildIndex)
	{
		const FSlot& Child = Children[ChildIndex];
		const EVisibility ChildVisibility = Child.Widget->GetVisibility();
		if ( ArrangedChildren.Accepts(ChildVisibility) )
		{
			// Do the standard arrangement of elements within a slot
			// Takes care of alignment and padding.
			AlignmentArrangeResult XAxisResult = AlignChild<Orient_Horizontal>( CellSize.X, Child, CurrentSlotPadding );
			AlignmentArrangeResult YAxisResult = AlignChild<Orient_Vertical>( CellSize.Y, Child, CurrentSlotPadding );

			ArrangedChildren.AddWidget( ChildVisibility,
				AllottedGeometry.MakeChild(Child.Widget,
				FVector2D(CellSize.X*Child.Column + XAxisResult.Offset, CellSize.Y*Child.Row + YAxisResult.Offset),
				FVector2D(XAxisResult.Size, YAxisResult.Size)
			));
		}
		
	}
}


FVector2D SUniformGridPanel::ComputeDesiredSize() const
{
	FVector2D MaxChildDesiredSize = FVector2D::ZeroVector;
	const FVector2D SlotPaddingDesiredSize = SlotPadding.Get().GetDesiredSize();
	
	const float CachedMinDesiredSlotWidth = MinDesiredSlotWidth.Get();
	const float CachedMinDesiredSlotHeight = MinDesiredSlotHeight.Get();
	
	for ( int32 ChildIndex=0; ChildIndex < Children.Num(); ++ChildIndex )
	{
		const FSlot& Child = Children[ ChildIndex ];
		if (Child.Widget->GetVisibility() != EVisibility::Collapsed)
		{
			FVector2D ChildDesiredSize = Child.Widget->GetDesiredSize() + SlotPaddingDesiredSize;

			ChildDesiredSize.X = FMath::Max( ChildDesiredSize.X, CachedMinDesiredSlotWidth);
			ChildDesiredSize.Y = FMath::Max( ChildDesiredSize.Y, CachedMinDesiredSlotHeight);

			MaxChildDesiredSize.X = FMath::Max( MaxChildDesiredSize.X, ChildDesiredSize.X );
			MaxChildDesiredSize.Y = FMath::Max( MaxChildDesiredSize.Y, ChildDesiredSize.Y );
		}
	}

	return FVector2D( NumColumns*MaxChildDesiredSize.X, NumRows*MaxChildDesiredSize.Y );
}


FChildren* SUniformGridPanel::GetChildren()
{
	return &Children;
}


SUniformGridPanel::FSlot& SUniformGridPanel::AddSlot( int32 Column, int32 Row )
{
	FSlot& NewSlot = SUniformGridPanel::Slot(Column, Row);

	// A single cell at (N,M) means our grid size is (N+1, M+1)
	NumColumns = FMath::Max( Column+1, NumColumns );
	NumRows = FMath::Max( Row+1, NumRows );

	Children.Add( &NewSlot );

	return NewSlot;
}


void SUniformGridPanel::ClearChildren()
{
	NumColumns = 0;
	NumRows = 0;
	Children.Empty();
}