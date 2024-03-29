// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "K2Node_CallParentFunction.generated.h"

UCLASS(MinimalAPI)
class UK2Node_CallParentFunction : public UK2Node_CallFunction
{
	GENERATED_UCLASS_BODY()

	// Begin EdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const OVERRIDE;
	virtual FString GetNodeNativeTitle(ENodeTitleType::Type TitleType) const OVERRIDE;
	virtual void AllocateDefaultPins() OVERRIDE;
	virtual FLinearColor GetNodeTitleColor() const OVERRIDE;
	virtual void PostPlacedNewNode() OVERRIDE;
	// End EdGraphNode interface

	virtual void SetFromFunction(const UFunction* Function) OVERRIDE;
};

