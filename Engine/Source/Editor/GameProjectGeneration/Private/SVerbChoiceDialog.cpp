// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "GameProjectGenerationPrivatePCH.h"
#include "SVerbChoiceDialog.h"

int32 SVerbChoiceDialog::ShowModal( const FText& InTitle, const FText& InMessage, const TArray<FText>& InButtons )
{
	TSharedRef<SWindow> ModalWindow = SNew(SWindow)
		.Title( InTitle )
		.SizingRule( ESizingRule::Autosized )
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SupportsMinimize(false) 
		.SupportsMaximize(false);

	TSharedRef<SVerbChoiceDialog> MessageBox = SNew(SVerbChoiceDialog)
		.ParentWindow(ModalWindow)
		.Message( InMessage )
		.Buttons( InButtons )
		.WrapMessageAt(640.0f);

	ModalWindow->SetContent( MessageBox );

	GEditor->EditorAddModalWindow(ModalWindow);

	return MessageBox->Response;
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SVerbChoiceDialog::Construct( const FArguments& InArgs )
{
	ParentWindow = InArgs._ParentWindow.Get();
	ParentWindow->SetWidgetToFocusOnActivate(SharedThis(this));
	Response = EAppReturnType::Cancel;

	FSlateFontInfo MessageFont( FEditorStyle::GetFontStyle("StandardDialog.LargeFont"));
	Message = InArgs._Message;
	Buttons = InArgs._Buttons;

	TSharedPtr<SUniformGridPanel> ButtonBox;

	this->ChildSlot
		[	
			SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
						.HAlign(HAlign_Fill)
						.VAlign(VAlign_Fill)
						.FillHeight(1.0f)
						.MaxHeight(550)
						.Padding(12.0f)
						[
							SNew(SScrollBox)

							+ SScrollBox::Slot()
								[
									SNew(STextBlock)
										.Text(Message)
										.Font(MessageFont)
										.WrapTextAt(InArgs._WrapMessageAt)
								]
						]

					+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f)
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.HAlign(HAlign_Left)
								.VAlign(VAlign_Center)
								.Padding(12.0f)
								[
									SNew(SHyperlink)
										.OnNavigate(this, &SVerbChoiceDialog::HandleCopyMessageHyperlinkNavigate)
										.Text( NSLOCTEXT("SVerbChoiceDialog", "CopyMessageHyperlink", "Copy Message") )
										.ToolTipText( NSLOCTEXT("SVerbChoiceDialog", "CopyMessageTooltip", "Copy the text in this message to the clipboard (CTRL+C)") )
								]

							+ SHorizontalBox::Slot()
								.AutoWidth()
								.HAlign(HAlign_Right)
								.VAlign(VAlign_Center)
								.Padding(2.f)
								[
									SAssignNew( ButtonBox, SUniformGridPanel )
										.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
										.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
										.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
								]
						]
				]
		];

	for(int32 Idx = 0; Idx < Buttons.Get().Num(); Idx++)
	{
		ButtonBox->AddSlot(Idx, 0)
			[
				SNew( SButton )
				.Text( Buttons.Get()[Idx] )
				.OnClicked( this, &SVerbChoiceDialog::HandleButtonClicked, Idx )
				.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				.HAlign(HAlign_Center)
			];
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FReply SVerbChoiceDialog::OnKeyDown( const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent )
{
	//see if we pressed the Enter or Spacebar keys
	if( InKeyboardEvent.GetKey() == EKeys::Escape )
	{
		return HandleButtonClicked(EAppReturnType::Cancel);
	}

	if (InKeyboardEvent.GetKey() == EKeys::C && InKeyboardEvent.IsControlDown())
	{
		CopyMessageToClipboard();

		return FReply::Handled();
	}

	//if it was some other button, ignore it
	return FReply::Unhandled();
}

bool SVerbChoiceDialog::SupportsKeyboardFocus() const
{
	return true;
}

void SVerbChoiceDialog::CopyMessageToClipboard( )
{
	FPlatformMisc::ClipboardCopy( *Message.Get().ToString() );
}

FReply SVerbChoiceDialog::HandleButtonClicked( int32 InResponse )
{
	Response = InResponse;
	ParentWindow->RequestDestroyWindow();

	return FReply::Handled();
}

void SVerbChoiceDialog::HandleCopyMessageHyperlinkNavigate( )
{
	CopyMessageToClipboard();
}
