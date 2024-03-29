// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"


void STextEntryPopup::Construct( const FArguments& InArgs )
{

	WidgetWithDefaultFocus.Reset();

	this->ChildSlot
	[
		SNew(SBorder)
		. BorderImage(FCoreStyle::Get().GetBrush("PopupText.Background"))
		. Padding(10)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.MaxWidth(InArgs._MaxWidth)
			[
				SAssignNew(Box, SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text( InArgs._Label )
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(WidgetWithDefaultFocus, SEditableTextBox)
					.MinDesiredWidth(10.0f)
					.Text( InArgs._DefaultText )
					.OnTextCommitted( InArgs._OnTextCommitted )
					.OnTextChanged( InArgs._OnTextChanged )
					.HintText( InArgs._HintText )
					.SelectAllTextWhenFocused( InArgs._SelectAllTextWhenFocused.Get() )
					.ClearKeyboardFocusOnCommit( InArgs._ClearKeyboardFocusOnCommit )
				]
			]
		]
	];

	ErrorReporting = InArgs._ErrorReporting;
	if ( ErrorReporting.IsValid() )
	{
		Box->AddSlot()
		.AutoHeight()
		.Padding(3,0)
		[
			ErrorReporting->AsWidget()
		];
	}
}

void STextEntryPopup::FocusDefaultWidget()
{
	// Focus the text widget
	FWidgetPath FocusMe;
	FSlateApplication::Get().GeneratePathToWidgetChecked( WidgetWithDefaultFocus.ToSharedRef(), FocusMe );
	FSlateApplication::Get().SetKeyboardFocus( FocusMe, EKeyboardFocusCause::SetDirectly );
}

void STextEntryPopup::SetError( const FText& InError )
{
	SetError( InError.ToString() );
}

void STextEntryPopup::SetError (const FString& InError)
{
	if ( !ErrorReporting.IsValid() )
	{
		// No error reporting was specified; make a default one
		TSharedPtr<SErrorText> ErrorTextWidget;
		Box->AddSlot()
		.AutoHeight()
		.Padding(3,0)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(3,1)
			[
				SAssignNew( ErrorTextWidget, SErrorText )
			]
		];
		ErrorReporting = ErrorTextWidget;
	}
	ErrorReporting->SetError( InError );
}
