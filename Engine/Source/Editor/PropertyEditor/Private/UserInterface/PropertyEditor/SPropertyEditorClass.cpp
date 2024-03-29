// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorPrivatePCH.h"

#include "PropertyNode.h"
#include "DragAndDrop/ClassDragDropOp.h"
#include "PropertyEditorHelpers.h"
#include "PropertyEditor.h"
#include "SPropertyEditorClass.h"
#include "SPropertyEditorCombo.h"
#include "Editor/ClassViewer/Public/ClassViewerModule.h"
#include "Editor/ClassViewer/Public/ClassViewerFilter.h"

#include "PropertyHandle.h"

#define LOCTEXT_NAMESPACE "PropertyEditor"

class FPropertyEditorClassFilter : public IClassViewerFilter
{
public:
	/** The meta class for the property that classes must be a child-of. */
	const UClass* ClassPropertyMetaClass;

	/** The interface that must be implemented. */
	const UClass* InterfaceThatMustBeImplemented;

	/** Whether or not abstract classes are allowed. */
	bool bAllowAbstract;

	bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs )
	{
		bool bMatchesFlags = !InClass->HasAnyClassFlags(CLASS_Hidden|CLASS_HideDropDown|CLASS_Deprecated) &&
			(bAllowAbstract || !InClass->HasAnyClassFlags(CLASS_Abstract));

		if(bMatchesFlags && InClass->IsChildOf(ClassPropertyMetaClass)
			&& (!InterfaceThatMustBeImplemented || InClass->ImplementsInterface(InterfaceThatMustBeImplemented)))
		{
			return true;
		}

		return false;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) OVERRIDE
	{
		bool bMatchesFlags = !InClass->HasAnyClassFlags(CLASS_Hidden|CLASS_HideDropDown|CLASS_Deprecated) &&
			(bAllowAbstract || !InClass->HasAnyClassFlags(CLASS_Abstract));

		if(bMatchesFlags && InClass->IsChildOf(ClassPropertyMetaClass)
			&& (!InterfaceThatMustBeImplemented || InClass->ImplementsInterface(InterfaceThatMustBeImplemented)))
		{
			return true;
		}

		return false;
	}
};

void SPropertyEditorClass::GetDesiredWidth(float& OutMinDesiredWidth, float& OutMaxDesiredWidth)
{
	OutMinDesiredWidth = 125.0f;
	OutMaxDesiredWidth = 400.0f;
}

bool SPropertyEditorClass::Supports(const TSharedRef< class FPropertyEditor >& InPropertyEditor)
{
	if(InPropertyEditor->IsEditConst())
	{
		return false;
	}

	const TSharedRef< FPropertyNode > PropertyNode = InPropertyEditor->GetPropertyNode();
	const UProperty* Property = InPropertyEditor->GetProperty();
	int32 ArrayIndex = PropertyNode->GetArrayIndex();

	if ((Property->IsA(UClassProperty::StaticClass()) || Property->IsA(UAssetClassProperty::StaticClass())) 
		&& ((ArrayIndex == -1 && Property->ArrayDim == 1) || (ArrayIndex > -1 && Property->ArrayDim > 0)))
	{
		return true;
	}

	return false;
}

void SPropertyEditorClass::Construct(const FArguments& InArgs, const TSharedPtr< class FPropertyEditor >& InPropertyEditor)
{
	PropertyEditor = InPropertyEditor;
	
	if (PropertyEditor.IsValid())
	{
		const TSharedRef<FPropertyNode> PropertyNode = PropertyEditor->GetPropertyNode();
		UProperty* const Property = PropertyNode->GetProperty();
		if (UClassProperty* const ClassProp = Cast<UClassProperty>(Property))
		{
			MetaClass = ClassProp->MetaClass;
		}
		else if (UAssetClassProperty* const AssetClassProperty = Cast<UAssetClassProperty>(Property))
		{
			MetaClass = AssetClassProperty->MetaClass;
		}
		else
		{
			check(false);
		}
		
		bAllowAbstract = Property->GetOwnerProperty()->HasMetaData(TEXT("AllowAbstract"));
		bIsBlueprintBaseOnly = Property->GetOwnerProperty()->HasMetaData(TEXT("BlueprintBaseOnly"));
		RequiredInterface = Property->GetOwnerProperty()->GetClassMetaData(TEXT("MustImplement"));
		bAllowNone = !(Property->PropertyFlags & CPF_NoClear);
	}
	else
	{
		check(InArgs._MetaClass);
		check(InArgs._SelectedClass.IsSet());
		check(InArgs._OnSetClass.IsBound());

		MetaClass = InArgs._MetaClass;
		RequiredInterface = InArgs._RequiredInterface;
		bAllowAbstract = InArgs._AllowAbstract;
		bIsBlueprintBaseOnly = InArgs._IsBlueprintBaseOnly;
		bAllowNone = InArgs._AllowNone;

		SelectedClass = InArgs._SelectedClass;
		OnSetClass = InArgs._OnSetClass;
	}
	
	SAssignNew(ComboButton, SComboButton)
		.OnGetMenuContent(this, &SPropertyEditorClass::GenerateClassPicker)
		.ContentPadding(FMargin(2.0f, 2.0f))
		.ToolTipText(this, &SPropertyEditorClass::GetDisplayValueAsString)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(this, &SPropertyEditorClass::GetDisplayValueAsString)
			.Font(InArgs._Font)
		];

	ChildSlot
	[
		ComboButton.ToSharedRef()
	];
}

/** Util to give better names for BP generated classes */
static FString GetClassDisplayName(const UObject* Object)
{
	const UClass* Class = Cast<UClass>(Object);
	if (Class != NULL)
	{
		UBlueprint* BP = UBlueprint::GetBlueprintFromClass(Class);
		if(BP != NULL)
		{
			return BP->GetName();
		}
	}
	return (Object) ? Object->GetName() : "None";
}

FString SPropertyEditorClass::GetDisplayValueAsString() const
{
	if(PropertyEditor.IsValid())
	{
		UObject* ObjectValue = NULL;
		FPropertyAccess::Result Result = PropertyEditor->GetPropertyHandle()->GetValue(ObjectValue);

		if(Result == FPropertyAccess::Success && ObjectValue != NULL)
		{
			return GetClassDisplayName(ObjectValue);
		}

		return FPaths::GetBaseFilename(PropertyEditor->GetValueAsString());
	}

	return GetClassDisplayName(SelectedClass.Get());
}

TSharedRef<SWidget> SPropertyEditorClass::GenerateClassPicker()
{
	FClassViewerInitializationOptions Options;
	Options.bShowUnloadedBlueprints = true;
	Options.bShowNoneOption = bAllowNone; 

	if(PropertyEditor.IsValid())
	{
		Options.PropertyHandle = PropertyEditor->GetPropertyHandle();
	}

	TSharedPtr<FPropertyEditorClassFilter> ClassFilter = MakeShareable(new FPropertyEditorClassFilter);
	Options.ClassFilter = ClassFilter;
	ClassFilter->ClassPropertyMetaClass = MetaClass;
	ClassFilter->InterfaceThatMustBeImplemented = RequiredInterface;
	ClassFilter->bAllowAbstract = bAllowAbstract;
	Options.bIsBlueprintBaseOnly = bIsBlueprintBaseOnly;

	FOnClassPicked OnPicked(FOnClassPicked::CreateRaw(this, &SPropertyEditorClass::OnClassPicked));

	return SNew(SBox)
		.WidthOverride(280)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(500)
			[
				FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, OnPicked)
			]			
		];
}

void SPropertyEditorClass::OnClassPicked(UClass* InClass)
{
	if(!InClass)
	{
		SendToObjects(TEXT("None"));
	}
	else
	{
		SendToObjects(InClass->GetPathName());
	}

	ComboButton->SetIsOpen(false);
}

void SPropertyEditorClass::SendToObjects(const FString& NewValue)
{
	if(PropertyEditor.IsValid())
	{
		const TSharedRef<IPropertyHandle> PropertyHandle = PropertyEditor->GetPropertyHandle();
		PropertyHandle->SetValueFromFormattedString(NewValue);
	}
	else
	{
		UClass* NewClass = FindObject<UClass>(ANY_PACKAGE, *NewValue);
		if(!NewClass)
		{
			NewClass = LoadObject<UClass>(nullptr, *NewValue);
		}
		OnSetClass.Execute(NewClass);
	}
}

FReply SPropertyEditorClass::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FClassDragDropOp> ClassOperation = DragDropEvent.GetOperationAs<FClassDragDropOp>();
	if (ClassOperation.IsValid())
	{
		// We can only drop one item into the combo box, so drop the first one.
		FString AssetName = ClassOperation->ClassesToDrop[0]->GetName();

		// Set the property, it will be verified as valid.
		SendToObjects(AssetName);

		return FReply::Handled();
	}
	
	TSharedPtr<FUnloadedClassDragDropOp> UnloadedClassOp = DragDropEvent.GetOperationAs<FUnloadedClassDragDropOp>();
	if (UnloadedClassOp.IsValid())
	{
		// Check if the asset is loaded, used to see if the context menu should be available
		bool bAllAssetWereLoaded = true;

		TArray<FClassPackageData>& AssetArray = *(UnloadedClassOp->AssetsToDrop.Get());

		// We can only drop one item into the combo box, so drop the first one.
		FString& AssetName = AssetArray[0].AssetName;

		// Check to see if the asset can be found, otherwise load it.
		UObject* Object = FindObject<UObject>(NULL, *AssetName);
		if(Object == NULL)
		{
			// Check to see if the dropped asset was a blueprint
			const FString& PackageName = AssetArray[0].GeneratedPackageName;
			Object = FindObject<UObject>(NULL, *FString::Printf(TEXT("%s.%s"), *PackageName, *AssetName));

			if(Object == NULL)
			{
				// Load the package.
				GWarn->BeginSlowTask(LOCTEXT("OnDrop_LoadPackage", "Fully Loading Package For Drop"), true, false);
				UPackage* Package = LoadPackage(NULL, *PackageName, LOAD_NoRedirects );
				if(Package)
				{
					Package->FullyLoad();
				}
				GWarn->EndSlowTask();

				Object = FindObject<UObject>(Package, *AssetName);
			}

			if(Object->IsA(UBlueprint::StaticClass()))
			{
				// Get the default object from the generated class.
				Object = Cast<UBlueprint>(Object)->GeneratedClass->GetDefaultObject();
			}
		}

		// Set the property, it will be verified as valid.
		SendToObjects(AssetName);

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
