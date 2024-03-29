// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

/////////////////////////////////////////////////////
// FAnimRefPoseNode

void FAnimNode_RefPose::Evaluate(FPoseContext& Output)
{
	// I don't have anything to evaluate. Should this be even here?
	// EvaluateGraphExposedInputs.Execute(Context);

	switch (RefPoseType) 
	{
	case EIT_LocalSpace:
		Output.ResetToRefPose();
		break;

	case EIT_Additive:
	default:
		Output.ResetToIdentity();
		break;
	}
}

void FAnimNode_MeshSpaceRefPose::EvaluateComponentSpace(FComponentSpacePoseContext& Output)
{
	Output.ResetToRefPose();
}

/** Helper for enum output... */
#ifndef CASE_ENUM_TO_TEXT
#define CASE_ENUM_TO_TEXT(txt) case txt: return TEXT(#txt);
#endif

const TCHAR* GetRefPostTypeText(ERefPoseType RefPose)
{
	switch (RefPose)
	{
		FOREACH_ENUM_EREFPOSETYPE(CASE_ENUM_TO_TEXT)
	}
	return TEXT("Unknown Ref Pose Type");
}

void FAnimNode_RefPose::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Ref Pose Type: %s)"), GetRefPostTypeText(RefPoseType));
	DebugData.AddDebugItem(DebugLine, true);
}