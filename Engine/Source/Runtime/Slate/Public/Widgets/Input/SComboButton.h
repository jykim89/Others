// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

DECLARE_DELEGATE( FOnComboBoxOpened )

/**
 * A button that, when clicked, brings up a popup.
 */
class SLATE_API SComboButton : public SMenuAnchor
{
public:

	SLATE_BEGIN_ARGS( SComboButton )
		: _ComboButtonStyle( &FCoreStyle::Get().GetWidgetStyle< FComboButtonStyle >( "ComboButton" ) )
		, _ButtonStyle(nullptr)
		, _ButtonContent()
		, _MenuContent()
		, _HasDownArrow(true)
		, _ForegroundColor(FCoreStyle::Get().GetSlateColor("InvertedForeground"))
		, _ButtonColorAndOpacity(FLinearColor::White)
		, _ContentScale(FVector2D(1,1))
		, _ContentPadding(FMargin(5))
		, _MenuPlacement(MenuPlacement_ComboBox)
		, _HAlign(HAlign_Fill)
		, _VAlign(VAlign_Fill)
		, _Method( SMenuAnchor::CreateNewWindow )
		{}

		SLATE_STYLE_ARGUMENT( FComboButtonStyle, ComboButtonStyle )

		/** The visual style of the button (overrides ComboButtonStyle) */
		SLATE_STYLE_ARGUMENT( FButtonStyle, ButtonStyle )
		
		SLATE_NAMED_SLOT( FArguments, ButtonContent )
		
		/** Optional static menu content.  If the menu content needs to be dynamically built, use the OnGetMenuContent event */
		SLATE_NAMED_SLOT( FArguments, MenuContent )
		
		/** Sets an event handler to generate a widget dynamically when the menu is needed. */
		SLATE_EVENT( FOnGetContent, OnGetMenuContent )
		
		SLATE_EVENT( FOnComboBoxOpened, OnComboBoxOpened )
		SLATE_ARGUMENT( bool, IsFocusable )
		SLATE_ARGUMENT( bool, HasDownArrow )

		SLATE_ATTRIBUTE( FSlateColor, ForegroundColor )
		SLATE_ATTRIBUTE( FSlateColor, ButtonColorAndOpacity )
		SLATE_ATTRIBUTE( FVector2D, ContentScale )
		SLATE_ATTRIBUTE( FMargin, ContentPadding )
		SLATE_ATTRIBUTE( EMenuPlacement, MenuPlacement )
		SLATE_ARGUMENT( EHorizontalAlignment, HAlign )
		SLATE_ARGUMENT( EVerticalAlignment, VAlign )

		/** Spawn a new window or reuse current window for this combo*/
		SLATE_ARGUMENT( SMenuAnchor::EMethod, Method )
		/** The max height of the combo menu list */
		SLATE_ATTRIBUTE( FOptionalSize, MenuHeight )
		/** The max width of the combo menu list */
		SLATE_ATTRIBUTE( FOptionalSize, MenuWidth )
	SLATE_END_ARGS()
	
	/**
	 * Sets the content for this button
	 *
	 * @param	InContent	The widget to use as content for this button
	 */
	void SetMenuContent( const TSharedRef< SWidget >& InContent );

	/** See the OnGetMenuContent event */
	void SetOnGetMenuContent( FOnGetContent InOnGetMenuContent );

	/**
	 * Construct the widget from a declaration
	 *
	 * @param InArgs  The declaration from which to construct
	 */
	void Construct(const FArguments& InArgs);

	void SetMenuContentWidgetToFocus( TWeakPtr<SWidget> InWidgetToFocusPtr )
	{
		WidgetToFocusPtr = InWidgetToFocusPtr;
	}

protected:
	/**
	 * Handle the button being clicked by summoning the ComboButton.
	 */
	virtual FReply OnButtonClicked();

protected:
	/** Area where the button's content resides */
	SHorizontalBox::FSlot* ButtonContentSlot;

	/** Delegate to execute to get the menu content of this button */
	FOnGetContent OnGetMenuContent;

	/** Delegate to execute when the combo list is opened */
	FOnComboBoxOpened OnComboBoxOpened;

	TWeakPtr<SWidget> WidgetToFocusPtr;

	/** Brush to use to add a "menu border" around the drop-down content */
	const FSlateBrush* MenuBorderBrush;

	/** Padding to use to add a "menu border" around the drop-down content */
	FMargin MenuBorderPadding;

	/** The max height of the menu */
	TAttribute<FOptionalSize> MenuWidth;

	/** When specified, ignore the content's desired size and report the.HeightOverride as the Box's desired height. */
	TAttribute<FOptionalSize> MenuHeight;

	/** The content widget, if any, set by the user on creation */
	TWeakPtr<SWidget> ContentWidgetPtr;
};
