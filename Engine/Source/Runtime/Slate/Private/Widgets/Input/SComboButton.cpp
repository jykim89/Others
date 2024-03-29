// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"


void SComboButton::Construct( const FArguments& InArgs )
{
	check(InArgs._ComboButtonStyle);

	// Work out which values we should use based on whether we were given an override, or should use the style's version
	const FButtonStyle* const OurButtonStyle = InArgs._ButtonStyle ? InArgs._ButtonStyle : &InArgs._ComboButtonStyle->ButtonStyle;

	MenuBorderBrush = &InArgs._ComboButtonStyle->MenuBorderBrush;
	MenuBorderPadding = InArgs._ComboButtonStyle->MenuBorderPadding;
	
	OnGetMenuContent = InArgs._OnGetMenuContent;
	OnComboBoxOpened = InArgs._OnComboBoxOpened;
	MenuHeight = InArgs._MenuHeight;
	MenuWidth = InArgs._MenuWidth;
	Method = InArgs._Method;
	ContentWidgetPtr = InArgs._MenuContent.Widget;
	ContentScale = InArgs._ContentScale;
	Placement = InArgs._MenuPlacement;

	TSharedPtr<SHorizontalBox> HBox;

	this->ChildSlot
	.HAlign( InArgs._HAlign )
	.VAlign( InArgs._VAlign )
	[
		SNew(SButton)
		.ContentPadding(FMargin(1,0))
		.ButtonStyle(OurButtonStyle)	
		.ClickMethod( EButtonClickMethod::MouseDown )
		.OnClicked( this, &SComboButton::OnButtonClicked )
		.ContentPadding( InArgs._ContentPadding )
		.ForegroundColor( InArgs._ForegroundColor )
		.ButtonColorAndOpacity( InArgs._ButtonColorAndOpacity )
		.IsFocusable( InArgs._IsFocusable )
		[
			// Button and down arrow on the right
			// +-------------------+---+
			// | Button Content    | v |
			// +-------------------+---+
			SAssignNew(HBox, SHorizontalBox)
			+ SHorizontalBox::Slot()
			.  Expose( ButtonContentSlot )
			.  FillWidth(1)
			.  VAlign(VAlign_Center)
			[
				InArgs._ButtonContent.Widget
			]
			+SHorizontalBox::Slot()
			.  AutoWidth()
			.  HAlign(HAlign_Center)
			.  VAlign(VAlign_Center)
			.  Padding( InArgs._HasDownArrow ? 2 : 0)
			[
				SNew(SImage)
				. Visibility( InArgs._HasDownArrow ? EVisibility::Visible : EVisibility::Collapsed )
				. Image(&InArgs._ComboButtonStyle->DownArrowImage)
				// Inherit tinting from parent
				. ColorAndOpacity( FSlateColor::UseForeground() )
			]
		]
	];
	
	// The menu that pops up when we press the button.
	// We keep this content around, and then put it into a new window when we need to pop
	// it up.
	SetMenuContent( InArgs._MenuContent.Widget );
}

FReply SComboButton::OnButtonClicked()
{
	TSharedPtr<SWidget> Content = NULL;
	if( OnGetMenuContent.IsBound() )
	{
		Content = OnGetMenuContent.Execute();
		SetMenuContent( Content.ToSharedRef() );
	}

	// Button was clicked; show the popup.
	// Do nothing if clicking on the button also dismissed the menu, because we will end up doing the same thing twice.
	this->SetIsOpen( ShouldOpenDueToClick() );	

	// If the menu is open, execute the related delegate.
	if( IsOpen() && OnComboBoxOpened.IsBound() )
	{
		OnComboBoxOpened.Execute();
	}

	// Focusing any newly-created widgets must occur after they have been added to the UI root.
	FReply ButtonClickedReply = FReply::Handled();
	
	TSharedPtr<SWidget> WidgetToFocus = WidgetToFocusPtr.Pin();
	if (!WidgetToFocus.IsValid())
	{
		// no explicitly focused widget, try to focus the content
		WidgetToFocus = Content;
	}

	if (!WidgetToFocus.IsValid())
	{
		// no content, so try to focus the original widget set on construction
		WidgetToFocus = ContentWidgetPtr.Pin();
	}

	if (WidgetToFocus.IsValid())
	{
		ButtonClickedReply.SetKeyboardFocus( WidgetToFocus.ToSharedRef(), EKeyboardFocusCause::SetDirectly );
	}

	return ButtonClickedReply;
}


/**
 * Sets the content for this border
 *
 * @param	InContent	The widget to use as content for the border
 */
void SComboButton::SetMenuContent( const TSharedRef< SWidget >& InContent )
{
	MenuContent = 
		// Wrap in configurable box to restrain height/width of menu
		SNew(SBox)
		.WidthOverride(MenuWidth)
		.HeightOverride(MenuHeight)
		[
			SNew(SBorder)
			.BorderImage(MenuBorderBrush)
			.Padding(MenuBorderPadding)
			[
				InContent
			]
		];
}

void SComboButton::SetOnGetMenuContent(FOnGetContent InOnGetMenuContent)
{
	OnGetMenuContent = InOnGetMenuContent;
}
