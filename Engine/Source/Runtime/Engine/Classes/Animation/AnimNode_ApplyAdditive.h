// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNodeBase.h"
#include "InputScaleBias.h"
#include "AnimNode_ApplyAdditive.generated.h"

USTRUCT()
struct ENGINE_API FAnimNode_ApplyAdditive : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Links)
	FPoseLink Base;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Links)
	FPoseLink Additive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(PinShownByDefault))
	mutable float Alpha;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FInputScaleBias AlphaScaleBias;

public:	
	FAnimNode_ApplyAdditive();

	// FAnimNode_Base interface
	virtual void Initialize(const FAnimationInitializeContext& Context) OVERRIDE;
	virtual void CacheBones(const FAnimationCacheBonesContext & Context) OVERRIDE;
	virtual void Update(const FAnimationUpdateContext& Context) OVERRIDE;
	virtual void Evaluate(FPoseContext& Output) OVERRIDE;
	virtual void GatherDebugData(FNodeDebugData& DebugData) OVERRIDE;
	// End of FAnimNode_Base interface
};
