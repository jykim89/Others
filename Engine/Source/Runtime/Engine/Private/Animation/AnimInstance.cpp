// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimInstance.cpp: Anim Instance implementation
=============================================================================*/ 

#include "EnginePrivate.h"
#include "AnimationRuntime.h"
#include "AnimationUtils.h"
#include "ParticleDefinitions.h"
#include "DisplayDebugHelpers.h"

#include "MessageLog.h"

/** Anim stats */


DEFINE_STAT(STAT_UpdateSkelMeshBounds);
DEFINE_STAT(STAT_MeshObjectUpdate);
DEFINE_STAT(STAT_BlendInPhysics);
DEFINE_STAT(STAT_SkelCompUpdateTransform);
//                         -->  Physics Engine here <--
DEFINE_STAT(STAT_UpdateRBBones);
DEFINE_STAT(STAT_UpdateRBJoints);
DEFINE_STAT(STAT_UpdateLocalToWorldAndOverlaps);
DEFINE_STAT(STAT_SkelComposeTime);
DEFINE_STAT(STAT_GetAnimationPose);
DEFINE_STAT(STAT_AnimNativeEvaluatePoses);
DEFINE_STAT(STAT_AnimNativeBlendPoses);
DEFINE_STAT(STAT_AnimNativeCopyPoses);
DEFINE_STAT(STAT_AnimGraphEvaluate);
DEFINE_STAT(STAT_AnimBlendTime);
DEFINE_STAT(STAT_RefreshBoneTransforms);
DEFINE_STAT(STAT_InterpolateSkippedFrames);
DEFINE_STAT(STAT_AnimTickTime);
DEFINE_STAT(STAT_SkinnedMeshCompTick);
DEFINE_STAT(STAT_TickUpdateRate);

DEFINE_STAT(STAT_AnimStateMachineUpdate);
DEFINE_STAT(STAT_AnimStateMachineFindTransition);
DEFINE_STAT(STAT_AnimStateMachineEvaluate);

// Define AnimNotify
DEFINE_LOG_CATEGORY(LogAnimNotify);

#define LOCTEXT_NAMESPACE "AnimInstance"

/////////////////////////////////////////////////////
// UAnimInstance
/////////////////////////////////////////////////////

UAnimInstance::UAnimInstance(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	RootNode = NULL;
}

void UAnimInstance::MakeSequenceTickRecord(FAnimTickRecord& TickRecord, class UAnimSequenceBase* Sequence, bool bLooping, float PlayRate, float FinalBlendWeight, float& CurrentTime) const
{
	TickRecord.SourceAsset = Sequence;
	TickRecord.TimeAccumulator = &CurrentTime;
	TickRecord.PlayRateMultiplier = PlayRate;
	TickRecord.EffectiveBlendWeight = FinalBlendWeight;
	TickRecord.bLooping = bLooping;
}

void UAnimInstance::MakeBlendSpaceTickRecord(FAnimTickRecord& TickRecord, class UBlendSpaceBase* BlendSpace, const FVector& BlendInput, TArray<FBlendSampleData> & BlendSampleDataCache, FBlendFilter & BlendFilter, bool bLooping, float PlayRate, float FinalBlendWeight, float& CurrentTime) const
{
	TickRecord.SourceAsset = BlendSpace;
	TickRecord.BlendSpacePosition = BlendInput;
	TickRecord.BlendSampleDataCache = &BlendSampleDataCache;
	TickRecord.BlendFilter = &BlendFilter;
	TickRecord.TimeAccumulator = &CurrentTime;
	TickRecord.PlayRateMultiplier = PlayRate;
	TickRecord.EffectiveBlendWeight = FinalBlendWeight;
	TickRecord.bLooping = bLooping;
}


void UAnimInstance::SequenceAdvanceImmediate(UAnimSequenceBase* Sequence, bool bLooping, float PlayRate, float DeltaSeconds, float& CurrentTime)
{
	FAnimTickRecord TickRecord;
	MakeSequenceTickRecord(TickRecord, Sequence, bLooping, PlayRate, /*FinalBlendWeight=*/ 1.0f, CurrentTime);

	FAnimAssetTickContext TickContext(DeltaSeconds);
	TickRecord.SourceAsset->TickAssetPlayerInstance(TickRecord, this, TickContext);
}

void UAnimInstance::BlendSpaceAdvanceImmediate(class UBlendSpaceBase* BlendSpace, const FVector& BlendInput, TArray<FBlendSampleData> & BlendSampleDataCache, FBlendFilter & BlendFilter, bool bLooping, float PlayRate, float DeltaSeconds, float& CurrentTime)
{
	FAnimTickRecord TickRecord;
	MakeBlendSpaceTickRecord(TickRecord, BlendSpace, BlendInput, BlendSampleDataCache, BlendFilter, bLooping, PlayRate, /*FinalBlendWeight=*/ 1.0f, CurrentTime);
	
	FAnimAssetTickContext TickContext(DeltaSeconds);
	TickRecord.SourceAsset->TickAssetPlayerInstance(TickRecord, this, TickContext);
}

// Creates an uninitialized tick record in the list for the correct group or the ungrouped array.  If the group is valid, OutSyncGroupPtr will point to the group.
FAnimTickRecord& UAnimInstance::CreateUninitializedTickRecord(int32 GroupIndex, FAnimGroupInstance*& OutSyncGroupPtr)
{
	// Find or create the sync group if there is one
	OutSyncGroupPtr = NULL;
	if (GroupIndex >= 0)
	{
		while (SyncGroups.Num() <= GroupIndex)
		{
			new (SyncGroups) FAnimGroupInstance();
		}
		OutSyncGroupPtr = &(SyncGroups[GroupIndex]);
	}

	// Create the record
	FAnimTickRecord* TickRecord = new ((OutSyncGroupPtr != NULL) ? OutSyncGroupPtr->ActivePlayers : UngroupedActivePlayers) FAnimTickRecord();
	return *TickRecord;
}

void UAnimInstance::SequenceEvaluatePose(UAnimSequenceBase* Sequence, FA2Pose& Pose, const FAnimExtractContext & ExtractionContext)
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNativeEvaluatePoses);
	checkSlow( RequiredBones.IsValid() );

	USkeletalMeshComponent* Component = GetSkelMeshComponent();

	if(const UAnimSequence* AnimSequence = Cast<const UAnimSequence>(Sequence))
	{
		FAnimationRuntime::GetPoseFromSequence(
			AnimSequence,
			RequiredBones,
			/*out*/ Pose.Bones, 
			ExtractionContext);
	}
	else if(const UAnimComposite* Composite = Cast<const UAnimComposite>(Sequence))
	{
		FAnimationRuntime::GetPoseFromAnimTrack(
			Composite->AnimationTrack, 
			RequiredBones, 
			/*out*/ Pose.Bones,
			ExtractionContext);
	}
	else
	{
#if 0
		UE_LOG(LogAnimation, Log, TEXT("FAnimationRuntime::GetPoseFromSequence - %s - No animation data!"), *GetFName());
#endif
		FAnimationRuntime::FillWithRefPose(Pose.Bones, RequiredBones);
	}
}

void UAnimInstance::BlendSequences(const FA2Pose& Pose1, const FA2Pose& Pose2, float Alpha, FA2Pose& Result)
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNativeBlendPoses);

	const FTransformArrayA2* Children[2];
	float Weights[2];

	Children[0] = &(Pose1.Bones);
	Children[1] = &(Pose2.Bones);

	Alpha = FMath::Clamp<float>(Alpha, 0.0f, 1.0f);
	Weights[0] = 1.0f - Alpha;
	Weights[1] = Alpha;

	if (Result.Bones.Num() < Pose1.Bones.Num())
	{
		ensureMsg (false, TEXT("Source Pose has more bones than Target Pose"));
		//@hack
		Result.Bones.AddUninitialized(Pose1.Bones.Num() - Result.Bones.Num());
	}
	FAnimationRuntime::BlendPosesTogether(2, Children, Weights, RequiredBones, /*out*/ Result.Bones);
}

void UAnimInstance::CopyPose(const FA2Pose& Source, FA2Pose& Destination)
{
	if (&Destination != &Source)
	{
		SCOPE_CYCLE_COUNTER(STAT_AnimNativeCopyPoses);
		Destination.Bones = Source.Bones;
	}
}

AActor* UAnimInstance::GetOwningActor()
{
	USkeletalMeshComponent* OwnerComponent = GetSkelMeshComponent();
	return OwnerComponent->GetOwner();
}

APawn* UAnimInstance::TryGetPawnOwner()
{
	USkeletalMeshComponent* OwnerComponent = GetSkelMeshComponent();
	if (AActor* OwnerActor = OwnerComponent->GetOwner())
	{
		return Cast<APawn>(OwnerActor);
	}

	return NULL;
}

USkeletalMeshComponent* UAnimInstance::GetOwningComponent()
{
	return GetSkelMeshComponent();
}

UWorld* UAnimInstance::GetWorld() const
{
	return GetSkelMeshComponent()->GetWorld();
}

void UAnimInstance::InitializeAnimation()
{
	// make sure your skeleton is initialized
	// you can overwrite different skeleton
	USkeletalMeshComponent* OwnerComponent = GetSkelMeshComponent();
	if (OwnerComponent->SkeletalMesh != NULL)
	{
		CurrentSkeleton = OwnerComponent->SkeletalMesh->Skeleton;
	}
	else
	{
		CurrentSkeleton = NULL;
	}

	if (UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>(GetClass()))
	{
		// Grab a pointer to the root node
		if (AnimBlueprintClass->RootAnimNodeProperty != NULL)
		{
			RootNode = AnimBlueprintClass->RootAnimNodeProperty->ContainerPtrToValuePtr<FAnimNode_Base>(this);
		}
		else
		{
			RootNode = NULL;
		}

		// if no mesh, use Blueprint Skeleton
		if (CurrentSkeleton == NULL)
		{
			CurrentSkeleton = AnimBlueprintClass->TargetSkeleton;
		}

#if WITH_EDITORONLY_DATA
		if (UAnimBlueprint* Blueprint = Cast<UAnimBlueprint>(AnimBlueprintClass->ClassGeneratedBy))
		{
			if (Blueprint->Status == BS_Error)
			{
				RootNode = NULL;
			}
		}
#endif

#if WITH_EDITOR
		LifeTimer = 0.0;
		CurrentLifeTimerScrubPosition = 0.0;

		if (UAnimBlueprint* Blueprint = Cast<UAnimBlueprint>(AnimBlueprintClass->ClassGeneratedBy))
		{
			if (Blueprint->GetObjectBeingDebugged() == this)
			{
				// Reset the snapshot buffer
				AnimBlueprintClass->GetAnimBlueprintDebugData().ResetSnapshotBuffer();
			}
		}
#endif
	}

	// before initialize, need to recalculate required bone list
	RecalcRequiredBones();

	// Clear cached list, we're about to re-update it.

	ActiveSlotWeights.Empty();

	ClearMorphTargets();
	NativeInitializeAnimation();
	BlueprintInitializeAnimation();

	if (RootNode != NULL)
	{
		IncrementContextCounter();
		FAnimationInitializeContext InitContext(this);
		RootNode->Initialize(InitContext);
	}
}

#if WITH_EDITORONLY_DATA
bool UAnimInstance::UpdateSnapshotAndSkipRemainingUpdate()
{
#if WITH_EDITOR
	// Avoid updating the instance if we're replaying the past
	if (UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>(GetClass()))
	{
		FAnimBlueprintDebugData& DebugData = AnimBlueprintClass->GetAnimBlueprintDebugData();
		if (DebugData.IsReplayingSnapshot())
		{
			if (UAnimBlueprint* Blueprint = Cast<UAnimBlueprint>(AnimBlueprintClass->ClassGeneratedBy))
			{
				if (Blueprint->GetObjectBeingDebugged() == this)
				{
					// Find the correct frame
					DebugData.SetSnapshotIndexByTime(this, CurrentLifeTimerScrubPosition);
					return true;
				}
			}
		}
	}
#endif
	return false;
}
#endif

void UAnimInstance::UpdateAnimation(float DeltaSeconds)
{
#if WITH_EDITORONLY_DATA
	if (GIsEditor)
	{
		// Reset the anim graph visualization
		if (RootNode != NULL)
		{
			if (UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>(GetClass()))
			{
				UAnimBlueprint* AnimBP = CastChecked<UAnimBlueprint>(AnimBlueprintClass->ClassGeneratedBy);

				if (AnimBP->GetObjectBeingDebugged() == this)
				{
					AnimBlueprintClass->GetAnimBlueprintDebugData().ResetNodeVisitSites();
				}
			}
		}

		// Update the lifetimer and see if we should use the snapshot instead
		CurrentLifeTimerScrubPosition += DeltaSeconds;
		LifeTimer = FMath::Max<double>(CurrentLifeTimerScrubPosition, LifeTimer);

		if (UpdateSnapshotAndSkipRemainingUpdate())
		{
			return;
		}
	}
#endif

	AnimNotifies.Empty();
	MorphTargetCurves.Empty();

	ClearSlotNodeWeights();

	//Track material params we set last time round so we can clear them if they aren't set again.
	MaterialParamatersToClear.Empty();
	for( auto Iter = MaterialParameterCurves.CreateConstIterator(); Iter; ++Iter )
	{
		if(Iter.Value() > 0.0f)
		{
			MaterialParamatersToClear.Add(Iter.Key());
		}
	}
	MaterialParameterCurves.Empty();
	VertexAnims.Empty();

	// Reset the player tick list (but keep it presized)
	UngroupedActivePlayers.Empty(UngroupedActivePlayers.Num());
	for (int32 GroupIndex = 0; GroupIndex < SyncGroups.Num(); ++GroupIndex)
	{
		SyncGroups[GroupIndex].Reset();
	}

	NativeUpdateAnimation(DeltaSeconds);
	BlueprintUpdateAnimation(DeltaSeconds);

	// update weight before all nodes update comes in
	Montage_UpdateWeight(DeltaSeconds);

	// Update the anim graph
	if (RootNode != NULL)
	{
		IncrementContextCounter();
		FAnimationUpdateContext UpdateContext(this, DeltaSeconds);
		RootNode->Update(UpdateContext);
	}

	// curve values can be used during update state, so we need to clear the array before ticking each elements
	// where we collect new items
	EventCurves.Empty();

	// Handle all players inside sync groups
	for (int32 GroupIndex = 0; GroupIndex < SyncGroups.Num(); ++GroupIndex)
	{
		FAnimGroupInstance& SyncGroup = SyncGroups[GroupIndex];

		if (SyncGroup.ActivePlayers.Num() > 0)
		{
			const int32 GroupLeaderIndex = FMath::Max(SyncGroup.GroupLeaderIndex, 0);

			// Tick the group leader
			FAnimAssetTickContext TickContext(DeltaSeconds);
			FAnimTickRecord& GroupLeader = SyncGroup.ActivePlayers[GroupLeaderIndex];
			GroupLeader.SourceAsset->TickAssetPlayerInstance(GroupLeader, this, TickContext);

			// Update everything else to follow the leader
			if (SyncGroup.ActivePlayers.Num() > 1)
			{
				TickContext.ConvertToFollower();

				for (int32 TickIndex = 0; TickIndex < SyncGroup.ActivePlayers.Num(); ++TickIndex)
				{
					if (TickIndex != GroupLeaderIndex)
					{
						const FAnimTickRecord& AssetPlayer = SyncGroup.ActivePlayers[TickIndex];
						AssetPlayer.SourceAsset->TickAssetPlayerInstance(AssetPlayer, this, TickContext);
					}
				}
			}
		}
	}

	// Handle the remaining ungrouped animation players
	for (int32 TickIndex = 0; TickIndex < UngroupedActivePlayers.Num(); ++TickIndex)
	{
		const FAnimTickRecord& AssetPlayerToTick = UngroupedActivePlayers[TickIndex];
		FAnimAssetTickContext TickContext(DeltaSeconds);
		AssetPlayerToTick.SourceAsset->TickAssetPlayerInstance(AssetPlayerToTick, this, TickContext);
	}

	// update montage should run in game thread
	// if we do multi threading, make sure this stays in game thread
	Montage_Advance(DeltaSeconds);

	// now trigger Notifies
	TriggerAnimNotifies(DeltaSeconds);

	// Add 0.0 curves to clear parameters that we have previously set but didn't set this tick.
	//   - Make a copy of MaterialParametersToClear as it will be modified by AddCurveValue
	TArray<FName> ParamsToClearCopy = MaterialParamatersToClear;
	for (int i = 0; i < ParamsToClearCopy.Num(); ++i)
	{
		AddCurveValue(ParamsToClearCopy[i], 0.0f, ACF_DrivesMaterial);
	}


#if WITH_EDITOR && 0
	{
		// Take a snapshot if the scrub control is locked to the end, we are playing, and we are the one being debugged
		if (UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>(GetClass()))
		{
			if (UAnimBlueprint* Blueprint = Cast<UAnimBlueprint>(AnimBlueprintClass->ClassGeneratedBy))
			{
				if (Blueprint->GetObjectBeingDebugged() == this)
				{
					if ((CurrentLifeTimerScrubPosition == LifeTimer) && (DeltaSeconds > 0.0f))
					{
						AnimBlueprintClass->GetAnimBlueprintDebugData().TakeSnapshot(this);
					}
				}
			}
		}
	}
#endif
}

void UAnimInstance::EvaluateAnimation(FPoseContext& Output)
{
	// If bone caches have been invalidated, have AnimNodes refresh those.
	if( bBoneCachesInvalidated && RootNode )
	{
		bBoneCachesInvalidated = false;

		IncrementContextCounter();
		FAnimationCacheBonesContext UpdateContext(this);
		RootNode->CacheBones(UpdateContext);
	}

	// Evaluate native code if implemented, otherwise evaluate the node graph
	if (!NativeEvaluateAnimation(Output))
	{
		if (RootNode != NULL)
		{
			SCOPE_CYCLE_COUNTER(STAT_AnimGraphEvaluate);

			RootNode->Evaluate(Output);
		}
		else
		{
			Output.ResetToRefPose();
		}
	}
}

void UAnimInstance::NativeInitializeAnimation()
{
}

void UAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
}

bool UAnimInstance::NativeEvaluateAnimation(FPoseContext& Output)
{
	return false;
}

void OutputCurveMap(TMap<FName, float>& CurveMap, UCanvas* Canvas, UFont* RenderFont, float Indent, float& YPos, FFontRenderInfo RenderInfo, float& YL)
{
	TArray<FName> Names;
	CurveMap.GetKeys(Names);
	Names.Sort();
	for (FName CurveName : Names)
	{
		FString CurveEntry = FString::Printf(TEXT("%s: %.3f"), *CurveName.ToString(), CurveMap[CurveName]);
		Canvas->DrawText(RenderFont, CurveEntry, Indent, YPos, 1.f, 1.f, RenderInfo);
		YPos += YL;
	}
}

void OutputTickRecords(const TArray<FAnimTickRecord>& Records, UCanvas* Canvas, float Indent, const int32 HighlightIndex, FLinearColor TextColor, FLinearColor HighlightColor, FLinearColor InactiveColor, UFont* RenderFont, float& YPos, FFontRenderInfo RenderInfo, float& YL, bool bFullBlendspaceDisplay)
{
	for (int32 PlayerIndex = 0; PlayerIndex < Records.Num(); ++PlayerIndex)
	{
		const FAnimTickRecord& Player = Records[PlayerIndex];

		Canvas->SetLinearDrawColor((PlayerIndex == HighlightIndex) ? HighlightColor : TextColor);

		FString PlayerEntry = FString::Printf(TEXT("%i) %s (%s) W:%.1f%%"), PlayerIndex, *Player.SourceAsset->GetName(), *Player.SourceAsset->GetClass()->GetName(), Player.EffectiveBlendWeight*100.f);
		Canvas->DrawText(RenderFont, PlayerEntry, Indent, YPos, 1.f, 1.f, RenderInfo);
		YPos += YL;

		if (UBlendSpaceBase* BlendSpace = Cast<UBlendSpaceBase>(Player.SourceAsset))
		{
			if (bFullBlendspaceDisplay && Player.BlendSampleDataCache && Player.BlendSampleDataCache->Num() > 0)
			{
				TArray<FBlendSampleData> SampleData = *Player.BlendSampleDataCache;
				SampleData.Sort([](const FBlendSampleData& L, const FBlendSampleData& R) { return L.SampleDataIndex < R.SampleDataIndex; });

				FIndenter BlendspaceIndent(Indent);
				FString BlendspaceHeader = FString::Printf(TEXT("Blendspace Input (%.2f, %.2f, %.2f)"), Player.BlendSpacePosition.X, Player.BlendSpacePosition.Y, Player.BlendSpacePosition.Z);
				Canvas->DrawText(RenderFont, BlendspaceHeader, Indent, YPos, 1.f, 1.f, RenderInfo);
				YPos += YL;

				const TArray<FBlendSample>& BlendSamples = BlendSpace->GetBlendSamples();

				int32 WeightedSampleIndex = 0;

				for (int32 SampleIndex = 0; SampleIndex < BlendSamples.Num(); ++SampleIndex)
				{
					const FBlendSample& BlendSample = BlendSamples[SampleIndex];

					float Weight = 0.f;
					for (; WeightedSampleIndex < SampleData.Num(); ++WeightedSampleIndex)
					{
						FBlendSampleData& WeightedSample = SampleData[WeightedSampleIndex];
						if (WeightedSample.SampleDataIndex == SampleIndex)
						{
							Weight += WeightedSample.GetWeight();
						}
						else if (WeightedSample.SampleDataIndex > SampleIndex)
						{
							break;
						}
					}

					FIndenter SampleIndent(Indent);

					Canvas->SetLinearDrawColor((Weight > 0.f) ? TextColor : InactiveColor);

					FString SampleEntry = FString::Printf(TEXT("%s W:%.1f%%"), *BlendSample.Animation->GetName(), Weight*100.f);
					Canvas->DrawText(RenderFont, SampleEntry, Indent, YPos, 1.f, 1.f, RenderInfo);
					YPos += YL;
				}
			}
		}
	}
}

void UAnimInstance::DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	float Indent = 0.f;

	UFont* RenderFont = GEngine->GetSmallFont();

	FLinearColor TextYellow(0.86f, 0.69f, 0.f);
	FLinearColor TextWhite(0.9f, 0.9f, 0.9f);
	FLinearColor ActiveColor(0.1f, 0.6f, 0.1f);
	FLinearColor InactiveColor(0.2f, 0.2f, 0.2f);
	FLinearColor PoseSourceColor(0.5f, 0.25f, 0.5f);

	Canvas->SetLinearDrawColor(TextYellow);

	static FName CAT_SyncGroups(TEXT("SyncGroups"));
	static FName CAT_Montages(TEXT("Montages"));
	static FName CAT_Graph(TEXT("Graph"));
	static FName CAT_Curves(TEXT("Curves"));
	static FName CAT_Notifies(TEXT("Notifies"));
	static FName CAT_FullAnimGraph(TEXT("FullGraph"));
	static FName CAT_FullBlendspaceDisplay(TEXT("FullBlendspaceDisplay"));

	const bool bShowSyncGroups = DebugDisplay.IsCategoryToggledOn(CAT_SyncGroups, true);
	const bool bShowMontages = DebugDisplay.IsCategoryToggledOn(CAT_Montages, true);
	const bool bShowGraph = DebugDisplay.IsCategoryToggledOn(CAT_Graph, true);
	const bool bShowCurves = DebugDisplay.IsCategoryToggledOn(CAT_Curves, true);
	const bool bShowNotifies = DebugDisplay.IsCategoryToggledOn(CAT_Notifies, true);
	const bool bFullGraph = DebugDisplay.IsCategoryToggledOn(CAT_FullAnimGraph, false);
	const bool bFullBlendspaceDisplay = DebugDisplay.IsCategoryToggledOn(CAT_FullBlendspaceDisplay, true);

	FFontRenderInfo RenderInfo;
	RenderInfo.bEnableShadow = true;

	YPos += YL;

	Canvas->SetLinearDrawColor(TextYellow);

	FString Heading = FString::Printf(TEXT("Animation: %s"), *GetName());
	Canvas->DrawText(RenderFont, Heading, Indent, YPos, 1.f, 1.f, RenderInfo);
	YPos += YL;
	if (bShowSyncGroups)
	{
		FIndenter AnimIndent(Indent);

		//Display Sync Groups
		Heading = FString::Printf(TEXT("SyncGroups: %i"), SyncGroups.Num());
		Canvas->DrawText(RenderFont, Heading, Indent, YPos, 1.f, 1.f, RenderInfo);
		YPos += YL;

		for (int32 GroupIndex = 0; GroupIndex < SyncGroups.Num(); ++GroupIndex)
		{
			FIndenter GroupIndent(Indent);
			FAnimGroupInstance& SyncGroup = SyncGroups[GroupIndex];

			Canvas->SetLinearDrawColor(TextYellow);

			FString GroupLabel = FString::Printf(TEXT("Group %i - Players %i"), GroupIndex, SyncGroup.ActivePlayers.Num());
			Canvas->DrawText(RenderFont, GroupLabel, Indent, YPos, 1.f, 1.f, RenderInfo);
			YPos += YL;

			if (SyncGroup.ActivePlayers.Num() > 0)
			{
				const int32 GroupLeaderIndex = FMath::Max(SyncGroup.GroupLeaderIndex, 0);
				OutputTickRecords(SyncGroup.ActivePlayers, Canvas, Indent, GroupLeaderIndex, TextWhite, ActiveColor, InactiveColor, RenderFont, YPos, RenderInfo, YL, bFullBlendspaceDisplay);
			}
		}

		Canvas->SetLinearDrawColor(TextYellow);

		Heading = FString::Printf(TEXT("Ungrouped: %i"), UngroupedActivePlayers.Num());
		Canvas->DrawText(RenderFont, Heading, Indent, YPos, 1.f, 1.f, RenderInfo);
		YPos += YL;

		Canvas->SetLinearDrawColor(TextWhite);

		OutputTickRecords(UngroupedActivePlayers, Canvas, Indent, -1, TextWhite, ActiveColor, InactiveColor, RenderFont, YPos, RenderInfo, YL, bFullBlendspaceDisplay);
	}

	if (bShowMontages)
	{
		Canvas->SetLinearDrawColor(TextYellow);

		Heading = FString::Printf(TEXT("Montages: %i"), MontageInstances.Num());
		Canvas->DrawText(RenderFont, Heading, Indent, YPos, 1.f, 1.f, RenderInfo);
		YPos += YL;

		FAnimMontageInstance* ActiveMontageInstance = GetActiveMontageInstance();

		for (int32 MontageIndex = 0; MontageIndex < MontageInstances.Num(); ++MontageIndex)
		{
			FIndenter PlayerIndent(Indent);

			FAnimMontageInstance* MontageInstance = MontageInstances[MontageIndex];

			Canvas->SetLinearDrawColor((MontageInstance == ActiveMontageInstance) ? ActiveColor : TextWhite);

			FString MontageEntry = FString::Printf(TEXT("%i) %s Sec: %s W:%.3f DW:%.3f"), MontageIndex, *MontageInstance->Montage->GetName(), *MontageInstance->GetCurrentSection().ToString(), *MontageInstance->GetNextSection().ToString(), MontageInstance->Weight, MontageInstance->DesiredWeight);
			Canvas->DrawText(RenderFont, MontageEntry, Indent, YPos, 1.f, 1.f, RenderInfo);
			YPos += YL;
		}
	}

	if (bShowNotifies)
	{
		Canvas->SetLinearDrawColor(TextYellow);

		Heading = FString::Printf(TEXT("Active Notify States: %i"), ActiveAnimNotifyState.Num());
		Canvas->DrawText(RenderFont, Heading, Indent, YPos, 1.f, 1.f, RenderInfo);
		YPos += YL;

		Canvas->SetLinearDrawColor(TextWhite);

		for (int32 NotifyIndex = 0; NotifyIndex < ActiveAnimNotifyState.Num(); ++NotifyIndex)
		{
			FIndenter NotifyIndent(Indent);

			const FAnimNotifyEvent* NotifyState = ActiveAnimNotifyState[NotifyIndex];

			FString NotifyEntry = FString::Printf(TEXT("%i) %s Class: %s Dur:%.3f"), NotifyIndex, *NotifyState->NotifyName.ToString(), *NotifyState->NotifyStateClass->GetName(), NotifyState->Duration);
			Canvas->DrawText(RenderFont, NotifyEntry, Indent, YPos, 1.f, 1.f, RenderInfo);
			YPos += YL;
		}
	}

	if (bShowCurves)
	{
		Canvas->SetLinearDrawColor(TextYellow);

		Canvas->DrawText(RenderFont, TEXT("Curves"), Indent, YPos, 1.f, 1.f, RenderInfo);
		YPos += YL;

		{
			FIndenter CurveIndent(Indent);

			Heading = FString::Printf(TEXT("Morph Curves: %i"), MorphTargetCurves.Num());
			Canvas->DrawText(RenderFont, Heading, Indent, YPos, 1.f, 1.f, RenderInfo);
			YPos += YL;

			Canvas->SetLinearDrawColor(TextWhite);

			{
				FIndenter MorphCurveIndent(Indent);
				OutputCurveMap(MorphTargetCurves, Canvas, RenderFont, Indent, YPos, RenderInfo, YL);
			}

			Canvas->SetLinearDrawColor(TextYellow);

			Heading = FString::Printf(TEXT("Material Curves: %i"), MaterialParameterCurves.Num());
			Canvas->DrawText(RenderFont, Heading, Indent, YPos, 1.f, 1.f, RenderInfo);
			YPos += YL;

			Canvas->SetLinearDrawColor(TextWhite);

			{
				FIndenter MaterialCurveIndent(Indent);
				OutputCurveMap(MaterialParameterCurves, Canvas, RenderFont, Indent, YPos, RenderInfo, YL);
			}

			Canvas->SetLinearDrawColor(TextYellow);

			Heading = FString::Printf(TEXT("Event Curves: %i"), EventCurves.Num());
			Canvas->DrawText(RenderFont, Heading, Indent, YPos, 1.f, 1.f, RenderInfo);
			YPos += YL;

			Canvas->SetLinearDrawColor(TextWhite);

			{
				FIndenter EventCurveIndent(Indent);
				OutputCurveMap(EventCurves, Canvas, RenderFont, Indent, YPos, RenderInfo, YL);
			}
		}
	}

	if (bShowGraph)
	{
		Canvas->SetLinearDrawColor(TextYellow);

		YPos += YL;
		Canvas->DrawText(RenderFont, TEXT("Anim Node Tree"), Indent, YPos, 1.f, 1.f, RenderInfo);

		const float NodeIndent = 8.f;
		const float LineIndent = 4.f;
		const float AttachLineLength = NodeIndent - LineIndent;

		YPos += YL;
		FIndenter AnimNodeTreeIndent(Indent);

		FNodeDebugData NodeDebugData(this);
		RootNode->GatherDebugData(NodeDebugData);

		TArray<FNodeDebugData::FFlattenedDebugData> FlattenedData = NodeDebugData.GetFlattenedDebugData();

		TArray<float> VerticalLineStarts; // Index represents indent level, track the current starting point for that 

		int32 HalfStep = int32(YL / 2);
		int32 PrevChainID = -1;

		for (FNodeDebugData::FFlattenedDebugData& Line : FlattenedData)
		{
			if (!Line.IsOnActiveBranch() && !bFullGraph)
			{
				continue;
			}
			float CurrIndent = Indent + (Line.Indent * NodeIndent);
			float CurrLineYBase = YPos + YL;

			if (PrevChainID != Line.ChainID)
			{
				YPos += HalfStep; // Extra spacing to delimit different chains, CurrLineYBase now 
				// roughly represents middle of text line, so we can use it for line drawing

				//Handle line drawing
				int32 VerticalLineIndex = Line.Indent - 1;
				if (VerticalLineStarts.IsValidIndex(VerticalLineIndex))
				{
					float VerticalLineStartY = VerticalLineStarts[VerticalLineIndex];
					VerticalLineStarts[VerticalLineIndex] = CurrLineYBase;

					float EndX = CurrIndent;
					float StartX = EndX - AttachLineLength;

					//horizontal line to node
					DrawDebugCanvas2DLine(Canvas, FVector(StartX, CurrLineYBase, 0.f), FVector(EndX, CurrLineYBase, 0.f), ActiveColor);

					//vertical line
					DrawDebugCanvas2DLine(Canvas, FVector(StartX, VerticalLineStartY, 0.f), FVector(StartX, CurrLineYBase, 0.f), ActiveColor);
				}

				CurrLineYBase += HalfStep; // move CurrYLineBase back to base of line
			}

			// Update our base position for subsequent line drawing
			if (!VerticalLineStarts.IsValidIndex(Line.Indent))
			{
				VerticalLineStarts.AddZeroed(Line.Indent + 1 - VerticalLineStarts.Num());
			}
			VerticalLineStarts[Line.Indent] = CurrLineYBase;

			PrevChainID = Line.ChainID;
			FLinearColor ItemColor = Line.bPoseSource ? PoseSourceColor : ActiveColor;
			Canvas->SetLinearDrawColor(Line.IsOnActiveBranch() ? ItemColor : InactiveColor);
			Canvas->DrawText(RenderFont, Line.DebugLine, CurrIndent, YPos, 1.f, 1.f, RenderInfo);
			YPos += YL;
		}
	}
}

void UAnimInstance::BlendSpaceEvaluatePose(class UBlendSpaceBase* BlendSpace, TArray<FBlendSampleData>& BlendSampleDataCache, struct FA2Pose& Pose, bool bIsLooping)
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNativeEvaluatePoses);

	FAnimationRuntime::GetPoseFromBlendSpace(
		BlendSpace,
		BlendSampleDataCache, 
		bIsLooping,
		RequiredBones,
		/*out*/ Pose.Bones);
}

void UAnimInstance::BlendRotationOffset(const struct FA2Pose& BasePose/* local space base pose */, struct FA2Pose const & RotationOffsetPose/* mesh space rotation only additive **/, float Alpha, struct FA2Pose& Pose /** local space blended pose **/)
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNativeBlendPoses);

	check ( RotationOffsetPose.Bones.Num() == RequiredBones.GetNumBones() );
	check ( BasePose.Bones.Num() == RotationOffsetPose.Bones.Num() );
	check ( Pose.Bones.Num() == RotationOffsetPose.Bones.Num() );

	FA2Pose BlendedPose;
	BlendedPose.Bones.AddUninitialized(Pose.Bones.Num());

	// now Pose has Mesh based BasePose
	// apply additive
	if (Alpha > ZERO_ANIMWEIGHT_THRESH)
	{
		FA2Pose MeshBasePose;
		MeshBasePose.Bones.AddUninitialized(Pose.Bones.Num());

		// note that RotationOffsetPose has MeshSpaceRotation additive but everything else (translation/scale) is local space
		// First calculate Mesh space for Base Pose
		const TArray<FBoneIndexType> & RequiredBoneIndices = RequiredBones.GetBoneIndicesArray();

		for (int32 I=0; I<RequiredBoneIndices.Num(); ++I)
		{
			int32 BoneIndex = RequiredBoneIndices[I];
			int32 ParentIndex = RequiredBones.GetParentBoneIndex(BoneIndex);
			if ( ParentIndex != INDEX_NONE )
			{
				MeshBasePose.Bones[BoneIndex] = BasePose.Bones[BoneIndex] * MeshBasePose.Bones[ParentIndex];
			}
			else
			{
				MeshBasePose.Bones[BoneIndex] = BasePose.Bones[BoneIndex];
			}
		}	
		
		const ScalarRegister VBlendWeight(Alpha);
		for (int32 I=0; I<RequiredBoneIndices.Num(); ++I)
		{
			int32 BoneIndex = RequiredBoneIndices[I];
			
			FTransform & Result = BlendedPose.Bones[BoneIndex];

			// We want Base pose (local Pose)
			Result = BasePose.Bones[BoneIndex];

			// set result rotation to be mesh space rotation, so that it applys to mesh space rotation
			Result.SetRotation(MeshBasePose.Bones[BoneIndex].GetRotation());

			// @fixme laurent - we should make a read only version so we can avoid the copy.
			FTransform Additive = RotationOffsetPose.Bones[BoneIndex];
			FTransform::BlendFromIdentityAndAccumulate(Result, Additive, VBlendWeight);
		}

		// Ensure that all of the resulting rotations are normalized
		FAnimationRuntime::NormalizeRotations(RequiredBones, BlendedPose.Bones);

		// now convert back to Local
		for(int32 I=0; I<RequiredBoneIndices.Num(); ++I)
		{
			int32 BoneIndex = RequiredBoneIndices[I];
			int32 ParentIndex = RequiredBones.GetParentBoneIndex(BoneIndex);

			Pose.Bones[BoneIndex] = BlendedPose.Bones[BoneIndex];
			if(ParentIndex != INDEX_NONE)
			{
				// convert to local space first
				FQuat Rotation = BlendedPose.Bones[ParentIndex].GetRotation().Inverse() * BlendedPose.Bones[BoneIndex].GetRotation();
				Pose.Bones[BoneIndex].SetRotation(Rotation);
			}
		}
	}
	else
	{
		BlendedPose = BasePose;
	}
}

void UAnimInstance::ApplyAdditiveSequence(const struct FA2Pose& BasePose,const struct FA2Pose& AdditivePose,float Alpha,struct FA2Pose& Blended)
{
	if (Blended.Bones.Num() < BasePose.Bones.Num())
	{
		// see if this happens
		ensureMsg (false, TEXT("BasePose has more bones than Blended pose"));
		//@hack
		Blended.Bones.AddUninitialized(BasePose.Bones.Num() - Blended.Bones.Num());
	}

	float BlendWeight = FMath::Clamp<float>(Alpha, 0.f, 1.f);

	FAnimationRuntime::BlendAdditivePose(BasePose.Bones, AdditivePose.Bones, BlendWeight, RequiredBones, Blended.Bones);
}

void UAnimInstance::RecalcRequiredBones()
{
	USkeletalMeshComponent * SkelMeshComp = GetSkelMeshComponent();
	check( SkelMeshComp )

	if( SkelMeshComp->SkeletalMesh && SkelMeshComp->SkeletalMesh->Skeleton )
	{
		RequiredBones.InitializeTo(SkelMeshComp->RequiredBones, *SkelMeshComp->SkeletalMesh);
	}
	else if( CurrentSkeleton != NULL )
	{
		RequiredBones.InitializeTo(SkelMeshComp->RequiredBones, *CurrentSkeleton);
	}

	// When RequiredBones mapping has changed, AnimNodes need to update their bones caches. 
	bBoneCachesInvalidated = true;
}

/** Global unique context counter */
static int16 ContextCounter = 0;
void UAnimInstance::IncrementContextCounter()
{
	// Increase frame counter, so that SavedCacheNode will call children only once.
	ContextCounter++;
	// Can't be INDEX_NONE
	if( ContextCounter == INDEX_NONE )
	{
		ContextCounter++;
	}
}

int16 UAnimInstance::GetContextCounter() const
{
	return ContextCounter;
}

void UAnimInstance::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (!Ar.IsLoading() || !Ar.IsSaving())
	{
		Ar << RequiredBones;
	}
}

bool UAnimInstance::CanTransitionSignature() const
{
	return false;
}

void UAnimInstance::AddAnimNotifies(const TArray<const FAnimNotifyEvent*>& NewNotifies, const float InstanceWeight)
{
	// for now there is no filter whatsoever, it just adds everything requested
	for (auto Iter=NewNotifies.CreateConstIterator(); Iter; ++Iter)
	{
		// only add if it is over TriggerWeightThreshold
		if ((*Iter)->TriggerWeightThreshold <= InstanceWeight)
		{
			// Only add unique AnimNotifyState instances just once. We can get multiple triggers if looping over an animation.
			// It is the same state, so just report it once.
			(*Iter)->NotifyStateClass ? AnimNotifies.AddUnique(*Iter) : AnimNotifies.Add(*Iter);
		}
	}
}

void UAnimInstance::AddAnimNotifyFromGeneratedClass(int32 NotifyIndex)
{
	if(NotifyIndex==INDEX_NONE)
	{
		return;
	}

	if (UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>(GetClass()))
	{
		check(AnimBlueprintClass->AnimNotifies.IsValidIndex(NotifyIndex));
		const FAnimNotifyEvent * Notify =& AnimBlueprintClass->AnimNotifies[NotifyIndex];
		AnimNotifies.Add(Notify);
	}
}

void UAnimInstance::AddCurveValue(const FName & CurveName, float Value, int32 CurveTypeFlags)
{
	// save curve value, it will overwrite if same exists, 
	//CurveValues.Add(CurveName, Value);
	if (CurveTypeFlags & ACF_TriggerEvent)
	{
		float *CurveValPtr = EventCurves.Find(CurveName);
		if ( CurveValPtr )
		{
			// sum up, in the future we might normalize, but for now this just sums up
			// this won't work well if all of them have full weight - i.e. additive 
			*CurveValPtr += Value;
		}
		else
		{
			EventCurves.Add(CurveName, Value);
		}
	}

	if (CurveTypeFlags & ACF_DrivesMorphTarget)
	{
		float *CurveValPtr = MorphTargetCurves.Find(CurveName);
		if ( CurveValPtr )
		{
			// sum up, in the future we might normalize, but for now this just sums up
			// this won't work well if all of them have full weight - i.e. additive 
			*CurveValPtr += Value;
		}
		else
		{
			MorphTargetCurves.Add(CurveName, Value);
		}
	}

	if (CurveTypeFlags & ACF_DrivesMaterial)
	{
		MaterialParamatersToClear.RemoveSwap(CurveName);
		float* CurveValPtr = MaterialParameterCurves.Find(CurveName);
		if( CurveValPtr)
		{
			*CurveValPtr += Value;
		}
		else
		{
			MaterialParameterCurves.Add(CurveName, Value);
		}
	}
}

void UAnimInstance::TriggerAnimNotifies(float DeltaSeconds)
{
	TArray<const FAnimNotifyEvent *> NewActiveAnimNotifyState;
	USkeletalMeshComponent * SkelMeshComp = GetSkelMeshComponent();

	// Remove NULL entries.
	ActiveAnimNotifyState.RemoveSwap(NULL);

	for (int32 Index=0; Index<AnimNotifies.Num(); Index++)
	{
		const FAnimNotifyEvent * AnimNotifyEvent = AnimNotifies[Index];

		// AnimNotifyState
		if( AnimNotifyEvent->NotifyStateClass )
		{
			if( !ActiveAnimNotifyState.RemoveSingleSwap(AnimNotifyEvent) )
			{
				AnimNotifyEvent->NotifyStateClass->NotifyBegin(SkelMeshComp, Cast<UAnimSequence>(AnimNotifyEvent->NotifyStateClass->GetOuter()));
			}
			NewActiveAnimNotifyState.Add(AnimNotifyEvent);
			continue;
		}

		if(AnimNotifyEvent->Notify != NULL)
		{
			// Implemented notify: just call Notify. UAnimNotify will forward this to the event which will do the work.
			AnimNotifyEvent->Notify->Notify(SkelMeshComp, Cast<UAnimSequence>(AnimNotifyEvent->Notify->GetOuter()));
		}
		else if( AnimNotifyEvent->NotifyName != NAME_None )
		{
			// Custom Event based notifies. These will call a AnimNotify_* function on the AnimInstance.
			FString FuncName = FString::Printf(TEXT("AnimNotify_%s"), *AnimNotifyEvent->NotifyName.ToString());
			FName FuncFName = FName(*FuncName);

			UFunction* Function = FindFunction(FuncFName);
			if( Function )
			{
				// if parameter is none, add event
				if ( Function->NumParms == 0 )
				{
					ProcessEvent( Function, NULL );								
				}
				else if ( Function->NumParms == 1 &&  
					Cast<UObjectProperty>(Function->PropertyLink) != NULL)
				{
					struct FAnimNotifierHandler_Parms
					{
						UAnimNotify* Notify;
					};

					FAnimNotifierHandler_Parms Parms;
					Parms.Notify = AnimNotifyEvent->Notify;
					ProcessEvent( Function, &Parms );								
				}
				else
				{
					// Actor has event, but with different parameters. Print warning
					UE_LOG(LogAnimNotify, Warning, TEXT("Anim notifier named %s, but the parameter number does not match or not of the correct type"), *FuncName);
				}
			}
		}
	}

	// Send end notification to AnimNotifyState not active anymore.
	for(int32 Index=0; Index<ActiveAnimNotifyState.Num(); Index++)
	{
		const FAnimNotifyEvent * AnimNotifyEvent = ActiveAnimNotifyState[Index];
		AnimNotifyEvent->NotifyStateClass->NotifyEnd(SkelMeshComp, Cast<UAnimSequence>(AnimNotifyEvent->NotifyStateClass->GetOuter()));
	}

	// Switch our arrays.
	ActiveAnimNotifyState = NewActiveAnimNotifyState;

	// Tick currently active AnimNotifyState
	for(int32 Index=0; Index<ActiveAnimNotifyState.Num(); Index++)
	{
		const FAnimNotifyEvent * AnimNotifyEvent = ActiveAnimNotifyState[Index];
		AnimNotifyEvent->NotifyStateClass->NotifyTick(SkelMeshComp, Cast<UAnimSequence>(AnimNotifyEvent->NotifyStateClass->GetOuter()), DeltaSeconds);
	}
}

void UAnimInstance::AnimNotify_Sound(UAnimNotify* AnimNotify)
{
	AnimNotify->Notify(GetSkelMeshComponent(), NULL);
}

//to debug montage weight
#define DEBUGMONTAGEWEIGHT 0

void UAnimInstance::GetSlotWeight(FName const & SlotNodeName, float & out_SlotNodeWeight, float & out_SourceWeight) const
{
	float NodeTotalWeight = 0.f;
	float NonAdditiveTotalWeight = 0.f;

#if DEBUGMONTAGEWEIGHT
	float TotalDesiredWeight = 0.f;
#endif
	// first get all the montage instance weight this slot node has
	for (int32 Index = 0; Index < MontageInstances.Num(); Index++)
	{
		FAnimMontageInstance const * const MontageInstance = MontageInstances[Index];
		if (MontageInstance && MontageInstance->IsValid() && MontageInstance->Montage->IsValidSlot(SlotNodeName))
		{
			NodeTotalWeight += MontageInstance->Weight;
			if( !MontageInstance->Montage->IsValidAdditive() )
			{
				NonAdditiveTotalWeight += MontageInstance->Weight;
			}

#if DEBUGMONTAGEWEIGHT			
			TotalDesiredWeight += MontageInstance->DesiredWeight;
#endif			
		}
	}

	// this can happen when it's blending in OR when newer animation comes in with shorter blendtime
	// say #1 animation was blending out time with current blendtime 1.0 #2 animation was blending in with 1.0 (old) but got blend out with new blendtime 0.2f
	// #3 animation was blending in with the new blendtime 0.2f, you'll have sum of #1, 2, 3 exceeds 1.f
	if (NodeTotalWeight > (1.f + ZERO_ANIMWEIGHT_THRESH))
	{
		// Re-normalize instance weights.
		for (int32 Index = 0; Index < MontageInstances.Num(); Index++)
		{
			FAnimMontageInstance * MontageInstance = MontageInstances[Index];
			if (MontageInstance && MontageInstance->IsValid() && MontageInstance->Montage->IsValidSlot(SlotNodeName))
			{
				MontageInstance->Weight /= NodeTotalWeight;
			}
		} 

		// Re-normalize totals
		NonAdditiveTotalWeight /= NodeTotalWeight;
		NodeTotalWeight = 1.f;
	}
#if DEBUGMONTAGEWEIGHT
	else if (TotalDesiredWeight == 1.f && TotalSum < 1.f - ZERO_ANIMWEIGHT_THRESH)
	{
		// this can happen when it's blending in OR when newer animation comes in with longer blendtime
		// say #1 animation was blending out time with current blendtime 0.2 #2 animation was blending in with 0.2 (old) but got blend out with new blendtime 1.f
		// #3 animation was blending in with the new blendtime 1.f, you'll have sum of #1, 2, 3 doesn't meet 1.f
		UE_LOG(LogAnimation, Warning, TEXT("[%s] Montage has less weight. Blending in?(%f)"), *SlotNodeName.ToString(), TotalSum);
	}
#endif

	out_SlotNodeWeight = NodeTotalWeight;
	out_SourceWeight = 1.f - NonAdditiveTotalWeight;
}

void UAnimInstance::SlotEvaluatePose(FName SlotNodeName, const FA2Pose & SourcePose, FA2Pose & BlendedPose, float SlotNodeWeight)
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNativeEvaluatePoses);
	if (SlotNodeWeight <= ZERO_ANIMWEIGHT_THRESH)
	{
		BlendedPose = SourcePose;
		return;
	}

	// Split our data into additive and non additive.
	TArray<FSlotEvaluationPose> AdditivePoses;
	TArray<FSlotEvaluationPose> NonAdditivePoses;

	// first pass we go through collect weights and valid montages. 
	float TotalWeight = 0.f;
	float NonAdditiveWeight = 0.f;
	for (auto Iter = MontageInstances.CreateConstIterator(); Iter; ++Iter)
	{
		FAnimMontageInstance * MontageInstance = (*Iter);
		if (MontageInstance->IsValid() && MontageInstance->Montage->IsValidSlot(SlotNodeName) && (MontageInstance->Weight > ZERO_ANIMWEIGHT_THRESH))
		{
			FAnimTrack const * const AnimTrack = MontageInstance->Montage->GetAnimationData(SlotNodeName);

			// Find out additive type for pose.
			EAdditiveAnimationType const AdditiveAnimType = AnimTrack->IsAdditive() 
				? (AnimTrack->IsRotationOffsetAdditive() ? AAT_RotationOffsetMeshSpace : AAT_LocalSpaceBase)
				: AAT_None;

			FSlotEvaluationPose NewPose(MontageInstance, MontageInstance->Weight, AdditiveAnimType);
			
			// Bone array has to be allocated prior to calling GetPoseFromAnimTrack
			NewPose.Pose.Bones.AddUninitialized(RequiredBones.GetNumBones());

			// Extract pose from Track
			UAnimMontage const * const MontageAsset = MontageInstance->Montage;
			FAnimExtractContext ExtractionContext(MontageInstance->Position, false, MontageAsset->bEnableRootMotionTranslation, MontageAsset->bEnableRootMotionRotation, MontageAsset->RootMotionRootLock);
			FAnimationRuntime::GetPoseFromAnimTrack(*AnimTrack, RequiredBones, NewPose.Pose.Bones, ExtractionContext);

			TotalWeight += MontageInstance->Weight;
			if (AdditiveAnimType == AAT_None)
			{
				NonAdditiveWeight += MontageInstance->Weight;
				NonAdditivePoses.Add(NewPose);
			}
			else
			{
				AdditivePoses.Add(NewPose);
			}
		}
	}

	// nothing else to do here, there is no weight
	if (TotalWeight <= ZERO_ANIMWEIGHT_THRESH)
	{
		BlendedPose = SourcePose;
		return;
	}
	// Make sure weights don't exceed 1.f, otherwise re-normalize.
	else if (TotalWeight > (1.f + ZERO_ANIMWEIGHT_THRESH))
	{
		// Re-normalize additive poses
		for (int32 Index = 0; Index < AdditivePoses.Num(); Index++)
		{
			AdditivePoses[Index].Weight /= TotalWeight;
		}
		// Re-normalize non-additive poses
		for (int32 Index = 0; Index < NonAdditivePoses.Num(); Index++)
		{
			NonAdditivePoses[Index].Weight /= TotalWeight;
		}
		// Re-normalize totals.
		NonAdditiveWeight /= TotalWeight;
		TotalWeight = 1.f;
	}

	// Make sure we have at least one montage here.
	check((AdditivePoses.Num() > 0) || (NonAdditivePoses.Num() > 0));

	// Second pass, blend non additive poses together
	{
		// If we're only playing additive animations, just copy source for base pose.
		if (NonAdditivePoses.Num() == 0)
		{
			BlendedPose = SourcePose;
		}
		// Otherwise we need to blend non additive poses together
		else
		{
			// allocate for blending
			// If source has any weight, add it to the blend array.
			float const SourceWeight = FMath::Clamp<float>(1.f - NonAdditiveWeight, 0.f, 1.f);
			int32 const NumPoses = NonAdditivePoses.Num() + ((SourceWeight > ZERO_ANIMWEIGHT_THRESH) ? 1 : 0);

			FTransformArrayA2 const ** BlendingPoses = new FTransformArrayA2 const *[NumPoses];
			TArray<float> BlendWeights;
			BlendWeights.AddUninitialized(NumPoses);
			for (int32 Index = 0; Index < NonAdditivePoses.Num(); Index++)
			{
				BlendingPoses[Index] = &NonAdditivePoses[Index].Pose.Bones;
				BlendWeights[Index] = NonAdditivePoses[Index].Weight;
			}

			if (SourceWeight > ZERO_ANIMWEIGHT_THRESH)
			{
				int32 const SourceIndex = BlendWeights.Num() - 1;
				BlendingPoses[SourceIndex] = &SourcePose.Bones;
				BlendWeights[SourceIndex] = SourceWeight;
			}

			// now time to blend all montages
			FAnimationRuntime::BlendPosesTogether(BlendWeights.Num(), (const FTransformArrayA2**)BlendingPoses, (const float*)BlendWeights.GetData(), RequiredBones, BlendedPose.Bones);

			// clean up memory
			delete[] BlendingPoses;
		}
	}

	// Third pass, layer on weighted additive poses.
	{
		for (int32 Index = 0; Index < AdditivePoses.Num(); Index++)
		{
			FSlotEvaluationPose const & AdditivePose = AdditivePoses[Index];
			// if additive, we should blend with source to make it full body
			if (AdditivePose.AdditiveType == AAT_LocalSpaceBase)
			{
				ApplyAdditiveSequence(BlendedPose, AdditivePose.Pose, AdditivePose.Weight, BlendedPose);
			}
			else if (AdditivePose.AdditiveType == AAT_RotationOffsetMeshSpace)
			{
				BlendRotationOffset(BlendedPose, AdditivePose.Pose, AdditivePose.Weight, BlendedPose);
			}
			else
			{
				check(false);
			}
		}
	}
}

void UAnimInstance::RegisterSlotNode(FName SlotNodeName)
{
	// verify if same slot node name exists
	// then warn users, this is invalid
	for (auto Iter = ActiveSlotWeights.CreateConstIterator(); Iter; ++Iter)
	{
		// if same name found, we should warn user, and make sure they know about it
		if ( SlotNodeName == Iter.Key() )
		{
			FMessageLog("AnimBlueprint").Warning(FText::Format(LOCTEXT("AnimInstance_SlotNode", "SLOTNODE: '{0}' already exists. Each slot node has to have unique name."), FText::FromString(SlotNodeName.ToString())));
			return;
		}
	}

	ActiveSlotWeights.Add(SlotNodeName, 0.f);
}

void UAnimInstance::UpdateSlotNodeWeight(FName SlotNodeName, float Weight)
{
	float * CurrentWeight = ActiveSlotWeights.Find(SlotNodeName);
	if (CurrentWeight)
	{
		*CurrentWeight = Weight;
	}
}

void UAnimInstance::ClearSlotNodeWeights()
{
	for (auto Iter = ActiveSlotWeights.CreateIterator(); Iter; ++Iter)
	{
		float & Weight = Iter.Value();
		Weight = 0.f;
	}
}

bool UAnimInstance::IsActiveSlotNode(FName SlotNodeName) const
{
	const float * SlotNodeWeight = ActiveSlotWeights.Find(SlotNodeName);
	return ( SlotNodeWeight && (*SlotNodeWeight > ZERO_ANIMWEIGHT_THRESH) );
}

float UAnimInstance::GetCurveValue(FName CurveName)
{
	float * Value = EventCurves.Find(CurveName);
	if (Value)
	{
		return *Value;
	}

	return 0.f;
}

float UAnimInstance::GetAnimAssetPlayerLength(class UAnimationAsset* AnimAsset)
{
	if (AnimAsset)
	{
		return AnimAsset->GetMaxCurrentTime();
	}

	return 0.f;
}

float UAnimInstance::GetAnimAssetPlayerTimeFraction(class UAnimationAsset* AnimAsset, float CurrentTime)
{
	float Length = (AnimAsset)? AnimAsset->GetMaxCurrentTime() : 0.f;
	if (Length > 0.f)
	{
		return CurrentTime / Length;
	}

	return 0.f;
}

float UAnimInstance::GetAnimAssetPlayerTimeFromEnd(class UAnimationAsset* AnimAsset, float CurrentTime)
{
	if (AnimAsset)
	{
		return AnimAsset->GetMaxCurrentTime() - CurrentTime;
	}

	return 0.f;
}

float UAnimInstance::GetAnimAssetPlayerTimeFromEndFraction(class UAnimationAsset* AnimAsset, float CurrentTime)
{
	float Length = (AnimAsset)? AnimAsset->GetMaxCurrentTime() : 0.f;
	if ( Length > 0.f )
	{
		return (Length- CurrentTime) / Length;
	}

	return 0.f;
}

float UAnimInstance::GetStateWeight(int32 MachineIndex, int32 StateIndex)
{
	if (UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>((UObject*)GetClass()))
	{
		if ((MachineIndex >= 0) && (MachineIndex < AnimBlueprintClass->AnimNodeProperties.Num()))
		{
			const int32 InstancePropertyIndex = AnimBlueprintClass->AnimNodeProperties.Num() - 1 - MachineIndex; //@TODO: ANIMREFACTOR: Reverse indexing

			UStructProperty* MachineInstanceProperty = AnimBlueprintClass->AnimNodeProperties[InstancePropertyIndex];
			checkSlow(MachineInstanceProperty->Struct->IsChildOf(FAnimNode_StateMachine::StaticStruct()));

			FAnimNode_StateMachine* MachineInstance = MachineInstanceProperty->ContainerPtrToValuePtr<FAnimNode_StateMachine>(this);

			return MachineInstance->GetStateWeight(StateIndex);
		}
	}

	return 0.0f;
}

float UAnimInstance::GetCurrentStateElapsedTime(int32 MachineIndex)
{
	if (UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>((UObject*)GetClass()))
	{
		if ((MachineIndex >= 0) && (MachineIndex < AnimBlueprintClass->AnimNodeProperties.Num()))
		{
			const int32 InstancePropertyIndex = AnimBlueprintClass->AnimNodeProperties.Num() - 1 - MachineIndex; //@TODO: ANIMREFACTOR: Reverse indexing

			UStructProperty* MachineInstanceProperty = AnimBlueprintClass->AnimNodeProperties[InstancePropertyIndex];
			checkSlow(MachineInstanceProperty->Struct->IsChildOf(FAnimNode_StateMachine::StaticStruct()));

			FAnimNode_StateMachine* MachineInstance = MachineInstanceProperty->ContainerPtrToValuePtr<FAnimNode_StateMachine>(this);

			return MachineInstance->GetCurrentStateElapsedTime();
		}
	}

	return 0.0f;
}

void UAnimInstance::Montage_SetEndDelegate(FOnMontageEnded & OnMontageEnded)
{
	FAnimMontageInstance * CurMontageInstance = GetActiveMontageInstance();
	if ( CurMontageInstance )
	{
		CurMontageInstance->OnMontageEnded = OnMontageEnded;
	}
}

void UAnimInstance::Montage_SetBlendingOutDelegate(FOnMontageBlendingOutStarted & OnMontageBlendingOut)
{
	FAnimMontageInstance * CurMontageInstance = GetActiveMontageInstance();
	if ( CurMontageInstance )
	{
		CurMontageInstance->OnMontageBlendingOutStarted = OnMontageBlendingOut;
	}
}

FOnMontageBlendingOutStarted* UAnimInstance::Montage_GetBlendingOutDelegate()
{
	FAnimMontageInstance * CurMontageInstance = GetActiveMontageInstance();
	if ( CurMontageInstance )
	{
		return &CurMontageInstance->OnMontageBlendingOutStarted;
	}

	return NULL;
}

void UAnimInstance::Montage_UpdateWeight(float DeltaSeconds)
{
	// go through all montage instances, and update them
	// and make sure their weight is updated properly
	for (int32 I=0; I<MontageInstances.Num(); ++I)
	{
		if ( MontageInstances[I] )
		{
			MontageInstances[I]->UpdateWeight(DeltaSeconds);
		}
	}
}

void UAnimInstance::Montage_Advance(float DeltaSeconds)
{
	bool bUpdateRootMotionMontageInstance = false;
	FRootMotionMovementParams ExtractedRootMotion;

	// go through all montage instances, and update them
	// and make sure their weight is updated properly
	for (int32 I=0; I<MontageInstances.Num(); ++I)
	{
		// should never be NULL
		ensure(MontageInstances[I]);
		if( MontageInstances[I] )
		{
			MontageInstances[I]->Advance(DeltaSeconds, ExtractedRootMotion);

			if( !MontageInstances[I]->IsValid() )
			{
				delete MontageInstances[I];
				MontageInstances.RemoveAt(I);
				bUpdateRootMotionMontageInstance = true;
				--I;
			}
#if DO_CHECK && WITH_EDITORONLY_DATA && 0
			else
			{
				FAnimMontageInstance * AnimMontageInstance = MontageInstances(I);
				// print blending time and weight and montage name
				UE_LOG(LogAnimation, Warning, TEXT("%d. Montage (%s), DesiredWeight(%0.2f), CurrentWeight(%0.2f), BlendingTime(%0.2f)"), 
					I+1, *AnimMontageInstance->Montage->GetName(), AnimMontageInstance->DesiredWeight, AnimMontageInstance->Weight,  
					AnimMontageInstance->BlendTime );
			}
#endif
		}
	}

	if( bUpdateRootMotionMontageInstance )
	{
		UpdateRootMotionMontageInstance();
	}

	// If Root Motion has been extracted, forward it to character physics.
	if( ExtractedRootMotion.bHasRootMotion )
	{
		ACharacter * CharacterOwner = Cast<ACharacter>(GetOwningActor());
		if( CharacterOwner && CharacterOwner->CharacterMovement )
		{
			CharacterOwner->CharacterMovement->RootMotionParams.Accumulate(ExtractedRootMotion);
		}
	}
}

float UAnimInstance::PlaySlotAnimation(UAnimSequenceBase* Asset, FName SlotNodeName, float BlendInTime, float BlendOutTime, float InPlayRate)
{
	// create temporary montage and play
	bool bValidAsset = Asset && !Asset->IsA(UAnimMontage::StaticClass());
	if (!bValidAsset)
	{
		// user warning
		UE_LOG(LogAnimation, Warning, TEXT("Invalid Asset. If Montage, use Montage_Play"));
		return 0.f;
	}

	if (SlotNodeName == NAME_None)
	{
		// user warning
		UE_LOG(LogAnimation, Warning, TEXT("SlotNode Name is required. Make sure to add Slot Node in your anim graph and name it."));
		return 0.f;
	}

	USkeleton * AssetSkeleton = Asset->GetSkeleton();
	if (!CurrentSkeleton->IsCompatible(AssetSkeleton))
	{
		UE_LOG(LogAnimation, Warning, TEXT("The Skeleton isn't compatible"));
		return 0.f;
	}

	// now play
	UAnimMontage * NewMontage = NewObject<UAnimMontage>();
	NewMontage->SetSkeleton(AssetSkeleton);

	// add new track
	FSlotAnimationTrack NewTrack;
	NewTrack.SlotName = SlotNodeName;
	FAnimSegment NewSegment;
	NewSegment.AnimReference = Asset;
	NewSegment.AnimStartTime = 0.f;
	NewSegment.AnimEndTime = Asset->SequenceLength;
	NewSegment.AnimPlayRate = 1.f;
	NewSegment.StartPos = 0.f;
	NewMontage->SequenceLength = Asset->SequenceLength;
	NewTrack.AnimTrack.AnimSegments.Add(NewSegment);
		
	FCompositeSection NewSection;
	NewSection.SectionName = TEXT("Default");
	NewSection.StartTime = 0.f;

	// add new section
	NewMontage->CompositeSections.Add(NewSection);
	NewMontage->BlendInTime = BlendInTime;
	NewMontage->BlendOutTime = BlendOutTime;
	NewMontage->SlotAnimTracks.Add(NewTrack);

	return Montage_Play(NewMontage, InPlayRate);
}

void UAnimInstance::StopSlotAnimation(float InBlendOutTime)
{
	// stop temporary montage
	// when terminate (in the Montage_Advance), we have to lose reference to the temporary montage
	Montage_Stop(InBlendOutTime);
}

bool UAnimInstance::IsPlayingSlotAnimation(UAnimSequenceBase* Asset, FName SlotNodeName )
{
	// check if this is playing
	FAnimMontageInstance * CurrentInstance = GetActiveMontageInstance();
	// make sure what is active right now is transient that we created by request
	if ( CurrentInstance && CurrentInstance->Montage && CurrentInstance->Montage->GetOuter() == GetTransientPackage() )
	{
		UAnimMontage * CurMontage = CurrentInstance->Montage;

		const FAnimTrack * AnimTrack = CurMontage->GetAnimationData(SlotNodeName);
		if (AnimTrack && AnimTrack->AnimSegments.Num() == 1)
		{
			// find if the 
			return (AnimTrack->AnimSegments[0].AnimReference == Asset);
		}
	}

	return false;
}

UAnimMontage * UAnimInstance::GetCurrentActiveMontage()
{
	FAnimMontageInstance* CurrentActiveMontage = GetActiveMontageInstance();
	if (CurrentActiveMontage)
	{
		return CurrentActiveMontage->Montage;
	}

	return NULL;
}

FAnimMontageInstance* UAnimInstance::GetActiveMontageInstance()
{
	if ( MontageInstances.Num() > 0 )
	{
		FAnimMontageInstance * RetVal = MontageInstances.Last();
		if ( RetVal->IsValid() )
		{
			return RetVal;
		}
	}

	return NULL;
}

void UAnimInstance::OnMontagePositionChanged(FAnimMontageInstance* MontageInstance, FName ToSectionName)
{
	if(MontageInstance && MontageInstance->bPlaying)
	{
		if(MontageInstance->DesiredWeight == 0.f)
		{
			UE_LOG( LogAnimation, Warning, TEXT("Changing section on Montage (%s) to '%s' during blend out. This can cause incorrect visuals!"), 
					*MontageInstance->Montage->GetName(), *ToSectionName.ToString() );
			MontageInstance->Play(MontageInstance->PlayRate);
		}
	}
}

void UAnimInstance::Montage_JumpToSection(FName SectionName)
{
	FAnimMontageInstance * CurMontageInstance = GetActiveMontageInstance();
	if ( CurMontageInstance && CurMontageInstance->ChangePositionToSection(SectionName, CurMontageInstance->PlayRate < 0.0f) == false )
	{
		UE_LOG(LogAnimation, Warning, TEXT("Jumping section to %s failed for Montage (%s) "), 
			*SectionName.ToString(), *CurMontageInstance->Montage->GetName() );
	}
	else
	{
		OnMontagePositionChanged(CurMontageInstance, SectionName);
	}
}

void UAnimInstance::Montage_JumpToSectionsEnd(FName SectionName)
{
	FAnimMontageInstance * CurMontageInstance = GetActiveMontageInstance();
	if ( CurMontageInstance && CurMontageInstance->ChangePositionToSection(SectionName, CurMontageInstance->PlayRate >= 0.0f) == false )
	{
		UE_LOG(LogAnimation, Warning, TEXT("Jumping section to %s failed for Montage (%s) "), 
			*SectionName.ToString(), *CurMontageInstance->Montage->GetName() );
	}
	else
	{
		OnMontagePositionChanged(CurMontageInstance, SectionName);
	}
}

FName UAnimInstance::Montage_GetCurrentSection()
{
	FAnimMontageInstance * CurMontageInstance = GetActiveMontageInstance();
	if ( CurMontageInstance )
	{
		return CurMontageInstance->GetCurrentSection();
	}

	return NAME_None;
}

void UAnimInstance::Montage_SetNextSection(FName SectionNameToChange, FName NextSection)
{
	FAnimMontageInstance * CurMontageInstance = GetActiveMontageInstance();
	if ( CurMontageInstance && CurMontageInstance->ChangeNextSection(SectionNameToChange, NextSection) == false )
	{
		UE_LOG(LogAnimation, Warning, TEXT("Changing section from %s to %s failed for Montage (%s) "), 
			*SectionNameToChange.ToString(), *NextSection.ToString(), *CurMontageInstance->Montage->GetName() );
	}
	else
	{
		OnMontagePositionChanged(CurMontageInstance, NextSection);
	}
}

/** Play a Montage. Returns Length of Montage in seconds. Returns 0.f if failed to play. */
float UAnimInstance::Montage_Play(UAnimMontage * MontageToPlay, float InPlayRate)
{
	if( MontageToPlay && (MontageToPlay->SequenceLength > 0.f) ) 
	{
		if( CurrentSkeleton->IsCompatible(MontageToPlay->GetSkeleton()) )
		{
			// when stopping old animations, make sure it does give current new blendintime to blend out
			StopAllMontages(MontageToPlay->BlendInTime);

			FAnimMontageInstance * NewInstance = new FAnimMontageInstance(this);
			check (NewInstance);

			NewInstance->Initialize(MontageToPlay);
			NewInstance->Play(InPlayRate);
			MontageInstances.Add(NewInstance);

			UpdateRootMotionMontageInstance();

			return NewInstance->Montage->SequenceLength;
		}
		else
		{
			UE_LOG(LogAnimation, Warning, TEXT("Playing a Montage (%s) for the wrong Skeleton (%s) instead of (%s)."), 
				*GetNameSafe(MontageToPlay), *GetNameSafe(MontageToPlay->GetSkeleton()), *GetNameSafe(CurrentSkeleton));
		}
	}
	else
	{
		UE_LOG(LogAnimation, Warning, TEXT("Trying to play invalid Montage (%s)"), *GetNameSafe(MontageToPlay));
	}

	return 0.f;
}

void UAnimInstance::UpdateRootMotionMontageInstance()
{
	FAnimMontageInstance * ActiveMontageInstance = GetActiveMontageInstance();

	const bool bValidRootMotionInstance = (ActiveMontageInstance && ActiveMontageInstance->IsValid() && ActiveMontageInstance->Montage 
		&& (ActiveMontageInstance->Montage->bEnableRootMotionTranslation || ActiveMontageInstance->Montage->bEnableRootMotionRotation) );

	RootMotionMontageInstance = bValidRootMotionInstance ? ActiveMontageInstance : NULL;
}

FAnimMontageInstance * UAnimInstance::GetRootMotionMontageInstance() const
{
	return RootMotionMontageInstance;
}

void UAnimInstance::Montage_Stop(float InBlendOutTime)
{
	FAnimMontageInstance * CurMontageInstance = GetActiveMontageInstance();
	if ( CurMontageInstance )
	{
		CurMontageInstance->Stop(InBlendOutTime);
	}
}

/** Has Montage been stopped? */
bool UAnimInstance::Montage_GetIsStopped(UAnimMontage* Montage)
{
	FAnimMontageInstance* CurMontageInstance = GetActiveMontageInstance();
	return (!CurMontageInstance || (CurMontageInstance->Montage != Montage) || (CurMontageInstance->DesiredWeight == 0.f));
}

bool UAnimInstance::Montage_IsActive(UAnimMontage * Montage)
{
	FAnimMontageInstance * CurMontageInstance = GetActiveMontageInstance();
	return ( CurMontageInstance  && CurMontageInstance->Montage == Montage);
}


bool UAnimInstance::Montage_IsPlaying(UAnimMontage * Montage)
{
	FAnimMontageInstance * CurMontageInstance = GetActiveMontageInstance();
	return ( CurMontageInstance  && CurMontageInstance->Montage == Montage && CurMontageInstance->IsPlaying() );
}

float UAnimInstance::Montage_GetPosition(UAnimMontage* Montage)
{
	FAnimMontageInstance * CurMontageInstance = GetActiveMontageInstance();
	return CurMontageInstance ? CurMontageInstance->Position : 0.f;
}

void UAnimInstance::Montage_SetPosition(UAnimMontage* Montage, float NewPosition)
{
	// @laurent we probably want (an option?) to advance time rather than jump? As that skips notifies/events?
	FAnimMontageInstance* CurMontageInstance = GetActiveMontageInstance();
	if( CurMontageInstance )
	{
		CurMontageInstance->Position = NewPosition;
	}
}

float UAnimInstance::Montage_GetPlayRate(UAnimMontage* Montage)
{
	FAnimMontageInstance * CurMontageInstance = GetActiveMontageInstance();
	return CurMontageInstance ? CurMontageInstance->PlayRate : 0.f;
}

void UAnimInstance::Montage_SetPlayRate(UAnimMontage* Montage, float NewPlayRate)
{
	FAnimMontageInstance* CurMontageInstance = GetActiveMontageInstance();
	if( CurMontageInstance && (CurMontageInstance->Montage == Montage) && CurMontageInstance->IsPlaying() )
	{
		CurMontageInstance->PlayRate = NewPlayRate;
	}
}

int32 UAnimInstance::Montage_GetNextSectionID(UAnimMontage* Montage, int32 CurrentSectionID)
{
	FAnimMontageInstance * CurMontageInstance = GetActiveMontageInstance();
	if( CurMontageInstance && CurrentSectionID < CurMontageInstance->NextSections.Num() )
	{
		return CurMontageInstance->NextSections[CurrentSectionID];
	}

	return INDEX_NONE;
}

void UAnimInstance::StopAllMontages(float BlendOut)
{
	for ( int32 Index=MontageInstances.Num()-1; Index>=0; Index-- )
	{
		MontageInstances[Index]->Stop(BlendOut, true);
	}
}

void UAnimInstance::SetMorphTarget(FName MorphTargetName, float Value)
{
	USkeletalMeshComponent * Component = GetOwningComponent();
	if (Component)
	{
		Component->SetMorphTarget(MorphTargetName, Value);
	}
}

void UAnimInstance::ClearMorphTargets()
{
	USkeletalMeshComponent * Component = GetOwningComponent();
	if (Component)
	{
		Component->ClearMorphTargets();
	}
}

float UAnimInstance::CalculateDirection(const FVector & Velocity, const FRotator & BaseRotation)
{
	FMatrix RotMatrix = FRotationMatrix(BaseRotation);
	FVector ForwardVector = RotMatrix.GetScaledAxis(EAxis::X);
	FVector RightVector = RotMatrix.GetScaledAxis(EAxis::Y);
	FVector NormalizedVel = Velocity.SafeNormal();
	ForwardVector.Z = RightVector.Z = NormalizedVel.Z = 0.f;

	// get a cos(alpha) of forward vector vs velocity
	float ForwardCosAngle = FVector::DotProduct(ForwardVector, NormalizedVel);
	// now get the alpha and convert to degree
	float ForwardDeltaDegree = FMath::RadiansToDegrees( FMath::Acos(ForwardCosAngle) );

	// depending on where right vector is, flip it
	float RightCosAngle = FVector::DotProduct(RightVector, NormalizedVel);
	if ( RightCosAngle < 0 )
	{
		ForwardDeltaDegree *= -1;
	}

	return ForwardDeltaDegree;
}

void UAnimInstance::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UAnimInstance* This = CastChecked<UAnimInstance>(InThis);
	if (This)
	{
		// go through all montage instances, and update them
		// and make sure their weight is updated properly
		for (int32 I=0; I<This->MontageInstances.Num(); ++I)
		{
			if( This->MontageInstances[I] )
			{
				This->MontageInstances[I]->AddReferencedObjects(Collector);
			}
		}
	}

	Super::AddReferencedObjects(This, Collector);
}
// 
void UAnimInstance::LockAIResources(bool bLockMovement, bool LockAILogic)
{
	APawn* PawnOwner = TryGetPawnOwner();
	if (PawnOwner)
	{
		AAIController* OwningAI = Cast<AAIController>(PawnOwner->Controller);
		if( OwningAI )
		{
			if (bLockMovement && OwningAI->PathFollowingComponent)
			{
				OwningAI->PathFollowingComponent->LockResource(EAILockSource::Animation);
			}
			if (LockAILogic && OwningAI->BrainComponent)
			{
				OwningAI->BrainComponent->LockResource(EAILockSource::Animation);
			}			
		}
	}
}

void UAnimInstance::UnlockAIResources(bool bUnlockMovement, bool UnlockAILogic)
{
	APawn* PawnOwner = TryGetPawnOwner();
	if (PawnOwner)
	{
		AAIController* OwningAI = Cast<AAIController>(PawnOwner->Controller);
		if( OwningAI )
		{
			if (bUnlockMovement && OwningAI->PathFollowingComponent)
			{
				OwningAI->PathFollowingComponent->ClearResourceLock(EAILockSource::Animation);
			}
			if (UnlockAILogic && OwningAI->BrainComponent)
			{
				OwningAI->BrainComponent->ClearResourceLock(EAILockSource::Animation);
			}			
		}
	}
}

#undef LOCTEXT_NAMESPACE 
