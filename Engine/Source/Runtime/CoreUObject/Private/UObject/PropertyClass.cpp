// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "PropertyHelper.h"

/*-----------------------------------------------------------------------------
	UClassProperty.
-----------------------------------------------------------------------------*/

void UClassProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << MetaClass;

	if( !(MetaClass||HasAnyFlags(RF_ClassDefaultObject)) )
	{
		// If we failed to load the MetaClass and we're not a CDO, that means we relied on a class that has been removed or doesn't exist.
		// The most likely cause for this is either an incomplete recompile, or if content was migrated between games that had native class dependencies
		// that do not exist in this game.  We allow blueprint classes to continue, because compile on load will error out, and stub the class that was using it
		UClass* TestClass = Cast<UClass>(GetOwnerStruct());
		if( TestClass && TestClass->HasAllClassFlags(CLASS_Native) && !TestClass->HasAllClassFlags(CLASS_NewerVersionExists) && (TestClass->GetOutermost() != GetTransientPackage()) )
		{
			checkf(false, TEXT("Class property tried to serialize a missing class.  Did you remove a native class and not fully recompile?"));
		}
	}
}
void UClassProperty::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UClassProperty* This = CastChecked<UClassProperty>(InThis);
	Collector.AddReferencedObject( This->MetaClass, This );
	Super::AddReferencedObjects( This, Collector );
}
const TCHAR* UClassProperty::ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const
{
	const TCHAR* Result = UObjectProperty::ImportText_Internal( Buffer, Data, PortFlags, Parent, ErrorText );
	if( Result )
	{
		// Validate metaclass.
		UClass* C = (UClass*)GetObjectPropertyValue(Data);
		if ((C != NULL) && ((Cast<UClass>(C) == NULL) || !C->IsChildOf(MetaClass)))
		{
			// the object we imported doesn't implement our interface class
			ErrorText->Logf(TEXT("Invalid object '%s' specified for property '%s'"), *C->GetFullName(), *GetName());
			SetObjectPropertyValue(Data, NULL);
			Result = NULL;
		}
	}
	return Result;
}

FString UClassProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, uint32 CPPExportFlags/*=0*/ ) const
{
//	return FString::Printf( TEXT("class %s%s*"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName() );
	return FString::Printf(TEXT("TSubclassOf<class %s%s> "),MetaClass->GetPrefixCPP(),*MetaClass->GetName());
}
FString UClassProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	ExtendedTypeText = TEXT("UClass");
	return TEXT("OBJECT");
}

bool UClassProperty::SameType(const UProperty* Other) const
{
	return Super::SameType(Other) && (MetaClass == ((UClassProperty*)Other)->MetaClass);
}

void UClassProperty::CheckValidObject(void* Value) const
{
#if WITH_EDITOR
	// Ugly hack to replace Blueprint references with Class references.

	struct FReplaceBlueprintWithClassHelper
	{
		bool bShouldReplace;
		UClass* BlueprintClass;
		UClassProperty* BPGeneratedClassProp;

		FReplaceBlueprintWithClassHelper() : bShouldReplace(false), BlueprintClass(NULL), BPGeneratedClassProp(NULL)
		{
			GConfig->GetBool(TEXT("EditoronlyBP"), TEXT("bReplaceBlueprintWithClass"), bShouldReplace, GEditorIni);
			if (bShouldReplace)
			{
				BlueprintClass = FindObject<UClass>(NULL, TEXT("/Script/Engine.Blueprint"));
				ensure(BlueprintClass);
				BPGeneratedClassProp = BlueprintClass ? FindField<UClassProperty>(BlueprintClass, TEXT("GeneratedClass")) : NULL;
				ensure(BPGeneratedClassProp);
			}
		}

		bool CanReplace() const
		{
			return bShouldReplace && BlueprintClass && BPGeneratedClassProp;
		}
	};
	static FReplaceBlueprintWithClassHelper Helper;

	const UObject* Object = GetObjectPropertyValue(Value);
	Super::CheckValidObject(Value);
	const UObject* CurrentObject = GetObjectPropertyValue(Value);

	if (Helper.CanReplace()
		&& !CurrentObject
		&& Object 
		&& Object->IsA(Helper.BlueprintClass)
		&& (UObject::StaticClass() == MetaClass))
	{
		UObject* RecoveredObject = Helper.BPGeneratedClassProp->GetPropertyValue_InContainer(Object);
		SetObjectPropertyValue(Value, RecoveredObject);
		UE_LOG(LogProperty, Log,
			TEXT("Blueprint '%s' is replaced with class '%s' in property '%s'"),
			*Object->GetFullName(),
			*RecoveredObject->GetFullName(),
			*GetFullName());
	}
#else	// WITH_EDITOR
	Super::CheckValidObject(Value);
#endif	// WITH_EDITOR
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UClassProperty, UObjectProperty,
	{
		Class->EmitObjectReference( STRUCT_OFFSET( UClassProperty, MetaClass ) );
	}
);

