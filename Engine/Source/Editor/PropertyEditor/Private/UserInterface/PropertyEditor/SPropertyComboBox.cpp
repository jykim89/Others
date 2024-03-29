// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorPrivatePCH.h"
#include "SPropertyComboBox.h"

#define LOCTEXT_NAMESPACE "PropertyComboBox"

void SPropertyComboBox::Construct( const FArguments& InArgs )
{
	ComboItemList = InArgs._ComboItemList.Get();
	RestrictedList = InArgs._RestrictedList.Get();
	ToolTipList = InArgs._ToolTipList;
	OnSelectionChanged = InArgs._OnSelectionChanged;
	Font = InArgs._Font;

	// find the initially selected item, if any
	const FString VisibleText = InArgs._VisibleText.Get();
	TSharedPtr<FString> InitiallySelectedItem = NULL;
	for(int32 ItemIndex = 0; ItemIndex < ComboItemList.Num(); ++ItemIndex)
	{
		if(*ComboItemList[ItemIndex].Get() == VisibleText)
		{
			SetToolTipText(TAttribute<FString>(*ToolTipList[ItemIndex]));
			InitiallySelectedItem = ComboItemList[ItemIndex];
			break;
		}
	}

	SComboBox< TSharedPtr<FString> >::Construct(SComboBox< TSharedPtr<FString> >::FArguments()
		.Content()
		[
			SNew( STextBlock )
			.Text( InArgs._VisibleText )
			.Font( Font )
		]
		.OptionsSource(&ComboItemList)
		.OnGenerateWidget(this, &SPropertyComboBox::OnGenerateComboWidget)
		.OnSelectionChanged(this, &SPropertyComboBox::OnSelectionChangedInternal)
		.OnComboBoxOpening(InArgs._OnComboBoxOpening)
		.InitiallySelectedItem(InitiallySelectedItem)
		);
}

SPropertyComboBox::~SPropertyComboBox()
{
	if (IsOpen())
	{
		SetIsOpen(false);
	}
}

void SPropertyComboBox::SetSelectedItem( const FString& InSelectedItem )
{
	// Look for the item, due to drag and dropping of Blueprints that may not be in this list.
	for(int32 ItemIndex = 0; ItemIndex < ComboItemList.Num(); ++ItemIndex)
	{
		if(*ComboItemList[ItemIndex].Get() == InSelectedItem)
		{
			SetToolTipText(TAttribute<FString>(*ToolTipList[ItemIndex]));

			SComboBox< TSharedPtr<FString> >::SetSelectedItem(ComboItemList[ItemIndex]);
			return;
		}
	}
}

void SPropertyComboBox::SetItemList(TArray< TSharedPtr< FString > >& InItemList, TArray< TSharedPtr< FString > >& InTooltipList, TArray<bool>& InRestrictedList)
{
	ComboItemList = InItemList;
	ToolTipList = InTooltipList;
	RestrictedList = InRestrictedList;
	RefreshOptions();
}

void SPropertyComboBox::OnSelectionChangedInternal( TSharedPtr<FString> InSelectedItem, ESelectInfo::Type SelectInfo )
{
	bool bEnabled = true;
	if (RestrictedList.Num() > 0)
	{
		int32 Index = 0;
		for( ; Index < ComboItemList.Num() ; ++Index )
		{
			if( *ComboItemList[Index] == *InSelectedItem )
				break;
		}

		if ( Index < ComboItemList.Num() )
		{
			bEnabled = !RestrictedList[Index];
		}
	}

	if( bEnabled )
	{
		OnSelectionChanged.ExecuteIfBound( InSelectedItem, SelectInfo );
	}
}

TSharedRef<SWidget> SPropertyComboBox::OnGenerateComboWidget( TSharedPtr<FString> InComboString )
{
	//Find the corresponding tool tip for this combo entry if any
	FString ToolTip;
	bool bEnabled = true;
	if (ToolTipList.Num() > 0)
	{
		int32 Index = ComboItemList.IndexOfByKey(InComboString);
		if (Index >= 0)
		{
			//A list of tool tips should have been populated in a 1 to 1 correspondance
			check(ComboItemList.Num() == ToolTipList.Num());
			ToolTip = *ToolTipList[Index];

			if( RestrictedList.Num() > 0 )
			{
				bEnabled = !RestrictedList[Index];
			}
		}
	}

	return
		SNew( STextBlock )
		.Text( *InComboString )
		.Font( Font )
		.ToolTipText(ToolTip)
		.IsEnabled(bEnabled);
}

FReply SPropertyComboBox::OnKeyDown( const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent )
{
	const FKey Key = InKeyboardEvent.GetKey();

	if(Key == EKeys::Up)
	{
		const int32 SelectionIndex = ComboItemList.Find( GetSelectedItem() );
		if ( SelectionIndex >= 1 )
		{
			if (RestrictedList.Num() > 0)
			{
				// find & select the previous unrestricted item
				for(int32 TestIndex = SelectionIndex - 1; TestIndex >= 0; TestIndex--)
				{
					if(!RestrictedList[TestIndex])
					{
						SComboBox< TSharedPtr<FString> >::SetSelectedItem(ComboItemList[TestIndex]);
						break;
					}
				}
			}
			else
			{
				SComboBox< TSharedPtr<FString> >::SetSelectedItem(ComboItemList[SelectionIndex - 1]);
			}
		}

		return FReply::Handled();
	}
	else if(Key == EKeys::Down)
	{
		const int32 SelectionIndex = ComboItemList.Find( GetSelectedItem() );
		if ( SelectionIndex < ComboItemList.Num() - 1 )
		{
			if (RestrictedList.Num() > 0)
			{
				// find & select the next unrestricted item
				for(int32 TestIndex = SelectionIndex + 1; TestIndex < RestrictedList.Num() && TestIndex < ComboItemList.Num(); TestIndex++)
				{
					if(!RestrictedList[TestIndex])
					{
						SComboBox< TSharedPtr<FString> >::SetSelectedItem(ComboItemList[TestIndex]);
						break;
					}
				}
			}
			else
			{
				SComboBox< TSharedPtr<FString> >::SetSelectedItem(ComboItemList[SelectionIndex + 1]);
			}
		}

		return FReply::Handled();
	}

	return SComboBox< TSharedPtr<FString> >::OnKeyDown( MyGeometry, InKeyboardEvent );
}

#undef LOCTEXT_NAMESPACE
