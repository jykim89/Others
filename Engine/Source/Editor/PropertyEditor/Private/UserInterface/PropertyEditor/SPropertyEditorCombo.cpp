// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorPrivatePCH.h"

#include "PropertyNode.h"
#include "PropertyEditorHelpers.h"
#include "PropertyEditor.h"
#include "SPropertyComboBox.h"
#include "SPropertyEditorCombo.h"

#include "PropertyHandle.h"

#define LOCTEXT_NAMESPACE "PropertyEditor"

static int32 FindEnumValueIndex(UEnum* Enum, FString const& ValueString)
{
	int32 Index = INDEX_NONE;
	for(int32 ValIndex = 0; ValIndex < Enum->NumEnums(); ++ValIndex)
	{
		FString const EnumName    = Enum->GetEnumName(ValIndex);
		FString const DisplayName = Enum->GetDisplayNameText(ValIndex).ToString();

		if (DisplayName.Len() > 0)
		{
			if (DisplayName == ValueString)
			{
				Index = ValIndex;
				break;
			}
		}
		
		if (EnumName == ValueString)
		{
			Index = ValIndex;
			break;
		}
	}
	return Index;
}

void SPropertyEditorCombo::GetDesiredWidth( float& OutMinDesiredWidth, float& OutMaxDesiredWidth )
{
	OutMinDesiredWidth = 125.0f;
	OutMaxDesiredWidth = 400.0f;
}

bool SPropertyEditorCombo::Supports( const TSharedRef< class FPropertyEditor >& InPropertyEditor )
{
	const TSharedRef< FPropertyNode > PropertyNode = InPropertyEditor->GetPropertyNode();
	const UProperty* Property = InPropertyEditor->GetProperty();
	int32 ArrayIndex = PropertyNode->GetArrayIndex();

	if(	((Property->IsA(UByteProperty::StaticClass()) && Cast<const UByteProperty>(Property)->Enum)
		||	(Property->IsA(UNameProperty::StaticClass()) && Property->GetFName() == NAME_InitialState)
		||	(Property->IsA(UStrProperty::StaticClass()) && Property->HasMetaData(TEXT("Enum")))
		)
		&&	( ( ArrayIndex == -1 && Property->ArrayDim == 1 ) || ( ArrayIndex > -1 && Property->ArrayDim > 0 ) ) )
	{
		return true;
	}

	return false;
}

void SPropertyEditorCombo::Construct( const FArguments& InArgs, const TSharedRef< class FPropertyEditor >& InPropertyEditor )
{
	PropertyEditor = InPropertyEditor;
	
	// Important to find certain info out about the combo box.
	TArray< TSharedPtr<FString> > ComboItems;
	TArray< TSharedPtr<FString> > ToolTips;
	TArray< bool > Restrictions;
	GenerateComboBoxStrings( ComboItems, ToolTips, Restrictions );

	SAssignNew(ComboBox, SPropertyComboBox)
		.Font( InArgs._Font )
		.ToolTipList( ToolTips )
		.ComboItemList( ComboItems )
		.RestrictedList( Restrictions )
		.OnSelectionChanged( this, &SPropertyEditorCombo::OnComboSelectionChanged )
		.OnComboBoxOpening( this, &SPropertyEditorCombo::OnComboOpening )
		.VisibleText( this, &SPropertyEditorCombo::GetDisplayValueAsString )
		.ToolTipText( InPropertyEditor, &FPropertyEditor::GetValueAsString );

	ChildSlot
	[
		ComboBox.ToSharedRef()
	];

	SetEnabled( TAttribute<bool>( this, &SPropertyEditorCombo::CanEdit ) );
}

FString SPropertyEditorCombo::GetDisplayValueAsString() const
{
	const UProperty* Property = PropertyEditor->GetProperty();
	const UByteProperty* ByteProperty = Cast<const UByteProperty>( Property );
	const bool bStringEnumProperty = Property && Property->IsA(UStrProperty::StaticClass()) && Property->HasMetaData(TEXT("Enum"));	

	if ( !(ByteProperty || bStringEnumProperty) )
	{
		UObject* ObjectValue = NULL;
		FPropertyAccess::Result Result = PropertyEditor->GetPropertyHandle()->GetValue( ObjectValue );

		if( Result == FPropertyAccess::Success && ObjectValue != NULL )
		{
			return ObjectValue->GetName();
		}
	}

	return (bUsesAlternateDisplayValues) ? PropertyEditor->GetValueAsDisplayString() : PropertyEditor->GetValueAsString();
}

void SPropertyEditorCombo::GenerateComboBoxStrings( TArray< TSharedPtr<FString> >& OutComboBoxStrings, TArray< TSharedPtr<FString> >& OutToolTips, TArray<bool>& OutRestrictedItems )
{
	const TSharedRef< IPropertyHandle > PropertyHandle = PropertyEditor->GetPropertyHandle();
	bUsesAlternateDisplayValues = PropertyHandle->GeneratePossibleValues(OutComboBoxStrings, OutToolTips, OutRestrictedItems);
}

void SPropertyEditorCombo::OnComboSelectionChanged( TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo )
{
	if ( NewValue.IsValid() )
	{
		SendToObjects( *NewValue );
	}
}

void SPropertyEditorCombo::OnComboOpening()
{
	// try and re-sync the selection in the combo list in case it was changed since Construct was called
	// this would fail if the displayed value doesn't match the equivalent value in the combo list
	FString CurrentDisplayValue = GetDisplayValueAsString();
	ComboBox->SetSelectedItem(CurrentDisplayValue);
}

void SPropertyEditorCombo::SendToObjects( const FString& NewValue )
{
	const TSharedRef< FPropertyNode > PropertyNode = PropertyEditor->GetPropertyNode();
	UProperty* Property = PropertyNode->GetProperty();

	FString Value;
	FString ToolTipValue;
	if ( bUsesAlternateDisplayValues && !Property->IsA(UStrProperty::StaticClass()))
	{
		// currently only enum properties can use alternate display values; this 
		// might change, so assert here so that if support is expanded to other 
		// property types without updating this block of code, we'll catch it quickly
		UEnum* Enum = CastChecked<UByteProperty>(Property)->Enum;
		check(Enum != nullptr);

		const int32 Index = FindEnumValueIndex(Enum, NewValue);
		check( Index != INDEX_NONE );

		Value = Enum->GetEnumName(Index);

		ToolTipValue = Enum->GetMetaData( TEXT("ToolTip"), Index );
		FString ToolTipText = Property->GetToolTipText().ToString();
		if (ToolTipValue.Len() > 0)
		{
			ToolTipText = FString::Printf(TEXT("%s\n\n%s"), *ToolTipText, *ToolTipValue);
		}
		SetToolTipText(TAttribute<FString>(ToolTipText));
	}
	else
	{
		Value = NewValue;
	}

	const TSharedRef< IPropertyHandle > PropertyHandle = PropertyEditor->GetPropertyHandle();
	PropertyHandle->SetValueFromFormattedString( Value );
}

bool SPropertyEditorCombo::CanEdit() const
{
	return PropertyEditor.IsValid() ? !PropertyEditor->IsEditConst() : true;
}

#undef LOCTEXT_NAMESPACE
