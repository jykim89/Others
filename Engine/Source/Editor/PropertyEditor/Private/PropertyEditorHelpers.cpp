// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorPrivatePCH.h"
#include "PropertyEditorHelpers.h"

#include "PropertyNode.h"
#include "PropertyHandle.h"
#include "PropertyHandleImpl.h"
#include "PropertyEditor.h"

#include "SPropertyEditor.h"
#include "SPropertyEditorNumeric.h"
#include "SPropertyEditorArray.h"
#include "SPropertyEditorAttribute.h"
#include "SPropertyEditorCombo.h"
#include "SPropertyEditorEditInline.h"
#include "SPropertyEditorText.h"
#include "SPropertyEditorBool.h"
#include "SPropertyEditorColor.h"
#include "SPropertyEditorArrayItem.h"
#include "SPropertyAssetPicker.h"
#include "SPropertyEditorTitle.h"
#include "SPropertyEditorDateTime.h"
#include "SResetToDefaultPropertyEditor.h"
#include "SPropertyEditorAsset.h"
#include "SPropertyEditorClass.h"
#include "IDocumentation.h"

#define LOCTEXT_NAMESPACE "PropertyEditor"

void SPropertyNameWidget::Construct( const FArguments& InArgs, TSharedPtr<FPropertyEditor> InPropertyEditor )
{
	PropertyEditor = InPropertyEditor;

	TSharedPtr<SHorizontalBox> HorizontalBox;
	ChildSlot
	[
		SAssignNew(HorizontalBox, SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding( FMargin( 0, 1, 0, 1 ) )
		.FillWidth(1)
		[
			SNew(SBorder)
			.BorderImage_Static( &PropertyEditorConstants::GetOverlayBrush, PropertyEditor.ToSharedRef() )
			.Padding( FMargin( 0.0f, 2.0f ) )
			.VAlign(VAlign_Center)
			[
				SNew( SPropertyEditorTitle, PropertyEditor.ToSharedRef() )
				.StaticDisplayName( PropertyEditor->GetDisplayName() )
				.OnDoubleClicked( InArgs._OnDoubleClicked )
                .ToolTip( IDocumentation::Get()->CreateToolTip( FText::FromString( PropertyEditor->GetToolTipText() ), NULL, PropertyEditor->GetDocumentationLink(), PropertyEditor->GetDocumentationExcerptName() ) )
			]
		]
	
	];

	if( InArgs._DisplayResetToDefault )
	{
		HorizontalBox->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2,1)
		[
			SNew( SResetToDefaultPropertyEditor, PropertyEditor.ToSharedRef() )
		];
	}
}

void SPropertyValueWidget::Construct( const FArguments& InArgs, TSharedPtr<FPropertyEditor> PropertyEditor, TSharedPtr<IPropertyUtilities> InPropertyUtilities )
{
	MinDesiredWidth = 0.0f;
	MaxDesiredWidth = 0.0f;

	SetEnabled( TAttribute<bool>( PropertyEditor.ToSharedRef(), &FPropertyEditor::IsPropertyEditingEnabled ) );


	ValueEditorWidget = ConstructPropertyEditorWidget( PropertyEditor, InPropertyUtilities );

	ValueEditorWidget->SetToolTipText( TAttribute<FString>(PropertyEditor->GetToolTipText()) );


	if( InArgs._ShowPropertyButtons )
	{
		TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

		HorizontalBox->AddSlot()
		.FillWidth(1) // Fill the entire width if possible
		.VAlign(VAlign_Center)	
		[
			ValueEditorWidget.ToSharedRef()
		];

		TArray< TSharedRef<SWidget> > RequiredButtons;
		PropertyEditorHelpers::MakeRequiredPropertyButtons( PropertyEditor.ToSharedRef(), /*OUT*/RequiredButtons );

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

		ChildSlot
		[
			HorizontalBox
		];

	}
	else
	{
		ChildSlot
		.VAlign(VAlign_Center)	
		[
			ValueEditorWidget.ToSharedRef()
		];
	}


}

TSharedRef<SWidget> SPropertyValueWidget::ConstructPropertyEditorWidget( TSharedPtr<FPropertyEditor>& PropertyEditor, TSharedPtr<IPropertyUtilities> InPropertyUtilities )
{
	const TSharedRef<FPropertyEditor> PropertyEditorRef = PropertyEditor.ToSharedRef();
	const TSharedRef<IPropertyUtilities> PropertyUtilitiesRef = InPropertyUtilities.ToSharedRef();

	const TSharedRef< FPropertyNode > PropertyNode = PropertyEditorRef->GetPropertyNode();
	const int32 NodeArrayIndex = PropertyNode->GetArrayIndex();
	UProperty* Property = PropertyNode->GetProperty();
	
	FSlateFontInfo FontStyle = FEditorStyle::GetFontStyle( PropertyEditorConstants::PropertyFontStyle );
	TSharedPtr<SWidget> PropertyWidget; 
	if( Property )
	{
		// ORDER MATTERS: first widget type to support the property node wins!
		if ( SPropertyEditorArray::Supports(PropertyEditorRef) )
		{
			TSharedRef<SPropertyEditorArray> ArrayWidget = 
				SAssignNew( PropertyWidget, SPropertyEditorArray, PropertyEditorRef )
				.Font( FontStyle );

			ArrayWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );
		}
		else if( SPropertyEditorAttribute::Supports(PropertyEditorRef) )
		{
			TSharedRef<SPropertyEditorAttribute> AttributeWidget =
				SAssignNew(PropertyWidget, SPropertyEditorAttribute, PropertyEditorRef)
				.Font(FontStyle);

			AttributeWidget->GetDesiredWidth(MinDesiredWidth, MaxDesiredWidth);
		}
		else if ( SPropertyEditorAsset::Supports( PropertyEditorRef ) )
		{
			TSharedRef<SPropertyEditorAsset> AssetWidget = 
				SAssignNew( PropertyWidget, SPropertyEditorAsset, PropertyEditorRef )
				.ThumbnailPool( PropertyUtilitiesRef->GetThumbnailPool() );

			AssetWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );
		}
		else if ( SPropertyEditorClass::Supports( PropertyEditorRef ) )
		{
			TSharedRef<SPropertyEditorClass> ClassWidget = 
				SAssignNew( PropertyWidget, SPropertyEditorClass, PropertyEditorRef )
				.Font( FontStyle );

			ClassWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );
		}
		else if ( SPropertyEditorNumeric<float>::Supports( PropertyEditorRef ) )
		{
			auto NumericWidget = 
				SAssignNew( PropertyWidget, SPropertyEditorNumeric<float>, PropertyEditorRef )
				.Font( FontStyle );

			NumericWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );
		}
		else if ( SPropertyEditorNumeric<int32>::Supports( PropertyEditorRef ) )
		{
			auto NumericWidget = 
				SAssignNew( PropertyWidget, SPropertyEditorNumeric<int32>, PropertyEditorRef )
				.Font( FontStyle );

			NumericWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );
		}
		else if ( SPropertyEditorNumeric<uint8>::Supports( PropertyEditorRef ) )
		{
			auto NumericWidget = 
				SAssignNew( PropertyWidget, SPropertyEditorNumeric<uint8>, PropertyEditorRef )
				.Font( FontStyle );

			NumericWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );
		}
		else if ( SPropertyEditorCombo::Supports( PropertyEditorRef ) )
		{
			TSharedRef<SPropertyEditorCombo> ComboWidget = 
				SAssignNew( PropertyWidget, SPropertyEditorCombo, PropertyEditorRef )
				.Font( FontStyle );

			ComboWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );
		}
		else if ( SPropertyEditorEditInline::Supports( PropertyEditorRef ) )
		{
			TSharedRef<SPropertyEditorEditInline> EditInlineWidget = 
				SAssignNew( PropertyWidget, SPropertyEditorEditInline, PropertyEditorRef )
				.Font( FontStyle );

			EditInlineWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );
		}
		else if ( SPropertyEditorText::Supports( PropertyEditorRef ) )
		{
			TSharedRef<SPropertyEditorText> TextWidget = 
				SAssignNew( PropertyWidget, SPropertyEditorText, PropertyEditorRef )
				.Font( FontStyle );

			TextWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );
		}
		else if ( SPropertyEditorBool::Supports( PropertyEditorRef ) )
		{
			TSharedRef<SPropertyEditorBool> BoolWidget = 
				SAssignNew( PropertyWidget, SPropertyEditorBool, PropertyEditorRef );

			BoolWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );

		}
		else if ( SPropertyEditorArrayItem::Supports( PropertyEditorRef ) )
		{
			TSharedRef<SPropertyEditorArrayItem> ArrayItemWidget = 
				SAssignNew( PropertyWidget, SPropertyEditorArrayItem, PropertyEditorRef )
				.Font( FontStyle );

			ArrayItemWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );
		}
		else if ( SPropertyEditorDateTime::Supports( PropertyEditorRef ) )
		{
			TSharedRef<SPropertyEditorDateTime> DateTimeWidget =
				SAssignNew( PropertyWidget, SPropertyEditorDateTime, PropertyEditorRef )
				.Font( FontStyle );
		}
	}

	if( !PropertyWidget.IsValid() )
	{
		TSharedRef<SPropertyEditor> BasePropertyEditorWidget = 
			SAssignNew( PropertyWidget, SPropertyEditor, PropertyEditorRef )
			.Font( FontStyle );

		BasePropertyEditorWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );

	}

	return PropertyWidget.ToSharedRef();
}

void SEditConditionWidget::Construct( const FArguments& Args, TSharedPtr<FPropertyEditor> InPropertyEditor )
{
	PropertyEditor = InPropertyEditor;
	CustomEditCondition = Args._CustomEditCondition;

	SetVisibility( HasEditCondition() ? EVisibility::Visible : EVisibility::Collapsed );

	ChildSlot
	[
		// Some properties become irrelevant depending on the value of other properties.
		// We prevent the user from editing those properties by disabling their widgets.
		// This is a shortcut for toggling the property that disables us.
		SNew( SCheckBox )
			.OnCheckStateChanged( this, &SEditConditionWidget::OnEditConditionCheckChanged )
			.IsChecked( this, &SEditConditionWidget::OnGetEditConditionCheckState )
	];
}

bool SEditConditionWidget::HasEditCondition() const
{	
	return
			( PropertyEditor.IsValid() && PropertyEditor->HasEditCondition() && PropertyEditor->SupportsEditConditionToggle() )
		||	( CustomEditCondition.OnEditConditionValueChanged.IsBound() );
}

void SEditConditionWidget::OnEditConditionCheckChanged( ESlateCheckBoxState::Type CheckState )
{
	if( PropertyEditor.IsValid() && PropertyEditor->HasEditCondition() && PropertyEditor->SupportsEditConditionToggle() )
	{
		PropertyEditor->SetEditConditionState( CheckState == ESlateCheckBoxState::Checked );
	}
	else
	{
		CustomEditCondition.OnEditConditionValueChanged.ExecuteIfBound( CheckState == ESlateCheckBoxState::Checked );
	}
}

ESlateCheckBoxState::Type SEditConditionWidget::OnGetEditConditionCheckState() const
{
	bool bEditConditionMet = ( PropertyEditor.IsValid() && PropertyEditor->HasEditCondition() && PropertyEditor->IsEditConditionMet() ) || CustomEditCondition.EditConditionValue.Get();
	return bEditConditionMet ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
}

namespace PropertyEditorHelpers
{
	bool IsBuiltInStructProperty( const UProperty* Property )
	{
		bool bIsBuiltIn = false;

		const UStructProperty* StructProp = Cast<const UStructProperty>( Property );
		if( StructProp && StructProp->Struct )
		{
			FName StructName = StructProp->Struct->GetFName();

			bIsBuiltIn = StructName == NAME_Rotator || 
						 StructName == NAME_Color ||  
						 StructName == NAME_LinearColor || 
						 StructName == NAME_Vector ||
						 StructName == NAME_Vector4 ||
						 StructName == NAME_Vector2D ||
						 StructName == NAME_IntPoint;
		}

		return bIsBuiltIn;
	}

	bool IsChildOfArray( const FPropertyNode& InPropertyNode )
	{
		return GetArrayParent( InPropertyNode ) != NULL;
	}

	bool IsStaticArray( const FPropertyNode& InPropertyNode )
	{
		const UProperty* NodeProperty = InPropertyNode.GetProperty();
		return NodeProperty && NodeProperty->ArrayDim != 1 && InPropertyNode.GetArrayIndex() == -1;
	}

	bool IsDynamicArray( const FPropertyNode& InPropertyNode )
	{
		const UProperty* NodeProperty = InPropertyNode.GetProperty();
		return NodeProperty && Cast<const UArrayProperty>(NodeProperty) != NULL;
	}

	const UProperty* GetArrayParent( const FPropertyNode& InPropertyNode )
	{
		const UProperty* ParentProperty = InPropertyNode.GetParentNode() != NULL ? InPropertyNode.GetParentNode()->GetProperty() : NULL;
		
		if( ParentProperty )
		{
			if( (ParentProperty->IsA<UArrayProperty>()) || // dynamic array
				(InPropertyNode.GetArrayIndex() != INDEX_NONE && ParentProperty->ArrayDim > 0) ) //static array
			{
				return ParentProperty;
			}
		}

		return NULL;
	}

	bool IsEditInlineClassAllowed( UClass* CheckClass, bool bAllowAbstract ) 
	{
		return !CheckClass->HasAnyClassFlags(CLASS_Hidden|CLASS_HideDropDown|CLASS_Deprecated)
			&&	(bAllowAbstract || !CheckClass->HasAnyClassFlags(CLASS_Abstract));
	}

	FString GetToolTipText( const UProperty* const Property )
	{
		if( Property )
		{
			FString ToolTipText = Property->GetToolTipText().ToString();

			return ToolTipText;
		}

		return FString();
	}

	FString GetDocumentationLink( const UProperty* const Property )
	{
		if ( Property != NULL )
		{
			UStruct* OwnerStruct = Property->GetOwnerStruct();

			if ( OwnerStruct != NULL )
			{
				return FString::Printf( TEXT("Shared/Types/%s%s"), OwnerStruct->GetPrefixCPP(), *OwnerStruct->GetName() );
			}
		}

		return TEXT("");
	}

	FString GetDocumentationExcerptName( const UProperty* const Property )
	{
		if ( Property != NULL )
		{
			return Property->GetName();
		}

		return TEXT("");
	}

	TSharedPtr<IPropertyHandle> GetPropertyHandle( TSharedRef<FPropertyNode> PropertyNode, FNotifyHook* NotifyHook, TSharedPtr<IPropertyUtilities> PropertyUtilities )
	{
		TSharedPtr<IPropertyHandle> PropertyHandle;

		// Always check arrays first, many types can be static arrays
		if( FPropertyHandleArray::Supports( PropertyNode ) )
		{
			PropertyHandle = MakeShareable( new FPropertyHandleArray( PropertyNode, NotifyHook, PropertyUtilities ) );
		}
		else if( FPropertyHandleInt::Supports( PropertyNode ) )
		{
			PropertyHandle = MakeShareable( new FPropertyHandleInt( PropertyNode, NotifyHook, PropertyUtilities ) );
		}
		else if( FPropertyHandleFloat::Supports( PropertyNode ) )
		{
			PropertyHandle = MakeShareable( new FPropertyHandleFloat( PropertyNode, NotifyHook, PropertyUtilities ) );
		}
		else if( FPropertyHandleBool::Supports( PropertyNode ) )
		{
			PropertyHandle = MakeShareable( new FPropertyHandleBool( PropertyNode, NotifyHook, PropertyUtilities ) ) ;
		}
		else if( FPropertyHandleByte::Supports( PropertyNode ) )
		{
			PropertyHandle = MakeShareable( new FPropertyHandleByte( PropertyNode, NotifyHook, PropertyUtilities ) );
		}
		else if( FPropertyHandleObject::Supports( PropertyNode ) )
		{
			PropertyHandle = MakeShareable( new FPropertyHandleObject( PropertyNode, NotifyHook, PropertyUtilities ) );
		}
		else if( FPropertyHandleString::Supports( PropertyNode ) ) 
		{
			PropertyHandle = MakeShareable( new FPropertyHandleString( PropertyNode, NotifyHook, PropertyUtilities ) );
		}
		else if( FPropertyHandleVector::Supports( PropertyNode ) )
		{
			PropertyHandle = MakeShareable( new FPropertyHandleVector( PropertyNode, NotifyHook, PropertyUtilities ) );
		}
		else if( FPropertyHandleRotator::Supports( PropertyNode ) )
		{
			PropertyHandle = MakeShareable( new FPropertyHandleRotator( PropertyNode, NotifyHook, PropertyUtilities ) );
		}
		else
		{
			// Untyped or doesn't support getting the property directly but the property is still valid(probably struct property)
			PropertyHandle = MakeShareable( new FPropertyHandleBase( PropertyNode, NotifyHook, PropertyUtilities ) ); 
		}

		return PropertyHandle;
	}

	static bool SupportsObjectPropertyButtons( UProperty* NodeProperty, bool bUsingAssetPicker )
	{
		return (NodeProperty->IsA<UObjectPropertyBase>() || NodeProperty->IsA<UInterfaceProperty>()) && (!bUsingAssetPicker || !SPropertyEditorAsset::Supports(NodeProperty));
	}
	
	static bool IsStringAssetReference( const UProperty* Property )
	{
		bool bIsStringAssetRef = false;

		const UStructProperty* StructProp = Cast<const UStructProperty>( Property );
		if( StructProp && StructProp->Struct )
		{
			FName StructName = StructProp->Struct->GetFName();

			static const FName StringAssetRef("StringAssetReference");

			bIsStringAssetRef = StructName == StringAssetRef;
		}

		return bIsStringAssetRef;
	}

	static bool IsStringClassReference( const UProperty* Property )
	{
		bool bIsStringClassRef = false;

		const UStructProperty* StructProp = Cast<const UStructProperty>( Property );
		if( StructProp && StructProp->Struct )
		{
			FName StructName = StructProp->Struct->GetFName();

			static const FName StringClassRef("StringClassReference");

			bIsStringClassRef = StructName == StringClassRef;
		}

		return bIsStringClassRef;
	}

	void GetRequiredPropertyButtons( TSharedRef<FPropertyNode> PropertyNode, TArray<EPropertyButton::Type>& OutRequiredButtons, bool bUsingAssetPicker )
	{
		UProperty* NodeProperty = PropertyNode->GetProperty();

		// If no property is bound, don't create any buttons.
		// If property is const, don't create any buttons.
		if ( !NodeProperty || PropertyNode->IsEditConst() )
		{
			return;
		}

		// If the property is an item of a const array, don't create any buttons.
		const UArrayProperty* OuterArrayProp = Cast<UArrayProperty>( NodeProperty->GetOuter() );
		if (OuterArrayProp != NULL && (OuterArrayProp->PropertyFlags & CPF_EditConst))
		{
			return;
		}

		//////////////////////////////
		// Handle an array property.
		if( NodeProperty->IsA(UArrayProperty::StaticClass() ) )
		{
			if( !(NodeProperty->PropertyFlags & CPF_EditFixedSize) )
			{
				const UArrayProperty* ArrayProp = Cast<const UArrayProperty>( NodeProperty );
				if (ArrayProp)
				{
					//if this array supports actors
					UObjectPropertyBase* ObjProp = Cast<UObjectPropertyBase>( ArrayProp->Inner );
					if( ObjProp )
					{
						UClass* ObjPropClass = ObjProp->PropertyClass;
						if ( ObjPropClass->IsChildOf( AActor::StaticClass() ) )
						{
							OutRequiredButtons.Add( EPropertyButton::Use );
						}
					}
				}

				OutRequiredButtons.Add( EPropertyButton::Add );
				OutRequiredButtons.Add( EPropertyButton::Empty );
			}
		}

		//////////////////////////////
		// Handle an object property.
		
		
		if( SupportsObjectPropertyButtons( NodeProperty, bUsingAssetPicker ) )
		{
			//ignore this node if the consistency check should happen for the children
			bool bStaticSizedArray = (NodeProperty->ArrayDim > 1) && (PropertyNode->GetArrayIndex() == -1);
			if (!bStaticSizedArray)
			{
				FReadAddressList ReadAddresses;
				// Only add buttons if read addresses are all NULL or non-NULL.
				PropertyNode->GetReadAddress( false, ReadAddresses, false );
				{
					if( PropertyNode->HasNodeFlags(EPropertyNodeFlags::EditInline) )
					{
						// hmmm, seems like this code could be removed and the code inside the 'if <UClassProperty>' check
						// below could be moved outside the else....but is there a reason to allow class properties to have the
						// following buttons if the class property is marked 'editinline' (which is effectively what this logic is doing)
						if( !(NodeProperty->PropertyFlags & CPF_NoClear) )
						{
							OutRequiredButtons.Add( EPropertyButton::Clear );
						}
					}
					else
					{
						// ignore class properties
						if( (Cast<const UClassProperty>( NodeProperty ) == NULL) && (Cast<const UAssetClassProperty>( NodeProperty ) == NULL) )
						{
							UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>( NodeProperty );

							if( ObjectProperty && ObjectProperty->PropertyClass->IsChildOf( AActor::StaticClass() ) )
							{
								// add button for picking the actor from the viewport
								OutRequiredButtons.Add( EPropertyButton::PickActorInteractive );
							}
							else
							{
								// add button for filling the value of this item with the selected object from the GB
								OutRequiredButtons.Add( EPropertyButton::Use );
							}

							// add button to display the generic browser
							OutRequiredButtons.Add( EPropertyButton::Browse );

							// reference to object resource that isn't dynamically created (i.e. some content package)
							if( !(NodeProperty->PropertyFlags & CPF_NoClear) )
							{
								// add button to clear the text
								OutRequiredButtons.Add( EPropertyButton::Clear );
							}
							
							// Do not allow actor object properties to show the asset picker
							if( ( ObjectProperty && !ObjectProperty->PropertyClass->IsChildOf( AActor::StaticClass() ) ) || IsStringAssetReference(NodeProperty) )
							{
								// add button for picking the asset from an asset picker
								OutRequiredButtons.Add( EPropertyButton::PickAsset );
							}
							else if( ObjectProperty && ObjectProperty->PropertyClass->IsChildOf( AActor::StaticClass() ) )
							{
								// add button for picking the actor from the scene outliner
								OutRequiredButtons.Add( EPropertyButton::PickActor );
							}
						}
					}
				}
			}
		}

		//////////////////////////////
		// Handle a class property.

		if( NodeProperty->IsA<UClassProperty>() || IsStringClassReference(NodeProperty) || NodeProperty->IsA<UAssetClassProperty>() )
		{
			OutRequiredButtons.Add( EPropertyButton::Use );
			
			OutRequiredButtons.Add( EPropertyButton::Browse );

			if( !(NodeProperty->PropertyFlags & CPF_NoClear) )
			{
				OutRequiredButtons.Add( EPropertyButton::Clear );
			}
		}

		if( OuterArrayProp )
		{
			if( PropertyNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly) && !(OuterArrayProp->PropertyFlags & CPF_EditFixedSize) )
			{
				OutRequiredButtons.Add( EPropertyButton::Insert_Delete_Duplicate );
			}
		}

	}
	
	void MakeRequiredPropertyButtons( const TSharedRef<FPropertyNode>& PropertyNode, const TSharedRef<IPropertyUtilities>& PropertyUtilities,  TArray< TSharedRef<SWidget> >& OutButtons, const TArray<EPropertyButton::Type>& ButtonsToIgnore, bool bUsingAssetPicker  )
	{
		const TSharedRef<FPropertyEditor> PropertyEditor = FPropertyEditor::Create( PropertyNode, PropertyUtilities );
		PropertyEditorHelpers::MakeRequiredPropertyButtons( PropertyEditor, OutButtons, ButtonsToIgnore, bUsingAssetPicker );
	}

	void MakeRequiredPropertyButtons( const TSharedRef< FPropertyEditor >& PropertyEditor, TArray< TSharedRef<SWidget> >& OutButtons, const TArray<EPropertyButton::Type>& ButtonsToIgnore, bool bUsingAssetPicker )
	{
		TArray< EPropertyButton::Type > RequiredButtons;
		GetRequiredPropertyButtons( PropertyEditor->GetPropertyNode(), RequiredButtons, bUsingAssetPicker );

		for( int32 ButtonIndex = 0; ButtonIndex < RequiredButtons.Num(); ++ButtonIndex )
		{
			if( !ButtonsToIgnore.Contains( RequiredButtons[ButtonIndex] ) )
			{
				OutButtons.Add( MakePropertyButton( RequiredButtons[ButtonIndex], PropertyEditor ) );
			}
		}
	}

	/**
	 * A helper function that retrieves the path name of the currently selected 
	 * item (the value that will be used to set the associated property from the 
	 * "use selection" button)
	 * 
	 * @param  PropertyNode		The associated property that the selection is a candidate for.
	 * @return Empty if the selection isn't compatible with the specified property, else the path-name of the object/class selected in the editor. 
	 */
	static FString GetSelectionPathNameForProperty(TSharedRef<FPropertyNode> PropertyNode)
	{
		FString SelectionPathName;

		UProperty* Property = PropertyNode->GetProperty();
		UClassProperty* ClassProperty = Cast<UClassProperty>(Property);
		UAssetClassProperty* AssetClassProperty = Cast<UAssetClassProperty>(Property);

		if (ClassProperty || AssetClassProperty)
		{
			UClass const* const SelectedClass = GEditor->GetFirstSelectedClass(ClassProperty ? ClassProperty->MetaClass : AssetClassProperty->MetaClass);
			if (SelectedClass != nullptr)
			{
				SelectionPathName = SelectedClass->GetPathName();
			}
		}
		else
		{
			UClass* ObjectClass = UObject::StaticClass();

			bool bMustBeLevelActor = false;
			UClass* RequiredInterface = nullptr;

			if (UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>(Property))
			{
				ObjectClass = ObjectProperty->PropertyClass;
				bMustBeLevelActor = ObjectProperty->GetOwnerProperty()->GetBoolMetaData(TEXT("MustBeLevelActor"));
				RequiredInterface = ObjectProperty->GetOwnerProperty()->GetClassMetaData(TEXT("MustImplement"));
			}
			else if (UInterfaceProperty* InterfaceProperty = Cast<UInterfaceProperty>(Property))
			{
				ObjectClass = InterfaceProperty->InterfaceClass;
			}

			UObject* SelectedObject = nullptr;
			if (bMustBeLevelActor)
			{
				USelection* const SelectedSet = GEditor->GetSelectedActors();
				SelectedObject = SelectedSet->GetTop(ObjectClass, RequiredInterface);
			}
			else 
			{
				USelection* const SelectedSet = GEditor->GetSelectedSet(ObjectClass);
				SelectedObject = SelectedSet->GetTop(ObjectClass, RequiredInterface);
			}

			if (SelectedObject != nullptr)
			{
				SelectionPathName = SelectedObject->GetPathName();
			}
		}

		return SelectionPathName;
	}

	/**
	 * A helper method that checks to see if the editor's current selection is 
	 * compatible with the specified property.
	 * 
	 * @param  PropertyNode		The property you desire to set from the "use selected" button.
	 * @return False if the currently selected object is restricted for the specified property, true otherwise.
	 */
	static bool IsUseSelectedUnrestricted(TSharedRef<FPropertyNode> PropertyNode)
	{
		return !PropertyNode->IsRestricted(GetSelectionPathNameForProperty(PropertyNode));
	}

	/**
	 * A helper method that checks to see if the editor's current selection is 
	 * restricted, and then returns a tooltip explaining why (otherwise, it 
	 * returns a default explanation of the "use selected" button).
	 * 
	 * @param  PropertyNode		The property that would be set from the "use selected" button.
	 * @return A tooltip for the "use selected" button.
	 */
	static FText GetUseSelectedTooltip(TSharedRef<FPropertyNode> PropertyNode)
	{
		FText ToolTip;
		if (!PropertyNode->GenerateRestrictionToolTip(GetSelectionPathNameForProperty(PropertyNode), ToolTip))
		{
			ToolTip = LOCTEXT("UseButtonToolTipText", "Use Selected Asset from Content Browser");
		}

		return ToolTip;
	}

	TSharedRef<SWidget> MakePropertyButton( const EPropertyButton::Type ButtonType, const TSharedRef< FPropertyEditor >& PropertyEditor )
	{
		TSharedPtr<SWidget> NewButton;

		switch( ButtonType )
		{
		case EPropertyButton::Add:
			NewButton = PropertyCustomizationHelpers::MakeAddButton( FSimpleDelegate::CreateSP( PropertyEditor, &FPropertyEditor::AddItem ) );
			break;

		case EPropertyButton::Empty:
			NewButton = PropertyCustomizationHelpers::MakeEmptyButton( FSimpleDelegate::CreateSP( PropertyEditor, &FPropertyEditor::EmptyArray ) );
			break;

		case EPropertyButton::Insert_Delete_Duplicate:
			{
				FExecuteAction InsertAction = FExecuteAction::CreateSP( PropertyEditor, &FPropertyEditor::InsertItem );
				FExecuteAction DeleteAction = FExecuteAction::CreateSP( PropertyEditor, &FPropertyEditor::DeleteItem );
				FExecuteAction DuplicateAction = FExecuteAction::CreateSP( PropertyEditor, &FPropertyEditor::DuplicateItem );

				NewButton = PropertyCustomizationHelpers::MakeInsertDeleteDuplicateButton( InsertAction, DeleteAction, DuplicateAction );

				break;
			}

		case EPropertyButton::Browse:
			NewButton = PropertyCustomizationHelpers::MakeBrowseButton( FSimpleDelegate::CreateSP( PropertyEditor, &FPropertyEditor::BrowseTo ) );
			break;

		case EPropertyButton::Clear:
			NewButton = PropertyCustomizationHelpers::MakeClearButton( FSimpleDelegate::CreateSP( PropertyEditor, &FPropertyEditor::ClearItem ) );
			break;

		case EPropertyButton::Use:
			{
				FSimpleDelegate OnClickDelegate = FSimpleDelegate::CreateSP(PropertyEditor, &FPropertyEditor::UseSelected);
				TAttribute<bool>::FGetter EnabledDelegate = TAttribute<bool>::FGetter::CreateStatic(&IsUseSelectedUnrestricted, PropertyEditor->GetPropertyNode());
				TAttribute<FText>::FGetter TooltipDelegate = TAttribute<FText>::FGetter::CreateStatic(&GetUseSelectedTooltip, PropertyEditor->GetPropertyNode());

				NewButton = PropertyCustomizationHelpers::MakeUseSelectedButton(OnClickDelegate, TAttribute<FText>::Create(TooltipDelegate), TAttribute<bool>::Create(EnabledDelegate));
				break;
			}

		case EPropertyButton::PickAsset:
			NewButton = PropertyCustomizationHelpers::MakeAssetPickerAnchorButton( FOnGetAllowedClasses::CreateSP( PropertyEditor, &FPropertyEditor::OnGetClassesForAssetPicker ), FOnAssetSelected::CreateSP( PropertyEditor, &FPropertyEditor::OnAssetSelected ) );
			break;

		case EPropertyButton::PickActor:
			NewButton = PropertyCustomizationHelpers::MakeActorPickerAnchorButton( FOnGetActorFilters::CreateSP( PropertyEditor, &FPropertyEditor::OnGetActorFiltersForSceneOutliner ), FOnActorSelected::CreateSP( PropertyEditor, &FPropertyEditor::OnActorSelected ) );
			break;

		case EPropertyButton::PickActorInteractive:
			NewButton = PropertyCustomizationHelpers::MakeInteractiveActorPicker( FOnGetAllowedClasses::CreateSP( PropertyEditor, &FPropertyEditor::OnGetClassesForAssetPicker ), FOnShouldFilterActor(), FOnActorSelected::CreateSP( PropertyEditor, &FPropertyEditor::OnActorSelected ) );
			break;

		default:
			checkf( 0, TEXT( "Unknown button type" ) );
			break;
		}

		return NewButton.ToSharedRef();
	}

	void CollectObjectNodes( TSharedPtr<FPropertyNode> StartNode, TArray<FObjectPropertyNode*>& OutObjectNodes )
	{
		if( StartNode->AsObjectNode() != NULL )
		{
			OutObjectNodes.Add( StartNode->AsObjectNode() );
		}
			
		for( int32 ChildIndex = 0; ChildIndex < StartNode->GetNumChildNodes(); ++ChildIndex )
		{
			CollectObjectNodes( StartNode->GetChildNode( ChildIndex ), OutObjectNodes );
		}
		
	}
}

#undef LOCTEXT_NAMESPACE
