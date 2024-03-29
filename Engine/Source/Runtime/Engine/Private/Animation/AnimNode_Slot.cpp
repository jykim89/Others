// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

/////////////////////////////////////////////////////
// FAnimNode_Slot

void FAnimNode_Slot::Initialize(const FAnimationInitializeContext& Context)
{
	Source.Initialize(Context);

	SourceWeight = 0.f;
	SlotNodeWeight = 0.f;

	Context.AnimInstance->RegisterSlotNode(SlotName);
}

void FAnimNode_Slot::CacheBones(const FAnimationCacheBonesContext & Context) 
{
	Source.CacheBones(Context);
}

void FAnimNode_Slot::Update(const FAnimationUpdateContext& Context)
{
	// Update weights.
	Context.AnimInstance->GetSlotWeight(SlotName, SlotNodeWeight, SourceWeight);

	// Update cache in AnimInstance.
	Context.AnimInstance->UpdateSlotNodeWeight(SlotName, SlotNodeWeight);

	if (SourceWeight > ZERO_ANIMWEIGHT_THRESH)
	{
		Source.Update(Context.FractionalWeight(SourceWeight));
	}
}

void FAnimNode_Slot::Evaluate(FPoseContext & Output)
{
	// If not playing a montage, just pass through
	if (SlotNodeWeight <= ZERO_ANIMWEIGHT_THRESH)
	{
		Source.Evaluate(Output);
	}
	else
	{
		FPoseContext SourceContext(Output);
		if (SourceWeight > ZERO_ANIMWEIGHT_THRESH)
		{
			Source.Evaluate(SourceContext);
		}

		Output.AnimInstance->SlotEvaluatePose(SlotName, SourceContext.Pose, Output.Pose, SlotNodeWeight);

		checkSlow(!Output.ContainsNaN());
		checkSlow(Output.IsNormalized());
	}
}

void FAnimNode_Slot::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Slot Name: '%s' Weight:%.1f%%)"), *SlotName.ToString(), SlotNodeWeight*100.f);

	bool const bIsPoseSource = (SourceWeight <= ZERO_ANIMWEIGHT_THRESH);
	DebugData.AddDebugItem(DebugLine, bIsPoseSource);
	Source.GatherDebugData(DebugData.BranchFlow(SourceWeight));

	for (FAnimMontageInstance* MontageInstance : DebugData.AnimInstance->MontageInstances)
	{
		if (MontageInstance->IsValid() && MontageInstance->Montage->IsValidSlot(SlotName))
		{
			const FAnimTrack* Track = MontageInstance->Montage->GetAnimationData(SlotName);
			for (const FAnimSegment& Segment : Track->AnimSegments)
			{
				float Weight;
				float CurrentAnimPos;
				if (UAnimSequenceBase* Anim = Segment.GetAnimationData(MontageInstance->Position, CurrentAnimPos, Weight))
				{
					FString MontageLine = FString::Printf(TEXT("Montage: '%s' Anim: '%s' Play Time:%.2f W:%.2f%%"), *MontageInstance->Montage->GetName(), *Anim->GetName(), CurrentAnimPos, Weight*100.f);
					DebugData.BranchFlow(1.0f).AddDebugItem(MontageLine, true);
					break;
				}
			}
		}
	}
}

FAnimNode_Slot::FAnimNode_Slot()
: SourceWeight(0.f)
, SlotNodeWeight(0.f)
{
}
