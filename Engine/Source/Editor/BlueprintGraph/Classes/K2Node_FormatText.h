// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "K2Node_FormatText.generated.h"

UCLASS(MinimalAPI)
class UK2Node_FormatText : public UK2Node
{
	GENERATED_UCLASS_BODY()

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
	// End of UObject interface

	// Begin UEdGraphNode interface.
	virtual void AllocateDefaultPins() OVERRIDE;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const OVERRIDE;
	virtual bool ShouldShowNodeProperties() const OVERRIDE { return true; }
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) OVERRIDE;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) OVERRIDE;
	virtual FString GetTooltip() const OVERRIDE;
	virtual FString GetPinDisplayName(const UEdGraphPin* Pin) const OVERRIDE;
	// End UEdGraphNode interface.

	// Begin UK2Node interface.
	//virtual bool IsNodePure() const OVERRIDE { return true; }
	virtual bool NodeCausesStructuralBlueprintChange() const OVERRIDE { return true; }
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) OVERRIDE;
	virtual ERedirectType DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const OVERRIDE;
	// End UK2Node interface.

public:
	/** Adds a new pin to the node */
	BLUEPRINTGRAPH_API void AddArgumentPin();

	/** Returns the number of arguments currently available in the node */
	int32 GetArgumentCount() const { return PinNames.Num(); }

	/**
	 * Returns argument name based on argument index
	 *
	 * @param InIndex		The argument's index to find the name for
	 * @return				Returns the argument's name if available
	 */
	BLUEPRINTGRAPH_API FText GetArgumentName(int32 InIndex) const;

	/** Removes the argument at a given index */
	BLUEPRINTGRAPH_API void RemoveArgument(int32 InIndex);

	/**
	 * Sets an argument name
	 *
	 * @param InIndex		The argument's index to find the name for
	 * @param InName		Name to set the argument to
	 */
	BLUEPRINTGRAPH_API void SetArgumentName(int32 InIndex, FText InName);

	/** Swaps two arguments by index */
	BLUEPRINTGRAPH_API void SwapArguments(int32 InIndexA, int32 InIndexB);

	/** returns Format pin */
	BLUEPRINTGRAPH_API UEdGraphPin* GetFormatPin() const;

	/** Returns TRUE if the arguments are allowed to be edited */
	bool CanEditArguments() const { return GetFormatPin()->LinkedTo.Num() > 0; }

	/**
	 * Finds an argument pin by name, checking strings in a strict, case sensitive fashion
	 *
	 * @param InPinName		The pin name to check for
	 * @return				NULL if the pin was not found, otherwise the found pin.
	 */
	BLUEPRINTGRAPH_API UEdGraphPin* FindArgumentPin(const FText& InPinName) const;

private:
	/** Returns a unique pin name to use for a pin */
	FText GetUniquePinName();

private:
	/** When adding arguments to the node, their names are placed here and are generated as pins during construction */
	UPROPERTY()
	TArray<FText> PinNames;

	/** The "Format" input pin, always available on the node */
	UEdGraphPin* CachedFormatPin;

	/** Tooltip text for this node. */
	FString NodeTooltip;
};

