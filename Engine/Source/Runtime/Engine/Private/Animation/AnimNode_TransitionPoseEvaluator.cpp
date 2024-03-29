// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

/////////////////////////////////////////////////////
// FAnimNode_TransitionPoseEvaluator

FAnimNode_TransitionPoseEvaluator::FAnimNode_TransitionPoseEvaluator()
	: DataSource(EEvaluatorDataSource::EDS_SourcePose)
	, EvaluatorMode(EEvaluatorMode::EM_Standard)
	, FramesToCachePose(1)
	, CacheFramesRemaining(1)
{
}

void FAnimNode_TransitionPoseEvaluator::Initialize(const FAnimationInitializeContext& Context)
{	
	if (EvaluatorMode == EEvaluatorMode::EM_Freeze)
	{
		// EM_Freeze must evaluate 1 frame to get the initial pose. This cached frame will not call update, only evaluate
		CacheFramesRemaining = 1;
	}
	else if (EvaluatorMode == EEvaluatorMode::EM_DelayedFreeze)
	{
		// EM_DelayedFreeze can evaluate multiple frames, but must evaluate at least one.
		CacheFramesRemaining = FMath::Max(FramesToCachePose, 1);
	}
}

void FAnimNode_TransitionPoseEvaluator::CacheBones(const FAnimationCacheBonesContext & Context) 
{
	const int32 NumBones = Context.AnimInstance->RequiredBones.GetNumBones();
	CachedPose.Bones.Empty(NumBones);
}

void FAnimNode_TransitionPoseEvaluator::Update(const FAnimationUpdateContext& Context)
{
	// updating is all handled in state machine
}

void FAnimNode_TransitionPoseEvaluator::Evaluate(FPoseContext& Output)
{	
	// the cached pose is evaluated in the state machine and set via CachePose(). 
	// This is because we need information about the transition that is not available at this level
	Output.AnimInstance->CopyPose(CachedPose, Output.Pose);

	if ((EvaluatorMode != EEvaluatorMode::EM_Standard) && (CacheFramesRemaining > 0))
	{
		CacheFramesRemaining = FMath::Max(CacheFramesRemaining - 1, 0);
	}
}

void FAnimNode_TransitionPoseEvaluator::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	
	DebugLine += FString::Printf(TEXT("(Cached Frames Remaining: %i)"), CacheFramesRemaining);
	DebugData.AddDebugItem(DebugLine);
}

bool FAnimNode_TransitionPoseEvaluator::InputNodeNeedsUpdate() const
{
	// EM_Standard mode always updates and EM_DelayedFreeze mode only updates if there are cache frames remaining
	return (EvaluatorMode == EEvaluatorMode::EM_Standard) || ((EvaluatorMode == EEvaluatorMode::EM_DelayedFreeze) && (CacheFramesRemaining > 0));
}

bool FAnimNode_TransitionPoseEvaluator::InputNodeNeedsEvaluate() const
{
	return (EvaluatorMode == EEvaluatorMode::EM_Standard) || (CacheFramesRemaining > 0);
}

void FAnimNode_TransitionPoseEvaluator::CachePose(FPoseContext& Output, FA2Pose& PoseToCache)
{
	Output.AnimInstance->CopyPose(PoseToCache, CachedPose);
}