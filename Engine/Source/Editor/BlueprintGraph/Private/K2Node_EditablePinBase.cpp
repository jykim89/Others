// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"

UK2Node_EditablePinBase::UK2Node_EditablePinBase(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

void UK2Node_EditablePinBase::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	// Add in pins based on the user defined pins in this node
	for(int32 i = 0; i < UserDefinedPins.Num(); i++)
	{
		CreatePinFromUserDefinition( UserDefinedPins[i] );
	}
}

UEdGraphPin* UK2Node_EditablePinBase::CreateUserDefinedPin(const FString& InPinName, const FEdGraphPinType& InPinType)
{
	// Sanitize the name, if needed
	const FString NewPinName = CreateUniquePinName(InPinName);

	// First, add this pin to the user-defined pins
	TSharedPtr<FUserPinInfo> NewPinInfo = MakeShareable( new FUserPinInfo() );
	NewPinInfo->PinName = NewPinName;
	NewPinInfo->PinType = InPinType;
	UserDefinedPins.Add(NewPinInfo);

	// Then, add the pin to the actual Pins array
	UEdGraphPin* NewPin = CreatePinFromUserDefinition(NewPinInfo);
	
	check(NewPin);

	return NewPin;
}

void UK2Node_EditablePinBase::RemoveUserDefinedPin(TSharedPtr<FUserPinInfo> PinToRemove)
{
	// Try to find the pin with the same name and params as the specified description, if any
	const FString PinName = PinToRemove->PinName;
	for(int32 i = 0; i < Pins.Num(); i++)
	{
		UEdGraphPin* Pin = Pins[i];
		if( Pin->PinName == PinName )
		{
			Pin->BreakAllPinLinks();
			Pins.Remove(Pin);
		}
	}

	// Remove the description from the user-defined pins array
	UserDefinedPins.Remove(PinToRemove);
}

void UK2Node_EditablePinBase::ExportCustomProperties(FOutputDevice& Out, uint32 Indent)
{
	for (int32 PinIndex = 0; PinIndex < UserDefinedPins.Num(); ++PinIndex)
	{
		const FUserPinInfo& PinInfo = *UserDefinedPins[PinIndex].Get();

		Out.Logf( TEXT("%sCustomProperties UserDefinedPin "), FCString::Spc(Indent));
		Out.Logf( TEXT("Name=%s "), *PinInfo.PinName);
		Out.Logf( TEXT("IsArray=%s "), (PinInfo.PinType.bIsArray ? TEXT("1") : TEXT("0")));
		Out.Logf( TEXT("IsReference=%s "), (PinInfo.PinType.bIsReference ? TEXT("1") : TEXT("0")));
		
		if (PinInfo.PinType.PinCategory.Len() > 0)
		{
			Out.Logf( TEXT("Category=%s "), *PinInfo.PinType.PinCategory);
		}

		if (PinInfo.PinType.PinSubCategory.Len() > 0)
		{
			Out.Logf( TEXT("SubCategory=%s "), *PinInfo.PinType.PinSubCategory);
		}

		if (PinInfo.PinType.PinSubCategoryObject.IsValid())
		{
			Out.Logf( TEXT("SubCategoryObject=%s "), *PinInfo.PinType.PinSubCategoryObject.Get()->GetPathName());
		}

		if (PinInfo.PinDefaultValue.Len() > 0)
		{
			Out.Logf( TEXT("DefaultValue=%s "), *PinInfo.PinDefaultValue);
		}

		Out.Logf( TEXT("\r\n"));
	}
}

void UK2Node_EditablePinBase::ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn)
{
	if (FParse::Command(&SourceText, TEXT("UserDefinedPin")))
	{
		TSharedPtr<FUserPinInfo> PinInfo = MakeShareable( new FUserPinInfo() );

		if (!FParse::Value(SourceText, TEXT("Name="), PinInfo->PinName))
		{
			Warn->Logf( *NSLOCTEXT( "Core", "SyntaxError", "Syntax Error" ).ToString() );
			return;
		}

		int32 BoolAsInt = 0;
		if (FParse::Value(SourceText, TEXT("IsArray="), BoolAsInt))
		{
			PinInfo->PinType.bIsArray = (BoolAsInt != 0);
		}

		if (FParse::Value(SourceText, TEXT("IsReference="), BoolAsInt))
		{
			PinInfo->PinType.bIsReference = (BoolAsInt != 0);
		}

		FParse::Value(SourceText, TEXT("Category="), PinInfo->PinType.PinCategory);
		FParse::Value(SourceText, TEXT("SubCategory="), PinInfo->PinType.PinSubCategory);

		FString ObjectPathName;
		if (FParse::Value(SourceText, TEXT("SubCategoryObject="), ObjectPathName))
		{
			PinInfo->PinType.PinSubCategoryObject = FindObject<UObject>(ANY_PACKAGE, *ObjectPathName);
			if (!PinInfo->PinType.PinSubCategoryObject.IsValid())
			{
				Warn->Logf( *NSLOCTEXT( "Core", "UnableToFindObject", "Unable to find object" ).ToString() );
				return;
			}
		}

		FParse::Value(SourceText, TEXT("DefaultValue="), PinInfo->PinDefaultValue);

		UserDefinedPins.Add(PinInfo);
	}
}

void UK2Node_EditablePinBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	TArray<FUserPinInfo> SerializedItems;
	if (Ar.IsLoading())
	{
		Ar << SerializedItems;

		UserDefinedPins.Empty(SerializedItems.Num());
		for (int32 Index = 0; Index < SerializedItems.Num(); ++Index)
		{
			UserDefinedPins.Add(MakeShareable( new FUserPinInfo(SerializedItems[Index]) ));
		}
	}
	else
	{
		SerializedItems.Empty(UserDefinedPins.Num());
		for (int32 Index = 0; Index < UserDefinedPins.Num(); ++Index)
		{
			SerializedItems.Add(*(UserDefinedPins[Index].Get()));
		}
		Ar << SerializedItems;
	}
}

void UK2Node_EditablePinBase::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UK2Node_EditablePinBase* This = CastChecked<UK2Node_EditablePinBase>(InThis);
	for (int32 Index = 0; Index < This->UserDefinedPins.Num(); ++Index)
	{
		FUserPinInfo PinInfo = *This->UserDefinedPins[Index].Get();
		UObject* PinSubCategoryObject = PinInfo.PinType.PinSubCategoryObject.Get();
		Collector.AddReferencedObject(PinSubCategoryObject, This);
	}
	Super::AddReferencedObjects( This, Collector );
}

bool UK2Node_EditablePinBase::ModifyUserDefinedPinDefaultValue(TSharedPtr<FUserPinInfo> PinInfo, const FString& InDefaultValue)
{
	FString NewDefaultValue = InDefaultValue;

	// Find and modify the current pin
	if (UEdGraphPin* OldPin = FindPin(PinInfo->PinName))
	{
		FString SavedDefaultValue = OldPin->DefaultValue;
		OldPin->DefaultValue = OldPin->AutogeneratedDefaultValue = NewDefaultValue;

		// Validate the new default value
		const UEdGraphSchema* Schema = GetSchema();
		FString ErrorString = Schema->IsCurrentPinDefaultValid(OldPin);

		if (!ErrorString.IsEmpty())
		{
			NewDefaultValue = SavedDefaultValue;
			OldPin->DefaultValue = OldPin->AutogeneratedDefaultValue = SavedDefaultValue;

			return false;
		}
	}

	PinInfo->PinDefaultValue = NewDefaultValue;
	return true;
}

bool UK2Node_EditablePinBase::CreateUserDefinedPinsForFunctionEntryExit(const UFunction* Function, bool bForFunctionEntry)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	// Create the inputs and outputs
	bool bAllPinsGood = true;
	for ( TFieldIterator<UProperty> PropIt(Function); PropIt && ( PropIt->PropertyFlags & CPF_Parm ); ++PropIt )
	{
		UProperty* Param = *PropIt;

		const bool bIsFunctionInput = !Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReferenceParm);

		if ( bIsFunctionInput == bForFunctionEntry )
		{
			const EEdGraphPinDirection Direction = bForFunctionEntry ? EGPD_Output : EGPD_Input;

			FEdGraphPinType PinType;
			K2Schema->ConvertPropertyToPinType(Param, /*out*/ PinType);

			const bool bPinGood = CreateUserDefinedPin(Param->GetName(), PinType) != NULL;

			bAllPinsGood = bAllPinsGood && bPinGood;
		}
	}

	return bAllPinsGood;
}