// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "MaterialEditorModule.h"
#include "Editor/PropertyEditor/Public/PropertyEditing.h"
#include "MaterialEditor.h"
#include "MaterialEditorDetailCustomization.h"

#define LOCTEXT_NAMESPACE "MaterialEditor"



TSharedRef<IDetailCustomization> FMaterialExpressionParameterDetails::MakeInstance(FOnCollectParameterGroups InCollectGroupsDelegate)
{
	return MakeShareable( new FMaterialExpressionParameterDetails(InCollectGroupsDelegate) );
}

FMaterialExpressionParameterDetails::FMaterialExpressionParameterDetails(FOnCollectParameterGroups InCollectGroupsDelegate)
	: CollectGroupsDelegate(InCollectGroupsDelegate)
{
}

void FMaterialExpressionParameterDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	// for expression parameters all their properties are in one category based on their class name.
	FName DefaultCategory = NAME_None;
	IDetailCategoryBuilder& Category = DetailLayout.EditCategory( DefaultCategory );

	Category.AddProperty("ParameterName");
	
	// Get a handle to the property we are about to edit
	GroupPropertyHandle = DetailLayout.GetProperty( "Group" );

	GroupPropertyHandle->MarkHiddenByCustomization();

	PopulateGroups();

	TSharedPtr<SComboButton> NewComboButton;
	TSharedPtr<SEditableText> NewEditBox;
	TSharedPtr<SListView<TSharedPtr<FString>>> NewListView;
	
	Category.AddCustomRow( GroupPropertyHandle->GetPropertyDisplayName() )
	.NameContent()
	[
		SNew( STextBlock )
		.Text( GroupPropertyHandle->GetPropertyDisplayName() )
		.Font( IDetailLayoutBuilder::GetDetailFont() )
	]
	.ValueContent()
	[
		SAssignNew(NewComboButton, SComboButton)
		.ContentPadding(0)
		.ButtonContent()
		[
			SAssignNew(NewEditBox, SEditableText)
				.Text(this, &FMaterialExpressionParameterDetails::OnGetText)
				.OnTextCommitted(this, &FMaterialExpressionParameterDetails::OnTextCommitted)
		]
		.MenuContent()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(400.0f)
			[
				SAssignNew(NewListView, SListView<TSharedPtr<FString>>)
					.ListItemsSource(&GroupsSource)
					.OnGenerateRow(this, &FMaterialExpressionParameterDetails::MakeDetailsGroupViewWidget)
					.OnSelectionChanged(this, &FMaterialExpressionParameterDetails::OnSelectionChanged)
			]
		]
	];

	GroupComboButton = NewComboButton;
	GroupEditBox = NewEditBox;
	GroupListView = NewListView;
}

void FMaterialExpressionParameterDetails::PopulateGroups()
{
	TArray<FString> Groups;
	CollectGroupsDelegate.ExecuteIfBound(&Groups);

	GroupsSource.Empty();
	for (int32 GroupIdx = 0; GroupIdx < Groups.Num(); ++GroupIdx)
	{
		GroupsSource.Add(MakeShareable(new FString(Groups[GroupIdx])));
	}
}

TSharedRef< ITableRow > FMaterialExpressionParameterDetails::MakeDetailsGroupViewWidget( TSharedPtr<FString> Item, const TSharedRef< STableViewBase >& OwnerTable )
{
	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		[
			SNew(STextBlock) .Text(*Item.Get())
		];
}

void FMaterialExpressionParameterDetails::OnSelectionChanged( TSharedPtr<FString> ProposedSelection, ESelectInfo::Type /*SelectInfo*/ )
{
	if (ProposedSelection.IsValid())
	{
		GroupPropertyHandle->SetValue(*ProposedSelection.Get());
		GroupListView.Pin()->ClearSelection();
		GroupComboButton.Pin()->SetIsOpen(false);
	}
}

void FMaterialExpressionParameterDetails::OnTextCommitted( const FText& InText, ETextCommit::Type /*CommitInfo*/)
{
	GroupPropertyHandle->SetValue(InText.ToString());
	PopulateGroups();
}

FString FMaterialExpressionParameterDetails::OnGetString() const
{
	FString OutText;
	if (GroupPropertyHandle->GetValue(OutText) == FPropertyAccess::MultipleValues)
	{
		return LOCTEXT("MultipleValues", "Multiple Values").ToString();
	}
	return OutText;
}

FText FMaterialExpressionParameterDetails::OnGetText() const
{
	FString NewString = OnGetString();
	return FText::FromString(NewString);
}



TSharedRef<IDetailCustomization> FMaterialExpressionCollectionParameterDetails::MakeInstance()
{
	return MakeShareable( new FMaterialExpressionCollectionParameterDetails() );
}

FMaterialExpressionCollectionParameterDetails::FMaterialExpressionCollectionParameterDetails()
{
}

FString FMaterialExpressionCollectionParameterDetails::GetToolTipText() const
{
	if (ParametersSource.Num() == 1)
	{
		return LOCTEXT("SpecifyCollection", "Specify a Collection to get parameter options").ToString();
	}
	else
	{
		return LOCTEXT("ChooseParameter", "Choose a parameter from the collection").ToString();
	}
}

FString FMaterialExpressionCollectionParameterDetails::GetParameterNameString() const
{
	FString CurrentParameterName;

	FPropertyAccess::Result Result = ParameterNamePropertyHandle->GetValue(CurrentParameterName);
	if( Result == FPropertyAccess::MultipleValues )
	{
		CurrentParameterName = NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values").ToString();
	}

	return CurrentParameterName;
}

bool FMaterialExpressionCollectionParameterDetails::IsParameterNameComboEnabled() const
{
	UObject* CollectionObject = NULL;
	verify(CollectionPropertyHandle->GetValue(CollectionObject) == FPropertyAccess::Success);
	UMaterialParameterCollection* Collection = Cast<UMaterialParameterCollection>(CollectionObject);
	return Collection != nullptr;
}

void FMaterialExpressionCollectionParameterDetails::OnCollectionChanged()
{
	PopulateParameters();
}

void FMaterialExpressionCollectionParameterDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	// for expression parameters all their properties are in one category based on their class name.
	FName DefaultCategory = NAME_None;
	IDetailCategoryBuilder& Category = DetailLayout.EditCategory( DefaultCategory );

	// Get a handle to the property we are about to edit
	ParameterNamePropertyHandle = DetailLayout.GetProperty( "ParameterName" );
	CollectionPropertyHandle = DetailLayout.GetProperty( "Collection" );

	// Register a changed callback on the collection property since we need to update the PropertyName vertical box when it changes
	FSimpleDelegate OnCollectionChangedDelegate = FSimpleDelegate::CreateSP( this, &FMaterialExpressionCollectionParameterDetails::OnCollectionChanged );
	CollectionPropertyHandle->SetOnPropertyValueChanged( OnCollectionChangedDelegate );

	ParameterNamePropertyHandle->MarkHiddenByCustomization();
	CollectionPropertyHandle->MarkHiddenByCustomization();

	PopulateParameters();

	TSharedPtr<SComboButton> NewComboButton;
	TSharedPtr<SListView<TSharedPtr<FString>>> NewListView;

	// This isn't strictly speaking customized, but we need it to appear before the "Parameter Name" property, 
	// so we manually add it and set MarkHiddenByCustomization on it to avoid it being automatically added
	Category.AddProperty(CollectionPropertyHandle);

	Category.AddCustomRow( ParameterNamePropertyHandle->GetPropertyDisplayName() )
	.NameContent()
	[
		SNew( STextBlock )
		.Text( ParameterNamePropertyHandle->GetPropertyDisplayName() )
		.Font( IDetailLayoutBuilder::GetDetailFont() )
	]
	.ValueContent()
	[
		SAssignNew(NewComboButton, SComboButton)
		.IsEnabled(this, &FMaterialExpressionCollectionParameterDetails::IsParameterNameComboEnabled)
		.ContentPadding(0)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(this, &FMaterialExpressionCollectionParameterDetails::GetParameterNameString )
		]
		.MenuContent()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(400.0f)
			[
				SAssignNew(NewListView, SListView<TSharedPtr<FString>>)
				.ListItemsSource(&ParametersSource)
				.OnGenerateRow(this, &FMaterialExpressionCollectionParameterDetails::MakeDetailsGroupViewWidget)
				.OnSelectionChanged(this, &FMaterialExpressionCollectionParameterDetails::OnSelectionChanged)
			]
		]
	];

	ParameterComboButton = NewComboButton;
	ParameterListView = NewListView;

	NewComboButton->SetToolTipText(TAttribute<FString>(GetToolTipText()));
}

void FMaterialExpressionCollectionParameterDetails::PopulateParameters()
{
	UObject* CollectionObject = NULL;
	verify(CollectionPropertyHandle->GetValue(CollectionObject) == FPropertyAccess::Success);
	UMaterialParameterCollection* Collection = Cast<UMaterialParameterCollection>(CollectionObject);

	ParametersSource.Empty();

	if (Collection)
	{
		for (int32 ParameterIndex = 0; ParameterIndex < Collection->ScalarParameters.Num(); ++ParameterIndex)
		{
			ParametersSource.Add(MakeShareable(new FString(Collection->ScalarParameters[ParameterIndex].ParameterName.ToString())));
		}

		for (int32 ParameterIndex = 0; ParameterIndex < Collection->VectorParameters.Num(); ++ParameterIndex)
		{
			ParametersSource.Add(MakeShareable(new FString(Collection->VectorParameters[ParameterIndex].ParameterName.ToString())));
		}
	}

	if (ParametersSource.Num() == 0)
	{
		ParametersSource.Add(MakeShareable(new FString(LOCTEXT("NoParameter", "None").ToString())));
	}
}

TSharedRef< ITableRow > FMaterialExpressionCollectionParameterDetails::MakeDetailsGroupViewWidget( TSharedPtr<FString> Item, const TSharedRef< STableViewBase >& OwnerTable )
{
	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		[
			SNew(STextBlock) .Text(*Item.Get())
		];
}

void FMaterialExpressionCollectionParameterDetails::OnSelectionChanged( TSharedPtr<FString> ProposedSelection, ESelectInfo::Type /*SelectInfo*/ )
{
	if (ProposedSelection.IsValid())
	{
		ParameterNamePropertyHandle->SetValue(*ProposedSelection.Get());
		ParameterListView.Pin()->ClearSelection();
		ParameterComboButton.Pin()->SetIsOpen(false);
	}
}


#undef LOCTEXT_NAMESPACE
