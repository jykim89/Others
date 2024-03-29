// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "K2Node_MatineeController.generated.h"

UCLASS(MinimalAPI)
class UK2Node_MatineeController : public UK2Node
{
	GENERATED_UCLASS_BODY()

	/** The matinee actor in the level that this node controls */
	UPROPERTY(EditAnywhere, Category=K2Node_MatineeController)
	class AMatineeActor* MatineeActor;

	// Begin UEdGraphNode interface
	virtual void AllocateDefaultPins() OVERRIDE;
	virtual FLinearColor GetNodeTitleColor() const OVERRIDE;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const OVERRIDE;
	virtual bool CanDuplicateNode() const OVERRIDE { return false; }
	virtual bool NodeCausesStructuralBlueprintChange() const OVERRIDE { return true; }
	// End UEdGraphNode interface

	// Begin UK2Node interface
	virtual bool IsNodeSafeToIgnore() const OVERRIDE { return true; }
	virtual AActor* GetReferencedLevelActor() const OVERRIDE;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) OVERRIDE;
	// End UK2Node interface

	// Begin UObject interface
	virtual void BeginDestroy() OVERRIDE;
	// End UObject interface

	/** Gets the "finished playing matinee sequence" pin */
	UEdGraphPin* GetFinishedPin() const;

private:
	void OnEventKeyframeAdded(const class AMatineeActor* InMatineeActor, const FName& InPinName, int32 InIndex);
	void OnEventKeyframeRenamed(const class AMatineeActor* InMatineeActor, const FName& InOldPinName, const FName& InNewPinName);
	void OnEventKeyframeRemoved(const class AMatineeActor* InMatineeActor, const TArray<FName>& InPinNames);
};

