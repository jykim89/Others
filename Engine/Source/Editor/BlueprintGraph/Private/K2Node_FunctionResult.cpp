// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"

#include "KismetCompiler.h"
#include "VariableSetHandler.h"

struct FFillDefaultPinValueHelper
{
private:
	static void FillInner(const UEdGraphSchema_K2* K2Schema, UEdGraphPin* Pin)
	{
		if(K2Schema && Pin)
		{
			const bool bValuePin = (Pin->PinType.PinCategory != K2Schema->PC_Exec);
			const bool bNotConnected = (Pin->Direction == EEdGraphPinDirection::EGPD_Input) && (0 == Pin->LinkedTo.Num());
			const bool bNeedToResetDefaultValue = !(K2Schema->IsPinDefaultValid(Pin, Pin->DefaultValue, Pin->DefaultObject, Pin->DefaultTextValue).IsEmpty());
			if (bValuePin && bNotConnected && bNeedToResetDefaultValue)
			{
				K2Schema->SetPinDefaultValueBasedOnType(Pin);
			}
		}
	}

public:
	static void Fill(UEdGraphPin* Pin)
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		if(K2Schema)
		{
			FillInner(K2Schema, Pin);
		}
	}

	static  void FillAll(UK2Node_FunctionResult* Node)
	{
		if(Node)
		{
			const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
			if(K2Schema)
			{
				for (int32 PinIdx = 0; PinIdx < Node->Pins.Num(); PinIdx++)
				{
					FillInner(K2Schema, Node->Pins[PinIdx]);
				}
			}
		}
	}
};

//////////////////////////////////////////////////////////////////////////
// FKCHandler_FunctionResult

class FKCHandler_FunctionResult : public FKCHandler_VariableSet
{
public:
	FKCHandler_FunctionResult(FKismetCompilerContext& InCompilerContext)
		: FKCHandler_VariableSet(InCompilerContext)
	{
	}

	virtual void RegisterNet(FKismetFunctionContext& Context, UEdGraphPin* Net) OVERRIDE
	{
		FBPTerminal* Term = new (Context.Results) FBPTerminal();
		Term->CopyFromPin(Net, Net->PinName);
		Context.NetMap.Add(Net, Term);
	}
};

UK2Node_FunctionResult::UK2Node_FunctionResult(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

FText UK2Node_FunctionResult::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("K2Node", "ReturnNode", "ReturnNode");
}

void UK2Node_FunctionResult::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	CreatePin(EGPD_Input, K2Schema->PC_Exec, TEXT(""), NULL, false, false, K2Schema->PN_Execute);

	UFunction* Function = FindField<UFunction>(SignatureClass, SignatureName);
	if (Function != NULL)
	{
		CreatePinsForFunctionEntryExit(Function, /*bIsFunctionEntry=*/ false);
	}

	Super::AllocateDefaultPins();

	FFillDefaultPinValueHelper::FillAll(this);
}

UEdGraphPin* UK2Node_FunctionResult::CreatePinFromUserDefinition(const TSharedPtr<FUserPinInfo> NewPinInfo)
{
	UEdGraphPin* Pin = CreatePin(
		EGPD_Input, 
		NewPinInfo->PinType.PinCategory, 
		NewPinInfo->PinType.PinSubCategory, 
		NewPinInfo->PinType.PinSubCategoryObject.Get(), 
		NewPinInfo->PinType.bIsArray, 
		NewPinInfo->PinType.bIsReference, 
		NewPinInfo->PinName);
	FFillDefaultPinValueHelper::Fill(Pin);
	return Pin;
}

FNodeHandlingFunctor* UK2Node_FunctionResult::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_FunctionResult(CompilerContext);
}
