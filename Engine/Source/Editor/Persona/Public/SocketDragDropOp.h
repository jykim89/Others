// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

//////////////////////////////////////////////////////////////////////////
// FSocketDragDropOp
class FSocketDragDropOp : public FDragDropOperation
{
public:	
	
	DRAG_DROP_OPERATOR_TYPE(FSocketDragDropOp, FDragDropOperation)

	/** The widget decorator to use */
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const OVERRIDE
	{
		return SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("Graph.ConnectorFeedback.Border"))
			.Content()
			[		
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SImage)
					.Image(this, &FSocketDragDropOp::GetIcon)
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock) 
					.Text( this, &FSocketDragDropOp::GetHoverText )
				]
			];
	}

	/** Passed into STextBlock so Slate can grab the current text for display */
	FString GetHoverText() const
	{
		FString HoverText = FString::Printf(TEXT("Socket %s"), *SocketInfo.Socket->SocketName.ToString() );
		return HoverText;
	}

	/** Passed into SImage so Slate can grab the current icon for display */
	const FSlateBrush* GetIcon( ) const
	{
		return CurrentIconBrush;
	}

	/** Sets the icon to be displayed */
	void SetIcon(const FSlateBrush* InIcon)
	{
		CurrentIconBrush = InIcon;
	}

	/** Accessor for the socket info */
	FSelectedSocketInfo& GetSocketInfo() { return SocketInfo; }

	/** Is this an alt-drag operation? */
	bool IsAltDrag() { return bIsAltDrag; }

	/* Use this function to create a new one of me */
	static TSharedRef<FSocketDragDropOp> New( FSelectedSocketInfo InSocketInfo, bool bInIsAltDrag )
	{
		check( InSocketInfo.Socket );
		TSharedRef<FSocketDragDropOp> Operation = MakeShareable(new FSocketDragDropOp);
		Operation->SocketInfo = InSocketInfo;
		Operation->bIsAltDrag = bInIsAltDrag;
		Operation->SetIcon( FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error")) );
		Operation->Construct();
		return Operation;
	}

private:
	/** The icon to display before the text */
	const FSlateBrush* CurrentIconBrush;

	/** The socket that we're dragging */
	FSelectedSocketInfo SocketInfo;

	/** Is this an alt-drag? */
	bool bIsAltDrag;
};
