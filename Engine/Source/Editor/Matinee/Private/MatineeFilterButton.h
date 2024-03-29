// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class SMatineeFilterButton : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SMatineeFilterButton )
		: _Text()
		, _IsChecked( ESlateCheckBoxState::Unchecked )
		, _OnCheckStateChanged()
		, _OnContextMenuOpening()
		{}

		/** Text to show on the button */
		SLATE_ARGUMENT( FText, Text )

		/** Whether the check box is currently in a checked state */
		SLATE_ATTRIBUTE( ESlateCheckBoxState::Type, IsChecked )

		/** Called when the checked state has changed */
		SLATE_EVENT( FOnCheckStateChanged, OnCheckStateChanged )

		/** Delegate to invoke when the context menu should be opening. If it is null, a context menu will not be summoned. */
		SLATE_EVENT( FOnContextMenuOpening, OnContextMenuOpening )

	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct(const FArguments& InArgs);

	// SWidget interface
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE;

private:
	/** Called when the checked state has changed */
	FOnCheckStateChanged OnCheckStateChanged;

	/** Delegate to invoke when the context menu should be opening. If it is null, a context menu will not be summoned. */
	FOnContextMenuOpening OnContextMenuOpening;
};
