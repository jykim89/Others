// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorPrivatePCH.h"
#include "SPropertyEditorTableRow.h"

#include "PropertyNode.h"
#include "PropertyEditorConstants.h"
#include "PropertyTreeConstants.h"
#include "IPropertyUtilities.h"
#include "PropertyEditor.h" 
#include "PropertyEditorHelpers.h"
#include "PropertyPath.h"

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


void SPropertyEditorTableRow::Construct( const FArguments& InArgs, const TSharedRef<FPropertyEditor>& InPropertyEditor, const TSharedRef< class IPropertyUtilities >& InPropertyUtilities, const TSharedRef<STableViewBase>& InOwnerTable )
{
	PropertyEditor = InPropertyEditor;
	PropertyUtilities = InPropertyUtilities;
	OnMiddleClicked = InArgs._OnMiddleClicked;
	ConstructExternalColumnCell = InArgs._ConstructExternalColumnCell;

	PropertyPath = FPropertyNode::CreatePropertyPath( PropertyEditor->GetPropertyNode() );

	this->SetToolTipText(TAttribute<FString>(PropertyEditor->GetToolTipText()));

	SMultiColumnTableRow< TSharedPtr<FPropertyNode*> >::Construct( FSuperRowType::FArguments(), InOwnerTable );
}

TSharedRef<SWidget> SPropertyEditorTableRow::GenerateWidgetForColumn( const FName& ColumnName )
{
	if (ColumnName == PropertyTreeConstants::ColumnId_Name)
	{
		return ConstructNameColumnWidget();
	}
	else if (ColumnName == PropertyTreeConstants::ColumnId_Property)
	{
		return ConstructValueColumnWidget();
	}
	else if ( ConstructExternalColumnCell.IsBound() )
	{
		return ConstructExternalColumnCell.Execute( ColumnName, SharedThis( this ) );
	}

	return SNew(STextBlock).Text(NSLOCTEXT("PropertyEditor", "UnknownColumnId", "Unknown Column Id"));
}

TSharedRef< SWidget > SPropertyEditorTableRow::ConstructNameColumnWidget()
{
	TSharedRef< SHorizontalBox > Box =
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding( FMargin( 0, 1, 0, 1 ) )
		.VAlign(VAlign_Center)
		[
			SNew(SExpanderArrow, SharedThis( this ) )
		]
		+SHorizontalBox::Slot()
		.Padding( FMargin( 0, 1, 0, 1 ) )
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew( SEditConditionWidget, PropertyEditor )
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew( SPropertyNameWidget, PropertyEditor )
			.OnDoubleClicked( this, &SPropertyEditorTableRow::OnNameDoubleClicked )
		];

		return Box;
}


TSharedRef< SWidget > SPropertyEditorTableRow::ConstructValueColumnWidget()
{
	TSharedRef<SHorizontalBox> HorizontalBox =
		SNew(SHorizontalBox)
		.IsEnabled(	PropertyEditor.ToSharedRef(), &FPropertyEditor::IsPropertyEditingEnabled );

	ValueEditorWidget = ConstructPropertyEditorWidget();

	HorizontalBox->AddSlot()
		.FillWidth(1) // Fill the entire width if possible
		.VAlign(VAlign_Center)	
		[
			ValueEditorWidget.ToSharedRef()
		];

	// The favorites star for this property
	HorizontalBox->AddSlot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		[
			SNew( SButton )
			.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
			.Visibility( this, &SPropertyEditorTableRow::OnGetFavoritesVisibility )
			.OnClicked( this, &SPropertyEditorTableRow::OnToggleFavoriteClicked )
			.ContentPadding(0)
			[
				SNew( SImage )
				.Image( this, &SPropertyEditorTableRow::OnGetFavoriteImage )
			]
		];

	TArray< TSharedRef<SWidget> > RequiredButtons;
	PropertyEditorHelpers::MakeRequiredPropertyButtons( PropertyEditor.ToSharedRef(), /*OUT*/RequiredButtons, TArray<EPropertyButton::Type>(), false );

	for( int32 ButtonIndex = 0; ButtonIndex < RequiredButtons.Num(); ++ButtonIndex )
	{
		HorizontalBox->AddSlot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding( 2.0f, 1.0f )
			[ 
				RequiredButtons[ButtonIndex]
			];
	}

	return SNew(SBorder)
		.Padding( FMargin( 0, 1, 0, 1 ) )
		.BorderImage_Static( &PropertyEditorConstants::GetOverlayBrush, PropertyEditor.ToSharedRef() )
		.VAlign(VAlign_Fill)
		[
			HorizontalBox
		];
}

FReply SPropertyEditorTableRow::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	FReply Reply = FReply::Unhandled();

	if ( MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton && OnMiddleClicked.IsBound() )
	{
		OnMiddleClicked.Execute( PropertyPath.ToSharedRef() );
		Reply = FReply::Handled();
	}

	return Reply;
}

TSharedPtr< FPropertyPath > SPropertyEditorTableRow::GetPropertyPath() const
{
	return PropertyPath.ToSharedRef();
}


bool SPropertyEditorTableRow::IsCursorHovering() const
{
	return SWidget::IsHovered();
}


EVisibility SPropertyEditorTableRow::OnGetFavoritesVisibility() const
{
	if( PropertyUtilities->AreFavoritesEnabled() && (!PropertyEditor->IsChildOfFavorite()) )
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

FReply SPropertyEditorTableRow::OnToggleFavoriteClicked() 
{
	PropertyEditor->ToggleFavorite();
	return FReply::Handled();
}

const FSlateBrush* SPropertyEditorTableRow::OnGetFavoriteImage() const
{
	if( PropertyEditor->IsFavorite() )
	{
		return FEditorStyle::GetBrush(TEXT("PropertyWindow.Favorites_Enabled"));
	}

	return FEditorStyle::GetBrush(TEXT("PropertyWindow.Favorites_Disabled"));
}

void SPropertyEditorTableRow::OnEditConditionCheckChanged( ESlateCheckBoxState::Type CheckState )
{
	PropertyEditor->SetEditConditionState( CheckState == ESlateCheckBoxState::Checked );
}

ESlateCheckBoxState::Type SPropertyEditorTableRow::OnGetEditConditionCheckState() const
{
	bool bEditConditionMet = PropertyEditor->IsEditConditionMet();
	return bEditConditionMet ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
}

FReply SPropertyEditorTableRow::OnNameDoubleClicked()
{
	FReply Reply = FReply::Unhandled();

	if ( ValueEditorWidget.IsValid() )
	{
		// Get path to editable widget
		FWidgetPath EditableWidgetPath;
		FSlateApplication::Get().GeneratePathToWidgetUnchecked( ValueEditorWidget.ToSharedRef(), EditableWidgetPath );

		// Set keyboard focus directly
		FSlateApplication::Get().SetKeyboardFocus( EditableWidgetPath, EKeyboardFocusCause::SetDirectly );
		Reply = FReply::Handled();
	}
	else if ( DoesItemHaveChildren() )
	{
		ToggleExpansion();
		Reply = FReply::Handled();
	}

	return Reply;
}

TSharedRef<SWidget> SPropertyEditorTableRow::ConstructPropertyEditorWidget()
{
	TSharedPtr<SWidget> PropertyWidget; 
	const UProperty* const Property = PropertyEditor->GetProperty();

	const TSharedRef< FPropertyEditor > PropertyEditorRef = PropertyEditor.ToSharedRef();
	const TSharedRef< IPropertyUtilities > PropertyUtilitiesRef = PropertyUtilities.ToSharedRef();

	if( Property )
	{
		// ORDER MATTERS: first widget type to support the property node wins!
		if ( SPropertyEditorNumeric<float>::Supports(PropertyEditorRef) )
		{
			PropertyWidget = SNew( SPropertyEditorNumeric<float>, PropertyEditorRef );
		}
		if ( SPropertyEditorNumeric<int32>::Supports(PropertyEditorRef) )
		{
			PropertyWidget = SNew( SPropertyEditorNumeric<int32>, PropertyEditorRef );
		}
		if ( SPropertyEditorNumeric<uint8>::Supports(PropertyEditorRef) )
		{
			PropertyWidget = SNew( SPropertyEditorNumeric<uint8>, PropertyEditorRef );
		}
		else if ( SPropertyEditorArray::Supports(PropertyEditorRef) )
		{
			PropertyWidget = SNew( SPropertyEditorArray, PropertyEditorRef );
		}
		else if ( SPropertyEditorCombo::Supports(PropertyEditorRef) )
		{
			PropertyWidget = SNew( SPropertyEditorCombo, PropertyEditorRef );
		}
		else if ( SPropertyEditorEditInline::Supports(PropertyEditorRef) )
		{
			PropertyWidget = SNew( SPropertyEditorEditInline, PropertyEditorRef );
		}
		else if ( SPropertyEditorText::Supports(PropertyEditorRef) )
		{
			PropertyWidget = SNew( SPropertyEditorText, PropertyEditorRef );
		}
		else if ( SPropertyEditorBool::Supports(PropertyEditorRef) )
		{
			PropertyWidget = SNew( SPropertyEditorBool, PropertyEditorRef );
		}
		else if ( SPropertyEditorColor::Supports(PropertyEditorRef) )
		{
			PropertyWidget = SNew( SPropertyEditorColor, PropertyEditorRef, PropertyUtilitiesRef );
		}
		else if ( SPropertyEditorArrayItem::Supports(PropertyEditorRef) )
		{
			PropertyWidget = SNew( SPropertyEditorArrayItem, PropertyEditorRef );
		}
		else if ( SPropertyEditorDateTime::Supports(PropertyEditorRef) )
		{
			PropertyWidget = SNew( SPropertyEditorDateTime, PropertyEditorRef );
		}
	}

	if( !PropertyWidget.IsValid() )
	{
		PropertyWidget = SNew( SPropertyEditor, PropertyEditorRef );
	}

	PropertyWidget->SetToolTipText( TAttribute<FString>(PropertyEditor->GetToolTipText()) );

	return PropertyWidget.ToSharedRef();
}