// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNode_TransitionPoseEvaluator.generated.h"

// Indicates which state is being evaluated by this node (source or destination)
UENUM()
namespace EEvaluatorDataSource
{
	enum Type
	{
		EDS_SourcePose UMETA(DisplayName="Source Pose"),
		EDS_DestinationPose UMETA(DisplayName="Destination Pose")
	};
}

// Determines the behavior this node will use when updating and evaluating.
UENUM()
namespace EEvaluatorMode
{
	enum Mode
	{
		// DataSource is ticked and evaluated every frame
		EM_Standard UMETA(DisplayName="Standard"),

		// DataSource is never ticked and only evaluated on the first frame. Every frame after uses the cached pose from the first frame.
		EM_Freeze UMETA(DisplayName="Freeze"),

		// DataSource is ticked and evaluated for a given number of frames, then freezes after and uses the cached pose for future frames.
		EM_DelayedFreeze UMETA(DisplayName="Delayed Freeze")
	};
}


// Animation data node for state machine transitions.
// Can be set to supply either the animation data from the transition source (From State) or the transition destination (To State).
USTRUCT()
struct ENGINE_API FAnimNode_TransitionPoseEvaluator : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Pose, meta=(NeverAsPin))
	TEnumAsByte<EEvaluatorDataSource::Type> DataSource;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Pose, meta=(NeverAsPin))
	TEnumAsByte<EEvaluatorMode::Mode> EvaluatorMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Pose, meta=(NeverAsPin, ClampMin="1", UIMin="1"))
	int32 FramesToCachePose;

	UPROPERTY(transient)
	FA2Pose CachedPose;

	UPROPERTY(transient)
	int32 CacheFramesRemaining;

public:	
	FAnimNode_TransitionPoseEvaluator();

	// FAnimNode_Base interface
	virtual void Initialize(const FAnimationInitializeContext& Context) OVERRIDE;
	virtual void CacheBones(const FAnimationCacheBonesContext & Context) OVERRIDE;
	virtual void Update(const FAnimationUpdateContext& Context) OVERRIDE;
	virtual void Evaluate(FPoseContext& Output) OVERRIDE;
	virtual void GatherDebugData(FNodeDebugData& DebugData) OVERRIDE;
	// End of FAnimNode_Base interface

	bool InputNodeNeedsUpdate() const;
	bool InputNodeNeedsEvaluate() const;
	void CachePose(FPoseContext& Output, FA2Pose& PoseToCache);
};
