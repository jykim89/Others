// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "LayoutUtils.h"


SGridPanel::FSlot& SGridPanel::AddSlot( int32 Column, int32 Row, SGridPanel::Layer InLayer )
{
	return InsertSlot( new FSlot( Column, Row, InLayer.TheLayer ) );
}

void SGridPanel::ClearChildren()
{
	Columns.Empty();
	Rows.Empty();
	Slots.Empty();
}

void SGridPanel::Construct( const FArguments& InArgs )
{
	TotalDesiredSizes = FVector2D::ZeroVector;

	// Populate the slots such that they are sorted by Layer (order preserved within layers)
	// Also determine the grid size
	for (int32 SlotIndex = 0; SlotIndex < InArgs.Slots.Num(); ++SlotIndex )
	{
		InsertSlot( InArgs.Slots[SlotIndex] );
	}

	ColFillCoefficients = InArgs.ColFillCoefficients;
	RowFillCoefficients = InArgs.RowFillCoefficients;

//	check( ColFillCoefficients.Num() <= Columns.Num() );
//	check( RowFillCoefficients.Num() <= Rows.Num() );
}


int32 SGridPanel::OnPaint( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	FArrangedChildren ArrangedChildren(EVisibility::All);
	this->ArrangeChildren(AllottedGeometry, ArrangedChildren);

	// Because we paint multiple children, we must track the maximum layer id that they produced in case one of our parents
	// wants to an overlay for all of its contents.
	int32 MaxLayerId = LayerId;

	// We need to iterate over slots, because slots know the GridLayers. This isn't available in the arranged children.
	// Some slots do not show up (they are hidden/collapsed). We need a 2nd index to skip over them.
	//
	// GridLayers must ensure that everything in LayerN is below LayerN+1. In other words,
	// every grid layer group must start at the current MaxLayerId (similar to how SOverlay works).
	int32 LastGridLayer = 0;
	for (int32 ChildIndex = 0; ChildIndex < Slots.Num(); ++ChildIndex)
	{
		FArrangedWidget& CurWidget = ArrangedChildren(ChildIndex);
		if (CurWidget.Widget->GetVisibility().IsVisible())
		{
			const FSlot& CurSlot = Slots[ChildIndex];

			FSlateRect ChildClipRect = MyClippingRect.IntersectionWith( CurWidget.Geometry.GetClippingRect() );

			if ( LastGridLayer != CurSlot.LayerParam )
			{
				// We starting a new grid layer group?
				LastGridLayer = CurSlot.LayerParam;
				// Ensure that everything here is drawn on top of 
				// previously drawn grid content.
				LayerId = MaxLayerId+1;
			}

			const int32 CurWidgetsMaxLayerId = CurWidget.Widget->OnPaint(
				CurWidget.Geometry,
				ChildClipRect,
				OutDrawElements,
				LayerId,
				InWidgetStyle,
				ShouldBeEnabled( bParentEnabled )
			);
			
			MaxLayerId = FMath::Max( MaxLayerId, CurWidgetsMaxLayerId );
		}
	}

//#define LAYOUT_DEBUG

#ifdef LAYOUT_DEBUG
	LayerId = LayoutDebugPaint( AllottedGeometry, MyClippingRect, OutDrawElements, LayerId );
#endif

	return MaxLayerId;
}


void SGridPanel::ArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	// PREPARE PHASE
	// Prepare some data for arranging children.
	// FinalColumns will be populated with column sizes that include the stretched column sizes.
	// Then we will build partial sums so that we can easily handle column spans.
	// Repeat the same for rows.

	float ColumnCoeffTotal = 0.0f;
	TArray<float> FinalColumns;
	if ( Columns.Num() > 0 )
	{
		FinalColumns.AddUninitialized(Columns.Num());
		FinalColumns[FinalColumns.Num()-1] = 0.0f;
	}

	float RowCoeffTotal = 0.0f;
	TArray<float> FinalRows;
	if ( Rows.Num() > 0 )
	{
		FinalRows.AddUninitialized(Rows.Num());
		FinalRows[FinalRows.Num()-1] = 0.0f;
	}
	
	FVector2D FlexSpace = AllottedGeometry.Size;
	const int32 ColFillCoeffsLength = ColFillCoefficients.Num();
	for(int32 ColIndex=0; ColIndex < Columns.Num(); ++ColIndex)
	{
		// Compute the total space available for stretchy columns.
		if (ColIndex >= ColFillCoeffsLength || ColFillCoefficients[ColIndex] == 0)
		{
			FlexSpace.X -= Columns[ColIndex];
		}
		else //(ColIndex < ColFillCoeffsLength)
		{
			// Compute the denominator for dividing up the stretchy column space
			ColumnCoeffTotal += ColFillCoefficients[ColIndex];
		}

	}

	for(int32 ColIndex=0; ColIndex < Columns.Num(); ++ColIndex)
	{
		// Figure out how big each column needs to be
		FinalColumns[ColIndex] = (ColIndex < ColFillCoeffsLength && ColFillCoefficients[ColIndex] != 0)
			? (ColFillCoefficients[ColIndex] / ColumnCoeffTotal * FlexSpace.X)
			: Columns[ColIndex];
	}

	const int32 RowFillCoeffsLength = RowFillCoefficients.Num();
	for(int32 RowIndex=0; RowIndex < Rows.Num(); ++RowIndex)
	{
		// Compute the total space available for stretchy rows.
		if (RowIndex >= RowFillCoeffsLength || RowFillCoefficients[RowIndex] == 0)
		{
			FlexSpace.Y -= Rows[RowIndex];
		}
		else //(RowIndex < RowFillCoeffsLength)
		{
			// Compute the denominator for dividing up the stretchy row space
			RowCoeffTotal += RowFillCoefficients[RowIndex];
		}

	}

	for(int32 RowIndex=0; RowIndex < Rows.Num(); ++RowIndex)
	{
		// Compute how big each row needs to be
		FinalRows[RowIndex] = (RowIndex < RowFillCoeffsLength && RowFillCoefficients[RowIndex] != 0)
			? (RowFillCoefficients[RowIndex] / RowCoeffTotal * FlexSpace.Y)
			: Rows[RowIndex];
	}
	
	// Build up partial sums for row and column sizes so that we can handle column and row spans conveniently.
	ComputePartialSums(FinalColumns);
	ComputePartialSums(FinalRows);
	
	// ARRANGE PHASE
	for( int32 SlotIndex=0; SlotIndex < Slots.Num(); ++SlotIndex )
	{
		const FSlot& CurSlot = Slots[SlotIndex];
		const EVisibility ChildVisibility = CurSlot.Widget->GetVisibility();
		if ( ArrangedChildren.Accepts(ChildVisibility) )
		{
			// Figure out the position of this cell.
			const FVector2D ThisCellOffset( FinalColumns[CurSlot.ColumnParam], FinalRows[CurSlot.RowParam] );
			// Figure out the size of this slot; takes row span into account.
			// We use the properties of partial sums arrays to achieve this.
			const FVector2D CellSize(
				FinalColumns[CurSlot.ColumnParam+CurSlot.ColumnSpanParam] - ThisCellOffset.X ,
				FinalRows[CurSlot.RowParam+CurSlot.RowSpanParam] - ThisCellOffset.Y );

			// Do the standard arrangement of elements within a slot
			// Takes care of alignment and padding.
			const FMargin SlotPadding(CurSlot.SlotPadding.Get());
			AlignmentArrangeResult XAxisResult = AlignChild<Orient_Horizontal>( CellSize.X, CurSlot, SlotPadding );
			AlignmentArrangeResult YAxisResult = AlignChild<Orient_Vertical>( CellSize.Y, CurSlot, SlotPadding );

			// Output the result
			ArrangedChildren.AddWidget( ChildVisibility, AllottedGeometry.MakeChild( 
				CurSlot.Widget,
				ThisCellOffset + FVector2D( XAxisResult.Offset, YAxisResult.Offset ) + CurSlot.NudgeParam,
				FVector2D(XAxisResult.Size, YAxisResult.Size)
			));
		}
	}
}


void SGridPanel::CacheDesiredSize()
{
	// The desired size of the grid is the sum of the desires sizes for every row and column.
	ComputeDesiredCellSizes( Columns, Rows );

	TotalDesiredSizes = FVector2D::ZeroVector;
	for (int ColId=0; ColId < Columns.Num(); ++ColId)
	{
		TotalDesiredSizes.X += Columns[ColId];
	}

	for(int RowId=0; RowId < Rows.Num(); ++RowId)
	{
		TotalDesiredSizes.Y += Rows[RowId];
	}
	
	SPanel::CacheDesiredSize();
}


FVector2D SGridPanel::ComputeDesiredSize() const
{
	return TotalDesiredSizes;
}


FChildren* SGridPanel::GetChildren()
{
	return &Slots;
}


FVector2D SGridPanel::GetDesiredSize( const FIntPoint& StartCell, int32 Width, int32 Height ) const
{
	if (Columns.Num() > 0 && Rows.Num() > 0)
	{
		const int32 FirstColumn = FMath::Clamp(StartCell.X, 0, Columns.Num()-1);
		const int32 LastColumn = FMath::Clamp(StartCell.X + Width, 0, Columns.Num()-1);

		const int32 FirstRow = FMath::Clamp(StartCell.Y, 0, Rows.Num()-1);
		const int32 LastRow = FMath::Clamp(StartCell.Y + Height, 0,  Rows.Num()-1);

		return FVector2D( Columns[LastColumn] - Columns[FirstColumn], Rows[LastRow] - Rows[FirstRow] );
	}
	else
	{
		return FVector2D::ZeroVector;
	}
}

void SGridPanel::SetColumnFill( int32 ColumnId, float Coefficient )
{
	if (ColFillCoefficients.Num() <= ColumnId)
	{
		ColFillCoefficients.AddZeroed( ColumnId - ColFillCoefficients.Num() + 1 );
	}
	ColFillCoefficients[ColumnId] = Coefficient;
}

void SGridPanel::SetRowFill( int32 RowId, float Coefficient )
{
	if (RowFillCoefficients.Num() <= RowId)
	{
		RowFillCoefficients.AddZeroed( RowId - RowFillCoefficients.Num() + 1 );
	}
	RowFillCoefficients[RowId] = Coefficient;
}

void SGridPanel::ComputePartialSums( TArray<float>& TurnMeIntoPartialSums )
{
	// We assume there is a 0-valued item already at the end of this array.
	// We need it so that we can  compute the original values
	// by doing Array[N] - Array[N-1];
	

	float LastValue = 0;
	float SumSoFar = 0;
	for(int32 Index=0; Index < TurnMeIntoPartialSums.Num(); ++Index)
	{
		LastValue = TurnMeIntoPartialSums[Index];
		TurnMeIntoPartialSums[Index] = SumSoFar;
		SumSoFar += LastValue;
	}
}


void SGridPanel::DistributeSizeContributions( float SizeContribution, TArray<float>& DistributeOverMe, int32 StartIndex, int32 UpperBound )
{
	for ( int32 Index = StartIndex; Index < UpperBound; ++Index )
	{
		// Each column or row only needs to get bigger if its current size does not already accommodate it.
		DistributeOverMe[Index] = FMath::Max( SizeContribution, DistributeOverMe[Index] );
	}
}


SGridPanel::FSlot& SGridPanel::InsertSlot( SGridPanel::FSlot* InSlot )
{
	bool bInserted = false;

	InSlot->Panel = SharedThis(this);

	// Insert the slot in the list such that slots are sorted by LayerOffset.
	for( int32 SlotIndex=0; !bInserted && SlotIndex < Slots.Num(); ++SlotIndex )
	{
		if ( InSlot->LayerParam < this->Slots[SlotIndex].LayerParam )
		{
			Slots.Insert( InSlot, SlotIndex );
			bInserted = true;
		}
	}

	// We haven't inserted yet, so add to the end of the list.
	if ( !bInserted )
	{
		Slots.Add( InSlot );
	}

	NotifySlotChanged(InSlot);

	return *InSlot;
}

void SGridPanel::NotifySlotChanged(SGridPanel::FSlot* InSlot)
{
	// Keep the size of the grid up to date.
	// We need an extra cell at the end for easily figuring out the size across any number of cells
	// by doing Columns[End] - Columns[Start] or Rows[End] - Rows[Start].
	// The first Columns[]/Rows[] entry will be 0.
	const int32 NumColumnsRequiredForThisSlot = InSlot->ColumnParam + InSlot->ColumnSpanParam + 1;
	if ( NumColumnsRequiredForThisSlot > Columns.Num() )
	{
		Columns.AddZeroed( NumColumnsRequiredForThisSlot - Columns.Num() );
	}

	const int32 NumRowsRequiredForThisSlot = InSlot->RowParam + InSlot->RowSpanParam + 1;
	if ( NumRowsRequiredForThisSlot > Rows.Num() )
	{
		Rows.AddZeroed( NumRowsRequiredForThisSlot - Rows.Num() );
	}
}


void SGridPanel::ComputeDesiredCellSizes( TArray<float>& OutColumns, TArray<float>& OutRows ) const
{
	FMemory::Memzero( OutColumns.GetData(), OutColumns.Num() * sizeof(float) );
	FMemory::Memzero( OutRows.GetData(), OutRows.Num() * sizeof(float) );

	for (int32 SlotIndex=0; SlotIndex<Slots.Num(); ++SlotIndex)
	{
		const FSlot& CurSlot = Slots[SlotIndex];
		if (CurSlot.Widget->GetVisibility() != EVisibility::Collapsed)
		{
			// The slots wants to be as big as its content along with the required padding.
			const FVector2D SlotDesiredSize = CurSlot.Widget->GetDesiredSize() + CurSlot.SlotPadding.Get().GetDesiredSize();

			// If the slot has a (colspan,rowspan) of (1,1) it will only affect that slot.
			// For larger spans, the slots size will be evenly distributed across all the affected slots.
			const FVector2D SizeContribution( SlotDesiredSize.X / CurSlot.ColumnSpanParam, SlotDesiredSize.Y / CurSlot.RowSpanParam );

			// Distribute the size contributions over all the columns and rows that this slot spans
			DistributeSizeContributions( SizeContribution.X, OutColumns, CurSlot.ColumnParam, CurSlot.ColumnParam + CurSlot.ColumnSpanParam );
			DistributeSizeContributions( SizeContribution.Y, OutRows, CurSlot.RowParam, CurSlot.RowParam + CurSlot.RowSpanParam );
		}
	}
}


int32 SGridPanel::LayoutDebugPaint(const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId ) const
{
	float XOffset = 0;
	for (int32 Column=0; Column<Columns.Num(); ++Column)
	{
		float YOffset = 0;
		for (int32 Row=0; Row<Rows.Num(); ++Row)
		{
			FSlateDrawElement::MakeDebugQuad
			(
				OutDrawElements, 
				LayerId,
				AllottedGeometry.ToPaintGeometry( FVector2D(XOffset, YOffset), FVector2D( Columns[Column], Rows[Row] ) ),
				MyClippingRect
			);

			YOffset += Rows[Row];
		}
		XOffset += Columns[Column];
	}

	return LayerId;
}

