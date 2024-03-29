// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

/////////////////////////////////////////////////////
// FAnimNode_ConvertComponentToLocalSpace

FAnimNode_ConvertComponentToLocalSpace::FAnimNode_ConvertComponentToLocalSpace()
{
}

void FAnimNode_ConvertComponentToLocalSpace::Initialize(const FAnimationInitializeContext& Context)
{
	ComponentPose.Initialize(Context);
}

void FAnimNode_ConvertComponentToLocalSpace::CacheBones(const FAnimationCacheBonesContext & Context) 
{
	ComponentPose.CacheBones(Context);
}

void FAnimNode_ConvertComponentToLocalSpace::Update(const FAnimationUpdateContext& Context)
{
	ComponentPose.Update(Context);
}

void FAnimNode_ConvertComponentToLocalSpace::Evaluate(FPoseContext & Output)
{
	// Evaluate the child and convert
	FComponentSpacePoseContext InputCSPose(Output.AnimInstance);
	ComponentPose.EvaluateComponentSpace(InputCSPose);

	checkSlow( InputCSPose.Pose.IsValid() );
	InputCSPose.Pose.ConvertToLocalPoses(Output.Pose);
}

void FAnimNode_ConvertComponentToLocalSpace::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugData.AddDebugItem(DebugLine);
	ComponentPose.GatherDebugData(DebugData);
}

/////////////////////////////////////////////////////
// FAnimNode_ConvertLocalToComponentSpace

FAnimNode_ConvertLocalToComponentSpace::FAnimNode_ConvertLocalToComponentSpace()
{
}

void FAnimNode_ConvertLocalToComponentSpace::Initialize(const FAnimationInitializeContext& Context)
{
	LocalPose.Initialize(Context);
}

void FAnimNode_ConvertLocalToComponentSpace::CacheBones(const FAnimationCacheBonesContext & Context) 
{
	LocalPose.CacheBones(Context);
}

void FAnimNode_ConvertLocalToComponentSpace::Update(const FAnimationUpdateContext& Context)
{
	LocalPose.Update(Context);
}

void FAnimNode_ConvertLocalToComponentSpace::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugData.AddDebugItem(DebugLine);
	LocalPose.GatherDebugData(DebugData);
}

void FAnimNode_ConvertLocalToComponentSpace::EvaluateComponentSpace(FComponentSpacePoseContext & OutputCSPose)
{
	// Evaluate the child and convert
	FPoseContext InputPose(OutputCSPose.AnimInstance);
	LocalPose.Evaluate(InputPose);

	OutputCSPose.Pose.AllocateLocalPoses(OutputCSPose.AnimInstance->RequiredBones, InputPose.Pose);
}
