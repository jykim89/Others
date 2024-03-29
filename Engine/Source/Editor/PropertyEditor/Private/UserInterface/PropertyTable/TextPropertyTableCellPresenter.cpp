// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorPrivatePCH.h"
#include "TextPropertyTableCellPresenter.h"
#include "PropertyTableConstants.h"
#include "ItemPropertyNode.h"
#include "PropertyEditor.h"
#include "IPropertyTableUtilities.h"
#include "PropertyEditorHelpers.h"

#include "SPropertyEditor.h"
#include "SPropertyEditorTitle.h"
#include "SResetToDefaultPropertyEditor.h"
#include "SPropertyEditorNumeric.h"
#include "SPropertyEditorArray.h"
#include "SPropertyEditorCombo.h"
#include "SPropertyEditorEditInline.h"
#include "SPropertyEditorText.h"
#include "SPropertyEditorBool.h"
#include "SPropertyEditorColor.h"
#include "SPropertyEditorArrayItem.h"
#include "SPropertyEditorDateTime.h"


FTextPropertyTableCellPresenter::FTextPropertyTableCellPresenter( const TSharedRef< class FPropertyEditor >& InPropertyEditor, const TSharedRef< class IPropertyTableUtilities >& InPropertyUtilities, FSlateFontInfo InFont /*= FEditorStyle::GetFontStyle( PropertyTableConstants::NormalFontStyle ) */ )
	: PropertyEditor( InPropertyEditor )
	, PropertyUtilities( InPropertyUtilities )
	, PropertyWidget( NULL )
	, HasReadOnlyEditingWidget( false )
	, Font(InFont)
{
	HasReadOnlyEditingWidget = CalculateIfUsingReadOnlyEditingWidget();
}

TSharedRef< class SWidget > FTextPropertyTableCellPresenter::ConstructDisplayWidget()
{
	return SNew( SHorizontalBox )
		+SHorizontalBox::Slot()
		.FillWidth( 1.0 )
		.VAlign( VAlign_Center )
		.Padding( FMargin( 2, 0, 0, 0 ) )
		[
			SNew( STextBlock )
			.Text( PropertyEditor->GetValueAsString() )
			.ToolTipText( PropertyEditor->GetToolTipText() )
			.Font( Font )
		]
	+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign( VAlign_Center )
		.Padding( FMargin( 0, 0, 2, 0 ) )
		[
			SNew( SResetToDefaultPropertyEditor, PropertyEditor )
		];
}

bool FTextPropertyTableCellPresenter::RequiresDropDown()
{
	// Don't create Anchor
	TArray<EPropertyButton::Type> OutRequiredButtons;
	PropertyEditorHelpers::GetRequiredPropertyButtons( PropertyEditor->GetPropertyNode(), OutRequiredButtons );
	return OutRequiredButtons.Num() > 0;
}

TSharedRef< class SWidget > FTextPropertyTableCellPresenter::ConstructEditModeDropDownWidget()
{
	TSharedRef< SWidget > Result = SNullWidget::NullWidget;

	TArray< TSharedRef<SWidget> > RequiredButtons;
	PropertyEditorHelpers::MakeRequiredPropertyButtons( PropertyEditor, /*OUT*/RequiredButtons );

	if ( RequiredButtons.Num() > 0 )
	{
		TSharedPtr< SHorizontalBox > ButtonBox;
		Result = SNew( SBorder )
			.BorderImage(FEditorStyle::GetBrush("PropertyTable.Cell.DropDown.Background"))
			.Padding( 0 )
			.Content()
			[
				SAssignNew( ButtonBox, SHorizontalBox )
			];

		for( int32 ButtonIndex = 0; ButtonIndex < RequiredButtons.Num(); ++ButtonIndex )
		{
			ButtonBox->AddSlot()
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding( 2.0f, 1.0f )
				[ 
					RequiredButtons[ButtonIndex]
				];
		}
	}

	return Result;
}

TSharedRef<SWidget> FTextPropertyTableCellPresenter::ConstructEditModeCellWidget()
{
	HasReadOnlyEditingWidget = false;
	const UProperty* const Property = PropertyEditor->GetProperty();

	if( Property )
	{
		// ORDER MATTERS: first widget type to support the property node wins!
		if ( SPropertyEditorNumeric<float>::Supports(PropertyEditor) )
		{
			PropertyWidget = SNew( SPropertyEditorNumeric<float>, PropertyEditor )
				.Font( Font );
		}
		if ( SPropertyEditorNumeric<int32>::Supports(PropertyEditor) )
		{
			PropertyWidget = SNew( SPropertyEditorNumeric<int32>, PropertyEditor )
				.Font( Font );
		}
		if ( SPropertyEditorNumeric<uint8>::Supports(PropertyEditor) )
		{
			PropertyWidget = SNew( SPropertyEditorNumeric<uint8>, PropertyEditor )
				.Font( Font );
		}
		else if ( SPropertyEditorCombo::Supports(PropertyEditor) )
		{
			PropertyWidget = SNew( SPropertyEditorCombo, PropertyEditor )
				.Font( Font );
		}
		else if ( SPropertyEditorEditInline::Supports(PropertyEditor) )
		{
			PropertyWidget = SNew( SPropertyEditorEditInline, PropertyEditor )
				.Font( Font );
		}
		else if ( SPropertyEditorText::Supports(PropertyEditor) )
		{
			PropertyWidget = SNew( SPropertyEditorText, PropertyEditor )
				.Font( Font );
		}
		else if ( SPropertyEditorBool::Supports(PropertyEditor) )
		{
			PropertyWidget = SNew( SPropertyEditorBool, PropertyEditor );
		}
		else if ( SPropertyEditorColor::Supports(PropertyEditor) )
		{
			PropertyWidget = SNew( SPropertyEditorColor, PropertyEditor, PropertyUtilities );
		}
		else if ( SPropertyEditorDateTime::Supports(PropertyEditor) )
		{
			PropertyWidget = SNew( SPropertyEditorDateTime, PropertyEditor )
				.Font( Font );
		}
	}

	if( !PropertyWidget.IsValid() )
	{
		PropertyWidget = SNew( SPropertyEditor, PropertyEditor )
			.Font( Font );
		HasReadOnlyEditingWidget = true;
	}

	PropertyWidget->SetToolTipText( TAttribute<FString>(PropertyEditor->GetToolTipText()) );

	return PropertyWidget.ToSharedRef();
}

bool FTextPropertyTableCellPresenter::CalculateIfUsingReadOnlyEditingWidget() const
{
	const UProperty* const Property = PropertyEditor->GetProperty();

	if( Property )
	{
		// ORDER MATTERS: first widget type to support the property node wins!
		if (SPropertyEditorNumeric<float>::Supports(PropertyEditor) || 
			SPropertyEditorNumeric<int32>::Supports(PropertyEditor) ||
			SPropertyEditorNumeric<uint8>::Supports(PropertyEditor) ||
			SPropertyEditorCombo::Supports(PropertyEditor) || 
			SPropertyEditorEditInline::Supports(PropertyEditor) || 
			SPropertyEditorText::Supports(PropertyEditor) || 
			SPropertyEditorBool::Supports(PropertyEditor) || 
			SPropertyEditorColor::Supports(PropertyEditor) ||
			SPropertyEditorDateTime::Supports(PropertyEditor))
		{
			return false;
		}
	}

	return true;
}

TSharedRef< class SWidget > FTextPropertyTableCellPresenter::WidgetToFocusOnEdit()
{
	return PropertyWidget.ToSharedRef();
}

FString FTextPropertyTableCellPresenter::GetValueAsString()
{
	return PropertyEditor->GetValueAsString();
}

FText FTextPropertyTableCellPresenter::GetValueAsText()
{
	return PropertyEditor->GetValueAsText();
}