// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AnimGraphNode_RefPoseBase.h"
#include "AnimGraphNode_LocalRefPose.generated.h"

UCLASS(MinimalAPI)
class UAnimGraphNode_LocalRefPose : public UAnimGraphNode_RefPoseBase
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode interface
	virtual FString GetTooltip() const OVERRIDE;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const OVERRIDE;
	// End of UEdGraphNode interface
};
