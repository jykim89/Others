// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "BehaviorTreeDecoratorGraphNode.generated.h"

UCLASS()
class UBehaviorTreeDecoratorGraphNode : public UEdGraphNode
{
	GENERATED_UCLASS_BODY()

	/** Get the BT graph that owns this node */
	virtual class UBehaviorTreeDecoratorGraph* GetDecoratorGraph();

	virtual void AutowireNewNode(UEdGraphPin* FromPin) OVERRIDE;

	// @return the input pin for this state
	virtual UEdGraphPin* GetInputPin(int32 InputIndex=0) const;
	// @return the output pin for this state
	virtual UEdGraphPin* GetOutputPin(int32 InputIndex=0) const;

	virtual EBTDecoratorLogic::Type GetOperationType() const;

	/** allow creating / removing inputs */
	uint32 bAllowModifingInputs : 1;

	virtual void NodeConnectionListChanged() OVERRIDE;

	virtual bool CanCreateUnderSpecifiedSchema(const UEdGraphSchema* DesiredSchema) const OVERRIDE;

	virtual bool CanUserDeleteNode() const OVERRIDE;
};
