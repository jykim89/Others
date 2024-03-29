// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"
#include "StructMemberNodeHandlers.h"

//////////////////////////////////////////////////////////////////////////
// UK2Node_StructMemberGet

#define LOCTEXT_NAMESPACE "K2Node"

UK2Node_StructMemberGet::UK2Node_StructMemberGet(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

void UK2Node_StructMemberGet::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if ((PropertyName == TEXT("bShowPin")))
	{
		GetSchema()->ReconstructNode(*this);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UK2Node_StructMemberGet::AllocateDefaultPins()
{
	//@TODO: Create a context pin

	// Display any currently visible optional pins
	{
		FStructOperationOptionalPinManager OptionalPinManager;
		OptionalPinManager.RebuildPropertyList(ShowPinForProperties, StructType);
		OptionalPinManager.CreateVisiblePins(ShowPinForProperties, StructType, EGPD_Output, this);
	}
}

void UK2Node_StructMemberGet::AllocatePinsForSingleMemberGet(FName MemberName)
{
	//@TODO: Create a context pin

	// Updater for subclasses that allow hiding pins
	struct FSingleVariablePinManager : public FOptionalPinManager
	{
		FName MatchName;

		FSingleVariablePinManager(FName InMatchName)
			: MatchName(InMatchName)
		{
		}

		// FOptionalPinsUpdater interface
		virtual void GetRecordDefaults(UProperty* TestProperty, FOptionalPinFromProperty& Record) const OVERRIDE
		{
			Record.bCanToggleVisibility = false;
			Record.bShowPin = TestProperty->GetFName() == MatchName;
		}
		// End of FOptionalPinsUpdater interface
	};


	// Display any currently visible optional pins
	{
		FSingleVariablePinManager PinManager(MemberName);
		PinManager.RebuildPropertyList(ShowPinForProperties, StructType);
		PinManager.CreateVisiblePins(ShowPinForProperties, StructType, EGPD_Output, this);
	}
}

FString UK2Node_StructMemberGet::GetTooltip() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("VariableName"), FText::FromString(GetVarNameString()));
	return FText::Format(LOCTEXT("K2Node_StructMemberGet_Tooltip", "Get member variables of {VariableName}"), Args).ToString();
}

FText UK2Node_StructMemberGet::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("VariableName"), FText::FromString(GetVarNameString()));
	return FText::Format(LOCTEXT("GetMembersInVariable", "Get members in {VariableName}"), Args);
}

FString UK2Node_StructMemberGet::GetNodeNativeTitle(ENodeTitleType::Type TitleType) const
{
	// Do not setup this function for localization, intentionally left unlocalized!

	return FString::Printf(TEXT("Get members in %s"), *GetVarNameString());
}

FNodeHandlingFunctor* UK2Node_StructMemberGet::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_StructMemberVariableGet(CompilerContext);
}

#undef LOCTEXT_NAMESPACE