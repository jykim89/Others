// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "K2Node_LocalVariable.generated.h"

UCLASS(MinimalAPI, deprecated)
class UDEPRECATED_K2Node_LocalVariable : public UK2Node_TemporaryVariable
{
	GENERATED_UCLASS_BODY()

	/** If this is not an override, allow user to specify a name for the function created by this entry point */
	UPROPERTY()
	FName CustomVariableName;

	/** The local variable's assigned tooltip */
	UPROPERTY()
	FText VariableTooltip;

	// Begin UEdGraphNode interface.
 	virtual FString GetTooltip() const OVERRIDE;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const OVERRIDE;
	virtual FString GetNodeNativeTitle(ENodeTitleType::Type TitleType) const OVERRIDE;
	virtual void OnRenameNode(const FString& NewName) OVERRIDE;
	virtual TSharedPtr<class INameValidatorInterface> MakeNameValidator() const OVERRIDE;
	virtual void PostPlacedNewNode() OVERRIDE;
	virtual void PostPasteNode() OVERRIDE;
	virtual bool CanPasteHere(const UEdGraph* TargetGraph, const UEdGraphSchema* Schema) const OVERRIDE;
	virtual void ReconstructNode() OVERRIDE;
	// End UEdGraphNode interface.

	// Begin UK2Node interface.
	virtual bool NodeCausesStructuralBlueprintChange() const OVERRIDE { return true; }
	virtual bool ShouldShowNodeProperties() const OVERRIDE { return true; }
	// End UK2Node interface.

	/**
	 * Assigns the local variable a type
	 *
	 * @param InVariableType		The type to assign this local variable to
	 */
	BLUEPRINTGRAPH_API void ChangeVariableType(const FEdGraphPinType& InVariableType);
};