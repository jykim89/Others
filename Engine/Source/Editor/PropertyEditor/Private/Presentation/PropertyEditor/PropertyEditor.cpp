// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorPrivatePCH.h"
#include "PropertyEditorConstants.h"
#include "PropertyEditorHelpers.h"
#include "IPropertyUtilities.h"

#include "PropertyHandleImpl.h"

#include "PropertyEditor.h"

#include "DelegateFilter.h"

#define LOCTEXT_NAMESPACE "PropertyEditor"

TSharedRef< FPropertyEditor > FPropertyEditor::Create( const TSharedRef< class FPropertyNode >& InPropertyNode, const TSharedRef<class IPropertyUtilities >& InPropertyUtilities )
{
	return MakeShareable( new FPropertyEditor( InPropertyNode, InPropertyUtilities ) );
}

FPropertyEditor::FPropertyEditor( const TSharedRef<FPropertyNode>& InPropertyNode, const TSharedRef<IPropertyUtilities>& InPropertyUtilities )
	: PropertyEditConditions()
	, PropertyHandle( NULL )
	, PropertyNode( InPropertyNode )
	, PropertyUtilities( InPropertyUtilities )
	, EditConditionProperty( NULL )
{
	// FPropertyEditor isn't built to handle CategoryNodes
	check( InPropertyNode->AsCategoryNode() == NULL );

	UProperty* Property = InPropertyNode->GetProperty();

	if( Property )
	{
		//see if the property supports some kind of edit condition and this isn't the "parent" property of a static array
		const bool bStaticArray = Property->ArrayDim > 1 && InPropertyNode->GetArrayIndex() == INDEX_NONE;

		if ( Property->HasMetaData( TEXT( "EditCondition" ) ) && !bStaticArray ) 
		{
			if ( !GetEditConditionPropertyAddress( /*OUT*/EditConditionProperty, *InPropertyNode, PropertyEditConditions ) )
			{
				EditConditionProperty = NULL;
			}
		}
	}

	PropertyHandle = PropertyEditorHelpers::GetPropertyHandle( InPropertyNode, PropertyUtilities->GetNotifyHook(), PropertyUtilities );
	check( PropertyHandle.IsValid() && PropertyHandle->IsValidHandle() );
}


FString FPropertyEditor::GetDisplayName() const
{
	FCategoryPropertyNode* CategoryNode = PropertyNode->AsCategoryNode();
	FItemPropertyNode* ItemPropertyNode = PropertyNode->AsItemPropertyNode();

	if ( CategoryNode != NULL )
	{
		return CategoryNode->GetDisplayName();
	}
	else if ( ItemPropertyNode != NULL )
	{
		return ItemPropertyNode->GetDisplayName();
	}
	else
	{
		FString DisplayName;
		PropertyNode->GetQualifiedName( DisplayName, true );

		return DisplayName;
	}
}

FString FPropertyEditor::GetToolTipText() const
{
	return PropertyNode->GetToolTipText();
}

FString FPropertyEditor::GetDocumentationLink() const
{
	FString DocumentationLink;

	if( PropertyNode->AsItemPropertyNode() )
	{
		UProperty* Property = PropertyNode->GetProperty();
		DocumentationLink = PropertyEditorHelpers::GetDocumentationLink( Property );
	}

	return DocumentationLink;
}

FString FPropertyEditor::GetDocumentationExcerptName() const
{
	FString ExcerptName;

	if( PropertyNode->AsItemPropertyNode() )
	{
		UProperty* Property = PropertyNode->GetProperty();
		ExcerptName = PropertyEditorHelpers::GetDocumentationExcerptName( Property );
	}

	return ExcerptName;
}

FString FPropertyEditor::GetValueAsString() const 
{
	FString Str;

	if( PropertyHandle->GetValueAsFormattedString( Str ) == FPropertyAccess::MultipleValues )
	{
		Str = NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values").ToString();
	}

	return Str;
}

FString FPropertyEditor::GetValueAsDisplayString() const
{
	FString Str;

	if( PropertyHandle->GetValueAsDisplayString( Str ) == FPropertyAccess::MultipleValues )
	{
		Str = NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values").ToString();
	}

	return Str;
}

FText FPropertyEditor::GetValueAsText() const
{
	FText Text;

	if( PropertyHandle->GetValueAsFormattedText( Text ) == FPropertyAccess::MultipleValues )
	{
		Text = NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values");
	}

	return Text;
}

FText FPropertyEditor::GetValueAsDisplayText() const
{
	FText Text;

	if( PropertyHandle->GetValueAsDisplayText( Text ) == FPropertyAccess::MultipleValues )
	{
		Text = NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values");
	}

	return Text;
}

bool FPropertyEditor::PropertyIsA(const UClass* Class) const
{
	return PropertyNode->GetProperty() != NULL ? PropertyNode->GetProperty()->IsA( Class ) : false;
}

bool FPropertyEditor::IsFavorite() const 
{ 
	return PropertyNode->HasNodeFlags( EPropertyNodeFlags::IsFavorite ) != 0; 
}

bool FPropertyEditor::IsChildOfFavorite() const 
{ 
	return PropertyNode->IsChildOfFavorite(); 
}

void FPropertyEditor::ToggleFavorite() 
{ 
	PropertyUtilities->ToggleFavorite( SharedThis( this ) ); 
}

void FPropertyEditor::UseSelected()
{
	OnUseSelected();
}

void FPropertyEditor::OnUseSelected()
{
	PropertyHandle->SetObjectValueFromSelection();
}

void FPropertyEditor::AddItem()
{
	// This action must be deferred until next tick so that we avoid accessing invalid data before we have a chance to tick
	PropertyUtilities->EnqueueDeferredAction( FSimpleDelegate::CreateSP( this, &FPropertyEditor::OnAddItem ) );
}

void FPropertyEditor::OnAddItem()
{
	TSharedPtr<IPropertyHandleArray> ArrayHandle = PropertyHandle->AsArray();
	check( ArrayHandle.IsValid() );

	// Expand arrays when an item is added to them
	PropertyNode->SetNodeFlags( EPropertyNodeFlags::Expanded, true );

	ArrayHandle->AddItem();
}

void FPropertyEditor::ClearItem()
{
	OnClearItem();
}

void FPropertyEditor::OnClearItem()
{
	static const FString None("None");
	PropertyHandle->SetValueFromFormattedString( None );
}

void FPropertyEditor::InsertItem()
{
	// This action must be deferred until next tick so that we avoid accessing invalid data before we have a chance to tick
	PropertyUtilities->EnqueueDeferredAction( FSimpleDelegate::CreateSP( this, &FPropertyEditor::OnInsertItem ) );
}

void FPropertyEditor::OnInsertItem()
{
	TSharedPtr<IPropertyHandleArray> ArrayHandle = PropertyHandle->GetParentHandle()->AsArray();
	check( ArrayHandle.IsValid() );

	int32 Index = PropertyNode->GetArrayIndex();
	ArrayHandle->Insert( Index );
}

void FPropertyEditor::DeleteItem()
{
	// This action must be deferred until next tick so that we avoid accessing invalid data before we have a chance to tick
	PropertyUtilities->EnqueueDeferredAction( FSimpleDelegate::CreateSP( this, &FPropertyEditor::OnDeleteItem ) );
}

void FPropertyEditor::OnDeleteItem()
{
	TSharedPtr<IPropertyHandleArray> ArrayHandle = PropertyHandle->GetParentHandle()->AsArray();
	check( ArrayHandle.IsValid() );

	int32 Index = PropertyNode->GetArrayIndex();
	ArrayHandle->DeleteItem( Index );
}

void FPropertyEditor::DuplicateItem()
{
	// This action must be deferred until next tick so that we avoid accessing invalid data before we have a chance to tick
	PropertyUtilities->EnqueueDeferredAction( FSimpleDelegate::CreateSP( this, &FPropertyEditor::OnDuplicateItem ) );
}

void FPropertyEditor::OnDuplicateItem()
{
	TSharedPtr<IPropertyHandleArray> ArrayHandle = PropertyHandle->GetParentHandle()->AsArray();
	check( ArrayHandle.IsValid() );

	int32 Index = PropertyNode->GetArrayIndex();
	ArrayHandle->DuplicateItem( Index );
}

void FPropertyEditor::BrowseTo()
{
	OnBrowseTo();
}

void FPropertyEditor::OnBrowseTo()
{
	// Sync the content browser or level editor viewport to the object(s) specified by the given property.
	SyncToObjectsInNode( PropertyNode );
}

void FPropertyEditor::EmptyArray()
{
	// This action must be deferred until next tick so that we avoid accessing invalid data before we have a chance to tick
	PropertyUtilities->EnqueueDeferredAction( FSimpleDelegate::CreateSP( this, &FPropertyEditor::OnEmptyArray ) );
}

void FPropertyEditor::OnEmptyArray()
{
	TSharedPtr<IPropertyHandleArray> ArrayHandle = PropertyHandle->AsArray();
	check( ArrayHandle.IsValid() );

	ArrayHandle->EmptyArray();
}

bool FPropertyEditor::DoesPassFilterRestrictions() const
{
	return PropertyNode->HasNodeFlags( EPropertyNodeFlags::IsSeenDueToFiltering ) != 0;
}

bool FPropertyEditor::IsEditConst() const
{
	return PropertyNode->IsEditConst();
}

void FPropertyEditor::SetEditConditionState( bool bShouldEnable )
{
	PropertyNode->NotifyPreChange( PropertyNode->GetProperty(), PropertyUtilities->GetNotifyHook() );
	for ( int32 ValueIdx = 0; ValueIdx < PropertyEditConditions.Num(); ValueIdx++ )
	{
		uint8* ValueAddr = PropertyEditConditions[ValueIdx].Address;
		if ( XOR(bShouldEnable, PropertyEditConditions[ValueIdx].bNegateValue) )
		{
			EditConditionProperty->SetPropertyValue( ValueAddr, true );
		}
		else
		{
			EditConditionProperty->SetPropertyValue( ValueAddr, false );
		}
	}

	const bool bTopologyChange = true;
	FPropertyChangedEvent ChangeEvent(PropertyNode->GetProperty(), bTopologyChange);
	PropertyNode->NotifyPostChange( ChangeEvent, PropertyUtilities->GetNotifyHook() );
}

void FPropertyEditor::ResetToDefault()
{
	// This action must be deferred until next tick so that we avoid accessing invalid data before we have a chance to tick
	PropertyUtilities->EnqueueDeferredAction( FSimpleDelegate::CreateSP( this, &FPropertyEditor::OnResetToDefault ) );
}

void FPropertyEditor::CustomResetToDefault( FSimpleDelegate OnCustomResetToDefaultDelegate )
{
	// This action must be deferred until next tick so that we avoid accessing invalid data before we have a chance to tick
	PropertyUtilities->EnqueueDeferredAction( FSimpleDelegate::CreateSP( this, &FPropertyEditor::OnCustomResetToDefault, OnCustomResetToDefaultDelegate ) );
}

void FPropertyEditor::OnGetClassesForAssetPicker( TArray<const UClass*>& OutClasses )
{
	UProperty* NodeProperty = GetPropertyNode()->GetProperty();

	UObjectPropertyBase* ObjProp = Cast<UObjectPropertyBase>( NodeProperty );

	// This class and its children are the classes that we can show objects for
	UClass* AllowedClass = ObjProp ? ObjProp->PropertyClass : UObject::StaticClass();

	OutClasses.Add( AllowedClass );
}

void FPropertyEditor::OnAssetSelected( const FAssetData& AssetData )
{
	// Set the object found from the asset picker
	GetPropertyHandle()->SetValueFromFormattedString( AssetData.IsValid() ? AssetData.GetAsset()->GetPathName() : TEXT("None") );
}

void FPropertyEditor::OnActorSelected( AActor* InActor )
{
	// Update the name like we would a picked asset
	OnAssetSelected(InActor);
}

void FPropertyEditor::OnGetActorFiltersForSceneOutliner( TSharedPtr<SceneOutliner::FOutlinerFilters>& OutFilters )
{
	struct Local
	{
		static bool IsFilteredActor( const AActor* const Actor, TSharedRef<FPropertyEditor> PropertyEditor )
		{
			const TSharedRef<FPropertyNode> PropertyNode = PropertyEditor->GetPropertyNode();
			UProperty* NodeProperty = PropertyNode->GetProperty();

			UObjectPropertyBase* ObjProp = Cast<UObjectPropertyBase>( NodeProperty );

			// This class and its children are the classes that we can show objects for
			UClass* AllowedClass = ObjProp ? ObjProp->PropertyClass : AActor::StaticClass();

			return Actor->IsA( AllowedClass );
		}
	};

	OutFilters->Add( MakeShareable( new TDelegateFilter< const AActor* const >( TDelegateFilter< const AActor* const >::FPredicate::CreateStatic( &Local::IsFilteredActor, AsShared() ) ) ) );
}

void FPropertyEditor::OnResetToDefault()
{
	PropertyNode->ResetToDefault( PropertyUtilities->GetNotifyHook() );
}

void FPropertyEditor::OnCustomResetToDefault( FSimpleDelegate OnCustomResetToDefaultDelegate )
{
	if(OnCustomResetToDefaultDelegate.IsBound())
	{
		PropertyNode->NotifyPreChange( PropertyNode->GetProperty(), PropertyUtilities->GetNotifyHook() );

		OnCustomResetToDefaultDelegate.Execute();

		// Call PostEditchange on all the objects
		// Assume reset to default, can change topology
		const bool bTopologyChange = true;
		FPropertyChangedEvent ChangeEvent( PropertyNode->GetProperty(), bTopologyChange );
		PropertyNode->NotifyPostChange( ChangeEvent, PropertyUtilities->GetNotifyHook() );
	}
}

bool FPropertyEditor::IsPropertyEditingEnabled() const
{
	return ( PropertyUtilities->IsPropertyEditingEnabled() ) && 
		(EditConditionProperty == NULL || IsEditConditionMet( EditConditionProperty, PropertyEditConditions ));
}

bool FPropertyEditor::HasEditCondition() const 
{ 
	return EditConditionProperty != NULL; 
}

bool FPropertyEditor::IsEditConditionMet() const 
{ 
	return IsEditConditionMet( EditConditionProperty, PropertyEditConditions ); 
}

bool FPropertyEditor::SupportsEditConditionToggle() const
{
	return SupportsEditConditionToggle( PropertyNode->GetProperty() );
}

bool FPropertyEditor::IsResetToDefaultAvailable() const
{
	UProperty* Property = PropertyNode->GetProperty();

	// Should not be able to reset fixed size arrays
	const bool bFixedSized = Property && Property->PropertyFlags & CPF_EditFixedSize;

	return Property && !PropertyHandle->IsEditConst() && !bFixedSized && PropertyHandle->DiffersFromDefault();
}

bool FPropertyEditor::ValueDiffersFromDefault() const
{
	return PropertyHandle->DiffersFromDefault();
}

FText FPropertyEditor::GetResetToDefaultLabel() const
{
	return PropertyNode->GetResetToDefaultLabel();
}

void FPropertyEditor::AddPropertyEditorChild( const TSharedRef<FPropertyEditor>& Child )
{
	ChildPropertyEditors.Add( Child );
}

void FPropertyEditor::RemovePropertyEditorChild( const TSharedRef<FPropertyEditor>& Child )
{
	ChildPropertyEditors.Remove( Child );
}

const TArray< TSharedRef< FPropertyEditor > >& FPropertyEditor::GetPropertyEditorChildren() const
{
	return ChildPropertyEditors;
}

TSharedRef< FPropertyNode > FPropertyEditor::GetPropertyNode() const
{
	return PropertyNode;
}

const UProperty* FPropertyEditor::GetProperty() const
{
	return PropertyNode->GetProperty();
}

TSharedRef< IPropertyHandle > FPropertyEditor::GetPropertyHandle() const
{
	return PropertyHandle.ToSharedRef();
}

bool FPropertyEditor::IsEditConditionMet( UBoolProperty* ConditionProperty, const TArray<FPropertyConditionInfo>& ConditionValues )
{
	check( ConditionProperty );

	bool bResult = false;
	bool bAllConditionsMet = true;

	for ( int32 ValueIdx = 0; bAllConditionsMet && ValueIdx < ConditionValues.Num(); ValueIdx++ )
	{
		uint8* ValueAddr = ConditionValues[ValueIdx].Address;
		if (ConditionValues[ValueIdx].bNegateValue)
		{
			bAllConditionsMet = !ConditionProperty->GetPropertyValue( ValueAddr );
		}
		else
		{
			bAllConditionsMet = ConditionProperty->GetPropertyValue( ValueAddr );
		}
	}

	bResult = bAllConditionsMet;

	return bResult;
}

bool FPropertyEditor::GetEditConditionPropertyAddress( UBoolProperty*& ConditionProperty, FPropertyNode& InPropertyNode, TArray<FPropertyConditionInfo>& ConditionPropertyAddresses )
{
	bool bResult = false;
	bool bNegate = false;
	UBoolProperty* EditConditionProperty = GetEditConditionProperty( InPropertyNode.GetProperty(), bNegate);
	if ( EditConditionProperty != NULL )
	{
		FPropertyNode* ParentNode = InPropertyNode.GetParentNode();
		check(ParentNode);

		UProperty* Property = InPropertyNode.GetProperty();
		if (Property)
		{
			bool bStaticArray = (Property->ArrayDim > 1) && (InPropertyNode.GetArrayIndex() != INDEX_NONE);
			if (bStaticArray)
			{
				//in the case of conditional static arrays, we have to go up one more level to get the proper parent struct.
				ParentNode = ParentNode->GetParentNode();
				check(ParentNode);
			}
		}

		FObjectPropertyNode* ObjectNode = ParentNode->FindObjectItemParent();
		for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
		{
			TWeakObjectPtr<UObject> Object = *Itor;

			if( Object.IsValid() )
			{
				UObject* Obj = Object.Get();

				// Get the address corresponding to the base of this property (i.e. if a struct property, set BaseOffset to the address of value for the whole struct)
				uint8* BaseOffset = ParentNode->GetValueAddress((uint8*)Obj);
				check(BaseOffset != NULL);

				FPropertyConditionInfo NewCondition;
				// now calculate the address of the property value being used as the condition and add it to the array.
				NewCondition.Address = EditConditionProperty->ContainerPtrToValuePtr<uint8>(BaseOffset);
				NewCondition.bNegateValue = bNegate;
				ConditionPropertyAddresses.Add(NewCondition);
				bResult = true;
			}
		}
	}

	if ( bResult )
	{
		// set the output variable
		ConditionProperty = EditConditionProperty;
	}

	return bResult;
}

bool FPropertyEditor::SupportsEditConditionToggle( UProperty* InProperty ) 
{
	bool bIsConditionalPropertyVisible = false;
	{
		bool bNegateValue = false;
		UBoolProperty* ConditionalProperty = GetEditConditionProperty( InProperty, bNegateValue );
		if( ConditionalProperty != NULL )
		{
			if( ConditionalProperty->HasAllPropertyFlags( CPF_Edit ) )
			{
				// Conditionally-dependent property is already exposed for editing, so no need to draw another
				// check box next to this property's label
				bIsConditionalPropertyVisible = true;
			}
		}
	}

	return !bIsConditionalPropertyVisible;
}

UBoolProperty* FPropertyEditor::GetEditConditionProperty( const UProperty* InProperty, bool& bNegate ) 
{
	UBoolProperty* EditConditionProperty = NULL;
	bNegate = false;

	if ( InProperty != NULL )
	{
		// find the name of the property that should be used to determine whether this property should be editable
		FString ConditionPropertyName = InProperty->GetMetaData(TEXT("EditCondition"));

		// Support negated edit conditions whose syntax is !BoolProperty
		if (ConditionPropertyName.StartsWith(FString(TEXT("!"))))
		{
			bNegate = true;
			// Chop off the negation from the property name
			ConditionPropertyName = ConditionPropertyName.Right(ConditionPropertyName.Len() - 1);
		}

		// for now, only support boolean conditions, and only allow use of another property within the same struct as the conditional property
		if ( ConditionPropertyName.Len() > 0 && !ConditionPropertyName.Contains(TEXT(".")) )
		{
			UStruct* Scope = InProperty->GetOwnerStruct();
			EditConditionProperty = FindField<UBoolProperty>(Scope, *ConditionPropertyName);
		}
	}

	return EditConditionProperty;
}

void FPropertyEditor::SyncToObjectsInNode( const TWeakPtr< FPropertyNode >& WeakPropertyNode )
{
#if WITH_EDITOR

	if ( !GUnrealEd )
	{
		return;
	}

	TSharedPtr< FPropertyNode > PropertyNode = WeakPropertyNode.Pin();
	check(PropertyNode.IsValid());
	UProperty* NodeProperty = PropertyNode->GetProperty();

	UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>( NodeProperty );
	UInterfaceProperty* IntProp = Cast<UInterfaceProperty>( NodeProperty );
	{
		UClass* PropertyClass = UObject::StaticClass();
		if( ObjectProperty )
		{
			PropertyClass = ObjectProperty->PropertyClass;
		}
		else if( IntProp )
		{
			PropertyClass = IntProp->InterfaceClass;
		}

		// Get a list of addresses for objects handled by the property window.
		FReadAddressList ReadAddresses;
		PropertyNode->GetReadAddress( !!PropertyNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), ReadAddresses, false );

		// GetReadAddresses will only provide a list of addresses if the property was properly formed, objects were selected, and only one object was selected if the node has the SingleSelectOnly flag.
		// If a list of addresses is provided, GetReadAddress may still return false but we can operate on the property addresses even if they have different values.
		check( ReadAddresses.Num() > 0 );

		// Create a list of object names.
		TArray<FString> ObjectNames;
		ObjectNames.Empty( ReadAddresses.Num() );

		// Copy each object's object property name off into the name list.
		for ( int32 AddrIndex = 0 ; AddrIndex < ReadAddresses.Num() ; ++AddrIndex )
		{
			new( ObjectNames ) FString();
			uint8* Address = ReadAddresses.GetAddress(AddrIndex);
			if( Address )
			{
				NodeProperty->ExportText_Direct(ObjectNames[AddrIndex], Address, Address, NULL, PPF_Localized );
			}
		}


		// Create a list of objects to sync the generic browser to.
		TArray<UObject*> Objects;
		for ( int32 ObjectIndex = 0 ; ObjectIndex < ObjectNames.Num() ; ++ObjectIndex )
		{

			UObject* Package = ANY_PACKAGE;
			if( ObjectNames[ObjectIndex].Contains( TEXT(".")) )
			{
				// Formatted text string, use the exact path instead of any package
				Package = NULL;
			}

			UObject* Object = StaticFindObject( PropertyClass, Package, *ObjectNames[ObjectIndex] );
			if( !Object && Package != ANY_PACKAGE )
			{
				Object = StaticLoadObject(PropertyClass, Package, *ObjectNames[ObjectIndex]);
			}
			if ( Object )
			{
				// If the selected object is a blueprint generated class, then browsing to it in the content browser should instead point to the blueprint
				// Note: This code needs to change once classes are the top level asset in the content browser and/or blueprint classes are displayed in the content browser
				if (UClass* ObjectAsClass = Cast<UClass>(Object))
				{
					if (ObjectAsClass->ClassGeneratedBy != NULL)
					{
						Object = ObjectAsClass->ClassGeneratedBy;
					}
				}

				Objects.Add( Object );
			}
		}

		// If a single actor is selected, sync to its location in the level editor viewport instead of the content browser.
		if( Objects.Num() == 1 && Objects[0]->IsA(AActor::StaticClass()) )
		{
			TArray<AActor*> Actors;
			Actors.Add(Cast<AActor>(Objects[0]));

			GEditor->SelectNone(/*bNoteSelectionChange=*/false, /*bDeselectBSPSurfs=*/true);
			GEditor->SelectActor(Actors[0], /*InSelected=*/true, /*bNotify=*/true, /*bSelectEvenIfHidden=*/true);

			// Jump to the location of the actor
			GEditor->MoveViewportCamerasToActor( Actors, /*bActiveViewportOnly=*/false );
		}
		else if ( Objects.Num() > 0 )
		{
			GEditor->SyncBrowserToObjects(Objects);
		}
	}

#endif
}

#undef LOCTEXT_NAMESPACE
