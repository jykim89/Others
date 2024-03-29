// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UICommandDragDropOp.cpp: Implements the FUICommandDragDropOp class.
=============================================================================*/

#include "SlatePrivatePCH.h"


TSharedRef<FUICommandDragDropOp> FUICommandDragDropOp::New( TSharedRef<const FUICommandInfo> InCommandInfo, FName InOriginMultiBox, TSharedPtr<SWidget> CustomDectorator, FVector2D DecoratorOffset )
{
	TSharedRef<FUICommandDragDropOp> Operation = MakeShareable(new FUICommandDragDropOp(InCommandInfo, InOriginMultiBox, CustomDectorator, DecoratorOffset));
	Operation->Construct();

	return Operation;
}


void FUICommandDragDropOp::OnDragged( const class FDragDropEvent& DragDropEvent )
{
	CursorDecoratorWindow->SetOpacity(0.85f);
	CursorDecoratorWindow->MoveWindowTo(DragDropEvent.GetScreenSpacePosition() + Offset);
}


void FUICommandDragDropOp::OnDrop( bool bDropWasHandled, const FPointerEvent& MouseEvent )
{
	FDragDropOperation::OnDrop(bDropWasHandled, MouseEvent);
	OnDropNotification.ExecuteIfBound();
}


TSharedPtr<SWidget> FUICommandDragDropOp::GetDefaultDecorator() const
{
	TSharedPtr<SWidget> Content;

	if (CustomDecorator.IsValid())
	{
		Content = CustomDecorator;
	}
	else
	{
		Content = SNew(STextBlock)
			.Text(UICommand->GetLabel());
	}

	// Create hover widget
	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.Content()
		[	
			Content.ToSharedRef()
		];
}
