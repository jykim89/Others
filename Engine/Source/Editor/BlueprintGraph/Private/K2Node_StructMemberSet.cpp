// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"

#include "StructMemberNodeHandlers.h"

//////////////////////////////////////////////////////////////////////////
// UK2Node_StructMemberSet

#define LOCTEXT_NAMESPACE "K2Node"

UK2Node_StructMemberSet::UK2Node_StructMemberSet(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

void UK2Node_StructMemberSet::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if ((PropertyName == TEXT("bShowPin")))
	{
		GetSchema()->ReconstructNode(*this);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UK2Node_StructMemberSet::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	// Add the execution sequencing pin
	CreatePin(EGPD_Input, Schema->PC_Exec, TEXT(""), NULL, false, false, Schema->PN_Execute);
	CreatePin(EGPD_Output, Schema->PC_Exec, TEXT(""), NULL, false, false, Schema->PN_Then);

	// Display any currently visible optional pins
	{
		FStructOperationOptionalPinManager OptionalPinManager;
		OptionalPinManager.RebuildPropertyList(ShowPinForProperties, StructType);
		OptionalPinManager.CreateVisiblePins(ShowPinForProperties, StructType, EGPD_Input, this);
	}
}

FString UK2Node_StructMemberSet::GetTooltip() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("VariableName"), FText::FromName(VariableReference.GetMemberName()));
	return FText::Format(LOCTEXT("K2Node_StructMemberSet_Tooltip", "Set member variables of {VariableName}"), Args).ToString();
}

FText UK2Node_StructMemberSet::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("VariableName"), FText::FromName(VariableReference.GetMemberName()));
	return FText::Format(LOCTEXT("SetMembersInVariable", "Set members in {VariableName}"), Args);
}

FString UK2Node_StructMemberSet::GetNodeNativeTitle(ENodeTitleType::Type TitleType) const
{
	// Do not setup this function for localization, intentionally left unlocalized!

	return FString::Printf(TEXT("Set members in %s"), *VariableReference.GetMemberName().ToString());
}

UK2Node::ERedirectType UK2Node_StructMemberSet::DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex)  const
{
	return UK2Node::DoPinsMatchForReconstruction(NewPin, NewPinIndex, OldPin, OldPinIndex);
}

FNodeHandlingFunctor* UK2Node_StructMemberSet::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_StructMemberVariableSet(CompilerContext);
}

#undef LOCTEXT_NAMESPACE