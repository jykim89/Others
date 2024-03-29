// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

UAnimationAsset::UAnimationAsset(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

void UAnimationAsset::PostLoad()
{
	Super::PostLoad();

	// Load skeleton, to make sure anything accessing from PostLoad
	// skeleton is ready
	if (Skeleton)
	{
		Skeleton ->ConditionalPostLoad();
	}

	ValidateSkeleton();


	check( Skeleton==NULL || SkeletonGuid.IsValid() );
}

void UAnimationAsset::ResetSkeleton(USkeleton* NewSkeleton)
{
// @TODO LH, I'd like this to work outside of editor, but that requires unlocking track names data in game
#if WITH_EDITOR
	Skeleton = NULL;
	ReplaceSkeleton(NewSkeleton);
#endif
}

void UAnimationAsset::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.UE4Ver() >= VER_UE4_SKELETON_GUID_SERIALIZATION)
	{
		Ar << SkeletonGuid;
	}
}

void UAnimationAsset::SetSkeleton(USkeleton* NewSkeleton)
{
	if (NewSkeleton && NewSkeleton != Skeleton)
	{
		Skeleton = NewSkeleton;
		SkeletonGuid = NewSkeleton->GetGuid();
	}
}

#if WITH_EDITOR
bool UAnimationAsset::ReplaceSkeleton(USkeleton* NewSkeleton)
{
	// if it's not same 
	if (NewSkeleton != Skeleton)
	{
		// get all sequences that need to change
		TArray<UAnimSequence*> AnimSeqsToReplace;
		if (GetAllAnimationSequencesReferred(AnimSeqsToReplace))
		{
			for (auto Iter = AnimSeqsToReplace.CreateIterator(); Iter; ++Iter)
			{
				UAnimSequence* AnimSeq = *Iter;
				if (AnimSeq && AnimSeq->Skeleton != NewSkeleton)
				{
					AnimSeq->RemapTracksToNewSkeleton(NewSkeleton);
				}
			}
		}

		SetSkeleton(NewSkeleton);

		PostEditChange();
		MarkPackageDirty();
		return true;
	}

	return false;
}

bool UAnimationAsset::GetAllAnimationSequencesReferred(TArray<UAnimSequence*>& AnimationSequences)
{
	return false;
}

void UAnimationAsset::ReplaceReferredAnimations(const TMap<UAnimSequence*, UAnimSequence*>& ReplacementMap)
{
}

#endif

void UAnimationAsset::ValidateSkeleton()
{
	if (Skeleton && Skeleton->GetGuid() != SkeletonGuid)
	{
		// reset Skeleton
		ResetSkeleton(Skeleton);
	}
}

void FBoneContainer::InitializeTo(const TArray<FBoneIndexType>& InRequiredBoneIndexArray, UObject& InAsset)
{
	BoneIndicesArray = InRequiredBoneIndexArray;
	Asset = &InAsset;

	Initialize();
}

void FBoneContainer::Initialize()
{
	RefSkeleton = NULL;
	AssetSkeletalMesh = Cast<USkeletalMesh>(Asset.Get());
	if( AssetSkeletalMesh.IsValid() )
	{
		RefSkeleton = &AssetSkeletalMesh->RefSkeleton;
		AssetSkeleton = AssetSkeletalMesh->Skeleton;
	}
	else
	{
		AssetSkeleton = Cast<USkeleton>(Asset.Get());
		if( AssetSkeleton.IsValid() )
		{
			RefSkeleton = &AssetSkeleton->GetReferenceSkeleton();
		}
	}
	// Only supports SkeletalMeshes or Skeletons.
	check( AssetSkeletalMesh.Get() || AssetSkeleton.Get() );
	// Skeleton should always be there.
	checkf( AssetSkeleton.Get(), TEXT("%s missing skeleton"), *GetNameSafe(AssetSkeletalMesh.Get()));
	check( RefSkeleton );

	// Take biggest amount of bones between SkeletalMesh and Skeleton for BoneSwitchArray.
	// SkeletalMesh can have less, but AnimSequences tracks will map to Skeleton which can have more.
	const int32 MaxBones = AssetSkeleton.IsValid() ? FMath::Max<int32>(RefSkeleton->GetNum(), AssetSkeleton->GetReferenceSkeleton().GetNum()) : RefSkeleton->GetNum();

	// Initialize BoneSwitchArray.
	BoneSwitchArray.Init(false, MaxBones);
	const int32 NumRequiredBones = BoneIndicesArray.Num();
	for(int32 Index=0; Index<NumRequiredBones; Index++)
	{
		const FBoneIndexType BoneIndex = BoneIndicesArray[Index];
		checkSlow( BoneIndex < MaxBones );
		BoneSwitchArray[BoneIndex] = true;
	}

	// Clear remapping table
	SkeletonToPoseBoneIndexArray.Empty();

	// Cache our mapping tables
	// Here we create look up tables between our target asset and its USkeleton's refpose.
	// Most times our Target is a SkeletalMesh
	if( AssetSkeletalMesh.IsValid() )
	{
		RemapFromSkelMesh(*AssetSkeletalMesh.Get(), *AssetSkeleton.Get());
	}
	// But we also support a Skeleton's RefPose.
	else
	{
		// Right now we only support a single Skeleton. Skeleton hierarchy coming soon!
		RemapFromSkeleton(*AssetSkeleton.Get());
	}
}

int32 FBoneContainer::GetPoseBoneIndexForBoneName(const FName& BoneName) const
{
	checkSlow( IsValid() );
	return RefSkeleton->FindBoneIndex(BoneName);
}

int32 FBoneContainer::GetParentBoneIndex(const int32& BoneIndex) const
{
	checkSlow( IsValid() );
	checkSlow(BoneIndex != INDEX_NONE);
	return RefSkeleton->GetParentIndex(BoneIndex);
}

int32 FBoneContainer::GetDepthBetweenBones(const int32& BoneIndex, const int32& ParentBoneIndex) const
{
	checkSlow( IsValid() );
	checkSlow( BoneIndex != INDEX_NONE );
	return RefSkeleton->GetDepthBetweenBones(BoneIndex, ParentBoneIndex);
}

bool FBoneContainer::BoneIsChildOf(const int32& BoneIndex, const int32& ParentBoneIndex) const
{
	checkSlow( IsValid() );
	checkSlow( (BoneIndex != INDEX_NONE) && (ParentBoneIndex != INDEX_NONE) );
	return RefSkeleton->BoneIsChildOf(BoneIndex, ParentBoneIndex);
}

void FBoneContainer::RemapFromSkelMesh(USkeletalMesh const & SourceSkeletalMesh, USkeleton & TargetSkeleton)
{
	int32 const SkelMeshLinkupIndex = TargetSkeleton.GetMeshLinkupIndex(&SourceSkeletalMesh);
	check(SkelMeshLinkupIndex != INDEX_NONE);

	FSkeletonToMeshLinkup const & LinkupTable = TargetSkeleton.LinkupCache[SkelMeshLinkupIndex];

	// Map SkeletonBoneIndex to the SkeletalMesh Bone Index, taking into account the required bone index array.
	SkeletonToPoseBoneIndexArray.Init(INDEX_NONE, LinkupTable.SkeletonToMeshTable.Num());

	for(int32 Index=0; Index<BoneIndicesArray.Num(); Index++)
	{
		int32 const & PoseBoneIndex = BoneIndicesArray[Index];
		checkSlow( (PoseBoneIndex != INDEX_NONE) && (PoseBoneIndex < GetNumBones()) );
		int32 const & SkeletonIndex = LinkupTable.MeshToSkeletonTable[PoseBoneIndex];
		if( SkeletonIndex != INDEX_NONE )
		{
			SkeletonToPoseBoneIndexArray[SkeletonIndex] = PoseBoneIndex;
		}
	}
}

void FBoneContainer::RemapFromSkeleton(USkeleton const & SourceSkeleton)
{
	// Map SkeletonBoneIndex to the SkeletalMesh Bone Index, taking into account the required bone index array.
	SkeletonToPoseBoneIndexArray.Init(INDEX_NONE, SourceSkeleton.GetRefLocalPoses().Num());

	for(int32 Index=0; Index<BoneIndicesArray.Num(); Index++)
	{
		int32 const & PoseBoneIndex = BoneIndicesArray[Index];
		SkeletonToPoseBoneIndexArray[PoseBoneIndex] = PoseBoneIndex;
	}
}

