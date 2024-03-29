// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"


/* FDragDropOperation structors
 *****************************************************************************/

FDragDropOperation::~FDragDropOperation()
{
	DestroyCursorDecoratorWindow();
}


/* 
 *****************************************************************************/

void FDragDropOperation::OnDrop( bool bDropWasHandled, const FPointerEvent& MouseEvent )
{
	DestroyCursorDecoratorWindow();
}

void FDragDropOperation::OnDragged( const class FDragDropEvent& DragDropEvent )
{
	if (CursorDecoratorWindow.IsValid())
	{
		CursorDecoratorWindow->MoveWindowTo(DragDropEvent.GetScreenSpacePosition() + FSlateApplicationBase::Get().GetCursorSize());
	}
}

FCursorReply FDragDropOperation::OnCursorQuery()
{
	if ( MouseCursorOverride.IsSet() )
	{
		return FCursorReply::Cursor( MouseCursorOverride.GetValue() );
	}

	if ( MouseCursor.IsSet() )
	{
		return FCursorReply::Cursor(MouseCursor.GetValue());
	}

	return FCursorReply::Unhandled();
}

void FDragDropOperation::SetDecoratorVisibility(bool bVisible)
{
	if (CursorDecoratorWindow.IsValid())
	{
		if (bVisible)
		{
			CursorDecoratorWindow->ShowWindow();
		}
		else
		{
			CursorDecoratorWindow->HideWindow();
		}
	}
}

void FDragDropOperation::SetCursorOverride( TOptional<EMouseCursor::Type> CursorType )
{
	MouseCursorOverride = CursorType;
}


/* FDragDropOperation implementation
 *****************************************************************************/

void FDragDropOperation::Construct()
{
	TSharedPtr<SWidget> DecoratorToUse = GetDefaultDecorator();

	if (DecoratorToUse.IsValid())
	{
		CursorDecoratorWindow = SWindow::MakeCursorDecorator();
		CursorDecoratorWindow->SetContent(DecoratorToUse.ToSharedRef());

		FSlateApplicationBase::Get().AddWindow(CursorDecoratorWindow.ToSharedRef(), true);
	}
}

void FDragDropOperation::DestroyCursorDecoratorWindow()
{
	if (CursorDecoratorWindow.IsValid())
	{
		CursorDecoratorWindow->RequestDestroyWindow();
		CursorDecoratorWindow.Reset();
	}
}


/* FDragDropOperation implementation
 *****************************************************************************/

TSharedRef<FExternalDragOperation> FExternalDragOperation::NewText( const FString& InText )
{
	TSharedRef<FExternalDragOperation> Operation = MakeShareable(new FExternalDragOperation);
	Operation->DragType = DragText;
	Operation->DraggedText = InText;
	Operation->Construct();
	return Operation;
}

TSharedRef<FExternalDragOperation> FExternalDragOperation::NewFiles( const TArray<FString>& InFileNames )
{
	TSharedRef<FExternalDragOperation> Operation = MakeShareable(new FExternalDragOperation);
	Operation->DragType = DragFiles;
	Operation->DraggedFileNames = InFileNames;
	Operation->Construct();
	return Operation;
}
