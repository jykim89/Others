// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "PropertyEditorPrivatePCH.h"
#include "PropertyNode.h"
#include "ItemPropertyNode.h"
#include "CategoryPropertyNode.h"
#include "ObjectPropertyNode.h"

FItemPropertyNode::FItemPropertyNode(void)
	: FPropertyNode()
{

}

FItemPropertyNode::~FItemPropertyNode(void)
{

}

/**
 * Calculates the memory address for the data associated with this item's property.  This is typically the value of a UProperty or a UObject address.
 *
 * @param	StartAddress	the location to use as the starting point for the calculation; typically the address of the object that contains this property.
 *
 * @return	a pointer to a UProperty value or UObject.  (For dynamic arrays, you'd cast this value to an FArray*)
 */
uint8* FItemPropertyNode::GetValueBaseAddress( uint8* StartAddress )
{
	UProperty* MyProperty = GetProperty();
	if( MyProperty )
	{
		UArrayProperty* OuterArrayProp = Cast<UArrayProperty>(MyProperty->GetOuter());
		if ( OuterArrayProp != NULL )
		{
			FScriptArrayHelper ArrayHelper(OuterArrayProp,ParentNode->GetValueBaseAddress(StartAddress));
			if ( ParentNode->GetValueBaseAddress(StartAddress) != NULL && ArrayIndex < ArrayHelper.Num() )
			{
				return ArrayHelper.GetRawPtr() + ArrayOffset;
			}

			return NULL;
		}
		else
		{
			uint8* ValueAddress = ParentNode->GetValueAddress(StartAddress);
			if (ValueAddress != NULL && ParentNode->GetProperty() != MyProperty)
			{
				// if this is not a fixed size array (in which the parent property and this property are the same), we need to offset from the property (otherwise, the parent already did that for us)
				ValueAddress = Property->ContainerPtrToValuePtr<uint8>(ValueAddress);
			}
			if ( ValueAddress != NULL )
			{
				ValueAddress += ArrayOffset;
			}
			return ValueAddress;
		}
	}

	return NULL;
}

/**
 * Calculates the memory address for the data associated with this item's value.  For most properties, identical to GetValueBaseAddress.  For items corresponding
 * to dynamic array elements, the pointer returned will be the location for that element's data. 
 *
 * @param	StartAddress	the location to use as the starting point for the calculation; typically the address of the object that contains this property.
 *
 * @return	a pointer to a UProperty value or UObject.  (For dynamic arrays, you'd cast this value to whatever type is the Inner for the dynamic array)
 */
uint8* FItemPropertyNode::GetValueAddress( uint8* StartAddress )
{
	uint8* Result = GetValueBaseAddress(StartAddress);

	UProperty* MyProperty = GetProperty();

	UArrayProperty* ArrayProperty;
	if( Result != NULL && (ArrayProperty=Cast<UArrayProperty>(MyProperty))!=NULL )
	{
		FScriptArrayHelper ArrayHelper(ArrayProperty,Result);
		Result = ArrayHelper.GetRawPtr();
	}

	return Result;
}

/**
 * Overridden function for special setup
 */
void FItemPropertyNode::InitExpansionFlags (void)
{
	
	UProperty* MyProperty = GetProperty();

	FReadAddressList Addresses;
	if(	Cast<UStructProperty>(MyProperty)
		||	( Cast<UArrayProperty>(MyProperty) && GetReadAddress(false,Addresses) )
		||  HasNodeFlags(EPropertyNodeFlags::EditInline)
		||	( Property->ArrayDim > 1 && ArrayIndex == -1 ) 
		|| Cast<UAttributeProperty>(MyProperty) )
	{
		SetNodeFlags(EPropertyNodeFlags::CanBeExpanded, true);
	}
}
/**
 * Overridden function for Creating Child Nodes
 */
void FItemPropertyNode::InitChildNodes()
{
	//NOTE - this is only turned off as to not invalidate child object nodes.
	UProperty* Property = GetProperty();
	UStructProperty* StructProperty = Cast<UStructProperty>(Property);
	UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property);
	UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>(Property);
	UAttributeProperty* AttributeProperty = Cast<UAttributeProperty>(Property);

	bool bShouldShowHiddenProperties = !!HasNodeFlags(EPropertyNodeFlags::ShouldShowHiddenProperties);

	if( Property->ArrayDim > 1 && ArrayIndex == -1 )
	{

		// Do not add array children which are defined by an enum but the enum at the array index is hidden
		// This only applies to static arrays
		static const FName NAME_ArraySizeEnum("ArraySizeEnum");
		UEnum* ArraySizeEnum = NULL; 
		if (Property->HasMetaData(NAME_ArraySizeEnum))
		{
			ArraySizeEnum	= FindObject<UEnum>(NULL, *Property->GetMetaData(NAME_ArraySizeEnum));
		}

	
		// Expand array.
		for( int32 ArrayIndex = 0 ; ArrayIndex < Property->ArrayDim ; ArrayIndex++ )
		{
		
			bool bShouldBeHidden = false;
			if( ArraySizeEnum )
			{
				// The enum at this array index is hidden
				bShouldBeHidden = ArraySizeEnum->HasMetaData(TEXT("Hidden"), ArrayIndex );
			}

			if( !bShouldBeHidden )
			{
				TSharedPtr<FItemPropertyNode> NewItemNode( new FItemPropertyNode);
				FPropertyNodeInitParams InitParams;
				InitParams.ParentNode = SharedThis(this);
				InitParams.Property = Property;
				InitParams.ArrayOffset = ArrayIndex*Property->ElementSize;
				InitParams.ArrayIndex = ArrayIndex;
				InitParams.bAllowChildren = true;
				InitParams.bForceHiddenPropertyVisibility = bShouldShowHiddenProperties;

				NewItemNode->InitNode( InitParams );
				AddChildNode(NewItemNode);
			}
		}
	}
	else if( ArrayProperty )
	{
		void* Array = NULL;
		FReadAddressList Addresses;
		if ( GetReadAddress(!!HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), Addresses ) )
		{
			Array = Addresses.GetAddress(0);
		}

		if( Array )
		{
			for( int32 ArrayIndex = 0 ; ArrayIndex < FScriptArrayHelper::Num(Array) ; ArrayIndex++ )
			{
				TSharedPtr<FItemPropertyNode> NewItemNode( new FItemPropertyNode );

				FPropertyNodeInitParams InitParams;
				InitParams.ParentNode = SharedThis(this);
				InitParams.Property = ArrayProperty->Inner;
				InitParams.ArrayOffset = ArrayIndex*ArrayProperty->Inner->ElementSize;
				InitParams.ArrayIndex = ArrayIndex;
				InitParams.bAllowChildren = true;
				InitParams.bForceHiddenPropertyVisibility = bShouldShowHiddenProperties;

				NewItemNode->InitNode( InitParams );
				AddChildNode(NewItemNode);
			}
		}
	}
	else if( StructProperty )
	{
		// Expand struct.
		for( TFieldIterator<UProperty> It(StructProperty->Struct); It; ++It )
		{
			UProperty* StructMember = *It;
			if( bShouldShowHiddenProperties || (StructMember->PropertyFlags & CPF_Edit) )
			{
				TSharedPtr<FItemPropertyNode> NewItemNode( new FItemPropertyNode );//;//CreatePropertyItem(StructMember,INDEX_NONE,this);
		
				FPropertyNodeInitParams InitParams;
				InitParams.ParentNode = SharedThis(this);
				InitParams.Property = StructMember;
				InitParams.ArrayOffset = 0;
				InitParams.ArrayIndex = INDEX_NONE;
				InitParams.bAllowChildren = true;
				InitParams.bForceHiddenPropertyVisibility = bShouldShowHiddenProperties;

				NewItemNode->InitNode( InitParams );
				AddChildNode(NewItemNode);

				if ( FPropertySettings::Get().ExpandDistributions() == false)
				{
					// auto-expand distribution structs
					if ( Cast<UObjectProperty>(StructMember) || Cast<UWeakObjectProperty>(StructMember) || Cast<ULazyObjectProperty>(StructMember) || Cast<UAssetObjectProperty>(StructMember) )
					{
						const FName StructName = StructProperty->Struct->GetFName();
						if (StructName == NAME_RawDistributionFloat || StructName == NAME_RawDistributionVector)
						{
							NewItemNode->SetNodeFlags(EPropertyNodeFlags::Expanded, true);
						}
					}
				}
			}
		}
	}
	else if( ObjectProperty || Property->IsA(UInterfaceProperty::StaticClass()))
	{
		uint8* ReadValue = NULL;

		FReadAddressList ReadAddresses;
		if( GetReadAddress(!!HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), ReadAddresses, false ) )
		{
			// We've got some addresses, and we know they're all NULL or non-NULL.
			// Have a peek at the first one, and only build an objects node if we've got addresses.
			UObject* obj = ObjectProperty->GetObjectPropertyValue(ReadAddresses.GetAddress(0));
			if( obj )
			{
				//verify it's not above in the hierarchy somewhere
				FObjectPropertyNode* ParentObjectNode = FindObjectItemParent();
				while (ParentObjectNode)
				{
					for ( TPropObjectIterator Itor( ParentObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
					{
						if (*Itor == obj)
						{
							SetNodeFlags(EPropertyNodeFlags::NoChildrenDueToCircularReference, true);
							//stop the circular loop!!!
							return;
						}
					}
					FPropertyNode* UpwardTravesalNode = ParentObjectNode->GetParentNode();
					ParentObjectNode = (UpwardTravesalNode==NULL) ? NULL : UpwardTravesalNode->FindObjectItemParent();
				}

			
				TSharedPtr<FObjectPropertyNode> NewObjectNode( new FObjectPropertyNode );
				for ( int32 AddressIndex = 0 ; AddressIndex < ReadAddresses.Num() ; ++AddressIndex )
				{
					NewObjectNode->AddObject( ObjectProperty->GetObjectPropertyValue(ReadAddresses.GetAddress(AddressIndex) ) );
				}

				FPropertyNodeInitParams InitParams;
				InitParams.ParentNode = SharedThis(this);
				InitParams.Property = Property;
				InitParams.ArrayOffset = 0;
				InitParams.ArrayIndex = INDEX_NONE;
				InitParams.bAllowChildren = true;
				InitParams.bForceHiddenPropertyVisibility = bShouldShowHiddenProperties;

				NewObjectNode->InitNode( InitParams );
				AddChildNode(NewObjectNode);
			}
		}
	}
	else if( AttributeProperty )
	{
		TSharedPtr<FItemPropertyNode> NewItemNode(new FItemPropertyNode);

		FPropertyNodeInitParams InitParams;
		InitParams.ParentNode = SharedThis(this);
		InitParams.Property = AttributeProperty->Inner;
		InitParams.ArrayOffset = 0;
		InitParams.ArrayIndex = INDEX_NONE;
		InitParams.bAllowChildren = true;
		InitParams.bForceHiddenPropertyVisibility = bShouldShowHiddenProperties;

		NewItemNode->InitNode(InitParams);
		AddChildNode(NewItemNode);
	}

	//needs to be after all the children are created
	if ( FPropertySettings::Get().ExpandDistributions() == true)
	{
		// auto-expand distribution structs
		if (Property->IsA(UStructProperty::StaticClass()))
		{
			FName StructName = ((UStructProperty*)Property)->Struct->GetFName();
			if (StructName == NAME_RawDistributionFloat || StructName == NAME_RawDistributionVector)
			{
				const bool bExpand = true;
				const bool bRecurse = true;
			}
		}
	}
}

void FItemPropertyNode::SetDisplayNameOverride( const FString& InDisplayNameOverride )
{
	DisplayNameOverride = InDisplayNameOverride;
}

FString FItemPropertyNode::GetDisplayName() const
{
	FString FinalDisplayName;

	if( DisplayNameOverride.Len() > 0 )
	{
		FinalDisplayName = DisplayNameOverride;
	}
	else 
	{
		const UProperty* PropertyPtr = GetProperty();
		if (GetParentNode() && GetParentNode()->GetProperty() && GetParentNode()->GetProperty()->IsA<UAttributeProperty>())
		{
			FinalDisplayName = NSLOCTEXT("PropertyEditor", "AttributeValue", "Value").ToString();
		}
		else if( GetArrayIndex()==-1 && PropertyPtr != NULL  )
		{
			// This item is not a member of an array, get a traditional display name
			if ( FPropertySettings::Get().ShowFriendlyPropertyNames() )
			{
				//We are in "readable display name mode"../ Make a nice name
				FinalDisplayName = PropertyPtr->GetDisplayNameText().ToString();
				if ( FinalDisplayName.Len() == 0 )
				{
					FString PropertyDisplayName;
					bool bIsBoolProperty = Cast<const UBoolProperty>(PropertyPtr) != NULL;
					const UStructProperty* ParentStructProperty = Cast<const UStructProperty>(ParentNode->GetProperty());
					if( ParentStructProperty && ParentStructProperty->Struct->GetFName() == NAME_Rotator )
					{
						if( Property->GetFName() == "Roll" )
						{
							PropertyDisplayName = TEXT("X");
						}
						else if( Property->GetFName() == "Pitch" )
						{
							PropertyDisplayName = TEXT("Y");
						}
						else if( Property->GetFName() == "Yaw" )
						{
							PropertyDisplayName = TEXT("Z");
						}
						else
						{
							check(0);
						}
					}
					else
					{
						PropertyDisplayName = Property->GetName();
					}
					if( GetDefault<UEditorStyleSettings>()->bShowFriendlyNames )
					{
						PropertyDisplayName = FName::NameToDisplayString( PropertyDisplayName, bIsBoolProperty );
					}

					FinalDisplayName = PropertyDisplayName;
				}
			}
			else
			{
				FinalDisplayName =  PropertyPtr->GetName();
			}
		}
		else
		{
			// Get the ArraySizeEnum class from meta data.
			static const FName NAME_ArraySizeEnum("ArraySizeEnum");
			UEnum* ArraySizeEnum = NULL; 
			if (PropertyPtr && PropertyPtr->HasMetaData(NAME_ArraySizeEnum))
			{
				ArraySizeEnum	= FindObject<UEnum>(NULL, *Property->GetMetaData(NAME_ArraySizeEnum));
			}
			// This item is a member of an array, its display name is its index 
			if ( PropertyPtr == NULL || ArraySizeEnum == NULL )
			{
				FinalDisplayName = *FString::Printf(TEXT("%i"), GetArrayIndex() );
			}
			else
			{
				FinalDisplayName = *FString::Printf(TEXT("%s"), *ArraySizeEnum->GetEnumName(GetArrayIndex()));
				//fixup the display name if we have displayname metadata
				AdjustEnumPropDisplayName(ArraySizeEnum, FinalDisplayName);
			}
		}
	}
	
	return FinalDisplayName;

}

void FItemPropertyNode::SetToolTipOverride( const FString& InToolTipOverride )
{
	ToolTipOverride = InToolTipOverride;
}

FString FItemPropertyNode::GetToolTipText() const
{
	if(ToolTipOverride.Len() > 0)
	{
		return ToolTipOverride;
	}

	return PropertyEditorHelpers::GetToolTipText(GetProperty());
}
