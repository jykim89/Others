// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "BlueprintGraphPrivatePCH.h"
#include "KismetCompiler.h"
#include "../../../Runtime/Engine/Classes/Kismet/KismetSystemLibrary.h"

UK2Node_GetNumEnumEntries::UK2Node_GetNumEnumEntries(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

void UK2Node_GetNumEnumEntries::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	// Create the return value pin
	CreatePin(EGPD_Output, Schema->PC_Int, TEXT(""), NULL, false, false, Schema->PN_ReturnValue);

	Super::AllocateDefaultPins();
}

FString UK2Node_GetNumEnumEntries::GetTooltip() const
{
	const FString EnumName = (Enum != NULL) ? Enum->GetName() : TEXT("(bad enum)");

	return FString::Printf(*NSLOCTEXT("K2Node", "GetNumEnumEntries_Tooltip", "Returns %s_MAX value").ToString(), *EnumName);
}

FText UK2Node_GetNumEnumEntries::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	const FText EnumName = (Enum != NULL) ? FText::FromString(Enum->GetName()) : NSLOCTEXT("K2Node", "BadEnum", "(bad enum)");

	FFormatNamedArguments Args;
	Args.Add(TEXT("EnumName"), EnumName);
	return FText::Format(NSLOCTEXT("K2Node", "GetNumEnumEntries_Title", "Get number of entries in {EnumName}"), Args);
}

FString UK2Node_GetNumEnumEntries::GetNodeNativeTitle(ENodeTitleType::Type TitleType) const
{
	// Do not setup this function for localization, intentionally left unlocalized!
	const FString EnumName = (Enum != NULL) ? Enum->GetName() : TEXT("(bad enum)");

	return FString::Printf(TEXT("Get number of entries in %s"), *EnumName);
}

void UK2Node_GetNumEnumEntries::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	if (CompilerContext.bIsFullCompile)
	{
		if(NULL == Enum)
		{
			CompilerContext.MessageLog.Error(*FString::Printf(*NSLOCTEXT("K2Node", "GetNumEnumEntries_Error", "@@ must have a valid enum defined").ToString()), this);
			return;
		}

		const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
		check(NULL != Schema);

		//MAKE LITERAL
		const FName FunctionName = GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, MakeLiteralInt);
		UK2Node_CallFunction* MakeLiteralInt = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph); 
		MakeLiteralInt->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(FunctionName));
		MakeLiteralInt->AllocateDefaultPins();

		//OPUTPUT PIN
		UEdGraphPin* OrgReturnPin = FindPinChecked(Schema->PN_ReturnValue);
		UEdGraphPin* NewReturnPin = MakeLiteralInt->GetReturnValuePin();
		check(NULL != NewReturnPin);
		CompilerContext.MovePinLinksToIntermediate(*OrgReturnPin, *NewReturnPin);

		//INPUT PIN
		UEdGraphPin* InputPin = MakeLiteralInt->FindPinChecked(TEXT("Value"));
		check(EGPD_Input == InputPin->Direction);
		const FString DefaultValue = FString::FromInt(Enum->NumEnums() - 1);
		InputPin->DefaultValue = DefaultValue;

		BreakAllNodeLinks();
	}
}