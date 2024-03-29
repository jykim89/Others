// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "InputSettingsDetails.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "InputSettingsDetails"



////////////////////////////////
// FActionMappingsNodeBuilder //
////////////////////////////////

FActionMappingsNodeBuilder::FActionMappingsNodeBuilder( IDetailLayoutBuilder* InDetailLayoutBuilder, const TSharedPtr<IPropertyHandle>& InPropertyHandle )
	: DetailLayoutBuilder( InDetailLayoutBuilder )
	, ActionMappingsPropertyHandle( InPropertyHandle )
{
	// Delegate for when the children in the array change
	FSimpleDelegate RebuildChildrenDelegate = FSimpleDelegate::CreateRaw( this, &FActionMappingsNodeBuilder::RebuildChildren );
	ActionMappingsPropertyHandle->SetOnPropertyValueChanged( RebuildChildrenDelegate );
	ActionMappingsPropertyHandle->AsArray()->SetOnNumElementsChanged( RebuildChildrenDelegate );
}

void FActionMappingsNodeBuilder::Tick( float DeltaTime )
{
	if (GroupsRequireRebuild())
	{
		RebuildChildren();
	}
}

void FActionMappingsNodeBuilder::GenerateHeaderRowContent( FDetailWidgetRow& NodeRow )
{
	TSharedRef<SWidget> AddButton = PropertyCustomizationHelpers::MakeAddButton( FSimpleDelegate::CreateSP( this, &FActionMappingsNodeBuilder::AddActionMappingButton_OnClick), 
		LOCTEXT("AddActionMappingToolTip", "Adds Action Mapping") );

	TSharedRef<SWidget> ClearButton = PropertyCustomizationHelpers::MakeEmptyButton( FSimpleDelegate::CreateSP( this, &FActionMappingsNodeBuilder::ClearActionMappingButton_OnClick), 
		LOCTEXT("ClearActionMappingToolTip", "Removes all Action Mappings") );

	NodeRow
	[
		SNew( SHorizontalBox )
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			ActionMappingsPropertyHandle->CreatePropertyNameWidget()
		]
		+SHorizontalBox::Slot()
		.Padding(2.0f)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			AddButton
		]
		+SHorizontalBox::Slot()
		.Padding(2.0f)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			ClearButton
		]
	];
}

void FActionMappingsNodeBuilder::GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder )
{
	RebuildGroupedMappings();

	for (int32 Index = 0; Index < GroupedMappings.Num(); ++Index)
	{
		const FMappingSet& MappingSet = GroupedMappings[Index];

		TSharedRef<SWidget> AddButton = PropertyCustomizationHelpers::MakeAddButton( FSimpleDelegate::CreateSP( this, &FActionMappingsNodeBuilder::AddActionMappingToGroupButton_OnClick, MappingSet), 
			LOCTEXT("AddActionMappingToGroupToolTip", "Adds Action Mapping to Group") );

		TSharedRef<SWidget> RemoveButton = PropertyCustomizationHelpers::MakeDeleteButton( FSimpleDelegate::CreateSP( this, &FActionMappingsNodeBuilder::RemoveActionMappingGroupButton_OnClick, MappingSet), 
			LOCTEXT("RemoveActionMappingGroupToolTip", "Removes Action Mapping Group") );

		FString GroupNameString(TEXT("ActionMappings."));
		MappingSet.SharedName.AppendString(GroupNameString);
		FName GroupName(*GroupNameString);
		IDetailGroup& ActionMappingGroup = ChildrenBuilder.AddChildGroup(GroupName, MappingSet.SharedName.ToString());
		ActionMappingGroup.HeaderRow()
		[
			SNew( SHorizontalBox )
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride( InputConstants::TextBoxWidth )
				[
					SNew(SEditableTextBox)
					.Padding(2.0f)
					.Text(FText::FromName(MappingSet.SharedName))
					.OnTextCommitted(FOnTextCommitted::CreateSP(this, &FActionMappingsNodeBuilder::OnActionMappingNameCommitted, MappingSet))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
			+SHorizontalBox::Slot()
			.Padding(InputConstants::PropertyPadding)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				AddButton
			]
			+SHorizontalBox::Slot()
			.Padding(InputConstants::PropertyPadding)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				RemoveButton
			]
		];
	
		for (int32 MappingIndex = 0; MappingIndex < MappingSet.Mappings.Num(); ++MappingIndex)
		{
			ActionMappingGroup.AddPropertyRow(MappingSet.Mappings[MappingIndex]).ShowPropertyButtons(false);
		}
	}
}

void FActionMappingsNodeBuilder::AddActionMappingButton_OnClick()
{
	ActionMappingsPropertyHandle->AsArray()->AddItem();
}

void FActionMappingsNodeBuilder::ClearActionMappingButton_OnClick()
{
	ActionMappingsPropertyHandle->AsArray()->EmptyArray();
}

void FActionMappingsNodeBuilder::OnActionMappingNameCommitted(const FText& InName, ETextCommit::Type CommitInfo, const FMappingSet MappingSet)
{
	const FScopedTransaction Transaction(LOCTEXT("RenameActionMapping_Transaction", "Rename Action Mapping"));

	FName NewName = FName(*InName.ToString());

	TSharedPtr<IPropertyHandleArray> ActionMappingsArrayHandle = ActionMappingsPropertyHandle->AsArray();

	for (int32 Index = 0; Index < MappingSet.Mappings.Num(); ++Index)
	{
		MappingSet.Mappings[Index]->GetChildHandle(GET_MEMBER_NAME_CHECKED(FInputActionKeyMapping, ActionName))->SetValue(NewName);
	}
}

void FActionMappingsNodeBuilder::AddActionMappingToGroupButton_OnClick(const FMappingSet MappingSet)
{
	const FScopedTransaction Transaction(LOCTEXT("AddActionMappingToGroup_Transaction", "Add Action Mapping To Group"));

	TArray<UObject*> OuterObjects;
	ActionMappingsPropertyHandle->GetOuterObjects(OuterObjects);

	if (OuterObjects.Num() == 1)
	{
		UInputSettings* InputSettings = CastChecked<UInputSettings>(OuterObjects[0]);
		InputSettings->Modify();
		ActionMappingsPropertyHandle->NotifyPreChange();

		FInputActionKeyMapping NewMapping(MappingSet.SharedName);
		InputSettings->ActionMappings.Add(NewMapping);

		ActionMappingsPropertyHandle->NotifyPostChange();
	}
}

void FActionMappingsNodeBuilder::RemoveActionMappingGroupButton_OnClick(const FMappingSet MappingSet)
{
	const FScopedTransaction Transaction(LOCTEXT("RemoveActionMappingGroup_Transaction", "Remove Action Mapping Group"));

	TSharedPtr<IPropertyHandleArray> ActionMappingsArrayHandle = ActionMappingsPropertyHandle->AsArray();

	TArray<int32> SortedIndices;
	for (int32 Index = 0; Index < MappingSet.Mappings.Num(); ++Index)
	{
		SortedIndices.AddUnique(MappingSet.Mappings[Index]->GetIndexInArray());
	}
	SortedIndices.Sort();

	for (int32 Index = SortedIndices.Num() - 1; Index >= 0; --Index)
	{
		ActionMappingsArrayHandle->DeleteItem(SortedIndices[Index]);
	}
}

bool FActionMappingsNodeBuilder::GroupsRequireRebuild() const
{
	for (int32 GroupIndex = 0; GroupIndex < GroupedMappings.Num(); ++GroupIndex)
	{
		const FMappingSet& MappingSet = GroupedMappings[GroupIndex];
		for (int32 MappingIndex = 0; MappingIndex < MappingSet.Mappings.Num(); ++MappingIndex)
		{
			FName ActionName;
			MappingSet.Mappings[MappingIndex]->GetChildHandle(GET_MEMBER_NAME_CHECKED(FInputActionKeyMapping, ActionName))->GetValue(ActionName);
			if (MappingSet.SharedName != ActionName)
			{
				return true;
			}
		}
	}
	return false;
}

void FActionMappingsNodeBuilder::RebuildGroupedMappings()
{
	GroupedMappings.Empty();

	TSharedPtr<IPropertyHandleArray> ActionMappingsArrayHandle = ActionMappingsPropertyHandle->AsArray();

	uint32 NumMappings;
	ActionMappingsArrayHandle->GetNumElements(NumMappings);
	for (uint32 Index = 0; Index < NumMappings; ++Index)
	{
		TSharedRef<IPropertyHandle> ActionMapping = ActionMappingsArrayHandle->GetElement(Index);
		FName ActionName;
		FPropertyAccess::Result Result = ActionMapping->GetChildHandle(GET_MEMBER_NAME_CHECKED(FInputActionKeyMapping, ActionName))->GetValue(ActionName);

		if (Result == FPropertyAccess::Success)
		{
			int32 FoundIndex = INDEX_NONE;
			for (int32 GroupIndex = 0; GroupIndex < GroupedMappings.Num(); ++GroupIndex)
			{
				if (GroupedMappings[GroupIndex].SharedName == ActionName)
				{
					FoundIndex = GroupIndex;
					break;
				}
			}
			if (FoundIndex == INDEX_NONE)
			{
				FoundIndex = GroupedMappings.Num();
				GroupedMappings.AddZeroed();
				GroupedMappings[FoundIndex].SharedName = ActionName;
			}
			GroupedMappings[FoundIndex].Mappings.Add(ActionMapping);
		}
	}
}

//////////////////////////////
// FAxisMappingsNodeBuilder //
//////////////////////////////

FAxisMappingsNodeBuilder::FAxisMappingsNodeBuilder( IDetailLayoutBuilder* InDetailLayoutBuilder, const TSharedPtr<IPropertyHandle>& InPropertyHandle )
	: DetailLayoutBuilder( InDetailLayoutBuilder )
	, AxisMappingsPropertyHandle( InPropertyHandle )
{
	// Delegate for when the children in the array change
	FSimpleDelegate RebuildChildrenDelegate = FSimpleDelegate::CreateRaw( this, &FAxisMappingsNodeBuilder::RebuildChildren );
	AxisMappingsPropertyHandle->SetOnPropertyValueChanged( RebuildChildrenDelegate );
	AxisMappingsPropertyHandle->AsArray()->SetOnNumElementsChanged( RebuildChildrenDelegate );
}

void FAxisMappingsNodeBuilder::Tick( float DeltaTime )
{
	if (GroupsRequireRebuild())
	{
		RebuildChildren();
	}
}

void FAxisMappingsNodeBuilder::GenerateHeaderRowContent( FDetailWidgetRow& NodeRow )
{
	TSharedRef<SWidget> AddButton = PropertyCustomizationHelpers::MakeAddButton( FSimpleDelegate::CreateSP( this, &FAxisMappingsNodeBuilder::AddAxisMappingButton_OnClick), 
		LOCTEXT("AddAxisMappingToolTip", "Adds Axis Mapping") );

	TSharedRef<SWidget> ClearButton = PropertyCustomizationHelpers::MakeEmptyButton( FSimpleDelegate::CreateSP( this, &FAxisMappingsNodeBuilder::ClearAxisMappingButton_OnClick), 
		LOCTEXT("ClearAxisMappingToolTip", "Removes all Axis Mappings") );

	NodeRow
	[
		SNew( SHorizontalBox )
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			AxisMappingsPropertyHandle->CreatePropertyNameWidget()
		]
		+SHorizontalBox::Slot()
		.Padding(2.0f)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			AddButton
		]
		+SHorizontalBox::Slot()
		.Padding(2.0f)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			ClearButton
		]
	];
}

void FAxisMappingsNodeBuilder::GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder )
{
	RebuildGroupedMappings();

	for (int32 Index = 0; Index < GroupedMappings.Num(); ++Index)
	{
		const FMappingSet& MappingSet = GroupedMappings[Index];

		TSharedRef<SWidget> AddButton = PropertyCustomizationHelpers::MakeAddButton( FSimpleDelegate::CreateSP( this, &FAxisMappingsNodeBuilder::AddAxisMappingToGroupButton_OnClick, MappingSet), 
			LOCTEXT("AddAxisMappingToGroupToolTip", "Adds Axis Mapping to Group") );

		TSharedRef<SWidget> RemoveButton = PropertyCustomizationHelpers::MakeDeleteButton( FSimpleDelegate::CreateSP( this, &FAxisMappingsNodeBuilder::RemoveAxisMappingGroupButton_OnClick, MappingSet), 
			LOCTEXT("RemoveAxisMappingGroupToolTip", "Removes Axis Mapping Group") );

		FString GroupNameString(TEXT("AxisMappings."));
		MappingSet.SharedName.AppendString(GroupNameString);
		FName GroupName(*GroupNameString);
		IDetailGroup& AxisMappingGroup = ChildrenBuilder.AddChildGroup(GroupName, MappingSet.SharedName.ToString());
		AxisMappingGroup.HeaderRow()
		[
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew( SBox )
				.WidthOverride( InputConstants::TextBoxWidth )
				[
					SNew(SEditableTextBox)
					.Padding(2.0f)
					.Text(FText::FromName(MappingSet.SharedName))
					.OnTextCommitted(FOnTextCommitted::CreateSP(this, &FAxisMappingsNodeBuilder::OnAxisMappingNameCommitted, MappingSet))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
			+SHorizontalBox::Slot()
			.Padding(InputConstants::PropertyPadding)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				AddButton
			]
			+SHorizontalBox::Slot()
			.Padding(InputConstants::PropertyPadding)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				RemoveButton
			]
		];

		for (int32 MappingIndex = 0; MappingIndex < MappingSet.Mappings.Num(); ++MappingIndex)
		{
			AxisMappingGroup.AddPropertyRow(MappingSet.Mappings[MappingIndex]).ShowPropertyButtons(false);
		}
	}
}

void FAxisMappingsNodeBuilder::AddAxisMappingButton_OnClick()
{
	AxisMappingsPropertyHandle->AsArray()->AddItem();
}

void FAxisMappingsNodeBuilder::ClearAxisMappingButton_OnClick()
{
	AxisMappingsPropertyHandle->AsArray()->EmptyArray();
}

void FAxisMappingsNodeBuilder::OnAxisMappingNameCommitted(const FText& InName, ETextCommit::Type CommitInfo, const FMappingSet MappingSet)
{
	const FScopedTransaction Transaction(LOCTEXT("RenameAxisMapping_Transaction", "Rename Axis Mapping"));

	FName NewName = FName(*InName.ToString());

	TSharedPtr<IPropertyHandleArray> AxisMappingsArrayHandle = AxisMappingsPropertyHandle->AsArray();

	for (int32 Index = 0; Index < MappingSet.Mappings.Num(); ++Index)
	{
		MappingSet.Mappings[Index]->GetChildHandle(GET_MEMBER_NAME_CHECKED(FInputAxisKeyMapping, AxisName))->SetValue(NewName);
	}
}

void FAxisMappingsNodeBuilder::AddAxisMappingToGroupButton_OnClick(const FMappingSet MappingSet)
{
	const FScopedTransaction Transaction(LOCTEXT("AddAxisMappingToGroup_Transaction", "Add Axis Mapping To Group"));

	TArray<UObject*> OuterObjects;
	AxisMappingsPropertyHandle->GetOuterObjects(OuterObjects);

	if (OuterObjects.Num() == 1)
	{
		UInputSettings* InputSettings = CastChecked<UInputSettings>(OuterObjects[0]);
		InputSettings->Modify();
		AxisMappingsPropertyHandle->NotifyPreChange();

		FInputAxisKeyMapping NewMapping(MappingSet.SharedName);
		InputSettings->AxisMappings.Add(NewMapping);

		AxisMappingsPropertyHandle->NotifyPostChange();
	}
}

void FAxisMappingsNodeBuilder::RemoveAxisMappingGroupButton_OnClick(const FMappingSet MappingSet)
{
	const FScopedTransaction Transaction(LOCTEXT("RemoveAxisMappingGroup_Transaction", "Remove Axis Mapping Group"));

	TSharedPtr<IPropertyHandleArray> AxisMappingsArrayHandle = AxisMappingsPropertyHandle->AsArray();

	TArray<int32> SortedIndices;
	for (int32 Index = 0; Index < MappingSet.Mappings.Num(); ++Index)
	{
		SortedIndices.AddUnique(MappingSet.Mappings[Index]->GetIndexInArray());
	}
	SortedIndices.Sort();

	for (int32 Index = SortedIndices.Num() - 1; Index >= 0; --Index)
	{
		AxisMappingsArrayHandle->DeleteItem(SortedIndices[Index]);
	}
}

bool FAxisMappingsNodeBuilder::GroupsRequireRebuild() const
{
	for (int32 GroupIndex = 0; GroupIndex < GroupedMappings.Num(); ++GroupIndex)
	{
		const FMappingSet& MappingSet = GroupedMappings[GroupIndex];
		for (int32 MappingIndex = 0; MappingIndex < MappingSet.Mappings.Num(); ++MappingIndex)
		{
			FName AxisName;
			MappingSet.Mappings[MappingIndex]->GetChildHandle(GET_MEMBER_NAME_CHECKED(FInputAxisKeyMapping, AxisName))->GetValue(AxisName);
			if (MappingSet.SharedName != AxisName)
			{
				return true;
			}
		}
	}
	return false;
}

void FAxisMappingsNodeBuilder::RebuildGroupedMappings()
{
	GroupedMappings.Empty();

	TSharedPtr<IPropertyHandleArray> AxisMappingsArrayHandle = AxisMappingsPropertyHandle->AsArray();

	uint32 NumMappings;
	AxisMappingsArrayHandle->GetNumElements(NumMappings);
	for (uint32 Index = 0; Index < NumMappings; ++Index)
	{
		TSharedRef<IPropertyHandle> AxisMapping = AxisMappingsArrayHandle->GetElement(Index);
		FName AxisName;
		FPropertyAccess::Result Result = AxisMapping->GetChildHandle(GET_MEMBER_NAME_CHECKED(FInputAxisKeyMapping, AxisName))->GetValue(AxisName);

		if (Result == FPropertyAccess::Success)
		{
			int32 FoundIndex = INDEX_NONE;
			for (int32 GroupIndex = 0; GroupIndex < GroupedMappings.Num(); ++GroupIndex)
			{
				if (GroupedMappings[GroupIndex].SharedName == AxisName)
				{
					FoundIndex = GroupIndex;
					break;
				}
			}
			if (FoundIndex == INDEX_NONE)
			{
				FoundIndex = GroupedMappings.Num();
				GroupedMappings.AddZeroed();
				GroupedMappings[FoundIndex].SharedName = AxisName;
			}
			GroupedMappings[FoundIndex].Mappings.Add(AxisMapping);
		}
	}
}

/////////////////////////
// FInputSettingsDetails //
/////////////////////////

TSharedRef<IDetailCustomization> FInputSettingsDetails::MakeInstance()
{
	return MakeShareable(new FInputSettingsDetails);
}

void FInputSettingsDetails::CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder)
{
	static const FName BindingsCategory = TEXT("Bindings");
	static const FName ActionMappings = GET_MEMBER_NAME_CHECKED(UInputSettings, ActionMappings);
	static const FName AxisMappings = GET_MEMBER_NAME_CHECKED(UInputSettings, AxisMappings);

	IDetailCategoryBuilder& MappingsDetailCategoryBuilder = DetailBuilder.EditCategory(BindingsCategory);

	// Custom Action Mappings
	const TSharedPtr<IPropertyHandle> ActionMappingsPropertyHandle = DetailBuilder.GetProperty(ActionMappings, UInputSettings::StaticClass());
	ActionMappingsPropertyHandle->MarkHiddenByCustomization();

	const TSharedRef<FActionMappingsNodeBuilder> ActionMappingsBuilder = MakeShareable( new FActionMappingsNodeBuilder( &DetailBuilder, ActionMappingsPropertyHandle ) );
	MappingsDetailCategoryBuilder.AddCustomBuilder(ActionMappingsBuilder);

	// Custom Axis Mappings
	const TSharedPtr<IPropertyHandle> AxisMappingsPropertyHandle = DetailBuilder.GetProperty(AxisMappings, UInputSettings::StaticClass());
	AxisMappingsPropertyHandle->MarkHiddenByCustomization();

	const TSharedRef<FAxisMappingsNodeBuilder> AxisMappingsBuilder = MakeShareable( new FAxisMappingsNodeBuilder( &DetailBuilder, AxisMappingsPropertyHandle ) );
	MappingsDetailCategoryBuilder.AddCustomBuilder(AxisMappingsBuilder);
}

#undef LOCTEXT_NAMESPACE
